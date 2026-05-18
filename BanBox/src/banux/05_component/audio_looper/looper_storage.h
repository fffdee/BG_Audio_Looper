/**
 **************************************************************************************
 * @file    looper_storage.h
 * @brief   Looper 存储抽象层接口定义
 *
 * @details 提供统一的存储访问接口，支持 NOR Flash、NAND Flash、PSRAM 等不同存储介质
 *          通过替换操作函数表 (ops) 即可适配不同硬件，无需修改 looper 核心逻辑
 *
 * @author  BanGO
 * @version V2.0.0
 * @date    2026-04-09
 *
 * @Copyright (C) 2026, Audio Looper Project. All rights reserved.
 **************************************************************************************
 */

#ifndef __LOOPER_STORAGE_H__
#define __LOOPER_STORAGE_H__

#include "type.h"
#include <stdint.h>
#include "audio_looper.h"

/* ============================================================================
 * 存储类型枚举
 * ============================================================================ */
typedef enum {
    LOOPER_STORAGE_NOR_FLASH = 0,   /* NOR Flash (W25Qxx 系列) */
    LOOPER_STORAGE_NAND_FLASH = 1,  /* NAND Flash (W25N02 等) */
    LOOPER_STORAGE_PSRAM = 2,       /* PSRAM (支持叠录) */
    LOOPER_STORAGE_SDCARD = 3       /* SD Card (预留扩展) */
} LooperStorageType_t;

/* ============================================================================
 * 存储状态枚举
 * ============================================================================ */
typedef enum {
    LOOPER_STORAGE_OK = 0,          /* 操作成功 */
    LOOPER_STORAGE_ERROR = 1,       /* 操作失败 */
    LOOPER_STORAGE_BUSY = 2,        /* 设备忙 */
    LOOPER_STORAGE_TIMEOUT = 3,     /* 超时 */
    LOOPER_STORAGE_NOT_READY = 4,   /* 设备未就绪 */
    LOOPER_STORAGE_INVALID_PARAM = 5 /* 参数错误 */
} LooperStorageStatus_t;

/* ============================================================================
 * 存储性能参数结构体 (带宽测试结果)
 * ============================================================================ */
typedef struct {
    uint32_t write_speed_kbps;      /* 写入速度 (KB/s) */
    uint32_t read_speed_kbps;       /* 读取速度 (KB/s) */
    uint32_t max_concurrent_tracks; /* 支持同时读写的最大段数 */
    uint32_t page_size;             /* 页大小 (字节) */
    uint32_t block_size;            /* 块大小 (字节) */
    uint32_t total_capacity;        /* 总容量 (字节) */
    uint8_t  support_overdub;       /* 是否支持叠录 (1=支持, 0=不支持) */
    uint8_t  bandwidth_tested;      /* 是否已执行带宽测试 (1=已测试, 0=未测试) */
} LooperStoragePerf_t;

/* ============================================================================
 * 存储设备信息结构体
 * ============================================================================ */
typedef struct {
    LooperStorageType_t type;       /* 存储类型 */
    uint32_t total_size;            /* 总容量 (字节) */
    uint32_t page_size;             /* 页大小 (字节) */
    uint32_t block_size;            /* 块大小 (字节) */
    uint32_t erase_block_size;      /* 擦除块大小 (字节) */
    const char *name;               /* 设备名称 */
    LooperStoragePerf_t performance; /* 性能参数 */
} LooperStorageInfo_t;

/* ============================================================================
 * 前向声明：存储操作函数表
 * ============================================================================ */
typedef struct LooperStorageOps LooperStorageOps_t;

/* ============================================================================
 * 存储设备句柄结构体
 * ============================================================================ */
typedef struct {
    const LooperStorageOps_t *ops;  /* 操作函数表指针 */
    LooperStorageInfo_t info;       /* 设备信息 */
    void *private_data;             /* 私有数据 (驱动特定上下文) */
    uint8_t initialized;            /* 初始化标志 */
} LooperStorageDevice_t;

/* ============================================================================
 * 存储操作函数表 (虚函数表)
 *
 * @details 不同存储介质需实现以下接口，looper 核心通过 ops 调用
 * ============================================================================ */
