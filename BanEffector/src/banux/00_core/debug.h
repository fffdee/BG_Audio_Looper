/** @file debug.h @brief Injectable Banux core logging API. */
#ifndef __BANUX_CORE_DEBUG_H__
#define __BANUX_CORE_DEBUG_H__

/* ---------------------------------------------------------------------------
 * BanEffector integration: this header shares its file name with the Mountain
 * View Silicon SDK header  middleware/mv_utils/inc/debug.h.  The SDK header
 * supplies DBG()/APP_DBG()/BT_DBG() and pulls in type.h/gpio.h that the
 * retained one-generation audio code depends on; this header adds the Banux
 * injectable logging API (BanuxDebug_*).
 *
 * To let both coexist, 00_core MUST be listed BEFORE middleware/mv_utils/inc
 * in the project include order (see .cproject).  We then chain to the SDK
 * header with #include_next so every translation unit sees both APIs.  The SDK
 * header defines DBG() unconditionally, so the #ifndef DBG fallback below only
 * takes effect when the SDK header is absent (e.g. building the bare banux2
 * framework tree without the MVS SDK).
 * ------------------------------------------------------------------------- */
#include_next <debug.h>

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
