/**
 * BG_Storage - 音源存储抽象层
 * 
 * 功能:
 * - 提供平台无关的音源数据存储/读取接口
 * - 支持固定大小(16MB)的音源数据区域
 * - 支持 SF2/BGS 等多种格式的音源数据
 * - 可配置为文件、内存映射或嵌入式 Flash 存储
 * 
 * 使用场景:
 * - Linux: 读写 16MB bin 文件
 * - STM32: 读取 Flash 中的音源数据
 * - ESP32: 读取 SPIFFS/LittleFS 文件系统
 */

#ifndef _BG_STORAGE_H__
#define _BG_STORAGE_H__

#include <stdint.h>
#include <stddef.h>
#include "err_handle.h"
#include "bg_config.h"

/* ============================================
 * 存储配置 (按平台设置)
 * ============================================ */
#if (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
#define BG_STORAGE_SIZE         (8 * 1024 * 1024)   /* BP10 Flash#1: 8MB */
#else
#define BG_STORAGE_SIZE         (64 * 1024 * 1024)  /* 其他平台: 64MB */
#endif
#define BG_STORAGE_SECTOR_SIZE  (4096)              /* 扇区大小 (4KB) */

/* ============================================
 * 存储访问模式
 * ============================================ */
typedef enum {
    BG_STORAGE_MODE_READ_ONLY  = 0x01,  /* 只读模式 */
    BG_STORAGE_MODE_WRITE_ONLY = 0x02,  /* 只写模式 */
    BG_STORAGE_MODE_READ_WRITE = 0x03,  /* 读写模式 */
} BG_Storage_Mode_t;

/* ============================================
 * 平台相关的存储操作接口
 * ============================================ */
typedef struct {
    /**
     * 初始化存储设备
     * 
     * @param path 存储路径 (Linux: 文件路径, STM32: Flash 地址, ESP32: 分区名)
     * @param mode 访问模式
     * @return SUCCESS=成功, 其他=失败
     */
    BG_ERR (*init)(const char *path, BG_Storage_Mode_t mode);
    
    /**
     * 反初始化存储设备
     * 
     * @return SUCCESS=成功
     */
    BG_ERR (*deinit)(void);
    
    /**
     * 从存储设备读取数据
     * 
     * @param offset 读取偏移 (相对于音源数据区起始地址)
     * @param buffer 数据缓冲区
     * @param size 读取字节数
     * @return 实际读取的字节数, <0 表示错误
     */
    int (*read)(uint32_t offset, void *buffer, size_t size);
    
    /**
     * 向存储设备写入数据
     * 
     * @param offset 写入偏移 (相对于音源数据区起始地址)
     * @param buffer 数据缓冲区
     * @param size 写入字节数
     * @return 实际写入的字节数, <0 表示错误
     */
    int (*write)(uint32_t offset, const void *buffer, size_t size);
    
    /**
     * 擦除存储扇区 (仅 Flash 等需要擦除的设备)
     * 
     * @param offset 扇区起始偏移 (必须对齐到扇区大小)
     * @param size 擦除字节数 (必须是扇区大小的倍数)
     * @return SUCCESS=成功, 其他=失败
     */
    BG_ERR (*erase)(uint32_t offset, size_t size);
    
    /**
     * 同步数据到存储设备 (确保数据写入)
     * 
     * @return SUCCESS=成功
     */
    BG_ERR (*sync)(void);
    
    /**
     * 获取存储设备信息
     * 
     * @param total_size 返回总容量
     * @param free_size 返回可用空间 (可选, Flash 设备返回 0)
     * @return SUCCESS=成功
     */
    BG_ERR (*get_info)(uint32_t *total_size, uint32_t *free_size);
    
} BG_Storage_Driver_t;

/* ============================================
 * 音源存储管理器
 * ============================================ */
typedef struct {
    /**
     * 初始化音源存储
     * 
     * 功能:
     * - 根据平台选择合适的存储驱动
     * - 打开音源数据区域
     * - 验证存储完整性
     * 
     * @param path 存储路径
     *   - Linux: "soundbank.bin" (16MB 文件)
     *   - STM32: "0x08100000" (Flash 地址字符串)
     *   - ESP32: "soundbank" (分区名)
     * @param mode 访问模式
     * @return SUCCESS=成功
     */
    BG_ERR (*Init)(const char *path, BG_Storage_Mode_t mode);
    
    /**
     * 反初始化音源存储
     */
    BG_ERR (*DeInit)(void);
    
    /**
     * 读取音源数据
     * 
     * @param offset 读取偏移 (0 ~ BG_STORAGE_SIZE-1)
     * @param buffer 数据缓冲区
     * @param size 读取字节数
     * @return 实际读取的字节数, <0 表示错误
     */
    int (*Read)(uint32_t offset, void *buffer, size_t size);
    
    /**
     * 写入音源数据
     * 
     * @param offset 写入偏移 (0 ~ BG_STORAGE_SIZE-1)
     * @param buffer 数据缓冲区
     * @param size 写入字节数
     * @return 实际写入的字节数, <0 表示错误
     */
    int (*Write)(uint32_t offset, const void *buffer, size_t size);
    
    /**
     * 擦除音源数据区域
     * 
     * @param offset 擦除起始偏移 (必须对齐到扇区)
     * @param size 擦除大小 (必须是扇区的倍数)
     * @return SUCCESS=成功
     */
    BG_ERR (*Erase)(uint32_t offset, size_t size);
    
    /**
     * 同步数据 (确保写入完成)
     */
    BG_ERR (*Sync)(void);
    
    /**
     * 获取存储信息
     */
    BG_ERR (*GetInfo)(uint32_t *total_size, uint32_t *used_size);
    
    /**
     * 设置自定义存储驱动 (用于非默认平台)
     * 
     * @param driver 自定义驱动实现
     */
    void (*SetDriver)(const BG_Storage_Driver_t *driver);
    
} BG_Storage_t;

/* 导出接口实例 */
extern BG_Storage_t BG_Storage;

/* ============================================
 * 平台驱动实现 (由平台 HAL 提供)
 * 仅声明当前平台使用的驱动
 * ============================================ */

#if (BG_TARGET_PLATFORM == BG_PLATFORM_LINUX)
extern const BG_Storage_Driver_t bg_storage_driver_linux;
#elif (BG_TARGET_PLATFORM == BG_PLATFORM_STM32)
extern const BG_Storage_Driver_t bg_storage_driver_stm32;
#elif (BG_TARGET_PLATFORM == BG_PLATFORM_ESP32)
extern const BG_Storage_Driver_t bg_storage_driver_esp32;
#elif (BG_TARGET_PLATFORM == BG_PLATFORM_BP10)
/* BP10 平台下按板级选择存储驱动 */
#ifdef BANDATAHUB
extern const BG_Storage_Driver_t bg_storage_driver_bandatahub;
#else
extern const BG_Storage_Driver_t bg_storage_driver_bp10;
#endif
#endif

/* 内嵌 SF2 音源存储驱动 (从 const 数组读取, 无需外部 Flash) */
extern const BG_Storage_Driver_t bg_storage_driver_embedded;

#endif /* _BG_STORAGE_H__ */
