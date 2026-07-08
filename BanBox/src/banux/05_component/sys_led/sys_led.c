/**
 * @file  sys_led.c
 * @brief System status LED driver — event-driven via BG_Event topics.
 */

#include "sys_led.h"
#include "gpio.h"
#include "product_def.h"
#include "banux/banux_config.h"
#include "sys_state.h"
#include "bg_event.h"

typedef struct {
    SysLedMode_t     mode;
    SysLedMode_t     manual_mode;
    SysLedPolicy_t   policy;
    uint16_t         blink_half_ms;
    uint16_t         breathe_period_ms;
    uint16_t         blink_ms;
    uint16_t         breathe_ms;
    uint8_t          blink_level;
    uint8_t          breathe_brightness;
    uint8_t          pwm_phase;
    uint8_t          inited;
} SysLedContext_t;

static SysLedContext_t s_led;

#if SYS_LED_EN

static void led_hw_write(uint8_t on)
{
#if SYS_LED_ACTIVE_HIGH
    if (on) {
        GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
    } else {
        GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
    }
#else
    if (on) {
        GPIO_RegOneBitClear(GPIO_A_OUT, HW_LED_GPIO_PIN);
    } else {
        GPIO_RegOneBitSet(GPIO_A_OUT, HW_LED_GPIO_PIN);
    }
#endif
}

static void led_apply_pwm(uint8_t brightness)
{
    s_led.pwm_phase++;
    led_hw_write(s_led.pwm_phase < brightness);
}

static uint8_t led_triangle_brightness(uint16_t elapsed_ms, uint16_t period_ms)
{
    uint32_t half;
    uint32_t pos;

    if (period_ms == 0U) {
        return 0U;
    }

    pos = elapsed_ms % period_ms;
    half = period_ms / 2U;
    if (half == 0U) {
        return 255U;
    }

    if (pos < half) {
        return (uint8_t)((pos * 255U) / half);
    }
    pos = period_ms - pos;
    return (uint8_t)((pos * 255U) / half);
}

static void led_render(void)
{
    switch (s_led.mode) {
    case SYS_LED_MODE_ON:
        led_hw_write(1U);
        break;
    case SYS_LED_MODE_OFF:
        led_hw_write(0U);
        break;
    case SYS_LED_MODE_BLINK:
        led_hw_write(s_led.blink_level);
        break;
    case SYS_LED_MODE_BREATHE:
        led_apply_pwm(s_led.breathe_brightness);
        break;
    default:
        led_hw_write(0U);
        break;
    }
}

static void led_set_mode_internal(SysLedMode_t mode)
{
    s_led.mode = mode;
    s_led.blink_ms = 0U;
    s_led.breathe_ms = 0U;
    s_led.blink_level = 0U;
    s_led.pwm_phase = 0U;

    if (mode == SYS_LED_MODE_ON) {
        s_led.breathe_brightness = 255U;
    } else if (mode == SYS_LED_MODE_OFF) {
        s_led.breathe_brightness = 0U;
    }

    led_render();
}

/**
 * @brief Map current SysState run/sub states to LED pattern.
 *
 * Priority (high → low):
 *   OFF/SHUTDOWN → off
 *   BOOT         → fast blink
 *   TRANSFER     → fast blink
 *   BATT_LOW     → slow blink
 *   USB full     → off
 *   CHARGING     → breathe
 *   IDLE         → slow breathe
 *   RUNNING      → solid on
 */
