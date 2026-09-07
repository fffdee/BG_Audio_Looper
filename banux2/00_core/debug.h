/** @file debug.h @brief Injectable Banux core logging API. */
#ifndef __BANUX_CORE_DEBUG_H__
#define __BANUX_CORE_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*BanuxDebugWriter_t)(const char *text);

/** Install a platform log writer. NULL disables framework log output. */
void BanuxDebug_SetWriter(BanuxDebugWriter_t writer);

/** Format and emit one framework log message without depending on Shell/UART. */
void BanuxDebug_Printf(const char *format, ...);

#ifndef DBG
#define DBG(format, ...) BanuxDebug_Printf(format, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BANUX_CORE_DEBUG_H__ */
