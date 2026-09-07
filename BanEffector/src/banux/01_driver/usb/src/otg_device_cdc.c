/**
 *****************************************************************************
 * @file     otg_device_cdc.c
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     15-December-2025
 * @brief    CDC serial port device implementation
 *****************************************************************************
 * @attention
 *
 * CDC (Communication Device Class) implementation for USB virtual COM port
 * 
 *****************************************************************************
 */

#include <string.h>
#include "type.h"
#include "otg_device_hcd.h"
#include "debug.h"
#include "otg_device_standard_request.h"
#include "otg_device_cdc.h"

// CDC Device Instance
UsbCDC_t UsbCDC;

// External variables
extern uint8_t Setup[];
extern uint8_t Request[];

// Forward declaration of interrupt callback
void OnDeviceCDC_BulkOutReceived(void);

/**
 * @brief  Initialize CDC device
 */
bool OTG_DeviceCDC_Init(void)
{
    memset(&UsbCDC, 0, sizeof(UsbCDC_t));
    
    // Default line coding: 115200 bps, 1 stop bit, no parity, 8 data bits
    UsbCDC.LineCoding.dwDTERate = 115200;
    UsbCDC.LineCoding.bCharFormat = 0;  // 1 stop bit
    UsbCDC.LineCoding.bParityType = 0;  // No parity
    UsbCDC.LineCoding.bDataBits = 8;    // 8 data bits
    
    // Initialize control line state
    UsbCDC.ControlLineState.DTR = 0;
    UsbCDC.ControlLineState.RTS = 0;
    UsbCDC.IsConnected = 0;  // 初始状态为未连接
    
    // Initialize buffers
    UsbCDC.RxHead = 0;
    UsbCDC.RxTail = 0;
    UsbCDC.RxCount = 0;
    
    UsbCDC.TxHead = 0;
    UsbCDC.TxTail = 0;
    UsbCDC.TxCount = 0;
    UsbCDC.TxBusy = 0;
    
    UsbCDC.InitOk = 1;
    
    // 注册USB Bulk OUT端点的中断回调（接收数据时自动触发）
    // 这样就不需要在Task中轮询，消除了error 001错误
    OTG_EndpointInterruptEnable(DEVICE_CDC_DATA_OUT_EP, OnDeviceCDC_BulkOutReceived);
    
    DBG("CDC: Interrupt mode enabled for Bulk OUT endpoint\n");
    
    return TRUE;
}

/**
 * @brief  DeInitialize CDC device
 */
bool OTG_DeviceCDC_DeInit(void)
{
    UsbCDC.InitOk = 0;
    memset(&UsbCDC, 0, sizeof(UsbCDC_t));
    return TRUE;
}

/**
 * @brief  CDC class specific request handler
 */
