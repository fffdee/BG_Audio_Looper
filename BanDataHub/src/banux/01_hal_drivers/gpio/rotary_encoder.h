#ifndef __ROTARY_ENCODER_H__
#define __ROTARY_ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#include "product_def.h"

#if HW_DRV_ENCODER_EN

#define ENCODER_EVT_NONE        0
#define ENCODER_EVT_CW          1
#define ENCODER_EVT_CCW         2
#define ENCODER_EVT_CLICK       3
#define ENCODER_EVT_LONG_PRESS  4

typedef struct {
    uint8_t last_a;
    uint8_t last_btn;
    uint16_t btn_press_cnt;
    uint8_t btn_clicked;
    uint8_t long_press_sent;
    int16_t delta;
} RotaryEncoder_t;

void RotaryEncoder_Init(void);
uint8_t RotaryEncoder_Scan(void);
int16_t RotaryEncoder_GetDelta(void);
uint8_t RotaryEncoder_IsButtonPressed(void);
uint8_t RotaryEncoder_IsButtonLongPressed(void);

#endif /* HW_DRV_ENCODER_EN */

#ifdef __cplusplus
}
#endif

#endif /* __ROTARY_ENCODER_H__ */
