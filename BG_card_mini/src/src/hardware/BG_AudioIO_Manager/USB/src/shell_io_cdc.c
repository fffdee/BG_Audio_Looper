/**
 *****************************************************************************
 * @file     shell_io_cdc.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     16-December-2025
 * @brief    Shell CDC IO适配器实现
 *****************************************************************************
 */

#include "shell_io_cdc.h"
#include "otg_device_cdc.h"

/*******************************************************************************
 * CDC IO适配函数
 ******************************************************************************/

static uint16_t CDC_Send(uint8_t *data, uint16_t len)
{
    return OTG_DeviceCDC_Send(data, len);
}

static uint16_t CDC_Recv(uint8_t *data, uint16_t maxLen)
{
    return OTG_DeviceCDC_Receive(data, maxLen);
}

static uint16_t CDC_Available(void)
{
    return OTG_DeviceCDC_GetRxCount();
}

/*******************************************************************************
 * CDC IO接口实例
 ******************************************************************************/
static const ShellIO_t g_CDC_IO = {
    .name      = "USB-CDC",
    .send      = CDC_Send,
    .recv      = CDC_Recv,
    .available = CDC_Available
};

/*******************************************************************************
 * 公共函数
 ******************************************************************************/

const ShellIO_t* ShellIO_CDC_Get(void)
{
    return &g_CDC_IO;
}

void ShellIO_CDC_Init(void)
{
    Shell_Init();
    Shell_SetIO(&g_CDC_IO);
    Shell_RegisterAllModules();
}
