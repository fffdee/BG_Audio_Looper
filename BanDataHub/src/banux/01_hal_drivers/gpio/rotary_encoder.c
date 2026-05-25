#include "rotary_encoder.h"

#if HW_DRV_ENCODER_EN

#include "gpio.h"
#include "debug.h"

#define ENCODER_LONG_PRESS_MS   1000
#define ENCODER_SCAN_PERIOD_MS  10

#ifndef HW_ENCODER_A_PIN
#define HW_ENCODER_A_PIN    22
#endif

#ifndef HW_ENCODER_B_PIN
#define HW_ENCODER_B_PIN    23
#endif

#ifndef HW_ENCODER_BTN_PIN
#define HW_ENCODER_BTN_PIN  24
#endif

static RotaryEncoder_t g_encoder;

#define ENCODER_A_READ()   ((GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_ENCODER_A_PIN))) ? 1 : 0)
#define ENCODER_B_READ()   ((GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_ENCODER_B_PIN))) ? 1 : 0)
#define ENCODER_BTN_READ() ((GPIO_RegOneBitGet(GPIO_A_IN, (1 << HW_ENCODER_BTN_PIN))) ? 1 : 0)

void RotaryEncoder_Init(void)
{
    GPIO_RegOneBitSet(GPIO_A_IE, (1 << HW_ENCODER_A_PIN));
    GPIO_RegOneBitClear(GPIO_A_OE, (1 << HW_ENCODER_A_PIN));
    GPIO_RegOneBitSet(GPIO_A_PU, (1 << HW_ENCODER_A_PIN));
    GPIO_RegOneBitClear(GPIO_A_PD, (1 << HW_ENCODER_A_PIN));

    GPIO_RegOneBitSet(GPIO_A_IE, (1 << HW_ENCODER_B_PIN));
    GPIO_RegOneBitClear(GPIO_A_OE, (1 << HW_ENCODER_B_PIN));
    GPIO_RegOneBitSet(GPIO_A_PU, (1 << HW_ENCODER_B_PIN));
    GPIO_RegOneBitClear(GPIO_A_PD, (1 << HW_ENCODER_B_PIN));

    GPIO_RegOneBitSet(GPIO_A_IE, (1 << HW_ENCODER_BTN_PIN));
    GPIO_RegOneBitClear(GPIO_A_OE, (1 << HW_ENCODER_BTN_PIN));
    GPIO_RegOneBitSet(GPIO_A_PU, (1 << HW_ENCODER_BTN_PIN));
    GPIO_RegOneBitClear(GPIO_A_PD, (1 << HW_ENCODER_BTN_PIN));

    g_encoder.last_a = ENCODER_A_READ();
    g_encoder.last_btn = 1;
    g_encoder.btn_press_cnt = 0;
    g_encoder.btn_clicked = 0;
    g_encoder.long_press_sent = 0;
    g_encoder.delta = 0;

    DBG("Rotary Encoder initialized (A=A%d, B=A%d, BTN=A%d)\n",
        HW_ENCODER_A_PIN, HW_ENCODER_B_PIN, HW_ENCODER_BTN_PIN);
}

uint8_t RotaryEncoder_Scan(void)
{
    uint8_t cur_a = ENCODER_A_READ();
    uint8_t cur_btn = ENCODER_BTN_READ();
    uint8_t event = ENCODER_EVT_NONE;

    if (cur_a != g_encoder.last_a) {
        uint8_t cur_b = ENCODER_B_READ();
        if (cur_a == cur_b) {
            g_encoder.delta++;
            event = ENCODER_EVT_CW;
        } else {
            g_encoder.delta--;
            event = ENCODER_EVT_CCW;
        }
        g_encoder.last_a = cur_a;
    }

    if (cur_btn == 0) {
        g_encoder.btn_press_cnt++;
        if (g_encoder.btn_press_cnt >= (ENCODER_LONG_PRESS_MS / ENCODER_SCAN_PERIOD_MS)) {
            if (!g_encoder.long_press_sent) {
                g_encoder.long_press_sent = 1;
                event = ENCODER_EVT_LONG_PRESS;
            }
        }
    } else {
        if (g_encoder.btn_press_cnt > 0 && g_encoder.btn_press_cnt < (ENCODER_LONG_PRESS_MS / ENCODER_SCAN_PERIOD_MS)) {
            g_encoder.btn_clicked = 1;
            event = ENCODER_EVT_CLICK;
        }
        g_encoder.btn_press_cnt = 0;
        g_encoder.long_press_sent = 0;
    }

    return event;
}

int16_t RotaryEncoder_GetDelta(void)
{
    int16_t d = g_encoder.delta;
    g_encoder.delta = 0;
    return d;
}

uint8_t RotaryEncoder_IsButtonPressed(void)
{
    uint8_t clicked = g_encoder.btn_clicked;
    g_encoder.btn_clicked = 0;
    return clicked;
}

uint8_t RotaryEncoder_IsButtonLongPressed(void)
{
    return g_encoder.long_press_sent;
}

#endif /* HW_DRV_ENCODER_EN */
