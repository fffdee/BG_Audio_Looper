/**
 * @file hal_sdio.h
 * @brief HAL层SDIO接口抽象
 * @note 封装SDK的SDIO和SD卡驱动接口
 */

#ifndef HAL_SDIO_H
#define HAL_SDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* SDK头文件 */
#include "sdio.h"
#include "sd_card.h"

/* SDIO端口定义 */
typedef enum {
    HAL_SDIO_PORT_A15_A16_A17 = SDIO_A15_A16_A17,  /* GPIO A15~A17 */
    HAL_SDIO_PORT_A20_A21_A22 = SDIO_A20_A21_A22,  /* GPIO A20~A22 */
} HAL_SDIO_Port_t;

/* SD卡类型 */
typedef enum {
    HAL_SD_CARD_TYPE_UNKNOWN = 0,
    HAL_SD_CARD_TYPE_SDSC,      /* SD Standard Capacity (≤2GB) */
    HAL_SD_CARD_TYPE_SDHC,      /* SD High Capacity (2GB~32GB) */
    HAL_SD_CARD_TYPE_SDXC,      /* SD eXtended Capacity (32GB~2TB) */
} HAL_SD_CardType_t;

/* SD卡信息 */
typedef struct {
    HAL_SD_CardType_t type;
    uint32_t block_count;       /* 总块数 */
    uint32_t block_size;        /* 块大小（通常512字节） */
    uint64_t capacity_bytes;    /* 总容量（字节） */
    bool is_initialized;
} HAL_SD_CardInfo_t;

/* 错误码 */
typedef enum {
    HAL_SD_OK = 0,
    HAL_SD_ERR_TIMEOUT = -1,
    HAL_SD_ERR_NO_CARD = -2,
    HAL_SD_ERR_INIT_FAILED = -3,
    HAL_SD_ERR_READ_FAILED = -4,
    HAL_SD_ERR_WRITE_FAILED = -5,
    HAL_SD_ERR_PARAM = -6,
} HAL_SD_Error_t;

/**
 * @brief 初始化SDIO端口
 * @param port SDIO端口选择
 * @return HAL_SD_OK成功，其他失败
 */
HAL_SD_Error_t HAL_SDIO_Init(HAL_SDIO_Port_t port);

/**
 * @brief 去初始化SDIO端口
 * @param port SDIO端口选择
 */
void HAL_SDIO_Deinit(HAL_SDIO_Port_t port);

/**
 * @brief 检测SD卡是否插入
 * @return true=已插入，false=未插入
 */
bool HAL_SD_Detect(void);

/**
 * @brief 初始化SD卡
 * @return HAL_SD_OK成功，其他失败
 */
HAL_SD_Error_t HAL_SD_Init(void);

/**
 * @brief 获取SD卡信息
 * @param info 输出SD卡信息
 * @return HAL_SD_OK成功，其他失败
 */
HAL_SD_Error_t HAL_SD_GetInfo(HAL_SD_CardInfo_t *info);

/**
 * @brief 读取SD卡块
 * @param block 起始块号
 * @param buffer 读取缓冲区
 * @param block_count 读取块数
 * @return HAL_SD_OK成功，其他失败
 */
HAL_SD_Error_t HAL_SD_ReadBlocks(uint32_t block, uint8_t *buffer, uint32_t block_count);

/**
 * @brief 写入SD卡块
 * @param block 起始块号
 * @param buffer 写入数据
 * @param block_count 写入块数
 * @return HAL_SD_OK成功，其他失败
 */
HAL_SD_Error_t HAL_SD_WriteBlocks(uint32_t block, const uint8_t *buffer, uint32_t block_count);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SDIO_H */