void OTG_DeviceCDC_Request(void)
{
    uint8_t bRequest = Setup[1];  // 重命名避免与全局Request[]数组冲突
    
    switch(bRequest)
    {
        case CDC_SET_LINE_CODING:
            // Data已经在全局Request[]缓冲区中，直接解析
            // Request[0-6]包含7字节的Line Coding数据
            UsbCDC.LineCoding.dwDTERate = Request[0] | (Request[1] << 8) | 
                                           (Request[2] << 16) | (Request[3] << 24);
            UsbCDC.LineCoding.bCharFormat = Request[4];
            UsbCDC.LineCoding.bParityType = Request[5];
            UsbCDC.LineCoding.bDataBits = Request[6];
            
            // Windows打开串口时会发送SET_LINE_CODING，此时认为串口已连接
            UsbCDC.IsConnected = 1;
            DBG("CDC: Set Line Coding - Baudrate: %u, Format: %u, Parity: %u, DataBits: %u\n", 
                UsbCDC.LineCoding.dwDTERate, UsbCDC.LineCoding.bCharFormat,
                UsbCDC.LineCoding.bParityType, UsbCDC.LineCoding.bDataBits);
            break;
            
        case CDC_GET_LINE_CODING:
            // Send current line coding data - 手动打包避免对齐问题
            {
                uint8_t lineCodingBuf[7];
                lineCodingBuf[0] = (uint8_t)(UsbCDC.LineCoding.dwDTERate & 0xFF);
                lineCodingBuf[1] = (uint8_t)((UsbCDC.LineCoding.dwDTERate >> 8) & 0xFF);
                lineCodingBuf[2] = (uint8_t)((UsbCDC.LineCoding.dwDTERate >> 16) & 0xFF);
                lineCodingBuf[3] = (uint8_t)((UsbCDC.LineCoding.dwDTERate >> 24) & 0xFF);
                lineCodingBuf[4] = UsbCDC.LineCoding.bCharFormat;
                lineCodingBuf[5] = UsbCDC.LineCoding.bParityType;
                lineCodingBuf[6] = UsbCDC.LineCoding.bDataBits;
                OTG_DeviceControlSend(lineCodingBuf, 7, 3);
            }
            DBG("CDC: Get Line Coding - Baudrate: %u\n", UsbCDC.LineCoding.dwDTERate);
            break;
            
        case CDC_SET_CONTROL_LINE_STATE:
            // Extract DTR and RTS from wValue
            UsbCDC.ControlLineState.DTR = (Setup[2] & 0x01) ? 1 : 0;
            UsbCDC.ControlLineState.RTS = (Setup[2] & 0x02) ? 1 : 0;
            // 注意：某些终端可能不设置DTR/RTS，但串口仍然打开
            // 所以不要在此处将IsConnected设为0，保持之前SET_LINE_CODING设置的状态
            DBG("CDC: Set Control Line State - DTR:%d RTS:%d Connected:%d\n", 
                UsbCDC.ControlLineState.DTR, UsbCDC.ControlLineState.RTS, UsbCDC.IsConnected);
            // Send ZLP (Zero Length Packet) status response
            OTG_DeviceControlSend(NULL, 0, 3);
            break;
            
        case CDC_SEND_BREAK:
            // Send break signal (not implemented)
            DBG("CDC: Send Break\n");
            OTG_DeviceControlSend(NULL, 0, 3);
            break;
            
        case CDC_SEND_ENCAPSULATED_COMMAND:
            // Data已经在全局Request[]缓冲区中
            DBG("CDC: Send Encapsulated Command (length=%u)\n", Setup[6] | (Setup[7] << 8));
            break;
            
        case CDC_GET_ENCAPSULATED_RESPONSE:
            // Not implemented
            OTG_DeviceControlSend(Request, Setup[6], 3);
            break;
            
        default:
            DBG("CDC: Unknown Request 0x%02X\n", bRequest);
            break;
    }
}

/**
 * @brief  USB Bulk OUT中断回调函数 (当USB硬件接收到数据时自动调用)
 * @note   此函数在USB硬件中断上下文中执行，需要快速返回
 */
void OnDeviceCDC_BulkOutReceived(void)
{
    uint8_t tmpBuf[CDC_DATA_FS_OUT_PACKET_SIZE];
    uint16_t i;
    uint32_t actualLen = 0;
    
    // 从USB硬件FIFO读取接收到的数据
    OTG_DEVICE_ERR_CODE ret = OTG_DeviceBulkReceive(DEVICE_CDC_DATA_OUT_EP, tmpBuf, CDC_DATA_FS_OUT_PACKET_SIZE, &actualLen, 0);
    
    if(ret == DEVICE_NONE_ERR && actualLen > 0)
    {
        // 快速将数据复制到环形缓冲区
        for(i = 0; i < actualLen; i++)
        {
            if(UsbCDC.RxCount < CDC_RX_BUFFER_SIZE)
            {
                UsbCDC.RxBuffer[UsbCDC.RxHead] = tmpBuf[i];
                UsbCDC.RxHead = (UsbCDC.RxHead + 1) % CDC_RX_BUFFER_SIZE;
                UsbCDC.RxCount++;
            }
            else
            {
                // 缓冲区溢出（静默丢弃，避免在中断中打印）
                break;
            }
        }
    }
    // 中断模式下无需打印错误信息，硬件会自动处理
}

