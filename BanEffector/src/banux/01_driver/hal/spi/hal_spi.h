/**
 *****************************************************************************
 * @file     hal_spi.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     02-January-2026
 * @brief    SPI HAL适配层 - 封装SDK底层SPI驱动
 *****************************************************************************
 * @attention
 *
 * 本文件是SDK SPI驱动的包装层，提供统一的HAL接口。
 * 实际驱动实现在 MVsB1_Base_SDK/driver/ 目录中。
 *
 * SDK原始文件:
 *   - driver/driver/inc/spim.h        (SPI Master 寄存器级驱动)
 *   - driver/driver_api/inc/spim_interface.h (SPI Master 接口层)
 *   - driver/driver/libDriver.a       (编译好的驱动库)
 *
 *****************************************************************************
 */

#ifndef __HAL_SPI_H__
#define __HAL_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

/* 直接引用SDK的SPI驱动头文件 */
#include "spim.h"
#include "spim_interface.h"

/*******************************************************************************
 * SPI模式定义 (来自SDK spim.h)
 ******************************************************************************/
#define HAL_SPI_MODE0       0   /* CPOL=0, CPHA=0, 空闲低电平，第一边沿采样(上升沿) */
#define HAL_SPI_MODE1       1   /* CPOL=0, CPHA=1, 空闲低电平，第二边沿采样(下降沿) */
#define HAL_SPI_MODE2       2   /* CPOL=1, CPHA=0, 空闲高电平，第一边沿采样(下降沿) */
#define HAL_SPI_MODE3       3   /* CPOL=1, CPHA=1, 空闲高电平，第一边沿采样(上升沿) */

/*******************************************************************************
 * SPI时钟分频定义 (来自SDK spim.h, 基于288M PLL)
 ******************************************************************************/
#define HAL_SPI_CLK_24M     SPIM_CLK_DIV_24M    /* 24MHz */
#define HAL_SPI_CLK_12M     SPIM_CLK_DIV_12M    /* 12MHz */
#define HAL_SPI_CLK_6M      SPIM_CLK_DIV_6M     /* 6MHz */
#define HAL_SPI_CLK_3M      SPIM_CLK_DIV_3M     /* 3MHz */
#define HAL_SPI_CLK_1M5     SPIM_CLK_DIV_1M5    /* 1.5MHz */
#define HAL_SPI_CLK_750K    SPIM_CLK_DIV_750K   /* 750KHz */

/*******************************************************************************
 * SPI端口定义 (来自SDK spim.h)
 ******************************************************************************/
#define HAL_SPI_PORT0       SPIM_PORT0_A5_A6_A7     /* A5/A6/A7 */
#define HAL_SPI_PORT1       SPIM_PORT1_A20_A21_A22  /* A20/A21/A22 */

/*******************************************************************************
 * HAL SPI接口 - 封装SDK函数
 ******************************************************************************/

/**
 * @brief  初始化SPI Master
 * @param  mode: SPI模式 (HAL_SPI_MODE0 ~ HAL_SPI_MODE3)
 * @param  clkDiv: 时钟分频 (HAL_SPI_CLK_xxx)
 * @return TRUE成功, FALSE失败
 */
static inline bool HAL_SPI_Init(uint8_t mode, uint8_t clkDiv)
{
    return SPIM_Init(mode, clkDiv);
}

/**
 * @brief  选择SPI端口
 * @param  port: 端口选择 (HAL_SPI_PORT0 或 HAL_SPI_PORT1)
 */
static inline void HAL_SPI_PortSelect(uint8_t port)
{
    SPIM_IoConfig(port);
}

/**
 * @brief  SPI发送数据 (阻塞)
 * @param  data: 发送的数据缓冲区
 * @param  len: 数据长度
 * @return 发送的字节数
 */
static inline int HAL_SPI_Send(uint8_t *data, uint32_t len)
{
    return SPIM_Send(data, len);
}

/**
 * @brief  SPI接收数据 (阻塞)
 * @param  data: 接收的数据缓冲区
 * @param  len: 数据长度
 * @return 接收的字节数
 */
static inline int HAL_SPI_Recv(uint8_t *data, uint32_t len)
{
    return SPIM_Recv(data, len);
}

/**
 * @brief  SPI全双工收发 (阻塞)
 * @param  sendBuf: 发送缓冲区
 * @param  recvBuf: 接收缓冲区
 * @param  len: 数据长度
 * @return 传输的字节数
 */
static inline int HAL_SPI_TransferBlocking(uint8_t *sendBuf, uint8_t *recvBuf, uint32_t len)
{
    return SPIM_SendRecv(sendBuf, recvBuf, len);
}

/**
 * @brief  DMA方式SPI发送
 * @param  data: 发送缓冲区
 * @param  len: 数据长度
 */
static inline void HAL_SPI_DMA_Send(uint8_t *data, uint32_t len)
{
    SPIM_DMA_Send_Start(data, len);
}

/**
 * @brief  DMA方式SPI接收
 * @param  data: 接收缓冲区
 * @param  len: 数据长度
 */
static inline void HAL_SPI_DMA_Recv(uint8_t *data, uint32_t len)
{
    SPIM_DMA_Recv_Start(data, len);
}

/**
 * @brief  DMA方式SPI全双工收发
 * @param  sendBuf: 发送缓冲区
 * @param  recvBuf: 接收缓冲区
 * @param  len: 数据长度
 */
static inline void HAL_SPI_DMA_Transfer(uint8_t *sendBuf, uint8_t *recvBuf, uint32_t len)
{
    SPIM_DMA_Send_Recive_Start(sendBuf, recvBuf, len);
}

/**
 * @brief  检查DMA传输是否完成
 * @return TRUE完成, FALSE未完成
 */
static inline bool HAL_SPI_DMA_IsDone(void)
{
    return SPIM_DMA_FullDone();
}

/**
 * @brief  清除DMA完成标志
 * @param  isRx: TRUE清除接收完成, FALSE清除发送完成
 */
static inline void HAL_SPI_DMA_ClearDone(bool isRx)
{
    if (isRx) {
        SPIM_DMA_Done_Clear(PERIPHERAL_ID_SPIM_RX);
    } else {
        SPIM_DMA_Done_Clear(PERIPHERAL_ID_SPIM_TX);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* __HAL_SPI_H__ */
