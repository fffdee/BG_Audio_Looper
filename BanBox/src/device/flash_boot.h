#ifndef __FLASH_BOOT_H__
#define __FLASH_BOOT_H__

#include "flash_config.h"
#include "type.h"
/*
SDK Flash Boot V2.2.3
*/
#define FLASH_BOOT_EN      0

//TX PIN
#define BOOT_UART_TX_OFF	0
#define BOOT_UART_TX_A0		1
#define BOOT_UART_TX_A1		2
#define BOOT_UART_TX_A6		3
#define BOOT_UART_TX_A10	4
#define BOOT_UART_TX_A19	5
#define BOOT_UART_TX_A25	6
#define BOOT_UART_TX_PIN	BOOT_UART_TX_OFF

#define BOOT_UART_BAUD_RATE_9600	0
#define BOOT_UART_BAUD_RATE_11520	1
#define BOOT_UART_BAUD_RATE_256000	2
#define BOOT_UART_BAUD_RATE_512000	3
#define BOOT_UART_BAUD_RATE_1000000	4
#define BOOT_UART_BAUD_RATE_1500000	5
#define BOOT_UART_BAUD_RATE_2000000	6
#define BOOT_UART_BAUD_RATE		BOOT_UART_BAUD_RATE_512000

#define BOOT_UART_CONFIG	((BOOT_UART_BAUD_RATE<<4)+BOOT_UART_TX_PIN)

#define JUDGEMENT_STANDARD		0x55

#define	SD_OFF				0x00
#define SD_A15A16A17		0x1
#define SD_A20A21A22		0x2
#if CFG_RES_CARD_GPIO == SDIO_A15_A16_A17
#define SD_PORT				SD_A15A16A17
#else
#define SD_PORT				SD_A20A21A22
#endif

#define UDisk_OFF			0x00
#define UDisk_ON			0x4

#define PCTOOL_OFF			0x00
#define PCTOOL_ON			0x08

#define	BTTOOL_OFF			0X00
#define BTTOOL_ON			0X10

#define UP_PORT				(BTTOOL_OFF + PCTOOL_ON + UDisk_OFF + SD_OFF)

#if FLASH_BOOT_EN
extern const unsigned char flash_data[];
#endif

#define USER_CODE_RUN_START		0
#define UPDAT_OK				1
#define NEEDLESS_UPDAT			2

#if FLASH_BOOT_EN
extern void report_up_grate(void);
extern void start_up_grate(uint32_t UpdateResource);
extern uint8_t Report_Error_Code(void);
extern void Clear_Error_Code(void);

#define AppResourceCard       0x01
#define AppResourceUDisk      0x04
#define AppResourceUsbDevice  0x40
#endif

#endif
