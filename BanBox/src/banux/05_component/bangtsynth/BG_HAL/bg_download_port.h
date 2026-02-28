/**
 * BG_Download_Port - 下载数据端口抽象接口
 * 
 * 功能:
 * - 提供平台无关的数据读取接口
 * - Linux: 从文件读取音源数据
 * - MCU: 从串口/网络接收音源数据
 * 
 * 移植说明:
 * - Linux平台: 实现 bg_download_port_linux.c
 * - STM32平台: 实现 bg_download_port_stm32.c (串口接收)
 * - ESP32平台: 实现 bg_download_port_esp32.c (WiFi/蓝牙)
 */

#ifndef _BG_DOWNLOAD_PORT_H__
#define _BG_DOWNLOAD_PORT_H__

#include <stdint.h>
#include <stddef.h>

/**
 * 读取下载数据
 * 
 * 平台实现:
 * - Linux: source = 文件路径, 从文件读取 size 字节
 * - STM32: source = 串口号字符串, 从串口接收 size 字节
 * - ESP32: source = URL/蓝牙地址, 从网络/蓝牙接收数据
 * 
 * @param source 数据源标识 (文件路径/串口号/URL等)
 * @param buffer 数据缓冲区
 * @param size 期望读取的字节数
 * @param bytes_read 实际读取的字节数 (输出)
 * @return 0=成功, <0=错误
 */
int bg_download_port_read(const char *source, void *buffer, size_t size, size_t *bytes_read);

#endif /* _BG_DOWNLOAD_PORT_H__ */
