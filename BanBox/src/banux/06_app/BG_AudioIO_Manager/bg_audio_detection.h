/**
 * @file bg_audio_detection.h
 * @brief 音频插拔检测：Line1=A29 GPIO；Line2=POWERKEY ADC（左右声道独立检测）。
 */
#ifndef __BG_AUDIO_DETECTION_H__
#define __BG_AUDIO_DETECTION_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Line1（Line In 左声道 / Guitar1）插入状态：1=已插入, 0=未插入 */
uint8_t BG_AudioDetection_Line1IsPlugged(void);

/* Line2（Line In 右声道 / Guitar2）插入状态：POWERKEY ADC 独立判定，1=已插入, 0=未插入 */
uint8_t BG_AudioDetection_Line2IsPlugged(void);

/* Line2 轮询（每 1 秒调用一次）：状态变化时发布 EVT_AUDIO_GUITAR_IN 事件（port_id=2） */
void BG_AudioDetection_Line2Poll(void);

/* MIC 插入状态：A30 下拉，低电平=已插入。1=已插入, 0=未插入 */
uint8_t BG_AudioDetection_MicIsPlugged(void);

/* MIC 数据就绪：检测到插入后延迟 1 秒（稳定期）才返回 1，未插入/稳定期内返回 0。
 * 需被周期调用以推进内部状态机。 */
uint8_t BG_AudioDetection_MicReady(void);

#ifdef __cplusplus
}
#endif

#endif /* __BG_AUDIO_DETECTION_H__ */