struct LooperStorageOps {
    /**
     * @brief 初始化存储设备
     * @param dev 存储设备句柄
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*init)(LooperStorageDevice_t *dev);

    /**
     * @brief 反初始化存储设备
     * @param dev 存储设备句柄
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*deinit)(LooperStorageDevice_t *dev);

    /**
     * @brief 读取数据
     * @param dev 存储设备句柄
     * @param offset 偏移地址 (字节)
     * @param buf 接收缓冲区
     * @param len 读取长度 (字节)
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*read)(LooperStorageDevice_t *dev, uint32_t offset, 
                                   uint8_t *buf, uint32_t len);

    /**
     * @brief 写入数据
     * @param dev 存储设备句柄
     * @param offset 偏移地址 (字节)
     * @param buf 数据缓冲区
     * @param len 写入长度 (字节)
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*write)(LooperStorageDevice_t *dev, uint32_t offset, 
                                    const uint8_t *buf, uint32_t len);

    /**
     * @brief 擦除扇区/块
     * @param dev 存储设备句柄
     * @param offset 扇区/块起始地址 (字节)
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*erase_block)(LooperStorageDevice_t *dev, uint32_t offset);

    /**
     * @brief 擦除整片存储设备
     * @param dev 存储设备句柄
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*erase_chip)(LooperStorageDevice_t *dev);

    /**
     * @brief 获取设备信息
     * @param dev 存储设备句柄
     * @param info 输出设备信息
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*get_info)(LooperStorageDevice_t *dev, LooperStorageInfo_t *info);

    /**
     * @brief 执行带宽测试并计算性能参数
     *
     * @details 测试读写速度并计算可支持的同时段数：
     *          max_concurrent_tracks = min(write_speed, read_speed) / 
     *                                  (audio_data_rate_per_track)
     *          其中 audio_data_rate = LOOPER_SAMPLE_RATE Hz × 4 Bytes/sample
     *
     * @param dev 存储设备句柄
     * @param perf 输出性能参数
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*benchmark)(LooperStorageDevice_t *dev, LooperStoragePerf_t *perf);

    /**
     * @brief 叠录写入 (仅 PSRAM 支持)
     *
     * @details 读取现有数据，混音后写回
     *          overdub_data[i] = existing_data[i] + new_data[i]
     *          支持叠录的设备必须实现此接口，不支持的可置为 NULL
     *
     * @param dev 存储设备句柄
     * @param offset 偏移地址 (字节)
     * @param buf 新录制数据
     * @param len 数据长度 (字节)
     * @param mix_mode 混音模式 (0=替换, 1=相加混音, 2=平均混音)
     * @return LOOPER_STORAGE_OK=成功, LOOPER_STORAGE_ERROR=不支持叠录
     */
    LooperStorageStatus_t (*overdub_write)(LooperStorageDevice_t *dev, uint32_t offset,
                                           const uint8_t *buf, uint32_t len, uint8_t mix_mode);

    /**
     * @brief 检查设备忙状态 (用于异步擦除)
     * @param dev 存储设备句柄
     * @return 1=忙, 0=空闲
     */
    uint8_t (*is_busy)(LooperStorageDevice_t *dev);

    /**
     * @brief 刷新待写缓冲区 (NAND 专用)
     *
     * @details 将内部页缓冲区中尚未提交的数据写入物理存储。
     *          NAND 适配器使用 256B 写入累积为 2048B 物理页，录制结束后
     *          必须调用此函数以确保最后一个不满页的数据被正确写入。
     *          其他存储介质可将此接口设为 NULL。
     * @param dev 存储设备句柄
     * @return LOOPER_STORAGE_OK=成功
     */
    LooperStorageStatus_t (*flush)(LooperStorageDevice_t *dev);
};

/* ============================================================================
 * 全局存储设备实例 (在 looper_storage.c 中定义)
 * ============================================================================ */
extern LooperStorageDevice_t g_looper_storage;

/* ============================================================================
 * 存储抽象层通用接口
 * ============================================================================ */

