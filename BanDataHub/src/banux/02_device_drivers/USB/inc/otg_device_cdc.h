/**
 *****************************************************************************
 * @file     otg_device_cdc.h
 * @author   BG Card Team
 * @version  V1.0.0
 * @date     15-December-2025
 * @brief    CDC serial port device interface
 *****************************************************************************
 * @attention
 *
 * CDC (Communication Device Class) implementation for USB virtual COM port
 * 
 *****************************************************************************
 */

#ifndef __OTG_DEVICE_CDC_H__
#define	__OTG_DEVICE_CDC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

// CDC Class-Specific Request Codes
#define CDC_SEND_ENCAPSULATED_COMMAND   0x00
#define CDC_GET_ENCAPSULATED_RESPONSE   0x01
#define CDC_SET_COMM_FEATURE            0x02
#define CDC_GET_COMM_FEATURE            0x03
#define CDC_CLEAR_COMM_FEATURE          0x04
#define CDC_SET_LINE_CODING             0x20
#define CDC_GET_LINE_CODING             0x21
#define CDC_SET_CONTROL_LINE_STATE      0x22
#define CDC_SEND_BREAK                  0x23

// CDC Functional Descriptors
#define CDC_CS_INTERFACE                0x24
#define CDC_CS_ENDPOINT                 0x25

// CDC Descriptor SubTypes
#define CDC_HEADER                      0x00
#define CDC_CALL_MANAGEMENT             0x01
#define CDC_ABSTRACT_CONTROL_MANAGEMENT 0x02
#define CDC_UNION                       0x06

// CDC Communication Interface Class Code
#define CDC_COMMUNICATION_INTERFACE_CLASS    0x02

// CDC Data Interface Class Code
#define CDC_DATA_INTERFACE_CLASS            0x0A

// CDC Buffer Sizes
#define CDC_CMD_PACKET_SIZE                 8
#define CDC_DATA_FS_IN_PACKET_SIZE          64
#define CDC_DATA_FS_OUT_PACKET_SIZE         64
#define CDC_RX_BUFFER_SIZE                  512
#define CDC_TX_BUFFER_SIZE                  512

// CDC Line Coding Structure (紧凑打包，避免对齐问题)
typedef struct __attribute__((packed))
{
    uint32_t dwDTERate;      // Data terminal rate (bits per second)
    uint8_t  bCharFormat;    // Stop bits: 0-1 Stop bit, 1-1.5 Stop bits, 2-2 Stop bits
    uint8_t  bParityType;    // Parity: 0-None, 1-Odd, 2-Even, 3-Mark, 4-Space
    uint8_t  bDataBits;      // Data bits (5, 6, 7, 8 or 16)
} CDC_LineCoding_t;

// CDC Control Line State
typedef struct
{
    uint8_t DTR;             // Data Terminal Ready
    uint8_t RTS;             // Request To Send
} CDC_ControlLineState_t;

// CDC Device Structure
typedef struct
{
    uint8_t                 InitOk;
    uint8_t                 IsConnected;  // 添加连接状态标志
    CDC_LineCoding_t        LineCoding;
    CDC_ControlLineState_t  ControlLineState;
    
    // RX Buffer (从主机接收)
    uint8_t                 RxBuffer[CDC_RX_BUFFER_SIZE];
    uint16_t                RxHead;
    uint16_t                RxTail;
    uint16_t                RxCount;
    
    // TX Buffer (发送到主机)
    uint8_t                 TxBuffer[CDC_TX_BUFFER_SIZE];
    uint16_t                TxHead;
    uint16_t                TxTail;
    uint16_t                TxCount;
    
    uint8_t                 TxBusy;
    
} UsbCDC_t;

// CDC Function Prototypes

/**
 * @brief  Initialize CDC device
 * @return TRUE if success, FALSE if failed
 */
bool OTG_DeviceCDC_Init(void);

/**
 * @brief  DeInitialize CDC device
 * @return TRUE if success, FALSE if failed
 */
bool OTG_DeviceCDC_DeInit(void);

/**
 * @brief  CDC class specific request handler
 * @return None
 */
void OTG_DeviceCDC_Request(void);

/**
 * @brief  CDC data received callback
 * @return None
 */
void OTG_DeviceCDC_DataReceived(void);

/**
 * @brief  CDC data transmitted callback
 * @return None
 */
void OTG_DeviceCDC_DataTransmitted(void);

/**
 * @brief  Send data through CDC
 * @param  buf: pointer to data buffer
 * @param  len: data length
 * @return number of bytes sent
 */
uint16_t OTG_DeviceCDC_Send(uint8_t *buf, uint16_t len);

/**
 * @brief  Receive data from CDC
 * @param  buf: pointer to data buffer
 * @param  len: maximum data length to receive
 * @return number of bytes received
 */
uint16_t OTG_DeviceCDC_Receive(uint8_t *buf, uint16_t len);

/**
 * @brief  Get available bytes in CDC RX buffer
 * @return number of available bytes
 */
uint16_t OTG_DeviceCDC_GetRxCount(void);

/**
 * @brief  Peek at next RX byte without consuming
 * @return 1 if peeked, 0 if empty
 */
uint16_t OTG_DeviceCDC_PeekByte(uint8_t *byte);

/**
 * @brief  Get available space in CDC TX buffer
 * @return number of available bytes
 */
uint16_t OTG_DeviceCDC_GetTxFreeSpace(void);

/**
 * @brief  Flush CDC TX buffer
 * @return TRUE if success
 */
bool OTG_DeviceCDC_FlushTx(void);

/**
 * @brief  Flush CDC RX buffer
 * @return TRUE if success
 */
bool OTG_DeviceCDC_FlushRx(void);

/**
 * @brief  Send a single character
 * @param  ch: character to send
 * @return number of bytes sent
 */
uint16_t OTG_DeviceCDC_SendChar(uint8_t ch);

/**
 * @brief  Get a single character
 * @return character received, or 0 if no data
 */
uint8_t OTG_DeviceCDC_GetChar(void);

/**
 * @brief  Flush RX buffer (wrapper)
 * @return TRUE if success
 */
bool OTG_DeviceCDC_FlushRxBuffer(void);

/**
 * @brief  Flush TX buffer (wrapper)
 * @return TRUE if success
 */
bool OTG_DeviceCDC_FlushTxBuffer(void);

/**
 * @brief  CDC task processing (call in main loop)
 */
void OTG_DeviceCDC_Task(void);


#ifdef  __cplusplus
}
#endif

#endif // __OTG_DEVICE_CDC_H__