static void led_apply_system_policy(void)
{
    SysRunState_t run_state;
    uint16_t sub_state;

    if (s_led.policy != SYS_LED_POLICY_SYSTEM) {
        return;
    }

    run_state = SysState_GetRunState();
    sub_state = SysState_GetSubState();

    if (run_state == SYS_RUN_OFF || run_state == SYS_RUN_SHUTDOWN) {
        led_set_mode_internal(SYS_LED_MODE_OFF);
        return;
    }

    if (run_state == SYS_RUN_BOOT) {
        led_set_mode_internal(SYS_LED_MODE_BLINK);
        SysLed_SetBlinkPeriod(150U);
        return;
    }

    if (sub_state & SYS_SUB_TRANSFER) {
        led_set_mode_internal(SYS_LED_MODE_BLINK);
        SysLed_SetBlinkPeriod(120U);
        return;
    }

    if (sub_state & SYS_SUB_BATT_LOW) {
        led_set_mode_internal(SYS_LED_MODE_BLINK);
        SysLed_SetBlinkPeriod(200U);
        return;
    }

    if ((sub_state & SYS_SUB_USB_CONNECTED) && !(sub_state & SYS_SUB_BATT_CHARGING)) {
        led_set_mode_internal(SYS_LED_MODE_OFF);
        return;
    }

    if (sub_state & SYS_SUB_BATT_CHARGING) {
        led_set_mode_internal(SYS_LED_MODE_BREATHE);
        SysLed_SetBreathePeriod(SYS_LED_BREATHE_PERIOD_MS);
        return;
    }

    if (run_state == SYS_RUN_IDLE) {
        led_set_mode_internal(SYS_LED_MODE_BREATHE);
        SysLed_SetBreathePeriod(3000U);
        return;
    }

    if (run_state == SYS_RUN_RUNNING) {
        led_set_mode_internal(SYS_LED_MODE_ON);
        return;
    }

    led_set_mode_internal(SYS_LED_MODE_ON);
}

static void led_on_system_event(void)
{
    if (s_led.policy == SYS_LED_POLICY_SYSTEM) {
        led_apply_system_policy();
    }
}

void SysLed_Init(void)
{
    s_led.mode = SYS_LED_MODE_OFF;
    s_led.manual_mode = SYS_LED_MODE_ON;
    s_led.policy = SYS_LED_POLICY_SYSTEM;
    s_led.blink_half_ms = SYS_LED_BLINK_PERIOD_MS / 2U;
    s_led.breathe_period_ms = SYS_LED_BREATHE_PERIOD_MS;
    s_led.blink_ms = 0U;
    s_led.breathe_ms = 0U;
    s_led.blink_level = 0U;
    s_led.breathe_brightness = 0U;
    s_led.pwm_phase = 0U;
    s_led.inited = 1U;

    GPIO_RegOneBitClear(GPIO_A_IE, HW_LED_GPIO_PIN);
    GPIO_RegOneBitSet(GPIO_A_OE, HW_LED_GPIO_PIN);
    led_hw_write(0U);

    led_apply_system_policy();
}

void SysLed_SetPolicy(SysLedPolicy_t policy)
{
    s_led.policy = policy;
    if (policy == SYS_LED_POLICY_MANUAL) {
        led_set_mode_internal(s_led.manual_mode);
    } else {
        led_apply_system_policy();
    }
}

SysLedPolicy_t SysLed_GetPolicy(void)
{
    return s_led.policy;
}

void SysLed_SetMode(SysLedMode_t mode)
{
    s_led.manual_mode = mode;
    s_led.policy = SYS_LED_POLICY_MANUAL;
    led_set_mode_internal(mode);
}

SysLedMode_t SysLed_GetMode(void)
{
    return s_led.mode;
}

void SysLed_SetBlinkPeriod(uint16_t half_period_ms)
{
    if (half_period_ms == 0U) {
        half_period_ms = 1U;
    }
    s_led.blink_half_ms = half_period_ms;
}

void SysLed_SetBreathePeriod(uint16_t period_ms)
{
    if (period_ms == 0U) {
        period_ms = 1U;
    }
    s_led.breathe_period_ms = period_ms;
}

