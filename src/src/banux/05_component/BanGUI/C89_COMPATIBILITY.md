# C89 兼容性修复指南

## 问题说明

编译错误：
```
error: 'for' loop initial declarations are only allowed in C99 or C11 mode
```

编译器使用 C89 标准，不支持在 for 循环中声明变量。

## 修复模式

### ❌ 错误写法（C99）
```c
for (uint8_t i = 0; i < count; i++) {
    // 代码
}
```

### ✅ 正确写法（C89）
```c
uint8_t i;
for (i = 0; i < count; i++) {
    // 代码
}
```

## 已修复的文件

- ✅ `ui/core/bg_ui.c` - 8处修复
- ✅ `ui/core/ui_page.c` - 2处修复  
- ✅ `ui/views/view_home.c` - 1处修复
- ✅ `ui/views/view_looper.c` - 1处修复

## 未来需要修复的文件（可选）

以下文件当前在编译中被使用，如果编译错误，需要进行相同修复：

### ui_system 文件
- `ui_system/ui_core.c` - 14处需要修复
- `ui_system/ui_system.c` - 检查是否有相同问题
- `ui_system/ui_menu.c` - 检查是否有相同问题
- `ui_system/ui_statusbar.c` - 检查是否有相同问题
- `ui_system/ui_button.c` - 检查是否有相同问题

### 其他文件
- `page/bg_page.c` - 需要修复（但应该从编译中移除）
- `BG_List/bg_list.c` - 检查是否有相同问题
- `base_func/gui_tool.c` - 检查是否有相同问题

## 自动修复脚本（如需要）

可以使用正则表达式和文本编辑器批量替换：

### VS Code 正则替换（全局）
**查找：**
```
for \((\w+)\s+(\w+)\s*=\s*([^)]+)\)\s*\{
```

**替换为：**
```
$1 $2;
for ($2 = $3) {
```

但建议逐个文件手动检查和修复，确保代码正确性。

---
Generated: 2026-01-08
