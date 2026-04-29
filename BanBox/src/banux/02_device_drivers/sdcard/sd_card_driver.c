/**
 * @file sd_card_driver.c
 * @brief SD卡驱动实现
 */

#include "sd_card_driver.h"
#include "hal_sdio.h"
#include "product_def.h"
#include <string.h>
#include <stdio.h>
#include "rtos_api.h"

#define DBG(format, ...) printf("[SD_CARD] " format, ##__VA_ARGS__)

/* SD卡私有数据 */
typedef struct {
    HAL_SD_CardInfo_t info;
    bool initialized;
} SDCard_Private_t;

/* 前向声明 */
static int SDCard_Init_Op(FlashDevice_t *dev);
static int SDCard_Deinit_Op(FlashDevice_t *dev);
static int SDCard_Read_Op(FlashDevice_t *dev, uint32_t addr, uint8_t *buf, uint32_t size);
static int SDCard_Write_Op(FlashDevice_t *dev, uint32_t addr, const uint8_t *buf, uint32_t size);
static int SDCard_Erase_Op(FlashDevice_t *dev, uint32_t addr, uint32_t size);
static int SDCard_GetCapacity_Op(FlashDevice_t *dev, uint32_t *capacity);
static int SDCard_GetBlockSize_Op(FlashDevice_t *dev, uint32_t *block_size);

/* SD卡操作接口 */
static const FlashOps_t s_sdcard_ops = {
    .init = (FlashStatus_t (*)(FlashDevice_t *))SDCard_Init_Op,
    .deinit = (FlashStatus_t (*)(FlashDevice_t *))SDCard_Deinit_Op,
    .read = (FlashStatus_t (*)(FlashDevice_t *, uint32_t, uint8_t *, uint32_t))SDCard_Read_Op,
    .write = (FlashStatus_t (*)(FlashDevice_t *, uint32_t, const uint8_t *, uint32_t))SDCard_Write_Op,
};

/* ============================================================
 * 轻量 FAT32 根目录扫描 — 打印 .sf2 音源信息
 * 不依赖 SYNTH_SD_NAND_PSRAM_EN，无堆内存分配，纯栈操作。
 * ============================================================ */

/** 直接读一个 512B 扇区 */
static int sdcard_read_block(uint32_t lba, uint8_t *buf)
{
    HAL_SD_Error_t err = HAL_SD_ReadBlocks(lba, buf, 1);
    return (err == HAL_SD_OK) ? 0 : -1;
}

/**
 * @brief 扫描 SD 卡根目录，打印所有 .SF2 音源的文件名和大小
 *
 * 解析 MBR → BPB，遍历根目录 8.3 短文件名条目，
 * 匹配扩展名 "SF2"。仅扫描根目录，不递归子目录。
 */
