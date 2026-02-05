# Shell命令功能绑定状态分析

## 问题概述
部分shell命令只修改了参数，但没有调用实际的驱动函数来应用更改，导致"虚有其表"。

## 需要修复的命令

### 1. LCD模块 (`lcd` 命令)

#### ✅ `lcd -c <color>` - 背景颜色设置
**当前状态**: ✅ 已修复 - 通过UI系统安全地应用背景色
**位置**: `bg_shell_commands.c:1242` (`lcd_bgcolor`)
**实现方案**: 
1. 在 `bg_ui.h/c` 中添加 `SetBackgroundColor()` 和 `GetBackgroundColor()` 接口
2. 在shell命令中调用 `BG_UI.SetBackgroundColor(color)` 设置背景色
3. 修改views使用 `BG_UI.GetBackgroundColor()` 获取背景色进行清屏
4. 避免直接调用 `BG_lcd.Clear()` 清除UI元素

**修复代码**:
```c
static int lcd_bgcolor(int argc, char *argv[])
{
    if(argc < 1) {
        Shell_Printf("BG Color: 0x%04X\r\n", SYSPARAM_LCD()->bg_color);
        return 0;
    }
    
    uint16_t color = (uint16_t)strtol(argv[0], NULL, 16);
    SYSPARAM_LCD()->bg_color = color;
    SysParam_MarkModified();
    
    /* Apply background color through UI system (safe, won't clear UI) */
    extern const BG_UI_t BG_UI;
    BG_UI.SetBackgroundColor(color);
    
    Shell_Printf("BG Color: 0x%04X (Applied)\r\n", color);
    return 0;
}
```

#### ❌ `lcd -b <0-100>` - 对比度/亮度设置
**当前状态**: 只修改 `SYSPARAM_LCD()->contrast`，没有实际应用
**位置**: `bg_shell_commands.c:1225` (`lcd_bl`)
**需要调用**: LCD驱动的对比度设置函数（需要在LCD驱动中实现）
**问题**: `BG_lcd` 结构体中没有 `SetContrast()` 函数指针
**修复方案**:
1. 在 `bg_lcd.h` 中添加 `void (*SetContrast)(uint8_t level);`
2. 在LCD驱动实现中添加对比度设置
3. 在命令中调用该函数

#### ⚠️ `lcd -o` / `lcd -f` - LCD开关
**当前状态**: 只打印消息，没有实际控制LCD
**位置**: `bg_shell_commands.c:1211, 1218` (`lcd_on`, `lcd_off`)
**需要调用**: LCD电源控制或背光控制函数
**修复方案**: 需要在LCD驱动中实现开关功能

---

### 2. Audio模块 (`audio` 命令)

#### ❌ `audio -g <vol>` - 吉他音量
**当前状态**: 只修改 `SYSPARAM_AUDIO()->guitar_volume`
**位置**: `bg_shell_commands.c:418` (`audio_guitar_vol`)
**需要调用**: 音频效果链的增益控制
**修复方案**:
```c
static int audio_guitar_vol(int argc, char *argv[])
{
    if(argc < 1) {
        Shell_Printf("Guitar volume: %d\r\n", SYSPARAM_AUDIO()->guitar_volume);
        return 0;
    }
    
    int vol = atoi(argv[0]);
    if(vol < 0) vol = 0;
    if(vol > 100) vol = 100;
    SYSPARAM_AUDIO()->guitar_volume = vol;
    SysParam_MarkModified();
    
    /* TODO: Apply to audio processing chain */
    // 需要找到Guitar输入节点并设置其音量
    // 或者调用音频引擎的API设置通道增益
    
    Shell_Printf("Guitar volume: %d\r\n", vol);
    return 0;
}
```

#### ❌ `audio -m <vol>` - 麦克风音量
**当前状态**: 只修改 `SYSPARAM_AUDIO()->mic_volume`
**位置**: `bg_shell_commands.c:436` (`audio_mic_vol`)
**需要调用**: 音频效果链的增益控制

