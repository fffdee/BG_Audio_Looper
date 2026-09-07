/** @file driver_init.h @brief BanEffector 平台驱动注册接口 (BanuxDriver_RegisterAll). */
#ifndef BANUX_DRIVER_INIT_H
#define BANUX_DRIVER_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Register all enabled board drivers after DrvFramework_Init(). */
int BanuxDriver_RegisterAll(void);

#ifdef __cplusplus
}
#endif

#endif /* BANUX_DRIVER_INIT_H */
