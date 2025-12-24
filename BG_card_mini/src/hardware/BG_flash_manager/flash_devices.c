/**
 * flash_devices.c - Flash设备注册和管理实现
 */

#include "flash_devices.h"
#include "debug.h"
#include "gpio.h"
#include <string.h>

/*===========================================================================
 * CS引脚控制函数
 *===========================================================================*/

/* Flash #0 CS控制 */
static void flash0_cs_init(void)
{
    /* 配置为GPIO输出模式，初始为高电平（未选中） */
    GPIO_RegOneBitClear(GPIO_A_IE, FLASH0_CS_GPIO_MASK);   /* 关闭输入 */
    GPIO_RegOneBitSet(GPIO_A_OE, FLASH0_CS_GPIO_MASK);     /* 使能输出 */
    GPIO_RegOneBitSet(GPIO_A_OUT, FLASH0_CS_GPIO_MASK);    /* 输出高电平 */
}

static void flash0_cs_select(void)
{
    GPIO_RegOneBitClear(GPIO_A_OUT, FLASH0_CS_GPIO_MASK);  /* 输出低电平 */
}

static void flash0_cs_deselect(void)
{
    GPIO_RegOneBitSet(GPIO_A_OUT, FLASH0_CS_GPIO_MASK);    /* 输出高电平 */
}

/* Flash #1 CS控制 */
static void flash1_cs_init(void)
{
    /* 配置为GPIO输出模式，初始为高电平（未选中） */
    GPIO_RegOneBitClear(GPIO_A_IE, FLASH1_CS_GPIO_MASK);   /* 关闭输入 */
    GPIO_RegOneBitSet(GPIO_A_OE, FLASH1_CS_GPIO_MASK);     /* 使能输出 */
    GPIO_RegOneBitSet(GPIO_A_OUT, FLASH1_CS_GPIO_MASK);    /* 输出高电平 */
}

static void flash1_cs_select(void)
{
    GPIO_RegOneBitClear(GPIO_A_OUT, FLASH1_CS_GPIO_MASK);
}

static void flash1_cs_deselect(void)
{
    GPIO_RegOneBitSet(GPIO_A_OUT, FLASH1_CS_GPIO_MASK);
}

/*===========================================================================
 * 设备实例
 *===========================================================================*/

static FlashDevice_t *g_flash0 = NULL;  /* 系统Flash */
static FlashDevice_t *g_flash1 = NULL;  /* 存储Flash */
static bool g_devices_initialized = false;

/*===========================================================================
 * 设备初始化
 *===========================================================================*/

FlashStatus_t FlashDevices_Init(void)
{
    FlashStatus_t ret;
    
    if (g_devices_initialized) {
        return FLASH_OK;
    }
    
    DBG("[FlashDevices] Initializing...\n");
    
    /* 初始化总线 */
    FlashBus_Init();
    
    /* 创建Flash #0 (系统Flash) */
    g_flash0 = W25Qxx_Create("flash0_sys",
                             flash0_cs_select,
                             flash0_cs_deselect,
                             flash0_cs_init);
    if (!g_flash0) {
        DBG("[FlashDevices] Failed to create flash0\n");
        return FLASH_ERR_NOMEM;
    }
    
    /* 注册到总线 */
    ret = FlashBus_Register(g_flash0);
    if (ret != FLASH_OK) {
        DBG("[FlashDevices] Failed to register flash0\n");
        W25Qxx_Destroy(g_flash0);
        g_flash0 = NULL;
        return ret;
    }
    
    /* 初始化设备 */
    ret = FlashDev_Init(g_flash0);
    if (ret != FLASH_OK) {
        DBG("[FlashDevices] Failed to init flash0\n");
        /* 继续执行，设备可能暂时离线 */
    }
    
    /* 创建Flash #1 (存储Flash) - 仅当硬件存在时 */
#if FLASH1_CS_PIN != 0  /* 如果配置了Flash#1 */
    g_flash1 = W25Qxx_Create("flash1_stor",
                             flash1_cs_select,
                             flash1_cs_deselect,
                             flash1_cs_init);
    if (g_flash1) {
        ret = FlashBus_Register(g_flash1);
        if (ret == FLASH_OK) {
            ret = FlashDev_Init(g_flash1);
            if (ret != FLASH_OK) {
                DBG("[FlashDevices] Flash1 init failed (may not present)\n");
            }
        }
    }
#endif
    
    g_devices_initialized = true;
    
    /* 打印设备信息 */
    FlashBus_PrintInfo();
    
    DBG("[FlashDevices] Initialized\n");
    return FLASH_OK;
}

