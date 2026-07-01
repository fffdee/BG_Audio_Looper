/**
 * @file fat32_nand.h
 * @brief NAND Flash FAT32 文件系统集成
 *
 * 在 NAND Flash (W25N02) 上提供 FAT32 文件系统支持。
 * 包括 NAND 坏块管理与 FAT32 扇区映射的对接。
 *
 * NAND Flash 特性:
 *   - 页大小:   2048 字节 (4个FAT32扇区)
 *   - 块大小:   128KB (64页)
 *   - 总容量:   256MB
 *   - 写入前须擦除 (块为单位)
 *
 * NAND FAT32 分区布局 (可配置):
 *   方案A - 整片NAND用于FAT32:
 *     [0MB  - 256MB] FAT32 文件系统
 *
 *   方案B - 与合成器音色数据共存:
 *     [0MB  - 32MB]  FAT32 文件系统 (采样/WAV/用户数据)
 *     [32MB - 64MB]  音色索引表 (nand_store)
 *     [64MB - 256MB] 音色数据 (SF2 blob)
 */

#ifndef __FAT32_NAND_H__
#define __FAT32_NAND_H__

#include "product_def.h"

#if FAT32_EN

#include <stdint.h>
#include <stdbool.h>

/* BG_ERR 错误码定义（原 err_handle.h 属于 bangtsynth，已移除，此处内联） */
#ifndef BG_ERR_DEFINED
#define BG_ERR_DEFINED
typedef enum {
    SUCCESS = 0,
    ENABLE_INVALID_INPUT,
    ENABLE_OUT_OF_MEMORY,
    ENABLE_NOT_FOUND,
    ENABLE_IO_ERROR,
} BG_ERR;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * NAND FAT32 分区配置
 * ============================================ */

/** NAND FAT32 分区在 NAND 中的起始偏移 */
#ifndef FAT32_NAND_PARTITION_OFFSET
#define FAT32_NAND_PARTITION_OFFSET   0
#endif

/** NAND FAT32 分区大小 (默认32MB, 用于存储WAV采样等) */
#ifndef FAT32_NAND_PARTITION_SIZE
#define FAT32_NAND_PARTITION_SIZE     (32u * 1024u * 1024u)
#endif

/* ============================================
 * NAND FAT32 接口
 * ============================================ */

/**
 * 初始化 NAND FAT32
 *
 * 注册 NAND 磁盘 IO 驱动，选择 NAND 后端，初始化 FAT32。
 * 如果 NAND 上没有有效的 FAT32 文件系统，可调用 FAT32_NAND_Format() 格式化。
 *
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_NAND_Init(void);

/**
 * 反初始化 NAND FAT32
 */
void FAT32_NAND_DeInit(void);

/**
 * 格式化 NAND 为 FAT32 文件系统
 *
 * 警告: 这将擦除 NAND FAT32 分区中的所有数据!
 *
 * @return SUCCESS 或错误码
 */
BG_ERR FAT32_NAND_Format(void);

/**
 * 检查 NAND 上是否存在有效的 FAT32 文件系统
 *
 * @return true=有效, false=需要格式化
 */
bool FAT32_NAND_IsFormatted(void);

/**
 * 获取 NAND FAT32 分区可用空间 (字节)
 *
 * @return 可用字节数
 */
uint32_t FAT32_NAND_GetFreeSpace(void);

#ifdef __cplusplus
}
#endif

#endif /* FAT32_EN */

#endif /* __FAT32_NAND_H__ */
