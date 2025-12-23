#ifndef _BG_MENU_SLIDER_H__
#define _BG_MENU_SLIDER_H__

#include <stdint.h>
#include <stddef.h>  /* for NULL */

// 配置宏
#define BG_MENU_SLIDER_MAX_ITEMS    10      // 最大菜单项数量
#define BG_MENU_SLIDER_ITEM_WIDTH   64      // 默认菜单项宽度
#define BG_MENU_SLIDER_ITEM_HEIGHT  64      // 默认菜单项高度
#define BG_MENU_SLIDER_BORDER_WIDTH 2       // 默认边框宽度

// 滑动方向枚举
typedef enum {
    BG_MENU_SLIDER_LEFT = 0,
    BG_MENU_SLIDER_RIGHT = 1
} BG_MenuSlider_Direction;

// 动画状态枚举
typedef enum {
    BG_MENU_SLIDER_IDLE = 0,
    BG_MENU_SLIDER_SLIDING = 1
} BG_MenuSlider_State;

// 缓动类型枚举
typedef enum {
    BG_MENU_SLIDER_EASE_LINEAR = 0,
    BG_MENU_SLIDER_EASE_IN_OUT = 1,
    BG_MENU_SLIDER_EASE_OUT = 2
} BG_MenuSlider_EaseType;

// 菜单项数据结构
typedef struct {
    const char* name;               // 菜单项名称
    const uint8_t* icon_data;       // 图标数据指针
    uint16_t icon_width;            // 图标宽度
    uint16_t icon_height;           // 图标高度
    void (*callback)(void);         // 选中回调函数
    uint32_t user_data;             // 用户自定义数据
} BG_MenuSlider_Item;

// 菜单项表结构 - 用于初始化时传入一张表
typedef struct {
    BG_MenuSlider_Item* items;      // 菜单项数组
    uint8_t item_count;             // 菜单项数量
} BG_MenuSlider_Table;

// 显示区域配置
typedef struct {
    uint16_t x;                     // 显示区域左上角X坐标
    uint16_t y;                     // 显示区域左上角Y坐标
    uint16_t width;                 // 显示区域宽度
    uint16_t height;                // 显示区域高度
    uint16_t center_x;              // 中心X坐标（选中项居中位置）
    uint16_t item_width;            // 菜单项宽度
    uint16_t item_height;           // 菜单项高度
} BG_MenuSlider_Region;

// 帧缓冲配置 - 支持复用帧缓冲数组
typedef struct {
    uint16_t* buffer;               // 帧缓冲指针（可以是共享的）
    uint16_t buffer_width;          // 缓冲区总宽度
    uint16_t buffer_height;         // 缓冲区总高度
    uint8_t use_dirty_region;       // 是否使用脏区域优化
    uint8_t use_shared_buffer;      // 是否使用共享缓冲区
    // 刷新区域设置
    uint16_t refresh_x;             // 刷新区域X坐标
    uint16_t refresh_y;             // 刷新区域Y坐标
    uint16_t refresh_width;         // 刷新区域宽度
    uint16_t refresh_height;        // 刷新区域高度
} BG_MenuSlider_FrameBuffer;

// 样式配置
typedef struct {
    uint16_t selected_color;        // 选中项边框颜色
    uint16_t normal_color;          // 普通项边框颜色
    uint16_t background_color;      // 背景颜色
    uint16_t text_color_selected;   // 选中文字颜色
    uint16_t text_color_normal;     // 普通文字颜色
    uint8_t border_width;           // 边框宽度
    uint8_t show_border;            // 是否显示边框
    uint8_t show_background;        // 是否显示背景
    uint8_t show_text;              // 是否显示文字
} BG_MenuSlider_Style;

// 动画配置
typedef struct {
    uint16_t slide_duration;        // 滑动动画持续时间(ms)
    BG_MenuSlider_EaseType ease_type; // 缓动类型
    uint8_t auto_slide_enable;      // 是否启用自动滑动
    uint16_t auto_slide_interval;   // 自动滑动间隔(ms)
    uint8_t scale_effect;           // 是否启用选中项缩放效果
} BG_MenuSlider_Animation;

// 控件内部数据
typedef struct {
    BG_MenuSlider_Item items[BG_MENU_SLIDER_MAX_ITEMS]; // 菜单项数组
    uint8_t item_count;             // 当前菜单项数量
    uint8_t selected_index;         // 当前选中项索引
    int16_t current_offset;         // 当前偏移量
    int16_t target_offset;          // 目标偏移量
    BG_MenuSlider_State state;      // 动画状态
    uint16_t animation_timer;       // 动画计时器
    uint32_t auto_slide_timer;      // 自动滑动计时器
    uint8_t need_redraw;            // 是否需要重绘
    uint8_t initialized;            // 是否已初始化
    uint8_t auto_slide_active;      // 自动滑动是否激活
} BG_MenuSlider_Data;

