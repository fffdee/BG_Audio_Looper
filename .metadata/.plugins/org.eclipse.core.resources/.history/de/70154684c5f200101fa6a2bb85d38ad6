#ifndef __FLASH_BOOT_H__
#define __FLASH_BOOT_H__

#include "app_config.h"
#include "flash_config.h"

/*
 * Flash Boot 头文件 - 用于BG_card_mini项目
 * 版本：V2.2.0
 * 移植自BT_Audio_APP
 */

//========================================
// 使能开关 - 1:启用USB升级功能, 0:禁用
//========================================
#define FLASH_BOOT_EN      1

//========================================
// UART TX引脚配置 (调试用)
//========================================
#define BOOT_UART_TX_OFF    0   //关闭串口
#define BOOT_UART_TX_A0     1
#define BOOT_UART_TX_A1     2
#define BOOT_UART_TX_A6     3
#define BOOT_UART_TX_A10    4
#define BOOT_UART_TX_A19    5
#define BOOT_UART_TX_A25    6
#define BOOT_UART_TX_PIN    BOOT_UART_TX_OFF

//========================================
// UART波特率配置
//========================================
#define BOOT_UART_BAUD_RATE_9600    0
#define BOOT_UART_BAUD_RATE_11520   1
#define BOOT_UART_BAUD_RATE_256000  2
#define BOOT_UART_BAUD_RATE_512000  3
#define BOOT_UART_BAUD_RATE_1000000 4
#define BOOT_UART_BAUD_RATE_1500000 5
#define BOOT_UART_BAUD_RATE_2000000 6
#define BOOT_UART_BAUD_RATE         BOOT_UART_BAUD_RATE_512000

#define BOOT_UART_CONFIG    ((BOOT_UART_BAUD_RATE<<4)+BOOT_UART_TX_PIN)

//========================================
// 判断标准配置
// 高4bit: 0xF=按code版本升级, 0x5=按CRC判断
// 低4bit: 保留
//========================================
#define JUDGEMENT_STANDARD      0x55

//========================================
// 升级接口定义
//========================================
// SD卡接口
#define SD_OFF              0x00
#define SD_A15A16A17        0x1
#define SD_A20A21A22        0x2

// 根据实际硬件配置选择SD卡接口
#ifndef CFG_RES_CARD_GPIO
#define CFG_RES_CARD_GPIO   0  // 默认不使用SD卡升级
#endif

#if CFG_RES_CARD_GPIO == 1
#define SD_PORT             SD_A15A16A17
#else
#define SD_PORT             SD_A20A21A22
#endif

// U盘升级
#define UDisk_OFF           0x00
#define UDisk_ON            0x4

// PC Tool升级 (USB HID方式)
#define PCTOOL_OFF          0x00
#define PCTOOL_ON           0x08

// 蓝牙升级
#define BTTOOL_OFF          0X00
#define BTTOOL_ON           0X10

// 组合升级端口配置
// 默认启用: PC Tool + U盘
#define UP_PORT             (BTTOOL_OFF + PCTOOL_ON + UDisk_ON + SD_OFF)

//========================================
// Flash Boot数据声明
//========================================
#if FLASH_BOOT_EN
extern const unsigned char flash_data[];
#endif

//========================================
// 升级返回状态码
//========================================
#define USER_CODE_RUN_START     0   // 正常运行，直接运行客户代码
#define UPDAT_OK                1   // 升级检测，升级成功
#define NEEDLESS_UPDAT          2   // 升级检测后，无需升级

//========================================
// 资源类型定义 (用于start_up_grate)
//========================================
#ifndef AppResourceCard
#define AppResourceCard         1   // SD卡升级
#endif
#ifndef AppResourceUDisk
#define AppResourceUDisk        2   // U盘升级
#endif
#ifndef AppResourceUsbDevice
#define AppResourceUsbDevice    3   // PC USB升级
#endif

//========================================
// 函数声明
//========================================
#if FLASH_BOOT_EN
/**
 * @brief 报告升级结果
 */
void report_up_grate(void);

/**
 * @brief 获取升级错误码
 * @return 错误码
 */
uint8_t Report_Error_Code(void);

/**
 * @brief 清除错误码
 */
void Clear_Error_Code(void);

/**
 * @brief 获取复位标志
 * @return 复位标志
 */
uint8_t Reset_FlagGet_Flash_Boot(void);

/**
 * @brief 启动升级
 * @param UpdateResource 升级资源类型 (AppResourceCard/AppResourceUDisk/AppResourceUsbDevice)
 */
void start_up_grate(uint32_t UpdateResource);
#endif

#endif /* __FLASH_BOOT_H__ */
