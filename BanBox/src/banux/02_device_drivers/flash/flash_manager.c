/**
 * flash_manager.c - Flash management layer implementation
 * 
 * Manage two W25Q64 NOR Flash chips:
 * - Flash #0: First 1MB system settings + Last 7MB for Looper
 * - Flash #1: 8MB pure storage
 */

#include "flash_manager.h"
#include "debug.h"
#include <string.h>

/*===========================================================================
 * CS Pin Control Functions
 *===========================================================================*/

/* Flash #0 CS control (GPIOA21) */
static void flash0_cs_enable(bool enable)
{
    if (enable) {
        FLASH_NOR0_CS_ENABLE();
    } else {
        FLASH_NOR0_CS_DISABLE();
    }
}

/* Flash #1 CS control (GPIOA23) */
static void flash1_cs_enable(bool enable)
{
    if (enable) {
        FLASH_NOR1_CS_ENABLE();
    } else {
        FLASH_NOR1_CS_DISABLE();
    }
}

/*===========================================================================
 * Flash Manager Singleton
 *===========================================================================*/

static FlashManager_t g_flash_manager = {0};

/*===========================================================================
 * Initialization and Deinitialization
 *===========================================================================*/

FlashStatus_t FlashManager_Init(void)
{
    FlashStatus_t ret;
    
    if (g_flash_manager.initialized) {
        return FLASH_OK;
    }
    
    DBG("FlashManager: Initializing...\n");
    
    /* Initialize CS pins */
    FLASH_NOR0_CS_INIT();
    FLASH_NOR1_CS_INIT();
    FLASH_WP_INIT();
    
    /* Disable all CS */
    FLASH_NOR0_CS_DISABLE();
    FLASH_NOR1_CS_DISABLE();
    FLASH_WP_DISABLE();
    
    /* Create Flash #0 driver */
    g_flash_manager.flash[FLASH_DEV_0] = FlashDriver_CreateNOR(0, flash0_cs_enable, NULL);
    if (!g_flash_manager.flash[FLASH_DEV_0]) {
        DBG("FlashManager: Failed to create Flash #0 driver\n");
        return FLASH_ERROR_PARAM;
    }
    
    /* Create Flash #1 driver */
    g_flash_manager.flash[FLASH_DEV_1] = FlashDriver_CreateNOR(1, flash1_cs_enable, NULL);
    if (!g_flash_manager.flash[FLASH_DEV_1]) {
        DBG("FlashManager: Failed to create Flash #1 driver\n");
        return FLASH_ERROR_PARAM;
    }
    
    /* Initialize Flash #0 */
    ret = g_flash_manager.flash[FLASH_DEV_0]->init(g_flash_manager.flash[FLASH_DEV_0]);
    if (ret != FLASH_OK) {
        DBG("FlashManager: Flash #0 init failed\n");
        return ret;
    }
    
    /* Initialize Flash #1 */
    ret = g_flash_manager.flash[FLASH_DEV_1]->init(g_flash_manager.flash[FLASH_DEV_1]);
    if (ret != FLASH_OK) {
        DBG("FlashManager: Flash #1 init failed\n");
        return ret;
    }
    
    /* Set partition information */
    /* System settings partition - Flash #0 first 1MB */
    g_flash_manager.partitions[PARTITION_TYPE_SYSTEM].type = PARTITION_TYPE_SYSTEM;
    g_flash_manager.partitions[PARTITION_TYPE_SYSTEM].flash_id = FLASH_DEV_0;
    g_flash_manager.partitions[PARTITION_TYPE_SYSTEM].start_addr = PARTITION_SYSTEM_START;
    g_flash_manager.partitions[PARTITION_TYPE_SYSTEM].size = PARTITION_SYSTEM_SIZE;
    g_flash_manager.partitions[PARTITION_TYPE_SYSTEM].name = "System";
    
    /* Looper partition - Flash #0 last 7MB */
    g_flash_manager.partitions[PARTITION_TYPE_LOOPER].type = PARTITION_TYPE_LOOPER;
    g_flash_manager.partitions[PARTITION_TYPE_LOOPER].flash_id = FLASH_DEV_0;
    g_flash_manager.partitions[PARTITION_TYPE_LOOPER].start_addr = PARTITION_LOOPER_START;
    g_flash_manager.partitions[PARTITION_TYPE_LOOPER].size = PARTITION_LOOPER_SIZE;
    g_flash_manager.partitions[PARTITION_TYPE_LOOPER].name = "Looper";
    
    /* Storage partition - Flash #1 entire 8MB */
    g_flash_manager.partitions[PARTITION_TYPE_STORAGE].type = PARTITION_TYPE_STORAGE;
    g_flash_manager.partitions[PARTITION_TYPE_STORAGE].flash_id = FLASH_DEV_1;
    g_flash_manager.partitions[PARTITION_TYPE_STORAGE].start_addr = PARTITION_STORAGE_START;
    g_flash_manager.partitions[PARTITION_TYPE_STORAGE].size = PARTITION_STORAGE_SIZE;
    g_flash_manager.partitions[PARTITION_TYPE_STORAGE].name = "Storage";
    
    g_flash_manager.initialized = true;
    
    DBG("FlashManager: Initialized successfully\n");
    FlashManager_PrintInfo();
    
    return FLASH_OK;
}

