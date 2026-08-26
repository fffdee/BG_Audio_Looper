/**
 * @file bg_extmem.h
 * @brief 外部大容量存储器抽象（PSRAM / NAND 源）
 *
 * 02_core 的音符缓存只通过本接口读写，不直接包含 flash_devices.h。
 */
#ifndef BG_EXTMEM_H
#define BG_EXTMEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 工作 RAM（通常是 PSRAM）是否就绪 */
int bg_extmem_ready(void);

/** 从工作 RAM 读 */
int bg_extmem_read(uint32_t addr, void *buf, uint32_t len);

/** 写到工作 RAM */
int bg_extmem_write(uint32_t addr, const void *buf, uint32_t len);

/**
 * 从音源源介质读（Hub: 与工作 RAM 相同；有 NAND 的板: NAND）
 */
int bg_extmem_src_read(uint32_t addr, void *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* BG_EXTMEM_H */