// 前向声明
struct BG_MenuSlider;

// 主控件结构体 - 面向对象设计，类似BG_List
typedef struct BG_MenuSlider {
    // 配置接口
    void (*SetRegion)(struct BG_MenuSlider* self, const BG_MenuSlider_Region* region);
    void (*SetFrameBuffer)(struct BG_MenuSlider* self, const BG_MenuSlider_FrameBuffer* fb);
    void (*SetStyle)(struct BG_MenuSlider* self, const BG_MenuSlider_Style* style);
    void (*SetAnimation)(struct BG_MenuSlider* self, const BG_MenuSlider_Animation* anim);
    
    // 菜单操作接口 - 支持传入菜单项表
    uint8_t (*AddItem)(struct BG_MenuSlider* self, const BG_MenuSlider_Item* item);
    void (*ClearItems)(struct BG_MenuSlider* self);
    void (*LoadTable)(struct BG_MenuSlider* self, const BG_MenuSlider_Table* table);
    uint8_t (*RemoveItem)(struct BG_MenuSlider* self, uint8_t index);
    
    // 控制接口 - 左滑右滑
    void (*SlideLeft)(struct BG_MenuSlider* self);
    void (*SlideRight)(struct BG_MenuSlider* self);
    void (*SlideTo)(struct BG_MenuSlider* self, uint8_t index);
    void (*Select)(struct BG_MenuSlider* self);
    
    // 更新和绘制接口
    void (*Update)(struct BG_MenuSlider* self);
    void (*Draw)(struct BG_MenuSlider* self);
    void (*Clear)(struct BG_MenuSlider* self);
    void (*Refresh)(struct BG_MenuSlider* self);
    void (*RefreshRegion)(struct BG_MenuSlider* self, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    
    // 自动滑动接口
    void (*StartAutoSlide)(struct BG_MenuSlider* self);
    void (*StopAutoSlide)(struct BG_MenuSlider* self);
    void (*ToggleAutoSlide)(struct BG_MenuSlider* self);
    void (*AutoSlideUpdate)(struct BG_MenuSlider* self, uint32_t delta_ms);
    
    // 状态查询接口
    uint8_t (*GetSelectedIndex)(struct BG_MenuSlider* self);
    const BG_MenuSlider_Item* (*GetSelectedItem)(struct BG_MenuSlider* self);
    uint8_t (*GetItemCount)(struct BG_MenuSlider* self);
    uint8_t (*IsSliding)(struct BG_MenuSlider* self);
    uint8_t (*IsAutoSliding)(struct BG_MenuSlider* self);
    uint8_t (*IsInitialized)(struct BG_MenuSlider* self);
    
    // 内部数据成员
    BG_MenuSlider_Data data;
    BG_MenuSlider_Region region;
    BG_MenuSlider_FrameBuffer framebuffer;
    BG_MenuSlider_Style style;
    BG_MenuSlider_Animation animation;
    
} BG_MenuSlider;

// 初始化和销毁接口 - 类似BG_List_Init
BG_MenuSlider BG_MenuSlider_Init(const BG_MenuSlider_Region* region, 
                                 const BG_MenuSlider_Style* style);
void BG_MenuSlider_DeInit(BG_MenuSlider* slider);

// 快速创建接口 - 传入菜单项表直接创建
BG_MenuSlider BG_MenuSlider_Create(const BG_MenuSlider_Region* region,
                                   const BG_MenuSlider_Style* style,
                                   const BG_MenuSlider_Table* table);

// 预定义样式常量
extern const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_DEFAULT;
extern const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_DARK;
extern const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_COLORFUL;
extern const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_MINIMAL;

// 预定义动画配置常量
extern const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_SMOOTH;
extern const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_FAST;
extern const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_AUTO;
extern const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_INSTANT;

// 预定义区域配置常量
extern const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_CENTER;
extern const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_TOP;
extern const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_BOTTOM;

// 预定义帧缓冲配置常量
extern const BG_MenuSlider_FrameBuffer BG_MENU_SLIDER_FB_FULL_SCREEN;
extern const BG_MenuSlider_FrameBuffer BG_MENU_SLIDER_FB_DIRTY_ONLY;

// 工具宏
#define BG_MENU_SLIDER_ITEM(name, icon, w, h, cb, data) \
    {(name), (icon), (w), (h), (cb), (data)}

#define BG_MENU_SLIDER_TEXT_ITEM(name, cb) \
    {(name), NULL, 0, 0, (cb), 0}

#define BG_MENU_SLIDER_TABLE(items_array) \
    {(items_array), sizeof(items_array)/sizeof(items_array[0])}

#endif // _BG_MENU_SLIDER_H__