void FlashManager_DeInit(void)
{
    if (!g_flash_manager.initialized) {
        return;
    }
    int i;
    for (i = 0; i < FLASH_DEV_MAX; i++) {
        if (g_flash_manager.flash[i]) {
            g_flash_manager.flash[i]->deinit(g_flash_manager.flash[i]);
            FlashDriver_Destroy(g_flash_manager.flash[i]);
            g_flash_manager.flash[i] = NULL;
        }
    }
    
    g_flash_manager.initialized = false;
}

FlashManager_t* FlashManager_GetInstance(void)
{
    return &g_flash_manager;
}

FlashDriver_t* FlashManager_GetFlash(uint8_t flash_id)
{
    if (flash_id >= FLASH_DEV_MAX) {
        return NULL;
    }
    return g_flash_manager.flash[flash_id];
}

const PartitionInfo_t* FlashManager_GetPartition(PartitionType_t type)
{
    if (type >= PARTITION_TYPE_MAX) {
        return NULL;
    }
    return &g_flash_manager.partitions[type];
}

/*===========================================================================
 * Partition Read/Write Implementation
 *===========================================================================*/

FlashStatus_t FlashManager_Read(PartitionType_t type, uint32_t offset, uint8_t *buf, uint32_t len)
{
    const PartitionInfo_t *part;
    FlashDriver_t *drv;
    uint32_t addr;
    
    if (!g_flash_manager.initialized) {
        return FLASH_ERROR_NOT_INIT;
    }
    
    part = FlashManager_GetPartition(type);
    if (!part || !buf) {
        return FLASH_ERROR_PARAM;
    }
    
    /* Check bounds */
    if (offset + len > part->size) {
        DBG("FlashManager: Read out of partition bounds\n");
        return FLASH_ERROR_PARAM;
    }
    
    drv = g_flash_manager.flash[part->flash_id];
    if (!drv) {
        return FLASH_ERROR_NOT_INIT;
    }
    
    addr = part->start_addr + offset;
    return drv->read(drv, addr, buf, len);
}

FlashStatus_t FlashManager_Write(PartitionType_t type, uint32_t offset, const uint8_t *buf, uint32_t len)
{
    const PartitionInfo_t *part;
    FlashDriver_t *drv;
    uint32_t addr;
    
    if (!g_flash_manager.initialized) {
        return FLASH_ERROR_NOT_INIT;
    }
    
    part = FlashManager_GetPartition(type);
    if (!part || !buf) {
        return FLASH_ERROR_PARAM;
    }
    
    /* Check bounds */
    if (offset + len > part->size) {
        DBG("FlashManager: Write out of partition bounds\n");
        return FLASH_ERROR_PARAM;
    }
    
    drv = g_flash_manager.flash[part->flash_id];
    if (!drv) {
        return FLASH_ERROR_NOT_INIT;
    }
    
    addr = part->start_addr + offset;
    return drv->write(drv, addr, buf, len);
}

FlashStatus_t FlashManager_EraseSector(PartitionType_t type, uint32_t offset)
{
    const PartitionInfo_t *part;
    FlashDriver_t *drv;
    uint32_t addr;
    
    if (!g_flash_manager.initialized) {
        return FLASH_ERROR_NOT_INIT;
    }
    
    part = FlashManager_GetPartition(type);
    if (!part) {
        return FLASH_ERROR_PARAM;
    }
    
    /* Check bounds */
    if (offset >= part->size) {
        return FLASH_ERROR_PARAM;
    }
    
    drv = g_flash_manager.flash[part->flash_id];
    if (!drv) {
        return FLASH_ERROR_NOT_INIT;
    }
    
    addr = part->start_addr + offset;
    return drv->erase_sector(drv, addr);
}

