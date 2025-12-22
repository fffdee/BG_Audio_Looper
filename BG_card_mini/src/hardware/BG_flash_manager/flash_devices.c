/**
 * flash_devices.c - Flash device registration and management implementation
 */

#include "flash_devices.h"
#include "debug.h"
#include "gpio.h"
#include <string.h>

/*===========================================================================
 * CS Pin Control Functions
 *===========================================================================*/

/* Flash #0 CS control */
static void flash0_cs_init(void)
{
    /* Configure as GPIO output mode, initial state is high (not selected) */
    GPIO_RegOneBitClear(GPIO_A_IE, FLASH0_CS_GPIO_MASK);   /* Disable input */
    GPIO_RegOneBitSet(GPIO_A_OE, FLASH0_CS_GPIO_MASK);     /* Enable output */
    GPIO_RegOneBitSet(GPIO_A_OUT, FLASH0_CS_GPIO_MASK);    /* Output high */
}

static void flash0_cs_select(void)
{
    GPIO_RegOneBitClear(GPIO_A_OUT, FLASH0_CS_GPIO_MASK);  /* Output low */
}

static void flash0_cs_deselect(void)
{
    GPIO_RegOneBitSet(GPIO_A_OUT, FLASH0_CS_GPIO_MASK);    /* Output high */
}

/* Flash #1 CS control */
static void flash1_cs_init(void)
{
    /* Configure as GPIO output mode, initial state is high (not selected) */
    GPIO_RegOneBitClear(GPIO_A_IE, FLASH1_CS_GPIO_MASK);   /* Disable input */
    GPIO_RegOneBitSet(GPIO_A_OE, FLASH1_CS_GPIO_MASK);     /* Enable output */
    GPIO_RegOneBitSet(GPIO_A_OUT, FLASH1_CS_GPIO_MASK);    /* Output high */
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
 * Device Instances
 *===========================================================================*/

static FlashDevice_t *g_flash0 = NULL;  /* System Flash */
static FlashDevice_t *g_flash1 = NULL;  /* Storage Flash */
static bool g_devices_initialized = false;

/*===========================================================================
 * Device Initialization
 *===========================================================================*/

FlashStatus_t FlashDevices_Init(void)
{
    FlashStatus_t ret;
    
    if (g_devices_initialized) {
        return FLASH_OK;
    }
    
    DBG("[FlashDevices] Initializing...\n");
    
    /* Initialize bus */
    FlashBus_Init();
    
    /* Create Flash #0 (System Flash) */
    g_flash0 = W25Qxx_Create("flash0_sys",
                             flash0_cs_select,
                             flash0_cs_deselect,
                             flash0_cs_init);
    if (!g_flash0) {
        DBG("[FlashDevices] Failed to create flash0\n");
        return FLASH_ERR_NOMEM;
    }
    
    /* Register to bus */
    ret = FlashBus_Register(g_flash0);
    if (ret != FLASH_OK) {
        DBG("[FlashDevices] Failed to register flash0\n");
        W25Qxx_Destroy(g_flash0);
        g_flash0 = NULL;
        return ret;
    }
    
    /* Initialize device */
    ret = FlashDev_Init(g_flash0);
    if (ret != FLASH_OK) {
        DBG("[FlashDevices] Failed to init flash0\n");
        /* Continue execution, device may be temporarily offline */
    }
    
    /* Create Flash #1 (Storage Flash) - only if hardware exists */
#if FLASH1_CS_PIN != 0  /* If Flash#1 is configured */
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
    
    /* Print device information */
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
 * Shell Commands
 *===========================================================================*/

void FlashDevices_RegisterShellCommands(void)
{
    /* Shell commands registered via FlashBus_ShellCmd */
    /* Add in shell_commands.c:
     *   {"flash", FlashBus_ShellCmd, "Flash operations"}
     */
    DBG("[FlashDevices] Shell commands: use 'flash' command\n");
}

/*===========================================================================
 * Partition Operation Implementation
 *===========================================================================*/

/* System Partition (Flash#0 first 1MB) */
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

/* Looper Partition (Flash#0 last 7MB) */
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

/* Storage Partition (Flash#1 entire 8MB) */
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