/**
 * @brief 注册存储设备 (绑定 ops 函数表)
 *
 * @param dev 存储设备句柄
 * @param ops 操作函数表
 * @param type 存储类型
 * @return LOOPER_STORAGE_OK=成功
 */
LooperStorageStatus_t LooperStorage_Register(LooperStorageDevice_t *dev, 
                                              const LooperStorageOps_t *ops,
                                              LooperStorageType_t type);

/**
 * @brief 初始化存储设备
 * @param dev 存储设备句柄
 * @return LOOPER_STORAGE_OK=成功
 */
LooperStorageStatus_t LooperStorage_Init(LooperStorageDevice_t *dev);

/**
 * @brief 反初始化存储设备
 * @param dev 存储设备句柄
 * @return LOOPER_STORAGE_OK=成功
 */
LooperStorageStatus_t LooperStorage_Deinit(LooperStorageDevice_t *dev);

/**
 * @brief 读取数据
 */
LooperStorageStatus_t LooperStorage_Read(LooperStorageDevice_t *dev, uint32_t offset, 
                                         uint8_t *buf, uint32_t len);

/**
 * @brief 写入数据
 */
LooperStorageStatus_t LooperStorage_Write(LooperStorageDevice_t *dev, uint32_t offset, 
                                          const uint8_t *buf, uint32_t len);

/**
 * @brief 擦除块
 */
LooperStorageStatus_t LooperStorage_EraseBlock(LooperStorageDevice_t *dev, uint32_t offset);

/**
 * @brief 擦除整片
 */
LooperStorageStatus_t LooperStorage_EraseChip(LooperStorageDevice_t *dev);

/**
 * @brief 刷新待写缓冲区 (NAND Flash 录制结束后必须调用)
 *
 * @details 对于 NAND Flash 适配器，写入操作会先缓存进 2048B 的页缓冲区，
 *          只有当缓冲区写满一个物理页时才真正写入 Flash。录制结束时需主动
 *          调用此函数，以强制提交最后一个不满页的数据。
 *          对于 NOR Flash / PSRAM 等其他介质，此函数是空操作，可安全调用。
 */
LooperStorageStatus_t LooperStorage_Flush(LooperStorageDevice_t *dev);

/**
 * @brief 获取设备信息
 */
LooperStorageStatus_t LooperStorage_GetInfo(LooperStorageDevice_t *dev, LooperStorageInfo_t *info);

/**
 * @brief 执行带宽测试
 *
 * @details 首次启动时调用，测试结果保存到 flash 参数管理
 *          后续启动直接从 flash 读取性能参数
 */
LooperStorageStatus_t LooperStorage_Benchmark(LooperStorageDevice_t *dev, LooperStoragePerf_t *perf);

/**
 * @brief 叠录写入
 */
LooperStorageStatus_t LooperStorage_OverdubWrite(LooperStorageDevice_t *dev, uint32_t offset,
                                                 const uint8_t *buf, uint32_t len, uint8_t mix_mode);

/**
 * @brief 检查设备忙状态
 */
uint8_t LooperStorage_IsBusy(LooperStorageDevice_t *dev);

/**
 * @brief 获取当前存储类型
 */
LooperStorageType_t LooperStorage_GetType(LooperStorageDevice_t *dev);

/**
 * @brief 获取性能参数
 */
LooperStoragePerf_t* LooperStorage_GetPerformance(LooperStorageDevice_t *dev);

/* ============================================================================
 * 存储适配器注册接口 (由各个 adapter 实现)
 * ============================================================================ */

/**
 * @brief 注册 NOR Flash 适配器
 * @return 操作函数表指针
 */
const LooperStorageOps_t* LooperStorageAdapter_GetNorFlashOps(void);

/**
 * @brief 注册 NAND Flash 适配器
 * @return 操作函数表指针
 */
const LooperStorageOps_t* LooperStorageAdapter_GetNandFlashOps(void);

/**
 * @brief 注册 PSRAM 适配器
 * @return 操作函数表指针
 */
const LooperStorageOps_t* LooperStorageAdapter_GetPsramOps(void);

#endif /* __LOOPER_STORAGE_H__ */