/**
 * @brief  CDC data received callback (保留兼容，但不再主动轮询)
 * @note   此函数已废弃，由中断回调OnDeviceCDC_BulkOutReceived取代
 */
void OTG_DeviceCDC_DataReceived(void)
{
    // 此函数仅为保持兼容性，实际接收由中断处理
    // 不再主动轮询USB，避免产生error 001错误
}

/**
 * @brief  CDC data transmitted callback (called when data transmission is complete)
 */
void OTG_DeviceCDC_DataTransmitted(void)
{
    UsbCDC.TxBusy = 0;
    
    // If there's more data in TX buffer, send it
    if(UsbCDC.TxCount > 0)
    {
        OTG_DeviceCDC_FlushTx();
    }
}

/**
 * @brief  Send data through CDC
 * @param  buf: pointer to data buffer
 * @param  len: data length
 * @return number of bytes sent (queued)
 */
uint16_t OTG_DeviceCDC_Send(uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint16_t count = 0;
    
    if(!UsbCDC.InitOk || buf == NULL || len == 0)
    {
        return 0;
    }
    
    // Copy data to TX buffer
    for(i = 0; i < len; i++)
    {
        if(UsbCDC.TxCount < CDC_TX_BUFFER_SIZE)
        {
            UsbCDC.TxBuffer[UsbCDC.TxHead] = buf[i];
            UsbCDC.TxHead = (UsbCDC.TxHead + 1) % CDC_TX_BUFFER_SIZE;
            UsbCDC.TxCount++;
            count++;
        }
        else
        {
            // Buffer full
            break;
        }
    }
    
    // Trigger transmission if not busy
    if(!UsbCDC.TxBusy && UsbCDC.TxCount > 0)
    {
        OTG_DeviceCDC_FlushTx();
    }
    
    return count;
}

/**
 * @brief  Receive data from CDC
 * @param  buf: pointer to data buffer
 * @param  len: maximum data length to receive
 * @return number of bytes received
 */
uint16_t OTG_DeviceCDC_Receive(uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint16_t count = 0;
    
    if(!UsbCDC.InitOk || buf == NULL || len == 0)
    {
        return 0;
    }
    
    // 直接从缓冲区读取数据，不触发USB接收（由Task主动轮询）
    for(i = 0; i < len && UsbCDC.RxCount > 0; i++)
    {
        buf[i] = UsbCDC.RxBuffer[UsbCDC.RxTail];
        UsbCDC.RxTail = (UsbCDC.RxTail + 1) % CDC_RX_BUFFER_SIZE;
        UsbCDC.RxCount--;
        count++;
    }
    
    return count;
}

/**
 * @brief  Get available bytes in CDC RX buffer
 * @return number of available bytes
 */
uint16_t OTG_DeviceCDC_GetRxCount(void)
{
    return UsbCDC.RxCount;
}

/**
 * @brief  Peek at the next byte in CDC RX buffer without consuming it.
 *         Used by SOF dispatchers to check protocol start byte before
 *         committing to consume the data.
 * @param  byte: pointer to receive the peeked byte
 * @return 1 if a byte was peeked, 0 if buffer empty
 */
uint16_t OTG_DeviceCDC_PeekByte(uint8_t *byte)
{
    if(!UsbCDC.InitOk || byte == NULL || UsbCDC.RxCount == 0)
    {
        return 0;
    }
    *byte = UsbCDC.RxBuffer[UsbCDC.RxTail];
    return 1;
}