void SysLed_Tick1ms(void)
{
    if (!s_led.inited) {
        return;
    }

    if (s_led.mode == SYS_LED_MODE_BLINK) {
        s_led.blink_ms++;
        if (s_led.blink_ms >= s_led.blink_half_ms) {
            s_led.blink_ms = 0U;
            s_led.blink_level = s_led.blink_level ? 0U : 1U;
        }
        led_hw_write(s_led.blink_level);
        return;
    }

    if (s_led.mode == SYS_LED_MODE_BREATHE) {
        s_led.breathe_ms++;
        if (s_led.breathe_ms >= s_led.breathe_period_ms) {
            s_led.breathe_ms = 0U;
        }
        s_led.breathe_brightness = led_triangle_brightness(s_led.breathe_ms,
                                                           s_led.breathe_period_ms);
        led_apply_pwm(s_led.breathe_brightness);
        return;
    }

    led_render();
}

void SysLed_Tick50ms(void)
{
    /* Reserved: system policy is event-driven; no polling needed. */
}

/* ---- BG_Event subscriptions (system topics) ---- */

static void on_run_state_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic;
    (void)data;
    (void)size;
    led_on_system_event();
}
BG_EVT_SUB(EVT_SYS_RUN_STATE, on_run_state_led);

static void on_sub_state_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    const BG_EventSysSubState_t *evt = (const BG_EventSysSubState_t *)data;
    (void)topic;
    (void)size;

    if (s_led.policy != SYS_LED_POLICY_SYSTEM) {
        return;
    }

    if (evt->changed_bits & (SYS_SUB_USB_CONNECTED |
                             SYS_SUB_BATT_CHARGING |
                             SYS_SUB_BATT_LOW |
                             SYS_SUB_TRANSFER |
                             SYS_SUB_BT_CONNECTED |
                             SYS_SUB_BLE_CONNECTED)) {
        led_apply_system_policy();
    }
}
BG_EVT_SUB(EVT_SYS_SUB_STATE, on_sub_state_led);

static void on_power_on_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic;
    (void)data;
    (void)size;
    led_on_system_event();
}
BG_EVT_SUB(EVT_SYS_POWER_ON, on_power_on_led);

static void on_power_off_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic;
    (void)data;
    (void)size;
    led_set_mode_internal(SYS_LED_MODE_OFF);
}
BG_EVT_SUB(EVT_SYS_POWER_OFF, on_power_off_led);

static void on_idle_enter_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic;
    (void)data;
    (void)size;
    led_on_system_event();
}
BG_EVT_SUB(EVT_SYS_IDLE_ENTER, on_idle_enter_led);

static void on_idle_exit_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic;
    (void)data;
    (void)size;
    led_on_system_event();
}
BG_EVT_SUB(EVT_SYS_IDLE_EXIT, on_idle_exit_led);

static void on_transfer_enter_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic;
    (void)data;
    (void)size;
    led_on_system_event();
}
BG_EVT_SUB(EVT_SYS_TRANSFER_ENTER, on_transfer_enter_led);

static void on_transfer_exit_led(BG_EventTopic_t topic, const void *data, uint8_t size)
{
    (void)topic;
    (void)data;
    (void)size;
    led_on_system_event();
}
BG_EVT_SUB(EVT_SYS_TRANSFER_EXIT, on_transfer_exit_led);

#else /* !SYS_LED_EN */

void SysLed_Init(void) {}
void SysLed_SetPolicy(SysLedPolicy_t policy) { (void)policy; }
SysLedPolicy_t SysLed_GetPolicy(void) { return SYS_LED_POLICY_MANUAL; }
void SysLed_SetMode(SysLedMode_t mode) { (void)mode; }
SysLedMode_t SysLed_GetMode(void) { return SYS_LED_MODE_OFF; }
void SysLed_SetBlinkPeriod(uint16_t half_period_ms) { (void)half_period_ms; }
void SysLed_SetBreathePeriod(uint16_t period_ms) { (void)period_ms; }
void SysLed_Tick1ms(void) {}
void SysLed_Tick50ms(void) {}

#endif /* SYS_LED_EN */