FlashStatus_t FlashManager_ErasePartition(PartitionType_t type)
{
    const PartitionInfo_t *part;
    FlashDriver_t *drv;
    uint32_t addr, end_addr;
    FlashStatus_t ret;
    
    if (!g_flash_manager.initialized) {
        return FLASH_ERROR_NOT_INIT;
    }
    
    part = FlashManager_GetPartition(type);
    if (!part) {
        return FLASH_ERROR_PARAM;
    }
    
    drv = g_flash_manager.flash[part->flash_id];
    if (!drv) {
        return FLASH_ERROR_NOT_INIT;
    }
    
    DBG("FlashManager: Erasing partition %s...\n", part->name);
    
    addr = part->start_addr;
    end_addr = part->start_addr + part->size;
    
    while (addr < end_addr) {
        ret = drv->erase_sector(drv, addr);
        if (ret != FLASH_OK) {
            DBG("FlashManager: Erase failed at 0x%08X\n", addr);
            return ret;
        }
        addr += NOR_SECTOR_SIZE_4K;
    }
    
    DBG("FlashManager: Partition %s erased\n", part->name);
    return FLASH_OK;
}

/*===========================================================================
 * System Settings API Implementation
 *===========================================================================*/

bool FlashManager_IsSettingsValid(void)
{
    uint32_t magic;
    
    if (FlashManager_Read(PARTITION_TYPE_SYSTEM, SETTINGS_MAGIC_ADDR, 
                          (uint8_t*)&magic, sizeof(magic)) != FLASH_OK) {
        return false;
    }
    
    return (magic == SETTINGS_MAGIC_VALUE);
}

FlashStatus_t FlashManager_InitSettings(void)
{
    uint32_t magic = SETTINGS_MAGIC_VALUE;
    uint32_t version = 0x0001;
    FlashStatus_t ret;
    
    DBG("FlashManager: Initializing settings partition...\n");
    
    /* Erase settings partition */
    ret = FlashManager_EraseSector(PARTITION_TYPE_SYSTEM, 0);
    if (ret != FLASH_OK) {
        return ret;
    }
    
    /* Write magic word */
    ret = FlashManager_Write(PARTITION_TYPE_SYSTEM, SETTINGS_MAGIC_ADDR, 
                             (uint8_t*)&magic, sizeof(magic));
    if (ret != FLASH_OK) {
        return ret;
    }
    
    /* Write version number */
    ret = FlashManager_Write(PARTITION_TYPE_SYSTEM, SETTINGS_VERSION_ADDR, 
                             (uint8_t*)&version, sizeof(version));
    if (ret != FLASH_OK) {
        return ret;
    }
    
    DBG("FlashManager: Settings initialized\n");
    return FLASH_OK;
}

FlashStatus_t FlashManager_ReadSettings(uint32_t key, uint8_t *buf, uint32_t len)
{
    return FlashManager_Read(PARTITION_TYPE_SYSTEM, SETTINGS_DATA_ADDR + key, buf, len);
}

FlashStatus_t FlashManager_WriteSettings(uint32_t key, const uint8_t *buf, uint32_t len)
{
    return FlashManager_Write(PARTITION_TYPE_SYSTEM, SETTINGS_DATA_ADDR + key, buf, len);
}

/*===========================================================================
 * Looper Dedicated API Implementation
 *===========================================================================*/

FlashStatus_t FlashManager_LooperRead(uint32_t offset, uint8_t *buf, uint32_t len)
{
    return FlashManager_Read(PARTITION_TYPE_LOOPER, offset, buf, len);
}

FlashStatus_t FlashManager_LooperWrite(uint32_t offset, const uint8_t *buf, uint32_t len)
{
    return FlashManager_Write(PARTITION_TYPE_LOOPER, offset, buf, len);
}

FlashStatus_t FlashManager_LooperEraseSector(uint32_t offset)
{
    return FlashManager_EraseSector(PARTITION_TYPE_LOOPER, offset);
}

uint32_t FlashManager_LooperGetSize(void)
{
    return PARTITION_LOOPER_SIZE;
}

/*===========================================================================
 * Storage Partition API Implementation
 *===========================================================================*/

FlashStatus_t FlashManager_StorageRead(uint32_t offset, uint8_t *buf, uint32_t len)
{
    return FlashManager_Read(PARTITION_TYPE_STORAGE, offset, buf, len);
}

