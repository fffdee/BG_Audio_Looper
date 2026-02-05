# UI命令故障排除指南

## 问题1: UI命令未注册 (Unknown module: ui)

### 可能原因
1. `shell_cmd_ui.c` 没有被编译
2. `UICmd_Register()` 没有被调用
3. Shell系统初始化顺序问题

### 解决步骤

#### 步骤1: 验证文件是否被编译
检查编译输出中是否有 `shell_cmd_ui.c`:
```bash
# 查看编译日志
grep "shell_cmd_ui" build.log
```

#### 步骤2: 清理并重新编译
```bash
# 清理项目
make clean

# 重新编译
make all
```

#### 步骤3: 检查注册调用
确认 `bg_shell_commands.c` 中包含：
```c
#include "shell_cmd_ui.h"

void Shell_RegisterAllModules(void)
{
    // ... 其他模块 ...
    
    /* UI控制命令 */
    UICmd_Register();
}
```

#### 步骤4: 检查Shell初始化顺序
在 `main.c` 中确认：
```c
Shell_Init();
Shell_SetIO(&g_CDC_IO);  // 或其他IO
Shell_RegisterAllModules();  // 这里会注册UI命令
```

#### 步骤5: 验证命令列表
连接串口后执行：
```bash
help -a
```
应该看到 `ui` 在列表中。

### 临时测试方法
如果重新编译不方便，可以先测试其他已注册的命令：
```bash
sys -i      # 系统信息
gpio -l     # GPIO列表
```

## 问题2: 开机界面立即跳转到旧Page系统

### 原因分析
- 旧的 `BG_page` 系统和新的 `BanGUI` 系统同时运行
- `BG_page.Loop()` 覆盖了 `BG_UI.Update()` 的显示

### 已修复内容
1. 禁用 `App_Pages_Init()` 调用
2. 注释掉 `BG_page.Loop()` 调用
3. 确保只有 `BG_UI.Update()` 在运行

### 验证修复
开机后应该：
1. 显示Boot动画（BG logo + 进度条）
2. 自动切换到Home界面（BanBox图片 + 4个图标）
3. 不再显示 "BanBox Home" 和 "Press ENTER for Menu" 文字

## 问题3: 状态栏显示字符而不是图标

### 原因
`UI_StatusBar_Draw()` 使用了 `BGUI_tool.ShowString()` 显示文字，而不是绘制图标。

### 已修复
修改为调用专门的图标绘制函数：
```c
draw_bt_icon();
draw_adc_icon();
draw_dac_icon();
draw_usb_icon();
draw_volume_icon();
```

### 验证修复
插入麦克风后，状态栏应该显示麦克风图标，而不是 "MIC" 文字。

## 问题4: 弹窗不断刷新

### 原因
弹窗在每次 `ui_update()` 时都会重新绘制，导致持续刷新。

### 已修复
添加 `drawn` 标志：
```c
struct {
    bool active;
    bool drawn;  // 标记是否已绘制
    // ...
} s_popup;

// 只在首次或状态改变时绘制
if (s_popup.active && !s_popup.drawn) {
    draw_popup();
    s_popup.drawn = true;
}
```

### 验证修复
显示弹窗后：
1. 弹窗只绘制一次
2. 持续时间到后自动消失
3. 不会闪烁或重复绘制

## 完整测试流程

### 1. 重新编译
```bash
cd BanBox/Debug
make clean
make all
```

### 2. 烧录并重启
烧录新固件并重启设备

### 3. 测试开机流程
- [ ] Boot动画正常显示
- [ ] 自动切换到Home界面
- [ ] Home显示BanBox图片和4个图标
- [ ] 不显示旧的文字界面

### 4. 测试状态栏
```bash
# 插入麦克风，观察状态栏
# 应该看到麦克风图标，不是 "MIC" 文字
```

### 5. 测试UI命令
```bash
# 连接USB CDC或BLE
help -a         # 应该看到 ui 命令
ui -g           # 获取当前状态
ui -p "Test"    # 测试弹窗
ui -k           # 查看按钮状态
```

### 6. 测试弹窗
```bash
ui -p "Test" "Popup Test" 3000
# 观察弹窗：
# - 应该只绘制一次
# - 3秒后自动消失
# - 不应该闪烁
```

## 调试技巧

### 如果UI命令仍然不可用
```bash
# 1. 检查可用命令
help -a

# 2. 检查Shell状态
sys -i

# 3. 检查IO状态
sys -o

# 4. 尝试切换IO
sys -o cdc    # 或 ble
```

### 如果状态栏仍然显示字符
添加调试输出查看icon_data是否正确：
```c
// 在 draw_adc_icon() 中添加
DBG("ADC Source: 0x%02X, MIC=%d, GTR=%d\n", 
    statusbar_data.adc_source,
    statusbar_data.adc_source & UI_ADC_MIC,
    statusbar_data.adc_source & UI_ADC_GUITAR);
```

### 如果弹窗仍然刷新
检查 `s_popup.drawn` 标志：
```c
// 在 ui_update() 中添加
if (s_popup.active) {
    DBG("Popup active, drawn=%d\n", s_popup.drawn);
}
```

## 预期结果

修复后的系统应该：
1. ✅ UI命令可用 (`ui -g`, `ui -p`, etc.)
2. ✅ 开机显示Boot动画，然后Home界面
3. ✅ Home界面显示BanBox图片，不是文字
4. ✅ 状态栏显示图标，不是字符
5. ✅ 弹窗只绘制一次，时间到后消失
6. ✅ 没有旧Page系统的干扰

## 如果问题仍然存在

请提供以下信息：
1. 编译日志中关于 `shell_cmd_ui.c` 的输出
2. `help -a` 的完整输出
3. 开机时的调试日志
4. 状态栏显示的截图或描述

## 相关文件清单

已修改的文件：
- ✅ `main.c` - 禁用旧Page系统
- ✅ `bg_ui.c` - 修复弹窗重复绘制
- ✅ `comp_statusbar.c` - 修复状态栏字符显示
- ✅ `bg_shell_commands.c` - 注册UI命令

新增的文件：
- ✅ `shell_cmd_ui.h` - UI命令头文件
- ✅ `shell_cmd_ui.c` - UI命令实现

文档文件：
- ✅ `UI_SHELL_COMMANDS.md` - 命令使用手册
- ✅ `UI_SHELL_TEST.md` - 测试脚本
- ✅ `UI_SHELL_INTEGRATION.md` - 集成说明
- ✅ `UI_TROUBLESHOOTING.md` - 本故障排除指南
