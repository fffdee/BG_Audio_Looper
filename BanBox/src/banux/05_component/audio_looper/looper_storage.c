/**
 **************************************************************************************
 * @file    looper_storage.c
 * @brief   Looper 存储抽象层实现
 *
 * @author  BanGO
 * @version V2.0.0
 * @date    2026-04-09
 *
 * @Copyright (C) 2026, Audio Looper Project. All rights reserved.
 **************************************************************************************
 */

#include "looper_storage.h"
#include "debug.h"
#include <string.h>

/* ============================================================================
 * 全局存储设备实例
 * ============================================================================ */
LooperStorageDevice_t g_looper_storage = {
    .ops = NULL,
    .initialized = 0
};

/* ============================================================================
 * 存储抽象层通用接口实现
 * ============================================================================ */

/**
 * @brief 注册存储设备 (绑定 ops 函数表)
 */
LooperStorageStatus_t LooperStorage_Register(LooperStorageDevice_t *dev, 
                                              const LooperStorageOps_t *ops,
                                              LooperStorageType_t type)
{
    if (dev == NULL || ops == NULL) {
        DBG("[LooperStorage] Register failed: NULL pointer\n");
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    dev->ops = ops;
    dev->info.type = type;
    dev->initialized = 0;

    DBG("[LooperStorage] Registered storage type: %d\n", type);
    return LOOPER_STORAGE_OK;
}

/**
 * @brief 初始化存储设备
 */
LooperStorageStatus_t LooperStorage_Init(LooperStorageDevice_t *dev)
{
    if (dev == NULL || dev->ops == NULL || dev->ops->init == NULL) {
        DBG("[LooperStorage] Init failed: invalid device or ops\n");
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (dev->initialized) {
        DBG("[LooperStorage] Already initialized\n");
        return LOOPER_STORAGE_OK;
    }

    LooperStorageStatus_t status = dev->ops->init(dev);
    if (status == LOOPER_STORAGE_OK) {
        dev->initialized = 1;
        DBG("[LooperStorage] Init success\n");
    } else {
        DBG("[LooperStorage] Init failed with status %d\n", status);
    }

    return status;
}

/**
 * @brief 反初始化存储设备
 */
LooperStorageStatus_t LooperStorage_Deinit(LooperStorageDevice_t *dev)
{
    if (dev == NULL || dev->ops == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return LOOPER_STORAGE_OK;
    }

    LooperStorageStatus_t status = LOOPER_STORAGE_OK;
    if (dev->ops->deinit != NULL) {
        status = dev->ops->deinit(dev);
    }

    if (status == LOOPER_STORAGE_OK) {
        dev->initialized = 0;
    }

    return status;
}

/**
 * @brief 读取数据
 */
LooperStorageStatus_t LooperStorage_Read(LooperStorageDevice_t *dev, uint32_t offset, 
                                         uint8_t *buf, uint32_t len)
{
    if (dev == NULL || dev->ops == NULL || dev->ops->read == NULL || buf == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    return dev->ops->read(dev, offset, buf, len);
}

/**
 * @brief 写入数据
 */
LooperStorageStatus_t LooperStorage_Write(LooperStorageDevice_t *dev, uint32_t offset, 
                                          const uint8_t *buf, uint32_t len)
{
    if (dev == NULL || dev->ops == NULL || dev->ops->write == NULL || buf == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    return dev->ops->write(dev, offset, buf, len);
}

/**
 * @brief 擦除块
 */
LooperStorageStatus_t LooperStorage_EraseBlock(LooperStorageDevice_t *dev, uint32_t offset)
{
    if (dev == NULL || dev->ops == NULL || dev->ops->erase_block == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    return dev->ops->erase_block(dev, offset);
}

/**
 * @brief 擦除整片
 */
LooperStorageStatus_t LooperStorage_EraseChip(LooperStorageDevice_t *dev)
{
    if (dev == NULL || dev->ops == NULL || dev->ops->erase_chip == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    return dev->ops->erase_chip(dev);
}

/**
 * @brief 刷新待写缓冲区
 */
LooperStorageStatus_t LooperStorage_Flush(LooperStorageDevice_t *dev)
{
    if (dev == NULL || dev->ops == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    if (dev->ops->flush == NULL) {
        return LOOPER_STORAGE_OK;  /* 不需要flush的存储介质直接返回成功 */
    }

    return dev->ops->flush(dev);
}

/**
 * @brief 获取设备信息
 */
LooperStorageStatus_t LooperStorage_GetInfo(LooperStorageDevice_t *dev, LooperStorageInfo_t *info)
{
    if (dev == NULL || info == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (dev->ops != NULL && dev->ops->get_info != NULL) {
        return dev->ops->get_info(dev, info);
    }

    /* 返回缓存的设备信息 */
    memcpy(info, &dev->info, sizeof(LooperStorageInfo_t));
    return LOOPER_STORAGE_OK;
}

/**
 * @brief 执行带宽测试
 */
LooperStorageStatus_t LooperStorage_Benchmark(LooperStorageDevice_t *dev, LooperStoragePerf_t *perf)
{
    if (dev == NULL || perf == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (dev->ops == NULL || dev->ops->benchmark == NULL) {
        DBG("[LooperStorage] Benchmark not supported\n");
        return LOOPER_STORAGE_ERROR;
    }

    if (!dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    return dev->ops->benchmark(dev, perf);
}

/**
 * @brief 叠录写入
 */
LooperStorageStatus_t LooperStorage_OverdubWrite(LooperStorageDevice_t *dev, uint32_t offset,
                                                 const uint8_t *buf, uint32_t len, uint8_t mix_mode)
{
    if (dev == NULL || buf == NULL) {
        return LOOPER_STORAGE_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return LOOPER_STORAGE_NOT_READY;
    }

    /* 检查是否支持叠录 */
    if (dev->ops == NULL || dev->ops->overdub_write == NULL) {
        DBG("[LooperStorage] Overdub not supported by this storage\n");
        return LOOPER_STORAGE_ERROR;
    }

    return dev->ops->overdub_write(dev, offset, buf, len, mix_mode);
}

/**
 * @brief 检查设备忙状态
 */
uint8_t LooperStorage_IsBusy(LooperStorageDevice_t *dev)
{
    if (dev == NULL || dev->ops == NULL) {
        return 0;
    }

    if (dev->ops->is_busy != NULL) {
        return dev->ops->is_busy(dev);
    }

    return 0; /* 默认不忙 */
}

/**
 * @brief 获取当前存储类型
 */
LooperStorageType_t LooperStorage_GetType(LooperStorageDevice_t *dev)
{
    if (dev == NULL) {
        return LOOPER_STORAGE_NOR_FLASH; /* 默认值 */
    }

    return dev->info.type;
}

/**
 * @brief 获取性能参数
 */
LooperStoragePerf_t* LooperStorage_GetPerformance(LooperStorageDevice_t *dev)
{
    if (dev == NULL) {
        return NULL;
    }

    return &dev->info.performance;
}