/**
 * @brief  Get available space in CDC TX buffer
 * @return number of available bytes
 */
uint16_t OTG_DeviceCDC_GetTxFreeSpace(void)
{
    return (CDC_TX_BUFFER_SIZE - UsbCDC.TxCount);
}

/**
 * @brief  Flush CDC TX buffer (send all pending data)
 * @return TRUE if success
 */
bool OTG_DeviceCDC_FlushTx(void)
{
    uint8_t tmpBuf[CDC_DATA_FS_IN_PACKET_SIZE];
    uint16_t len = 0;
    uint16_t i;
    
    if(!UsbCDC.InitOk || UsbCDC.TxBusy || UsbCDC.TxCount == 0)
    {
        return FALSE;
    }
    
    // Determine how many bytes to send
    len = UsbCDC.TxCount;
    if(len > CDC_DATA_FS_IN_PACKET_SIZE)
    {
        len = CDC_DATA_FS_IN_PACKET_SIZE;
    }
    
    // Copy data from TX buffer
    for(i = 0; i < len; i++)
    {
        tmpBuf[i] = UsbCDC.TxBuffer[UsbCDC.TxTail];
        UsbCDC.TxTail = (UsbCDC.TxTail + 1) % CDC_TX_BUFFER_SIZE;
    }
    UsbCDC.TxCount -= len;
    
    // Mark as busy
    UsbCDC.TxBusy = 1;
    
    // Send data through USB endpoint (使用Bulk IN端点)
    // 超时1000ms: 给主机充足时间读取数据，避免因主机轮询延迟而丢包
    OTG_DeviceBulkSend(DEVICE_CDC_DATA_IN_EP, tmpBuf, len, 1000);
    
    // 发送完成后清除busy标志（同步模式）
    UsbCDC.TxBusy = 0;
    
    // 如果还有数据，继续发送
    if(UsbCDC.TxCount > 0)
    {
        OTG_DeviceCDC_FlushTx();
    }
    
    return TRUE;
}

/**
 * @brief  Flush CDC RX buffer (clear all data)
 * @return TRUE if success
 */
bool OTG_DeviceCDC_FlushRx(void)
{
    if(!UsbCDC.InitOk)
    {
        return FALSE;
    }
    
    UsbCDC.RxHead = 0;
    UsbCDC.RxTail = 0;
    UsbCDC.RxCount = 0;
    
    return TRUE;
}

/**
 * @brief  CDC task - 处理接收到的数据（数据接收由中断自动完成）
 * @note   使用中断接收后，此Task只需处理缓冲区中的数据，无需轮询USB
 *         数据处理由Shell命令行系统接管
 */
void OTG_DeviceCDC_Task(void)
{
    // 此函数保留用于底层CDC维护
    // 数据接收由中断完成，数据处理由Shell_Task()完成
    // 应用层请调用Shell_Task()处理命令行
}

/**
 * @brief  Send a single character through CDC
 * @param  ch: character to send
 * @return number of bytes sent
 */
uint16_t OTG_DeviceCDC_SendChar(uint8_t ch)
{
    return OTG_DeviceCDC_Send(&ch, 1);
}

/**
 * @brief  Get a single character from CDC RX buffer
 * @return character received, or 0 if no data
 */
uint8_t OTG_DeviceCDC_GetChar(void)
{
    uint8_t ch = 0;
    OTG_DeviceCDC_Receive(&ch, 1);
    return ch;
}

/**
 * @brief  Flush CDC RX buffer (wrapper for compatibility)
 * @return TRUE if success
 */
bool OTG_DeviceCDC_FlushRxBuffer(void)
{
    return OTG_DeviceCDC_FlushRx();
}

/**
 * @brief  Flush CDC TX buffer (wrapper for compatibility)
 * @return TRUE if success
 */
bool OTG_DeviceCDC_FlushTxBuffer(void)
{
    return OTG_DeviceCDC_FlushTx();
}