static void SDCard_ScanSF2(void)
{
    uint8_t  sec[512];
    uint32_t part_lba;
    uint32_t spc;
    uint32_t rsvd;
    uint32_t num_fats;
    uint32_t fat_sz;
    uint32_t root_lba;
    uint32_t root_sectors; /* 根目录占用扇区数 */
    uint32_t s;
    uint32_t i;
    uint32_t j;
    uint16_t root_ent_cnt;
    uint16_t fat_sz16;
    uint8_t  attr;
    uint8_t  name[9];
    uint8_t  ext[3];
    uint32_t fsize;
    int      found    = 0;
    int      dir_done = 0;

    /* -------- 步骤 1: 读扇区 0 -------- */
    if (sdcard_read_block(0, sec) != 0) {
        DBG("[SF2 Scan] Read sector 0 failed\n");
        return;
    }
    if (sec[510] != 0x55 || sec[511] != 0xAA) {
        DBG("[SF2 Scan] Bad boot signature\n");
        return;
    }

    /* -------- 步骤 2: 判断扇区 0 是 BPB 还是 MBR -------- */
    /*
     * FAT BPB 固定以 jmp 指令开头: 0xEB(jmp short) 或 0xE9(jmp near)。
     * MBR 以 x86 引导代码开头（如 0x33/xor，0xFA/cli 等）。
     * Windows 对 ≤2GB SD 卡默认格式化为 FAT16, super-floppy 格式（无 MBR）。
     */
    if (sec[0] == 0xEBU || sec[0] == 0xE9U) {
        part_lba = 0;
        DBG("[SF2 Scan] Super-floppy format (no MBR)\n");
    } else {
        part_lba = (uint32_t)sec[454]
                 | ((uint32_t)sec[455] << 8)
                 | ((uint32_t)sec[456] << 16)
                 | ((uint32_t)sec[457] << 24);
        if (part_lba == 0 || part_lba > 0x10000000UL) {
            DBG("[SF2 Scan] Invalid partition LBA: %u\n", part_lba);
            return;
        }
        if (sdcard_read_block(part_lba, sec) != 0) {
            DBG("[SF2 Scan] BPB read failed (LBA=%u)\n", part_lba);
            return;
        }
        if (sec[510] != 0x55 || sec[511] != 0xAA) {
            DBG("[SF2 Scan] Bad BPB signature at LBA=%u\n", part_lba);
            return;
        }
    }

    /* 校验 BytesPerSector == 512 */
    if (((uint32_t)sec[11] | ((uint32_t)sec[12] << 8)) != 512U) {
        DBG("[SF2 Scan] Unsupported sector size\n");
        return;
    }

    spc          = sec[13];
    rsvd         = (uint32_t)sec[14] | ((uint32_t)sec[15] << 8);
    num_fats     = sec[16];
    root_ent_cnt = (uint16_t)sec[17] | ((uint16_t)sec[18] << 8);
    fat_sz16     = (uint16_t)sec[22] | ((uint16_t)sec[23] << 8);

    if (spc == 0) {
        DBG("[SF2 Scan] Invalid BPB: spc=0\n");
        return;
    }

    if (root_ent_cnt != 0) {
        /*
         * FAT16 / FAT12:
         *   根目录位于 FAT 区之后的固定区域，条目数固定为 root_ent_cnt。
         *   root_lba = part_lba + rsvd + num_fats * fat_sz16
         *   root_sectors = ceil(root_ent_cnt * 32 / 512)
         */
        fat_sz       = fat_sz16;
        root_lba     = part_lba + rsvd + num_fats * fat_sz;
        root_sectors = ((uint32_t)root_ent_cnt * 32U + 511U) / 512U;
        DBG("[SF2 Scan] FAT16, root dir LBA=%u, sectors=%u\n", root_lba, root_sectors);
    } else {
        /*
         * FAT32:
         *   根目录以簇形式存储，簇号在 BPB 偏移 44。
         *   data_lba = part_lba + rsvd + num_fats * fat_sz32
         *   root_lba = data_lba + (root_clust - 2) * spc
         *   每次扫描 spc 个扇区（一个簇，不跟随簇链，仅扫根目录第一簇）。
         */
        uint32_t fat_sz32   = (uint32_t)sec[36] | ((uint32_t)sec[37] << 8)
                            | ((uint32_t)sec[38] << 16) | ((uint32_t)sec[39] << 24);
        uint32_t root_clust = (uint32_t)sec[44] | ((uint32_t)sec[45] << 8)
                            | ((uint32_t)sec[46] << 16) | ((uint32_t)sec[47] << 24);
        if (root_clust < 2) {
            DBG("[SF2 Scan] Invalid FAT32 root_clust=%u\n", root_clust);
            return;
        }
        fat_sz       = fat_sz32;
        root_lba     = part_lba + rsvd + num_fats * fat_sz + (root_clust - 2U) * spc;
        root_sectors = spc;
        DBG("[SF2 Scan] FAT32, root dir LBA=%u, sectors=%u\n", root_lba, root_sectors);
    }

    /* -------- 步骤 3: 遍历根目录扇区 -------- */
    for (s = 0; s < root_sectors && !dir_done; s++) {
        if (sdcard_read_block(root_lba + s, sec) != 0) break;

        for (i = 0; i < 512U && !dir_done; i += 32U) {
            if (sec[i] == 0x00U) { dir_done = 1; break; } /* 目录结束 */
            if (sec[i] == 0xE5U) continue;                /* 已删除条目 */

            attr = sec[i + 11U];
            /* 跳过 LFN 条目(0x0F)、子目录(0x10)、卷标(0x08) */
            if (attr == 0x0FU || (attr & 0x18U)) continue;

            /* 检查 3 字节扩展名是否为 "SF2"（FAT32 8.3 名总是大写） */
            for (j = 0; j < 3U; j++) ext[j] = sec[i + 8U + j];
            if (ext[0] != 'S' || ext[1] != 'F' || ext[2] != '2') continue;

            /* 提取主文件名 8 字节，去掉尾部空格 */
            for (j = 0; j < 8U; j++) name[j] = sec[i + j];
            name[8] = '\0';
            for (j = 7U; j > 0U && name[j] == ' '; j--) name[j] = '\0';

            /* 文件大小位于条目偏移 28 (LE u32) */
            fsize = (uint32_t)sec[i + 28U]
                  | ((uint32_t)sec[i + 29U] << 8)
                  | ((uint32_t)sec[i + 30U] << 16)
                  | ((uint32_t)sec[i + 31U] << 24);

            DBG("[SF2] %-8s.SF2  %u bytes (%u KB)\n",
                name, fsize, fsize / 1024U);
            found++;
        }
    }

    if (found == 0) {
        DBG("[SF2 Scan] No .sf2 files found in root directory\n");
    } else {
        DBG("[SF2 Scan] Total: %d SF2 file(s) found\n", found);
    }
}