FlashStatus_t FlashManager_StorageWrite(uint32_t offset, const uint8_t *buf, uint32_t len)
{
    return FlashManager_Write(PARTITION_TYPE_STORAGE, offset, buf, len);
}

FlashStatus_t FlashManager_StorageEraseSector(uint32_t offset)
{
    return FlashManager_EraseSector(PARTITION_TYPE_STORAGE, offset);
}

uint32_t FlashManager_StorageGetSize(void)
{
    return PARTITION_STORAGE_SIZE;
}

/*===========================================================================
 * Debug and Test
 *===========================================================================*/

void FlashManager_PrintInfo(void)
{
    DBG("\n========== Flash Manager Info ==========\n");
    int i;
    for (i = 0; i < FLASH_DEV_MAX; i++) {
        FlashDriver_t *drv = g_flash_manager.flash[i];
        if (drv && drv->initialized) {
            DBG("Flash #%d:\n", i);
            DBG("  Model: %s\n", 
                drv->info.model == FLASH_MODEL_W25Q64 ? "W25Q64" :
                drv->info.model == FLASH_MODEL_W25Q128 ? "W25Q128" :
                drv->info.model == FLASH_MODEL_W25Q32 ? "W25Q32" : "Unknown");
            DBG("  Size: %d MB\n", drv->info.total_size / (1024*1024));
            DBG("  ID: Mfg=0x%02X Type=0x%02X Dev=0x%02X\n",
                drv->info.manufacturer_id, drv->info.memory_type, drv->info.device_id);
        }
    }
    
    DBG("\nPartitions:\n");
    for (i = 0; i < PARTITION_TYPE_MAX; i++) {
        PartitionInfo_t *part = &g_flash_manager.partitions[i];
        DBG("  [%s] Flash#%d, 0x%06X-0x%06X (%d KB)\n",
            part->name, part->flash_id,
            part->start_addr, part->start_addr + part->size - 1,
            part->size / 1024);
    }
    DBG("==========================================\n\n");
}

FlashStatus_t FlashManager_Test(uint8_t flash_id)
{
    FlashDriver_t *drv;
    uint8_t write_buf[256];
    uint8_t read_buf[256];
    FlashStatus_t ret;
    uint32_t test_addr;
    int i;
    drv = FlashManager_GetFlash(flash_id);
    if (!drv) {
        DBG("FlashManager: Flash #%d not found\n", flash_id);
        return FLASH_ERROR_PARAM;
    }
    
    DBG("\n=== Flash #%d Test ===\n", flash_id);
    
    /* Select test address based on Flash ID */
    if (flash_id == FLASH_DEV_0) {
        test_addr = PARTITION_LOOPER_START; /* Test in Looper partition to avoid damaging system settings */
    } else {
        test_addr = 0;
    }
    
    /* Prepare test data */
    for (i = 0; i < 256; i++) {
        write_buf[i] = i;
    }
    
    /* Erase test sector */
    DBG("Erasing sector at 0x%06X...\n", test_addr);
    ret = drv->erase_sector(drv, test_addr);
    if (ret != FLASH_OK) {
        DBG("Erase failed!\n");
        return ret;
    }
    
    /* Verify erase */
    ret = drv->read(drv, test_addr, read_buf, 256);
    if (ret != FLASH_OK) {
        DBG("Read after erase failed!\n");
        return ret;
    }
    
    for (i = 0; i < 256; i++) {
        if (read_buf[i] != 0xFF) {
            DBG("Erase verify failed at offset %d: 0x%02X\n", i, read_buf[i]);
            return FLASH_ERROR_ERASE_FAIL;
        }
    }
    DBG("Erase verified OK\n");
    
    /* Write test data */
    DBG("Writing test data...\n");
    ret = drv->write(drv, test_addr, write_buf, 256);
    if (ret != FLASH_OK) {
        DBG("Write failed!\n");
        return ret;
    }
    
    /* Read and verify */
    memset(read_buf, 0, 256);
    ret = drv->read(drv, test_addr, read_buf, 256);
    if (ret != FLASH_OK) {
        DBG("Read failed!\n");
        return ret;
    }
    
    for (i = 0; i < 256; i++) {
        if (read_buf[i] != write_buf[i]) {
            DBG("Verify failed at offset %d: wrote 0x%02X, read 0x%02X\n", 
                i, write_buf[i], read_buf[i]);
            return FLASH_ERROR_PROGRAM_FAIL;
        }
    }
    
    DBG("Write/Read verified OK\n");
    DBG("=== Flash #%d Test PASSED ===\n\n", flash_id);
    
    return FLASH_OK;
}
