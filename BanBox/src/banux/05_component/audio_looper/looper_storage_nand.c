/**
 **************************************************************************************
 * @file    looper_storage_nand.c
 * @brief   Looper NAND Flash 存储适配器实现
 *
 * @details 适配 W25N02 NAND Flash
 *
 * @author  BanGO
 * @version V2.0.0
 * @date    2026-04-09
 *
 * @Copyright (C) 2026, Audio Looper Project. All rights reserved.
 **************************************************************************************
 */

#include "looper_storage.h"
#include "flash_devices.h"
#include "flash_nand_w25n02.h"
#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ============================================================================
 * NAND Flash 常量定义
 * ============================================================================ */
#define NAND_TOTAL_SIZE         (256u * 1024u * 1024u) /* 256MB */
#define NAND_PAGE_SIZE          2048u                   /* 页大小 2KB */
#define NAND_BLOCK_SIZE         (128u * 1024u)          /* 块大小 128KB */

/* ============================================================================
 * NAND Flash 操作函数实现
 * ============================================================================ */

static LooperStorageStatus_t nand_flash_init(LooperStorageDevice_t *dev)
{
    if (dev == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 设置设备信息 */
    dev->info.total_size = NAND_TOTAL_SIZE;
    dev->info.page_size = NAND_PAGE_SIZE;
    dev->info.block_size = NAND_BLOCK_SIZE;
    dev->info.erase_block_size = NAND_BLOCK_SIZE;
    dev->info.name = "W25N02 NAND Flash";
    dev->info.performance.support_overdub = 0; /* NAND Flash 不支持叠录 */
    dev->info.performance.bandwidth_tested = 0;

    DBG("[LooperStorage NAND] Init complete\n");
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t nand_flash_deinit(LooperStorageDevice_t *dev)
{
    (void)dev;
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t nand_flash_read(LooperStorageDevice_t *dev, uint32_t offset, 
                                             uint8_t *buf, uint32_t len)
{
    FlashDevice_t *nand_dev;
    FlashStatus_t status;
    
    (void)dev;
    
    if (buf == NULL || len == 0) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 检查地址范围 */
    if (offset + len > NAND_TOTAL_SIZE) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 获取 NAND Flash 设备句柄 */
    nand_dev = FlashDevices_GetNandFlash();
    if (nand_dev == NULL || !nand_dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    /* 调用底层驱动 */
    status = FlashDev_Read(nand_dev, offset, buf, len);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t nand_flash_write(LooperStorageDevice_t *dev, uint32_t offset, 
                                              const uint8_t *buf, uint32_t len)
{
    FlashDevice_t *nand_dev;
    FlashStatus_t status;
    
    (void)dev;
    
    if (buf == NULL || len == 0) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 检查地址范围 */
    if (offset + len > NAND_TOTAL_SIZE) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 获取 NAND Flash 设备句柄 */
    nand_dev = FlashDevices_GetNandFlash();
    if (nand_dev == NULL || !nand_dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    /* 调用底层驱动 */
    status = FlashDev_Write(nand_dev, offset, buf, len);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t nand_flash_erase_block(LooperStorageDevice_t *dev, uint32_t offset)
{
    FlashDevice_t *nand_dev;
    FlashStatus_t status;
    
    (void)dev;
    
    /* 检查地址范围 */
    if (offset >= NAND_TOTAL_SIZE) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 获取 NAND Flash 设备句柄 */
    nand_dev = FlashDevices_GetNandFlash();
    if (nand_dev == NULL || !nand_dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    /* 调用底层驱动操除块 */
    status = FlashDev_EraseBlock(nand_dev, offset);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t nand_flash_erase_chip(LooperStorageDevice_t *dev)
{
    FlashDevice_t *nand_dev;
    FlashStatus_t status;
    
    (void)dev;
    
    /* 获取 NAND Flash 设备句柄 */
    nand_dev = FlashDevices_GetNandFlash();
    if (nand_dev == NULL || !nand_dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    /* 调用底层驱动全片擊除 */
    status = FlashDev_EraseChip(nand_dev);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t nand_flash_get_info(LooperStorageDevice_t *dev, LooperStorageInfo_t *info)
{
    if (dev == NULL || info == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    memcpy(info, &dev->info, sizeof(LooperStorageInfo_t));
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t nand_flash_benchmark(LooperStorageDevice_t *dev, LooperStoragePerf_t *perf)
{
    FlashDevice_t *nand_dev;
    uint32_t test_size;
    uint32_t test_addr;
    uint8_t *test_buf;
    TickType_t start_tick, end_tick;
    uint32_t elapsed_ms;
    uint32_t write_speed_kbps, read_speed_kbps;
    uint32_t i;
    
    if (dev == NULL || perf == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 获取 NAND Flash 设备句柄 */
    nand_dev = FlashDevices_GetNandFlash();
    if (nand_dev == NULL || !nand_dev->initialized) {
        DBG("[LooperStorage NAND] Device not initialized, using defaults\n");
        
        /* 使用保守的默认值 */
        perf->write_speed_kbps = 1000;  /* 1 MB/s */
        perf->read_speed_kbps = 2000;   /* 2 MB/s */
        perf->page_size = NAND_PAGE_SIZE;
        perf->block_size = NAND_BLOCK_SIZE;
        perf->total_capacity = NAND_TOTAL_SIZE;
        perf->support_overdub = 0;
        perf->bandwidth_tested = 0;
        
        /* 计算最大同时段数 */
        {
            uint32_t audio_rate_kbps = ((uint32_t)LOOPER_SAMPLE_RATE * 4) / 1024;
            uint32_t max_write_tracks = perf->write_speed_kbps / audio_rate_kbps;
            uint32_t max_read_tracks = perf->read_speed_kbps / audio_rate_kbps;
            
            perf->max_concurrent_tracks = (max_write_tracks < max_read_tracks) ? 
                                          max_write_tracks : max_read_tracks;
            
            if (perf->max_concurrent_tracks > 1) {
                perf->max_concurrent_tracks -= 1; /* 保守余量 */
            }
        }
        
        /* 保存到设备信息 */
        memcpy(&dev->info.performance, perf, sizeof(LooperStoragePerf_t));
        
        return LOOPER_STORAGE_OK;
    }

    DBG("[LooperStorage NAND] Starting bandwidth test...\n");

    /* 测试参数：128KB */
    test_size = NAND_BLOCK_SIZE;
    test_addr = 0;

    /* 分配测试缓冲区 */
    test_buf = (uint8_t *)pvPortMalloc(test_size);
    if (test_buf == NULL) {
        DBG("[LooperStorage NAND] Benchmark failed: out of memory\n");
        return LOOPER_STORAGE_ERROR;
    }

    /* 填充测试数据 */
    for (i = 0; i < test_size; i++) {
        test_buf[i] = (uint8_t)(i & 0xFF);
    }

    /* 1. 测试写入速度 */
    DBG("[LooperStorage NAND] Testing write speed...\n");
    
    /* 擦除测试区域 */
    if (FlashDev_EraseBlock(nand_dev, test_addr) != FLASH_OK) {
        vPortFree(test_buf);
        return LOOPER_STORAGE_ERROR;
    }

    start_tick = xTaskGetTickCount();
    
    if (FlashDev_Write(nand_dev, test_addr, test_buf, test_size) != FLASH_OK) {
        vPortFree(test_buf);
        return LOOPER_STORAGE_ERROR;
    }
    
    end_tick = xTaskGetTickCount();
    elapsed_ms = (end_tick - start_tick) * portTICK_PERIOD_MS;
    if (elapsed_ms == 0) elapsed_ms = 1;
    
    write_speed_kbps = (test_size / 1024) * 1000 / elapsed_ms;
    DBG("[LooperStorage NAND] Write: %lu KB in %lu ms = %lu KB/s\n", 
        (unsigned long)(test_size / 1024), (unsigned long)elapsed_ms, (unsigned long)write_speed_kbps);

    /* 2. 测试读取速度 */
    DBG("[LooperStorage NAND] Testing read speed...\n");
    
    start_tick = xTaskGetTickCount();
    
    if (FlashDev_Read(nand_dev, test_addr, test_buf, test_size) != FLASH_OK) {
        vPortFree(test_buf);
        return LOOPER_STORAGE_ERROR;
    }
    
    end_tick = xTaskGetTickCount();
    elapsed_ms = (end_tick - start_tick) * portTICK_PERIOD_MS;
    if (elapsed_ms == 0) elapsed_ms = 1;
    
    read_speed_kbps = (test_size / 1024) * 1000 / elapsed_ms;
    DBG("[LooperStorage NAND] Read: %lu KB in %lu ms = %lu KB/s\n", 
        (unsigned long)(test_size / 1024), (unsigned long)elapsed_ms, (unsigned long)read_speed_kbps);

    /* 3. 计算性能参数 */
    perf->write_speed_kbps = write_speed_kbps;
    perf->read_speed_kbps = read_speed_kbps;
    perf->page_size = NAND_PAGE_SIZE;
    perf->block_size = NAND_BLOCK_SIZE;
    perf->total_capacity = NAND_TOTAL_SIZE;
    perf->support_overdub = 0;
    perf->bandwidth_tested = 1;

    /* 计算最大同时段数 */
    {
        uint32_t audio_rate_kbps = ((uint32_t)LOOPER_SAMPLE_RATE * 4) / 1024;
        uint32_t max_write_tracks = write_speed_kbps / audio_rate_kbps;
        uint32_t max_read_tracks = read_speed_kbps / audio_rate_kbps;
        
        perf->max_concurrent_tracks = (max_write_tracks < max_read_tracks) ? 
                                      max_write_tracks : max_read_tracks;
        
        /* 保守起见，减少 1 段作为余量 */
        if (perf->max_concurrent_tracks > 1) {
            perf->max_concurrent_tracks -= 1;
        }
        
        DBG("[LooperStorage NAND] Max concurrent tracks: %lu (write: %lu, read: %lu)\n",
            (unsigned long)perf->max_concurrent_tracks,
            (unsigned long)max_write_tracks,
            (unsigned long)max_read_tracks);
    }

    /* 保存到设备信息 */
    memcpy(&dev->info.performance, perf, sizeof(LooperStoragePerf_t));

    /* 释放测试缓冲区 */
    vPortFree(test_buf);

    DBG("[LooperStorage NAND] Benchmark complete\n");
    return LOOPER_STORAGE_OK;
}

static uint8_t nand_flash_is_busy(LooperStorageDevice_t *dev)
{
    (void)dev;
    return 0;
}

/* ============================================================================
 * NAND Flash 操作函数表
 * ============================================================================ */
static const LooperStorageOps_t s_nand_flash_ops = {
    .init = nand_flash_init,
    .deinit = nand_flash_deinit,
    .read = nand_flash_read,
    .write = nand_flash_write,
    .erase_block = nand_flash_erase_block,
    .erase_chip = nand_flash_erase_chip,
    .get_info = nand_flash_get_info,
    .benchmark = nand_flash_benchmark,
    .overdub_write = NULL, /* NAND Flash 不支持叠录 */
    .is_busy = nand_flash_is_busy
};

/**
 * @brief 获取 NAND Flash 操作函数表
 */
const LooperStorageOps_t* LooperStorageAdapter_GetNandFlashOps(void)
{
    return &s_nand_flash_ops;
}
