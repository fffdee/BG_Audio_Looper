# 效果器命令使用指南

## ✅ 功能状态
**已完成并集成** - 2026年1月6日

## 📌 快速开始

### 命令注册
在 `bg_audio_io_manager.c` 的初始化函数中调用：
```c
#include "shell_cmd_effect.h"

void BG_AudioIO_Manager_Init(void)
{
    // ... 其他初始化
    ShellCmdEffect_Register();  // 注册effect命令
}
```

### 基本命令

```bash
# 列出所有效果器及状态
effect list

# 查看效果器详细信息
effect info 1        # 查看DRC (ID=1)
effect info 2        # 查看EQ (ID=2)

# 获取参数值
effect get 1 threshold
effect get 1 ratio

# 设置参数值
effect set 1 threshold -25
effect set 1 ratio 4
effect set 1 attack 10
effect set 1 release 100

# 启用/禁用效果器
effect enable 1 on   # 启用DRC
effect enable 1 off  # 禁用DRC
effect enable 1      # 查询当前状态

# 帮助信息
effect help
```

## 📊 效果器ID列表

| ID | 名称 | 描述 | 主要参数 |
|----|------|------|----------|
| 0 | reverb | 混响效果 | room, damp, wet |
| 1 | drc | 动态范围压缩(麦克风) | threshold, ratio, attack, release |
| 2 | eq | 均衡器(麦克风) | band0-9 |
| 3 | expander | 扩展器 | threshold, ratio |
| 4 | echo | 回声效果 | delay, feedback |
| 5 | howling | 啸叫抑制 | enable |
| 6 | 3d | 3D音效 | enable |
| 7 | vbass | 虚拟低音 | enable |
| 8 | plate_reverb | 板式混响 | enable |
| 9 | music_drc | 动态范围压缩(音乐) | threshold, ratio, attack, release |
| 10 | music_eq | 均衡器(音乐) | band0-9 |

## 🎛️ 常用调节场景

### DRC压缩器调节 (ID=1 或 ID=9)

```bash
# 轻度压缩 - 自然音色
effect set 1 threshold -20
effect set 1 ratio 2
effect set 1 attack 5
effect set 1 release 50

# 中度压缩 - 平衡动态
effect set 1 threshold -25
effect set 1 ratio 4
effect set 1 attack 10
effect set 1 release 100

# 重度压缩 - 控制峰值
effect set 1 threshold -30
effect set 1 ratio 8
effect set 1 attack 2
effect set 1 release 200

# 查询当前值
effect get 1 threshold
effect get 1 ratio
effect get 1 attack
effect get 1 release
```

### EQ均衡器调节 (ID=2 或 ID=10)

```bash
# 人声清晰化
effect set 2 band3 3    # 中频 +3dB
effect set 2 band4 2    # 中频 +2dB
effect set 2 band0 -2   # 低频 -2dB

# 增强低音
effect set 2 band0 6    # 低频 +6dB
effect set 2 band1 3    # 低中频 +3dB

# 高频提亮
effect set 2 band9 5    # 高频 +5dB
effect set 2 band8 3    # 中高频 +3dB

# 查询频段增益
effect get 2 band0
effect get 2 band5
```

### 扩展器调节 (ID=3)

```bash
# 降低背景噪声
effect set 3 threshold -60
effect set 3 ratio 2
effect enable 3 on

# 查询设置
effect get 3 threshold
effect get 3 ratio
```

### 回声效果 (ID=4)

```bash
# 短回声
effect set 4 delay 100
effect set 4 feedback 30
effect enable 4 on

# 长回声
effect set 4 delay 500
effect set 4 feedback 50

# 关闭回声
effect enable 4 off
```

## 🔧 API编程接口

如果需要在代码中控制效果器，可以使用以下API：

```c
#include "shell_cmd_effect.h"

// 获取/设置使能状态
bool enabled = Effect_GetEnabled(EFFECT_ID_DRC);
Effect_SetEnabled(EFFECT_ID_DRC, true);

// 获取/设置DRC参数
int32_t value;
Effect_GetDRCParam(EFFECT_ID_DRC, "threshold", &value);
Effect_SetDRCParam(EFFECT_ID_DRC, "threshold", -25);

// 获取/设置EQ参数
int8_t gain;
Effect_GetEQBandGain(EFFECT_ID_EQ, 0, &gain);  // band 0
Effect_SetEQBandGain(EFFECT_ID_EQ, 0, 6);       // +6dB

// 重置效果器
Effect_Reset(EFFECT_ID_DRC);

// 打印所有参数
Effect_PrintAllParams(EFFECT_ID_DRC);
```

## 🐛 故障排查

### 问题1：命令不可用
```bash
$ effect
Unknown module: effect
```

**解决方案：**
1. 确认已调用 `ShellCmdEffect_Register()`
2. 确认在 `shell_fs.c` 中注册了命令：
   ```c
   ShellFs_RegisterCommand("effect");
   ```
3. 重新编译并烧录固件

### 问题2：参数不生效
**检查项：**
- 参数名拼写是否正确
- 参数值是否在合理范围内
- 效果器是否已启用（`effect enable <id> on`）
- 是否需要重启音频流

### 问题3：查看当前配置
```bash
# 列出所有效果器状态
effect list

# 查看特定效果器详情
effect info 1

# 使用API打印完整参数（需在代码中调用）
Effect_PrintAllParams(EFFECT_ID_DRC);
```

## 📝 实现细节

### 代码文件
- `shell_cmd_effect.h` - API接口声明
- `shell_cmd_effect.c` - 完整实现 (710行)
- 集成位置：`bg_audio_io_manager.c`

### 命令注册
使用Shell模块系统注册：
```c
static const ShellModule_t g_EffectModule = {
    "effect",
    "Audio Effect Parameter Control",
    MOD_CAT_AUDIO,
    g_EffectOpts,
    5
};

void ShellCmdEffect_Register(void)
{
    Shell_RegisterModule(&g_EffectModule);
    ShellFs_RegisterCommand("effect");
}
```

### 参数映射
直接访问 `gCtrlVars` 全局结构体：
- DRC: `gCtrlVars.mic_drc_unit` / `gCtrlVars.music_drc_unit`
- EQ: `gCtrlVars.mic_out_eq_unit`
- Expander: `gCtrlVars.mic_expander_unit`
- Echo: `gCtrlVars.echo_unit`
- 其他效果器的enable字段

## 📚 相关文档

- `EFFECT_PARAMS_GUIDE.md` - 详细使用指南
- `EFFECT_QUICK_REFERENCE.md` - 快速参考
- `EFFECT_INTEGRATION_CHECKLIST.md` - 集成检查清单
- `EFFECT_TECHNICAL_DOCUMENT.md` - 技术文档
- `EFFECT_IMPLEMENTATION_SUMMARY.md` - 实现总结
- `EFFECT_DELIVERY_CHECKLIST.md` - 交付检查清单

## ✨ 下一步扩展

1. **持久化保存** - 实现 `Effect_SaveConfig()` / `Effect_LoadConfig()`
2. **预设管理** - 添加预设加载/保存功能
3. **参数范围检查** - 完善参数值校验
4. **实时监控** - 添加参数变化监控
5. **批量操作** - 支持一次设置多个参数

## 🎉 总结

效果器参数调节Shell命令已完整实现并可立即使用。通过命令行可以：
- ✅ 查询所有效果器状态
- ✅ 实时调节效果器参数
- ✅ 启用/禁用效果器
- ✅ 查看详细参数信息
- ✅ 通过API编程控制

适用于音频性能调试、参数优化、实时调节等场景。