/**
 * @brief 初始化SD卡
 */
static int SDCard_Init_Op(FlashDevice_t *dev)
{
    SDCard_Private_t *priv = (SDCard_Private_t *)dev->priv;
    HAL_SD_Error_t err;
    
    if (priv->initialized) {
        return 0;  /* 已初始化 */
    }
    
    /* 初始化SDIO端口（使用板级配置宏 HW_SDIO_PORT） */
    err = HAL_SDIO_Init(HW_SDIO_PORT);
    if (err != HAL_SD_OK) {
        DBG("SDIO port init failed: %d\n", err);
        return -1;
    }
    
    /* 检测SD卡
     * SDCard_Detect() 通过 SDIO 命令 (CMD0/CMD8/ACMD41) 检测卡是否在位，
     * 不依赖独立 DET GPIO 引脚，对所有平台（含BANBOX_II）均适用。
     * 无卡时约 100ms 返回；若跳过此步直接 SDCard_Init()，
     * 内部会重试 4 次 x 2000ms = 8 秒才返回错误。
     */
    if (!HAL_SD_Detect()) {
        DBG("No SD card detected\n");
        return -1;
    }
    
    /* 初始化SD卡 */
    err = HAL_SD_Init();
    if (err != HAL_SD_OK) {
        DBG("SD card init failed (err=%d) - card may not be inserted\n", err);
        return -1;
    }
    
    /* 获取SD卡信息 */
    err = HAL_SD_GetInfo(&priv->info);
    if (err != HAL_SD_OK) {
        DBG("Get SD card info failed: %d\n", err);
        return -1;
    }
    
    priv->initialized = true;
    
    /* 同步到 dev->info 供 FlashDev_PrintInfo / flash_test.c 使用 */
    dev->info.total_size  = (uint32_t)(priv->info.capacity_bytes > 0xFFFFFFFFU
                             ? 0xFFFFFFFFU : (uint32_t)priv->info.capacity_bytes);
    dev->info.block_size  = priv->info.block_size;
    dev->info.block_count = priv->info.block_count;
    dev->info.page_size   = priv->info.block_size;   /* SD: page = block = 512B */
    dev->info.sector_size = priv->info.block_size;
    dev->info.mfg_id      = 0;
    dev->info.mem_type    = 0;
    dev->info.dev_id      = (uint8_t)(priv->info.type);
    
    DBG("SD card initialized successfully\n");
    DBG("  Capacity: %u MB\n",
        (uint32_t)(priv->info.capacity_bytes / (1024 * 1024)));
    DBG("  Blocks: %u, Block size: %u\n",
        priv->info.block_count,
        priv->info.block_size);

    /* 扫描 SD 根目录，打印所有 .sf2 音源文件信息 */
    SDCard_ScanSF2();

    return 0;
}

/**
 * @brief 去初始化SD卡
 */
static int SDCard_Deinit_Op(FlashDevice_t *dev)
{
    SDCard_Private_t *priv = (SDCard_Private_t *)dev->priv;
    
    if (!priv->initialized) {
        return 0;
    }
    
    HAL_SDIO_Deinit(HW_SDIO_PORT);
    priv->initialized = false;
    
    DBG("SD card deinitialized\n");
    return 0;
}

/**
 * @brief 读取SD卡数据
 * @param addr 字节地址
 * @param buf 读取缓冲区
 * @param size 读取字节数
 */
