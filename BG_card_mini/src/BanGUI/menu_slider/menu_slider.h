#ifndef __MENU_SLIDER_H__
#define __MENU_SLIDER_H__

#include <stdint.h>
#include "gui_tool.h"
#include "bg_lcd.h"

// 菜单常量
#define MENU_ITEM_WIDTH 64     // 菜单项宽度(像素)
#define MENU_ITEM_HEIGHT 64    // 菜单项高度(像素)
#define MENU_SLIDER_SPEED 16   // 滑动速度(像素/帧) - 提高速度减少动画帧数
#define MAX_MENU_ITEMS 8       // 最大菜单项数量

// �˵���ṹ(����ͼ��)
typedef struct {
    const char* name;               // �˵�������
    const unsigned char* icon;      // ͼ������
    uint16_t icon_width;            // ͼ�����
    uint16_t icon_height;           // ͼ��߶�
    void (*callback)(void);         // ѡ�лص�����
} IconMenuItem;

// �˵������ؼ��ṹ
typedef struct {
    IconMenuItem items[MAX_MENU_ITEMS];  // �˵�������
    uint8_t item_count;                  // ʵ�ʲ˵�������
    uint8_t current_idx;                 // ��ǰѡ������
    int16_t target_offset;               // Ŀ��ƫ����(�����յ�)
    int16_t current_offset;              // ��ǰƫ����(��������)
    uint8_t is_sliding;                  // ����״̬(1:���ڻ���)
    uint16_t center_x;                   // �˵�����X����(ѡ�������)
    uint16_t start_y;                    // �˵�������ʼY����
    uint16_t selected_color;             // ѡ����߿���ɫ
    uint16_t normal_color;               // ��ͨ��߿���ɫ
} IconMenuSlider;

// ��ʼ��ͼ��˵�
void IconMenuSlider_Init(IconMenuSlider* slider,
                        uint16_t center_x,
                        uint16_t start_y,
                        uint16_t selected_color,
                        uint16_t normal_color);


// ���Ӳ˵���(��ͼ��)
uint8_t IconMenuSlider_AddItem(IconMenuSlider* slider,
                              const char* name,
                              const unsigned char* icon,
                              uint16_t icon_width,
                              uint16_t icon_height,
                              void (*callback)(void));

// ���������¼�(ѭ��)
void IconMenuSlider_Left(IconMenuSlider* slider);

// ���������¼�(ѭ��)
void IconMenuSlider_Right(IconMenuSlider* slider);

// ����ȷ���¼�(�����ص�)
void IconMenuSlider_Enter(IconMenuSlider* slider);

// ���¶���״̬(��ѭ������)
void IconMenuSlider_Update(IconMenuSlider* slider);

// ���Ʋ˵�(��ѭ������)
void IconMenuSlider_Draw(IconMenuSlider* slider);

// 高效清除菜单区域 - 避免全屏清屏
void IconMenuSlider_ClearArea(IconMenuSlider* slider);

// 快速跳转模式 - 无动画，直接跳转 (性能优先)
void IconMenuSlider_SetInstant(IconMenuSlider* slider, uint8_t target_idx);

// 弹性动画效果控制
void IconMenuSlider_SetElasticMode(IconMenuSlider* slider, uint8_t enable);

// 检查动画状态
uint8_t IconMenuSlider_IsAnimating(IconMenuSlider* slider);

// 强制停止动画
void IconMenuSlider_StopAnimation(IconMenuSlider* slider);

#endif
