// ============================================================================
// product_features.h - 产品硬件功能定义（只读，不可更改）
// ============================================================================
// 本文件定义了产品支持的硬件功能列表，编译后存储在 flash 的 .rodata 段，
// 无法通过任何方式在运行时修改。APP 连接后通过 BLE 协议读取此字符串，
// 解析后显示对应的功能界面。
//
// 功能名称约定（小写下划线）：
//   eq            - EQ 均衡器
//   reverb        - 混响
//   delay         - 延迟
//   drc           - 动态压缩
//   gain          - 增益调节
//   looper        - 循环录音
//   metronome     - 节拍器
//   drum          - 鼓机
//   wav_export    - WAV 导出
//   battery_calib - 电池校准
//   usb_audio     - USB 声卡
//   ble           - BLE 蓝牙
//   fw_upgrade    - 固件升级

#ifndef __PRODUCT_FEATURES_H__
#define __PRODUCT_FEATURES_H__

#include "build_config.h"

/* 宏字符串化工具 */
#define _STRINGIFY(x)       #x
#define STRINGIFY_VALUE(x)  _STRINGIFY(x)

/* 功能列表协议版本（格式变更时递增） */
#define FEATURE_LIST_VER  1

/* 产品 ID（与 ble_protocol.h 中 BG_PRODUCT_ID_BANBOX 一致） */
#define PRODUCT_ID        0x0001

/* 硬件版本 */
#define HW_VERSION        "v1.0"

/* 固件版本字符串 */
#define FW_VERSION_STR    "1.0.1"

/* ---------------------------------------------------------------------------
 * 产品功能定义 const 字符串（JSON 格式）
 *
 * 存储在 flash .rodata 段，运行时不可修改。
 * APP 通过 BLE_CMD_SYSTEM + BLE_SYSTEM_SUB_FEATURE_LIST 子命令读取。
 *
 * JSON 字段:
 *   ver     - 功能列表格式版本
 *   pid     - 产品 ID
 *   fw      - 固件版本
 *   hw      - 硬件版本
 *   features - 支持的功能名称数组
 * ------------------------------------------------------------------------- */
static const char PRODUCT_FEATURE_LIST[] =
    "{\"ver\":" STRINGIFY_VALUE(FEATURE_LIST_VER)
    ",\"pid\":" STRINGIFY_VALUE(PRODUCT_ID)
    ",\"fw\":\"" FW_VERSION_STR "\""
    ",\"hw\":\"" HW_VERSION "\""
    ",\"features\":["
        "\"eq\",\"reverb\",\"delay\",\"drc\",\"gain\","
        "\"looper\",\"metronome\",\"drum\",\"wav_export\","
        "\"battery_calib\",\"usb_audio\",\"ble\",\"fw_upgrade\""
    "]}";

/* 获取功能列表字符串指针和长度 */
#define ProductFeature_GetList()   ((const char *)PRODUCT_FEATURE_LIST)
#define ProductFeature_GetLength() (sizeof(PRODUCT_FEATURE_LIST) - 1)

#endif /* __PRODUCT_FEATURES_H__ */
