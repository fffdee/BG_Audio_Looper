/**
 * @file    app_sys_handler.h
 * @brief   应用层系统事件处理 — 50ms tick 接口
 */

#ifndef __APP_SYS_HANDLER_H__
#define __APP_SYS_HANDLER_H__

/**
 * @brief LED 闪烁时序 tick (50ms)
 * @note  需从 hardware_check() 中调用
 */
void AppSys_LedTick(void);

/**
 * @brief BLE 电量上报 tick (50ms)
 * @note  需从 hardware_check() 中调用
 */
void AppSys_BatteryTick(void);

#endif /* __APP_SYS_HANDLER_H__ */
