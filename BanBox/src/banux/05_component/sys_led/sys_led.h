/**
 * @file  sys_led.h
 * @brief System status LED driver (GPIOA15 by default).
 *
 * Supports off, solid on, blink, and software-PWM breathe effects.
 * Optional system policy maps run/sub states to LED patterns automatically.
 */
#ifndef __SYS_LED_H__
#define __SYS_LED_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pin: HW_LED_GPIO_PIN in product_def.h (default GPIO_A15) */
#ifndef SYS_LED_ACTIVE_HIGH
#define SYS_LED_ACTIVE_HIGH     1   /* 1: GPIO high = LED on */
#endif

#ifndef SYS_LED_BLINK_PERIOD_MS
#define SYS_LED_BLINK_PERIOD_MS     500U
#endif

#ifndef SYS_LED_BREATHE_PERIOD_MS
#define SYS_LED_BREATHE_PERIOD_MS   2000U
#endif

typedef enum {
    SYS_LED_MODE_OFF = 0,
    SYS_LED_MODE_ON,
    SYS_LED_MODE_BLINK,
    SYS_LED_MODE_BREATHE,
} SysLedMode_t;

typedef enum {
    SYS_LED_POLICY_MANUAL = 0,  /* Use SysLed_SetMode() directly */
    SYS_LED_POLICY_SYSTEM,      /* Follow system run/sub states */
} SysLedPolicy_t;

void SysLed_Init(void);
void SysLed_SetPolicy(SysLedPolicy_t policy);
SysLedPolicy_t SysLed_GetPolicy(void);

void SysLed_SetMode(SysLedMode_t mode);
SysLedMode_t SysLed_GetMode(void);

void SysLed_SetBlinkPeriod(uint16_t half_period_ms);
void SysLed_SetBreathePeriod(uint16_t period_ms);

/** Drive blink/breathe timing and soft-PWM. Call every 1 ms (e.g. Timer2 ISR). */
void SysLed_Tick1ms(void);

/** Refresh system-policy mapping. Call periodically (~50 ms). */
void SysLed_Tick50ms(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_LED_H__ */
