# 编译配置说明

## 📁 新增文件
- `frame_buffer_impl.c` - 帧缓冲具体实现

## 🔧 编译设置

### 1. 将新文件加入Makefile
在你的Makefile中添加：
```makefile
# 添加到源文件列表
SOURCES += frame_buffer_impl.c
```

### 2. 内存检查
确保你的MCU有足够的RAM：
- **帧缓冲**: 40KB (160×128×2字节)
- **系统RAM**: 建议总RAM > 64KB

### 3. 编译宏控制

#### 启用帧缓冲模式 (推荐)
在 `bg_lcd.h` 中：
```c
#define USE_FRAME_BUFFER 1
```

#### 禁用帧缓冲模式 (省内存)
在 `bg_lcd.h` 中：
```c
// #define USE_FRAME_BUFFER 1  // 注释掉这行
```

## 🚨 注意事项

### 内存不足时
如果编译时提示RAM不足：
1. 检查链接器脚本的RAM配置
2. 或者禁用帧缓冲模式
3. 或者优化其他内存使用

### 性能测试
```c
// 在main函数中可以测试性能
void test_frame_buffer() {
    uint32_t start_time, end_time;
    
    start_time = get_system_time();
    
    // 绘制测试
    BG_lcd.Clear(BLACK);
    for(int i = 0; i < 1000; i++) {
        BG_lcd.DrawPoint(i%160, i%128, RED);
    }
    BG_lcd.FlushFrameBuffer();
    
    end_time = get_system_time();
    printf("Frame buffer time: %d ms\n", end_time - start_time);
}
```

## ✅ 验证步骤

1. **编译成功** - 无错误和警告
2. **运行正常** - 菜单可以正常显示和切换
3. **性能提升** - 感受到明显的流畅度提升
4. **内存稳定** - 系统运行稳定，无内存溢出

如果遇到问题，可以先禁用帧缓冲模式确保基本功能正常。