static int SDCard_Read_Op(FlashDevice_t *dev, uint32_t addr, uint8_t *buf, uint32_t size)
{
    SDCard_Private_t *priv = (SDCard_Private_t *)dev->priv;
    HAL_SD_Error_t err;
    uint32_t block;
    uint32_t block_count;
    
    if (!priv->initialized) {
        DBG("SD card not initialized\n");
        return -1;
    }
    
    if (addr % SD_CARD_BLOCK_SIZE != 0 || size % SD_CARD_BLOCK_SIZE != 0) {
        DBG("Address and size must be block-aligned (512 bytes)\n");
        return -1;
    }
    
    block = addr / SD_CARD_BLOCK_SIZE;
    block_count = size / SD_CARD_BLOCK_SIZE;
    
    if (block + block_count > priv->info.block_count) {
        DBG("Read exceeds card capacity\n");
        return -1;
    }
    
    err = HAL_SD_ReadBlocks(block, buf, block_count);
    if (err != HAL_SD_OK) {
        DBG("Read failed at block %u: %d\n", block, err);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 写入SD卡数据
 * @param addr 字节地址
 * @param buf 写入数据
 * @param size 写入字节数
 */
static int SDCard_Write_Op(FlashDevice_t *dev, uint32_t addr, const uint8_t *buf, uint32_t size)
{
    SDCard_Private_t *priv = (SDCard_Private_t *)dev->priv;
    HAL_SD_Error_t err;
    uint32_t block;
    uint32_t block_count;
    
    if (!priv->initialized) {
        DBG("SD card not initialized\n");
        return -1;
    }
    
    if (addr % SD_CARD_BLOCK_SIZE != 0 || size % SD_CARD_BLOCK_SIZE != 0) {
        DBG("Address and size must be block-aligned (512 bytes)\n");
        return -1;
    }
    
    block = addr / SD_CARD_BLOCK_SIZE;
    block_count = size / SD_CARD_BLOCK_SIZE;
    
    if (block + block_count > priv->info.block_count) {
        DBG("Write exceeds card capacity\n");
        return -1;
    }
    
    err = HAL_SD_WriteBlocks(block, buf, block_count);
    if (err != HAL_SD_OK) {
        DBG("Write failed at block %u: %d\n", block, err);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 擦除SD卡数据（SD卡无需擦除，直接返回成功）
 */
static int SDCard_Erase_Op(FlashDevice_t *dev, uint32_t addr, uint32_t size)
{
    /* SD卡不需要擦除操作 */
    (void)dev;
    (void)addr;
    (void)size;
    return 0;
}

/**
 * @brief 获取SD卡容量
 */
static int SDCard_GetCapacity_Op(FlashDevice_t *dev, uint32_t *capacity)
{
    SDCard_Private_t *priv = (SDCard_Private_t *)dev->priv;
    
    if (!priv->initialized) {
        return -1;
    }
    
    *capacity = (uint32_t)priv->info.capacity_bytes;
    return 0;
}

/**
 * @brief 获取SD卡块大小
 */
static int SDCard_GetBlockSize_Op(FlashDevice_t *dev, uint32_t *block_size)
{
    SDCard_Private_t *priv = (SDCard_Private_t *)dev->priv;
    
    if (!priv->initialized) {
        *block_size = SD_CARD_BLOCK_SIZE;  /* 默认512字节 */
        return 0;
    }
    
    *block_size = priv->info.block_size;
    return 0;
}

/**
 * @brief 创建SD卡设备
 */
FlashDevice_t* SDCard_Create(const char *name)
{
    FlashDevice_t *dev;
    SDCard_Private_t *priv;
    
    /* 分配设备结构 */
    dev = (FlashDevice_t *)pvPortMalloc(sizeof(FlashDevice_t));
    if (!dev) {
        DBG("Failed to allocate device\n");
        return NULL;
    }
    memset(dev, 0, sizeof(FlashDevice_t));
    
    /* 分配私有数据 */
    priv = (SDCard_Private_t *)pvPortMalloc(sizeof(SDCard_Private_t));
    if (!priv) {
        DBG("Failed to allocate private data\n");
        vPortFree(dev);
        return NULL;
    }
    memset(priv, 0, sizeof(SDCard_Private_t));
    
    /* 初始化设备 */
    strncpy(dev->name, name, FLASH_NAME_MAX_LEN - 1);
    dev->name[FLASH_NAME_MAX_LEN - 1] = '\0';
    dev->type = FLASH_TYPE_SDCARD;
    dev->ops = &s_sdcard_ops;
    dev->priv = priv;
    dev->initialized = false;
    
    return dev;
}

/**
 * @brief 销毁SD卡设备
 */
void SDCard_Destroy(FlashDevice_t *dev)
{
    if (!dev) {
        return;
    }
    
    if (dev->priv) {
        vPortFree(dev->priv);
    }
    
    vPortFree(dev);
}

/**
 * @brief 获取SD卡操作接口
 */
const FlashOps_t* SDCard_GetOps(void)
{
    return &s_sdcard_ops;
}