void FlashDevices_DeInit(void)
{
    if (!g_devices_initialized) {
        return;
    }
    
    if (g_flash1) {
        FlashBus_Unregister(g_flash1);
        W25Qxx_Destroy(g_flash1);
        g_flash1 = NULL;
    }
    
    if (g_flash0) {
        FlashBus_Unregister(g_flash0);
        W25Qxx_Destroy(g_flash0);
        g_flash0 = NULL;
    }
    
    FlashBus_DeInit();
    
    g_devices_initialized = false;
    DBG("[FlashDevices] DeInitialized\n");
}

FlashDevice_t* FlashDevices_GetSystemFlash(void)
{
    return g_flash0;
}

FlashDevice_t* FlashDevices_GetStorageFlash(void)
{
    return g_flash1;
}

/*===========================================================================
 * Shell命令
 *===========================================================================*/

void FlashDevices_RegisterShellCommands(void)
{
    /* Shell命令通过 FlashBus_ShellCmd 注册 */
    /* 在shell_commands.c中添加:
     *   {"flash", FlashBus_ShellCmd, "Flash operations"}
     */
    DBG("[FlashDevices] Shell commands: use 'flash' command\n");
}

/*===========================================================================
 * 分区操作实现
 *===========================================================================*/

/* 系统分区 (Flash#0 前1MB) */
FlashStatus_t FlashPartition_SystemRead(uint32_t offset, uint8_t *buf, uint32_t len)
{
    if (!g_flash0 || !g_flash0->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset + len > FLASH0_PARTITION_SYSTEM_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_Read(g_flash0, FLASH0_PARTITION_SYSTEM_START + offset, buf, len);
}

FlashStatus_t FlashPartition_SystemWrite(uint32_t offset, const uint8_t *buf, uint32_t len)
{
    if (!g_flash0 || !g_flash0->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset + len > FLASH0_PARTITION_SYSTEM_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_Write(g_flash0, FLASH0_PARTITION_SYSTEM_START + offset, buf, len);
}

FlashStatus_t FlashPartition_SystemEraseSector(uint32_t offset)
{
    if (!g_flash0 || !g_flash0->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset >= FLASH0_PARTITION_SYSTEM_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_EraseSector(g_flash0, FLASH0_PARTITION_SYSTEM_START + offset);
}

/* Looper分区 (Flash#0 后7MB) */
FlashStatus_t FlashPartition_LooperRead(uint32_t offset, uint8_t *buf, uint32_t len)
{
    if (!g_flash0 || !g_flash0->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset + len > FLASH0_PARTITION_LOOPER_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_Read(g_flash0, FLASH0_PARTITION_LOOPER_START + offset, buf, len);
}

FlashStatus_t FlashPartition_LooperWrite(uint32_t offset, const uint8_t *buf, uint32_t len)
{
    if (!g_flash0 || !g_flash0->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset + len > FLASH0_PARTITION_LOOPER_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_Write(g_flash0, FLASH0_PARTITION_LOOPER_START + offset, buf, len);
}

FlashStatus_t FlashPartition_LooperEraseSector(uint32_t offset)
{
    if (!g_flash0 || !g_flash0->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset >= FLASH0_PARTITION_LOOPER_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_EraseSector(g_flash0, FLASH0_PARTITION_LOOPER_START + offset);
}

FlashStatus_t FlashPartition_LooperEraseBlock(uint32_t offset)
{
    if (!g_flash0 || !g_flash0->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset >= FLASH0_PARTITION_LOOPER_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_EraseBlock(g_flash0, FLASH0_PARTITION_LOOPER_START + offset);
}

/* 存储分区 (Flash#1 全部8MB) */
FlashStatus_t FlashPartition_StorageRead(uint32_t offset, uint8_t *buf, uint32_t len)
{
    if (!g_flash1 || !g_flash1->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset + len > FLASH1_PARTITION_STORAGE_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_Read(g_flash1, FLASH1_PARTITION_STORAGE_START + offset, buf, len);
}

FlashStatus_t FlashPartition_StorageWrite(uint32_t offset, const uint8_t *buf, uint32_t len)
{
    if (!g_flash1 || !g_flash1->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset + len > FLASH1_PARTITION_STORAGE_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_Write(g_flash1, FLASH1_PARTITION_STORAGE_START + offset, buf, len);
}

FlashStatus_t FlashPartition_StorageEraseSector(uint32_t offset)
{
    if (!g_flash1 || !g_flash1->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset >= FLASH1_PARTITION_STORAGE_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_EraseSector(g_flash1, FLASH1_PARTITION_STORAGE_START + offset);
}

FlashStatus_t FlashPartition_StorageEraseBlock(uint32_t offset)
{
    if (!g_flash1 || !g_flash1->initialized) {
        return FLASH_ERR_NOT_INIT;
    }
    if (offset >= FLASH1_PARTITION_STORAGE_SIZE) {
        return FLASH_ERR_PARAM;
    }
    return FlashDev_EraseBlock(g_flash1, FLASH1_PARTITION_STORAGE_START + offset);
}
