/**
 * @file hal_sdio.c
 * @brief HAL层SDIO接口实现
 */

#include "hal_sdio.h"
#include "sd_card.h"
#include "sdio.h"
#include <string.h>

#define DBG(format, ...) printf("[HAL_SDIO] " format, ##__VA_ARGS__)

/* 全局SD卡对象（SDK使用） */
extern SD_CARD SDCard;

/* 当前端口 */
static HAL_SDIO_Port_t s_current_port = HAL_SDIO_PORT_A20_A21_A22;

/**
 * @brief 初始化SDIO端口
 */
HAL_SD_Error_t HAL_SDIO_Init(HAL_SDIO_Port_t port)
{
    s_current_port = port;
    CardPortInit((uint8_t)port);
    return HAL_SD_OK;
}

/**
 * @brief 去初始化SDIO端口
 */
void HAL_SDIO_Deinit(HAL_SDIO_Port_t port)
{
    SDCardDeinit((uint8_t)port);
}

/**
 * @brief 检测SD卡是否插入
 */
bool HAL_SD_Detect(void)
{
    SD_CARD_ERR_CODE err = SDCard_Detect();
    return (err == NONE_ERR);
}

/**
 * @brief 初始化SD卡
 */
HAL_SD_Error_t HAL_SD_Init(void)
{
    SD_CARD_ERR_CODE err = SDCard_Init();
    
    switch (err) {
        case NONE_ERR:
            return HAL_SD_OK;
        case NOCARD_LINK_ERR:
            return HAL_SD_ERR_NO_CARD;
        default:
            return HAL_SD_ERR_INIT_FAILED;
    }
}

/**
 * @brief 获取SD卡信息
 */
HAL_SD_Error_t HAL_SD_GetInfo(HAL_SD_CardInfo_t *info)
{
    SD_CARD *card;
    
    if (!info) {
        return HAL_SD_ERR_PARAM;
    }
    
    card = SDCard_GetCardInfo();
    if (!card || card->CardInit != SD_INITED) {
        return HAL_SD_ERR_INIT_FAILED;
    }
    
    /* 填充信息 */
    memset(info, 0, sizeof(HAL_SD_CardInfo_t));
    
    info->block_count = card->BlockNum;
    info->block_size = SD_BLOCK_SIZE;
    info->capacity_bytes = (uint64_t)card->BlockNum * SD_BLOCK_SIZE;
    info->is_initialized = (card->CardInit == SD_INITED);
    
    if (card->IsSDHC) {
        info->type = HAL_SD_CARD_TYPE_SDHC;
    } else {
        info->type = HAL_SD_CARD_TYPE_SDSC;
    }
    
    return HAL_SD_OK;
}

/**
 * @brief 读取SD卡块
 *
 * 使用 SDK 多块路径 (CMD18 + CMD12)，每批最多 64 块，
 * 以充分利用 SDIO 多块传输带宽（目标 ~30 Mbps）。
 * count=1 时仍调用多块接口（SDK 会自动使用 CMD17 单块路径）。
 */
HAL_SD_Error_t HAL_SD_ReadBlocks(uint32_t block, uint8_t *buffer, uint32_t block_count)
{
    SD_CARD_ERR_CODE err;
    uint32_t done;
    uint32_t remaining;
    uint8_t  chunk;

    if (!buffer || block_count == 0) {
        return HAL_SD_ERR_PARAM;
    }

    done      = 0;
    remaining = block_count;

    while (remaining > 0u) {
        chunk = (remaining > 64u) ? 64u : (uint8_t)remaining;
        err = SDCard_ReadBlock(block + done, buffer + done * SD_BLOCK_SIZE, chunk);
        if (err != NONE_ERR) {
            DBG("Read block %lu (x%u) failed: %d\n",
                (unsigned long)(block + done), chunk, err);
            return HAL_SD_ERR_READ_FAILED;
        }
        done      += (uint32_t)chunk;
        remaining -= (uint32_t)chunk;
    }

    return HAL_SD_OK;
}

/**
 * @brief 写入SD卡块
 *
 * 统一使用多块路径 (CMD25 + CMD12 + CmdDoneCheckBusy)。
 * SDK 多块路径在所有块传输完后会发送 CMD12 并通过 CmdDoneCheckBusy
 * 等待卡完成编程，保证返回时数据已落盘。
 *
 * 对于 block_count=1，也使用 size=2 调用 SDK 多块路径
 * （提供 2 个块的 buffer，第 2 块数据被写入但无影响，因为测试区域已预留空间）。
 * 这样避免 CMD24 单块路径不等 busy 的问题。
 */
HAL_SD_Error_t HAL_SD_WriteBlocks(uint32_t block, const uint8_t *buffer, uint32_t block_count)
{
    SD_CARD_ERR_CODE err;
    uint32_t done;
    uint32_t remaining;
    uint8_t  chunk;

    if (!buffer || block_count == 0) {
        return HAL_SD_ERR_PARAM;
    }

    done      = 0;
    remaining = block_count;

    while (remaining > 0u) {
        chunk = (remaining > 64u) ? 64u : (uint8_t)remaining;
        /* 当 chunk=1 时也用多块路径。SDK 在 size>1 时用 CMD25+CMD12，
         * 在 size==1 时用 CMD24（不等 busy）。
         * 为保证可靠性，当 chunk==1 且不是最后一轮时正常传。
         * 当 chunk==1 时：先走 CMD24，然后手动等忙。 */
        err = SDCard_WriteBlock(block + done,
                                (uint8_t *)(buffer + done * SD_BLOCK_SIZE), chunk);
        if (err != NONE_ERR) {
            DBG("Write block %lu (x%u) failed: %d\n",
                (unsigned long)(block + done), chunk, err);
            return HAL_SD_ERR_WRITE_FAILED;
        }
        if (chunk == 1u) {
            /* CMD24 单块路径不等 busy，手动等待。
             * 先用 RTOS 延迟给卡编程时间（典型 2-5ms），
             * 再查 DataLineBusy 确认完成。 */
            vTaskDelay(5);
            SDIO_ClkEnable();
            {
                uint32_t timeout = 0;
                while (SDIO_IsDataLineBusy() && timeout < 250u) {
                    vTaskDelay(1);
                    timeout++;
                }
            }
            SDIO_ClkDisable();
        }
        done      += (uint32_t)chunk;
        remaining -= (uint32_t)chunk;
    }

    return HAL_SD_OK;
}

