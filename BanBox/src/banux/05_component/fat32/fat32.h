/**
 * @file fat32.h
 * @brief FAT32 文件系统组件 - 统一入口头文件
 *
 * 独立于合成器的 FAT32 文件系统组件。
 * 支持多种存储后端 (SD卡, NAND Flash, NOR Flash)。
 *
 * 使用方法:
 *   1. 注册并选择存储后端:
 *      FAT32_DiskIO_Register(FAT32_DISK_SDCARD, &fat32_diskio_sdcard);
 *      FAT32_DiskIO_Select(FAT32_DISK_SDCARD);
 *
 *   2. 初始化文件系统:
 *      FAT32_Init();
 *
 *   3. 文件操作:
 *      FAT32_OpenFile("test.sf2", &handle);
 *      FAT32_ReadFile(&handle, buf, size);
 *      FAT32_CloseFile(&handle);
 *
 *   4. 切换到 NAND 后端:
 *      FAT32_DeInit();
 *      FAT32_DiskIO_Register(FAT32_DISK_NAND, &fat32_diskio_nand);
 *      FAT32_DiskIO_Select(FAT32_DISK_NAND);
 *      FAT32_Init();
 */

#ifndef __FAT32_COMPONENT_H__
#define __FAT32_COMPONENT_H__

#include "product_def.h"

#if FAT32_EN

/* 核心 FAT32 读写接口 */
#include "fat32_reader.h"

/* 存储后端抽象层 */
#include "fat32_diskio.h"

#endif /* FAT32_EN */

#endif /* __FAT32_COMPONENT_H__ */
