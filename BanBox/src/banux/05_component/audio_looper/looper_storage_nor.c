/**
 **************************************************************************************
 * @file    looper_storage_nor.c
 * @brief   Looper NOR Flash 存储适配器实现
 *
 * @details 适配 W25Qxx 系列 NOR Flash
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
#include "flash_nor_w25qxx.h"
#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ============================================================================
 * NOR Flash 常量定义
 * ============================================================================ */
#define NOR_FLASH_PAGE_SIZE         256
#define NOR_FLASH_SECTOR_SIZE       4096
#define NOR_FLASH_BLOCK_SIZE        65536
#define NOR_FLASH_TOTAL_SIZE        LOOPER_FLASH_DEV_SIZE /* 8MB */


/* ============================================================================
 * NOR Flash 操作函数实现
 * ============================================================================ */

static LooperStorageStatus_t nor_flash_init(LooperStorageDevice_t *dev)
{
    if (dev == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 设置设备信息 */
    dev->info.total_size = NOR_FLASH_TOTAL_SIZE;
    dev->info.page_size = NOR_FLASH_PAGE_SIZE;
    dev->info.block_size = NOR_FLASH_SECTOR_SIZE;
    dev->info.erase_block_size = NOR_FLASH_BLOCK_SIZE;
    dev->info.name = "W25Q64 NOR Flash";
    dev->info.performance.support_overdub = 0; /* NOR Flash 不支持叠录 */
    dev->info.performance.bandwidth_tested = 0;

    DBG("[LooperStorage NOR] Init complete\n");
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t nor_flash_deinit(LooperStorageDevice_t *dev)
{
    (void)dev;
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t nor_flash_read(LooperStorageDevice_t *dev, uint32_t offset, 
                                            uint8_t *buf, uint32_t len)
{
    FlashStatus_t status;
    
    (void)dev;
    
    if (buf == NULL || len == 0) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    status = FlashPartition_LooperRead(offset, buf, len);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t nor_flash_write(LooperStorageDevice_t *dev, uint32_t offset, 
                                             const uint8_t *buf, uint32_t len)
{
    FlashStatus_t status;
    
    (void)dev;
    
    if (buf == NULL || len == 0) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    status = FlashPartition_LooperWrite(offset, buf, len);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t nor_flash_erase_block(LooperStorageDevice_t *dev, uint32_t offset)
{
    FlashStatus_t status;
    
    (void)dev;
    
    status = FlashPartition_LooperEraseBlock(offset);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t nor_flash_erase_chip(LooperStorageDevice_t *dev)
{
    FlashStatus_t status;
    
    (void)dev;
    
    status = FlashPartition_LooperEraseChip();
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t nor_flash_get_info(LooperStorageDevice_t *dev, LooperStorageInfo_t *info)
{
    if (dev == NULL || info == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    memcpy(info, &dev->info, sizeof(LooperStorageInfo_t));
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t nor_flash_benchmark(LooperStorageDevice_t *dev, LooperStoragePerf_t *perf)
{
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

    DBG("[LooperStorage NOR] Starting bandwidth test...\n");

    /* 测试参数：4个块 = 256KB */
    test_size = NOR_FLASH_BLOCK_SIZE * 4;
    test_addr = 0;

    /* 分配测试缓冲区 */
    test_buf = (uint8_t *)pvPortMalloc(test_size);
    if (test_buf == NULL) {
        DBG("[LooperStorage NOR] Benchmark failed: out of memory\n");
        return LOOPER_STORAGE_ERROR;
    }

    /* 填充测试数据 */
    for (i = 0; i < test_size; i++) {
        test_buf[i] = (uint8_t)(i & 0xFF);
    }

    /* 1. 测试写入速度 */
    DBG("[LooperStorage NOR] Testing write speed...\n");
    
    /* 擦除测试区域 */
    for (i = 0; i < 4; i++) {
        FlashPartition_LooperEraseBlock(test_addr + i * NOR_FLASH_BLOCK_SIZE);
    }

    start_tick = xTaskGetTickCount();
    
    if (FlashPartition_LooperWrite(test_addr, test_buf, test_size) != FLASH_OK) {
        vPortFree(test_buf);
        return LOOPER_STORAGE_ERROR;
    }
    
    end_tick = xTaskGetTickCount();
    elapsed_ms = (end_tick - start_tick) * portTICK_PERIOD_MS;
    if (elapsed_ms == 0) elapsed_ms = 1;
    
    write_speed_kbps = (test_size / 1024) * 1000 / elapsed_ms;
    DBG("[LooperStorage NOR] Write: %lu KB in %lu ms = %lu KB/s\n", 
        (unsigned long)(test_size / 1024), (unsigned long)elapsed_ms, (unsigned long)write_speed_kbps);

    /* 2. 测试读取速度 */
    DBG("[LooperStorage NOR] Testing read speed...\n");
    
    start_tick = xTaskGetTickCount();
    
    if (FlashPartition_LooperRead(test_addr, test_buf, test_size) != FLASH_OK) {
        vPortFree(test_buf);
        return LOOPER_STORAGE_ERROR;
    }
    
    end_tick = xTaskGetTickCount();
    elapsed_ms = (end_tick - start_tick) * portTICK_PERIOD_MS;
    if (elapsed_ms == 0) elapsed_ms = 1;
    
    read_speed_kbps = (test_size / 1024) * 1000 / elapsed_ms;
    DBG("[LooperStorage NOR] Read: %lu KB in %lu ms = %lu KB/s\n", 
        (unsigned long)(test_size / 1024), (unsigned long)elapsed_ms, (unsigned long)read_speed_kbps);

    /* 3. 计算性能参数 */
    perf->write_speed_kbps = write_speed_kbps;
    perf->read_speed_kbps = read_speed_kbps;
    perf->page_size = NOR_FLASH_PAGE_SIZE;
    perf->block_size = NOR_FLASH_BLOCK_SIZE;
    perf->total_capacity = NOR_FLASH_TOTAL_SIZE;
    perf->support_overdub = 0;
    perf->bandwidth_tested = 1;

    /* 计算最大同时段数
     * 音频数据速率：LOOPER_SAMPLE_RATE Hz × 4 Bytes/sample
     * 写入段数 = write_speed / audio_rate
     * 读取段数 = read_speed / audio_rate
     * 最大同时段数 = min(写入段数, 读取段数)
     */
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
        
        DBG("[LooperStorage NOR] Max concurrent tracks: %lu (write: %lu, read: %lu)\n",
            (unsigned long)perf->max_concurrent_tracks,
            (unsigned long)max_write_tracks,
            (unsigned long)max_read_tracks);
    }

    /* 保存到设备信息 */
    memcpy(&dev->info.performance, perf, sizeof(LooperStoragePerf_t));

    vPortFree(test_buf);
    DBG("[LooperStorage NOR] Bandwidth test complete\n");
    
    return LOOPER_STORAGE_OK;
}

static uint8_t nor_flash_is_busy(LooperStorageDevice_t *dev)
{
    (void)dev;
    
#if LOOPER_MULTI_FLASH_ENABLE
    /* TODO: 实现多 Flash 忙检测 */
    return 0;
#else
    /* 单 Flash 模式：查询 NOR Flash 是否正在全片擦除 */
    return FlashPartition_LooperIsErasing() ? 1u : 0u;
#endif
}

/* ============================================================================
 * NOR Flash 操作函数表
 * ============================================================================ */
static const LooperStorageOps_t s_nor_flash_ops = {
    .init = nor_flash_init,
    .deinit = nor_flash_deinit,
    .read = nor_flash_read,
    .write = nor_flash_write,
    .erase_block = nor_flash_erase_block,
    .erase_chip = nor_flash_erase_chip,
    .get_info = nor_flash_get_info,
    .benchmark = nor_flash_benchmark,
    .overdub_write = NULL, /* NOR Flash 不支持叠录 */
    .is_busy = nor_flash_is_busy
};

/**
 * @brief 获取 NOR Flash 操作函数表
 */
const LooperStorageOps_t* LooperStorageAdapter_GetNorFlashOps(void)
{
    return &s_nor_flash_ops;
}
