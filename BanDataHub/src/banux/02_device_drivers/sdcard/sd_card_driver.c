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
 * SD卡操作接口
 * ============================================================ */
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

    /* 重置 SDK 全局状态 SDCard.CardInit，否则重新初始化时
     * SDCard_Detect() 会走 CMD13 分支（认为卡已初始化），
     * 但 SDIO 控制器已关闭，CMD13 必然失败。
     * 重置为 SD_NOINIT 后，SDCard_Detect() 会重新走
     * SDCard_ControllerInit() + CMD0/CMD8/ACMD41 完整检测流程。
     */
    extern SD_CARD SDCard;
    SDCard.CardInit = SD_NOINIT;

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