#### ❌ `audio -o <vol>` - 输出音量
**当前状态**: 只修改 `SYSPARAM_AUDIO()->output_volume`
**位置**: `bg_shell_commands.c:454` (`audio_output_vol`)
**需要调用**: DAC输出音量控制或主音量控制

---

### 3. Chain模块 (`chain` 命令)

#### ❌ `chain -m <0-2>` - 输出模式切换
**当前状态**: 只修改 `SYSPARAM_AUDIOCHAIN()->output_mode`，没有实际切换音频路由
**位置**: `bg_shell_commands.c:1087` (`chain_mode`)
**需要调用**: 音频引擎的输出路由切换函数
**修复方案**: 需要通知音频引擎切换输出路由（耳机/扬声器）

---

### 3. LED模块 (`led` 命令)

#### ❌ `led -o` / `led -f` / `led -b` - LED控制
**当前状态**: 全部只打印消息，没有实际控制LED
**位置**: `bg_shell_commands.c:1275-1295`
**需要实现**: GPIO LED控制函数

---

## 已正确绑定的命令

### ✅ Chain模块 (`chain` 命令)
- 所有命令都正确操作了 `g_sys_param.audio_chain` 数据结构
- 通过 `SysParam_Save()` 可以持久化
- 注意：需要与实际音频引擎同步（待实现）

### ✅ Sys模块 (`sys` 命令)
- `sys -b` (reboot) - 调用 `Reset_McuSystem()`
- `sys -f` (factory reset) - 调用 `SysParam_LoadDefault()` + `SysParam_Save()`
- `sys -c` (console) - 调用 `Shell_ConsoleEnable()`
- `sys -d` (dbg to lcd) - 调用 `Shell_DbgToLcdEnable()`

---

## 修复优先级

### 高优先级 (影响用户体验)
1. ✅ **LCD背景颜色** - 立即可见的效果
2. **Audio音量控制** - 核心功能
3. **LCD亮度/对比度** - 显示质量

### 中优先级
4. **Chain mode命令** - 输出路由切换
5. **LED控制** - 状态指示
6. **LCD开关** - 省电功能

### 低优先级
7. 其他辅助功能

---

## 实现建议

### 短期方案（快速修复）
1. 在相关命令函数中直接调用驱动API
2. 添加注释说明需要与音频引擎同步

### 长期方案（架构优化）
1. 创建统一的参数应用层 (Parameter Apply Layer)
2. 当参数修改时，自动触发相应的驱动函数
3. 实现参数变更回调机制

```c
/* 参数应用层接口 */
typedef struct {
    void (*ApplyLcdContrast)(uint8_t contrast);
    void (*ApplyLcdBgColor)(uint16_t color);
    void (*ApplyGuitarVolume)(uint8_t volume);
    void (*ApplyMicVolume)(uint8_t volume);
    void (*ApplyOutputVolume)(uint8_t volume);
} ParamApplyCallbacks_t;

/* 注册回调 */
void SysParam_RegisterApplyCallbacks(const ParamApplyCallbacks_t *callbacks);

/* 应用参数（在命令中调用） */
void SysParam_ApplyLcdContrast(void);
void SysParam_ApplyLcdBgColor(void);
```

---

## 待确认信息

1. **音频引擎API**: 需要确认实际的音频处理引擎接口
   - 是否有设置通道增益的函数？
   - effect_graph是否已实现运行时参数修改？

2. **LCD驱动能力**: 
   - ST7735是否支持对比度调节？
   - 是否有硬件背光控制？

3. **LED硬件**: 
   - LED连接到哪个GPIO？
   - 是否已有GPIO驱动封装？

---

**生成时间**: 2026-01-10  
**分析文件**: `BanBox/src/banux/04_shell_commands/bg_shell_commands.c`
