/**
 **************************************************************************************
 * @file    looper_storage_psram.c
 * @brief   Looper PSRAM 存储适配器实现
 *
 * @details 适配 ESP-PSRAM64H，支持叠录功能
 *
 * @author  BanGO
 * @version V2.0.0
 * @date    2026-04-09
 *
 * @Copyright (C) 2026, Audio Looper Project. All rights reserved.
 **************************************************************************************
 */

#include "looper_storage.h"
#include "psram_esp64h.h"
#include "flash_devices.h"
#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ============================================================================
 * PSRAM 常量定义
 * ============================================================================ */
#define PSRAM_TOTAL_SIZE         (8u * 1024u * 1024u)  /* 8MB */
#define PSRAM_PAGE_SIZE          1024u                  /* 页大小 1KB */
#define PSRAM_SECTOR_SIZE        PSRAM_PAGE_SIZE        /* 无扇区概念 */
#define PSRAM_BLOCK_SIZE         (64u * 1024u)          /* 虚拟块 64KB */

/* ============================================================================
 * PSRAM 操作函数实现
 * ============================================================================ */

static LooperStorageStatus_t psram_init(LooperStorageDevice_t *dev)
{
    if (dev == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 设置设备信息 */
    dev->info.total_size = PSRAM_TOTAL_SIZE;
    dev->info.page_size = PSRAM_PAGE_SIZE;
    dev->info.block_size = PSRAM_BLOCK_SIZE;
    dev->info.erase_block_size = PSRAM_BLOCK_SIZE;
    dev->info.name = "ESP-PSRAM64H";
    dev->info.performance.support_overdub = 1; /* PSRAM 支持叠录 */
    dev->info.performance.bandwidth_tested = 0;

    DBG("[LooperStorage PSRAM] Init complete\n");
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t psram_deinit(LooperStorageDevice_t *dev)
{
    (void)dev;
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t psram_read(LooperStorageDevice_t *dev, uint32_t offset, 
                                        uint8_t *buf, uint32_t len)
{
    FlashStatus_t status;
    
    (void)dev;
    
    if (buf == NULL || len == 0) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 检查地址范围 */
    if (offset + len > PSRAM_TOTAL_SIZE) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    status = PSRAM64H_DirectRead(FlashDevices_GetPsramFlash(), offset, buf, len);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t psram_write(LooperStorageDevice_t *dev, uint32_t offset, 
                                         const uint8_t *buf, uint32_t len)
{
    FlashStatus_t status;
    
    (void)dev;
    
    if (buf == NULL || len == 0) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 检查地址范围 */
    if (offset + len > PSRAM_TOTAL_SIZE) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    status = PSRAM64H_DirectWrite(FlashDevices_GetPsramFlash(), offset, buf, len);
    
    return (status == FLASH_OK) ? LOOPER_STORAGE_OK : LOOPER_STORAGE_ERROR;
}

static LooperStorageStatus_t psram_erase_block(LooperStorageDevice_t *dev, uint32_t offset)
{
    /* PSRAM 作为 RAM，不需要擦除操作 */
    (void)dev;
    (void)offset;
    
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t psram_erase_chip(LooperStorageDevice_t *dev)
{
    /* PSRAM 作为 RAM，不需要擦除操作 */
    (void)dev;
    
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t psram_get_info(LooperStorageDevice_t *dev, LooperStorageInfo_t *info)
{
    if (dev == NULL || info == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    memcpy(info, &dev->info, sizeof(LooperStorageInfo_t));
    return LOOPER_STORAGE_OK;
}

static LooperStorageStatus_t psram_benchmark(LooperStorageDevice_t *dev, LooperStoragePerf_t *perf)
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

    DBG("[LooperStorage PSRAM] Starting bandwidth test...\n");

    /* 测试参数：1MB 测试数据 */
    test_size = 1024 * 1024; /* 1MB */
    test_addr = 0;

    /* 分配测试缓冲区 */
    test_buf = (uint8_t *)pvPortMalloc(test_size);
    if (test_buf == NULL) {
        DBG("[LooperStorage PSRAM] Benchmark failed: out of memory\n");
        return LOOPER_STORAGE_ERROR;
    }

    /* 填充测试数据 */
    for (i = 0; i < test_size; i++) {
        test_buf[i] = (uint8_t)(i & 0xFF);
    }

    /* 1. 测试写入速度 */
    DBG("[LooperStorage PSRAM] Testing write speed...\n");
    
    start_tick = xTaskGetTickCount();
    
    if (PSRAM64H_DirectWrite(FlashDevices_GetPsramFlash(), test_addr, test_buf, test_size) != FLASH_OK) {
        vPortFree(test_buf);
        return LOOPER_STORAGE_ERROR;
    }
    
    end_tick = xTaskGetTickCount();
    elapsed_ms = (end_tick - start_tick) * portTICK_PERIOD_MS;
    if (elapsed_ms == 0) elapsed_ms = 1;
    
    write_speed_kbps = (test_size / 1024) * 1000 / elapsed_ms;
    DBG("[LooperStorage PSRAM] Write: %lu KB in %lu ms = %lu KB/s\n", 
        (unsigned long)(test_size / 1024), (unsigned long)elapsed_ms, (unsigned long)write_speed_kbps);

    /* 2. 测试读取速度 */
    DBG("[LooperStorage PSRAM] Testing read speed...\n");
    
    start_tick = xTaskGetTickCount();
    
    if (PSRAM64H_DirectRead(FlashDevices_GetPsramFlash(), test_addr, test_buf, test_size) != FLASH_OK) {
        vPortFree(test_buf);
        return LOOPER_STORAGE_ERROR;
    }
    
    end_tick = xTaskGetTickCount();
    elapsed_ms = (end_tick - start_tick) * portTICK_PERIOD_MS;
    if (elapsed_ms == 0) elapsed_ms = 1;
    
    read_speed_kbps = (test_size / 1024) * 1000 / elapsed_ms;
    DBG("[LooperStorage PSRAM] Read: %lu KB in %lu ms = %lu KB/s\n", 
        (unsigned long)(test_size / 1024), (unsigned long)elapsed_ms, (unsigned long)read_speed_kbps);

    /* 3. 计算性能参数 */
    perf->write_speed_kbps = write_speed_kbps;
    perf->read_speed_kbps = read_speed_kbps;
    perf->page_size = PSRAM_PAGE_SIZE;
    perf->block_size = PSRAM_BLOCK_SIZE;
    perf->total_capacity = PSRAM_TOTAL_SIZE;
    perf->support_overdub = 1;
    perf->bandwidth_tested = 1;

    /* 计算最大同时段数
     * 音频数据速率：LOOPER_SAMPLE_RATE Hz × 4 Bytes/sample
     * PSRAM 速度通常比 NOR Flash 快很多，可以支持更多同时段
     */
    {
        uint32_t audio_rate_kbps = ((uint32_t)LOOPER_SAMPLE_RATE * 4) / 1024;
        uint32_t max_write_tracks = write_speed_kbps / audio_rate_kbps;
        uint32_t max_read_tracks = read_speed_kbps / audio_rate_kbps;
        
        perf->max_concurrent_tracks = (max_write_tracks < max_read_tracks) ? 
                                      max_write_tracks : max_read_tracks;
        
        /* PSRAM 速度快，可以支持更多段，但保守起见上限为 8 */
        if (perf->max_concurrent_tracks > 8) {
            perf->max_concurrent_tracks = 8;
        }
        
        DBG("[LooperStorage PSRAM] Max concurrent tracks: %lu (write: %lu, read: %lu)\n",
            (unsigned long)perf->max_concurrent_tracks,
            (unsigned long)max_write_tracks,
            (unsigned long)max_read_tracks);
    }

    /* 保存到设备信息 */
    memcpy(&dev->info.performance, perf, sizeof(LooperStoragePerf_t));

    vPortFree(test_buf);
    DBG("[LooperStorage PSRAM] Bandwidth test complete\n");
    
    return LOOPER_STORAGE_OK;
}

/**
 * @brief PSRAM 叠录写入实现
 *
 * @details 读取现有数据，混音后写回
 *          支持三种混音模式：
 *          0=替换: new_data 完全替换 existing_data
 *          1=相加: existing_data + new_data (可能溢出)
 *          2=平均: (existing_data + new_data) / 2
 */
static LooperStorageStatus_t psram_overdub_write(LooperStorageDevice_t *dev, uint32_t offset,
                                                 const uint8_t *buf, uint32_t len, uint8_t mix_mode)
{
    uint8_t *existing_data;
    uint32_t i;
    
    if (dev == NULL || buf == NULL || len == 0) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 检查地址范围 */
    if (offset + len > PSRAM_TOTAL_SIZE) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    /* 分配临时缓冲区 */
    existing_data = (uint8_t *)pvPortMalloc(len);
    if (existing_data == NULL) {
        return LOOPER_STORAGE_ERROR;
    }

    /* 读取现有数据 */
    if (PSRAM64H_DirectRead(FlashDevices_GetPsramFlash(), offset, existing_data, len) != FLASH_OK) {
        vPortFree(existing_data);
        return LOOPER_STORAGE_ERROR;
    }

    /* 混音处理 */
    if (mix_mode == 0) {
        memcpy(existing_data, buf, len);
    } else {
        uint32_t sample_count = len / 4;
        uint32_t i;
        for (i = 0; i < sample_count; i++) {
            uint32_t existing_sample = ((uint32_t*)existing_data)[i];
            uint32_t new_sample      = ((uint32_t*)buf)[i];
            
            int16_t ex_left  = (int16_t)(existing_sample & 0xFFFF);
            int16_t ex_right = (int16_t)((existing_sample >> 16) & 0xFFFF);
            int16_t nw_left  = (int16_t)(new_sample & 0xFFFF);
            int16_t nw_right = (int16_t)((new_sample >> 16) & 0xFFFF);
            
            int16_t mix_left, mix_right;
            if (mix_mode == 1) {
                int32_t sum_left  = (int32_t)ex_left + nw_left;
                int32_t sum_right = (int32_t)ex_right + nw_right;
                mix_left  = (sum_left > 32767) ? 32767 : ((sum_left < -32768) ? -32768 : (int16_t)sum_left);
                mix_right = (sum_right > 32767) ? 32767 : ((sum_right < -32768) ? -32768 : (int16_t)sum_right);
            } else {
                mix_left  = (int16_t)(((int32_t)ex_left + nw_left) / 2);
                mix_right = (int16_t)(((int32_t)ex_right + nw_right) / 2);
            }
            
            ((uint32_t*)existing_data)[i] = ((uint32_t)(uint16_t)mix_right << 16) | ((uint32_t)(uint16_t)mix_left & 0xFFFF);
        }
    }

    /* 写回混音后的数据 */
    if (PSRAM64H_DirectWrite(FlashDevices_GetPsramFlash(), offset, existing_data, len) != FLASH_OK) {
        vPortFree(existing_data);
        return LOOPER_STORAGE_ERROR;
    }

    vPortFree(existing_data);
    return LOOPER_STORAGE_OK;
}

static uint8_t psram_is_busy(LooperStorageDevice_t *dev)
{
    /* PSRAM 作为 RAM，始终 ready */
    (void)dev;
    return 0;
}

/* ============================================================================
 * PSRAM 操作函数表
 * ============================================================================ */
static const LooperStorageOps_t s_psram_ops = {
    .init = psram_init,
    .deinit = psram_deinit,
    .read = psram_read,
    .write = psram_write,
    .erase_block = psram_erase_block,
    .erase_chip = psram_erase_chip,
    .get_info = psram_get_info,
    .benchmark = psram_benchmark,
    .overdub_write = psram_overdub_write,
    .is_busy = psram_is_busy
};

/**
 * @brief 获取 PSRAM 操作函数表
 */
const LooperStorageOps_t* LooperStorageAdapter_GetPsramOps(void)
{
    return &s_psram_ops;
}