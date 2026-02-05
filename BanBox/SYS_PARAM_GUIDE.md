# 系统参数保存功能说明

## 概述

BanBox 现已支持系统参数的持久化存储。参数保存在芯片内部 Flash 的 Sector 255 (地址 0xFF000)，大小 4KB。

## 功能特性

- **开机自动加载**: 系统启动时自动从 Flash 读取保存的参数到全局变量
- **CRC32 校验**: 参数加载时进行 CRC 校验，防止数据损坏
- **模块化保存**: 支持按模块（audio, looper, bt, lcd）保存参数
- **命令行支持**: 通过 Shell 命令管理参数

## Shell 命令

### param 命令 - 参数管理

```bash
# 显示帮助
param

# 加载参数
param -l

# 保存所有参数
param -s

# 保存指定模块参数
param -s audio
param -s looper

# 重置为默认值
param -d

# 打印所有参数
param -p

# 打印指定模块参数
param -p audio
param -p looper
param -p bt
param -p lcd

# 显示参数系统信息
param -i

# 测试 Flash 读写
param -t

# 擦除参数区域（危险！）
param -e
```

### 模块保存命令

每个模块都有 `-S` 选项用于保存该模块的参数：

```bash
# 音量相关
audio -v 80          # 设置音量
audio -S             # 保存音频参数

# Looper 相关
looper -m song       # 设置循环模式
looper -S            # 保存 looper 参数

# 蓝牙相关
bt -S                # 保存蓝牙参数

# LCD 相关
lcd -b 50            # 设置背光
lcd -S               # 保存 LCD 参数
```

## 参数结构

```c
SysParam_t {
    // 头部信息
    magic         // 魔数 0x50415241 ("PARA")
    version       // 版本号
    size          // 结构大小
    crc32         // CRC校验值
    write_count   // 写入次数
    
    // 模块参数
    system {
        boot_status
        boot_count
    }
    
    volume {
        guitar_volume   // 0-100
        mic_volume      // 0-100
        output_volume   // 0-100
    }
    
    looper {
        loop_count
        overdub_mode
        quantize
        click_volume
        tempo           // 40-240 BPM
        time_signature
        fade_time
        max_loop_time
    }
    
    bluetooth {
        enabled
        discoverable
        auto_connect
        a2dp_volume
        device_name[16]
        paired_addr[6]
    }
    
    lcd {
        contrast
        color_scheme
        screen_saver
        bg_color
    }
    
    user {
        data[32]        // 用户自定义数据
    }
}
```

## 使用示例

### 典型使用流程

1. **修改参数**:
   ```bash
   audio -v 80        # 设置音量为 80
   looper -m song     # 设置 looper 为 song 模式
   ```

2. **保存参数**:
   ```bash
   audio -S           # 保存音频参数
   # 或者
   param -s           # 保存所有参数
   ```

3. **断电重启后参数自动恢复**

### 重置参数

```bash
param -d             # 加载默认值
param -s             # 保存到 Flash
```

## 技术细节

### Flash 配置

- 扇区号: 255
- 地址: 0xFF000
- 大小: 4KB
- 超时: 100ms

### API 函数

```c
// 初始化（开机时自动调用）
SysParam_Status_t SysParam_Init(void);

// 保存所有参数
SysParam_Status_t SysParam_Save(void);

// 保存指定模块
SysParam_Status_t SysParam_SaveModule(const char *module);

// 获取参数指针
SysParam_t* SysParam_Get(void);

// 加载默认值
SysParam_Status_t SysParam_LoadDefault(void);

// 检查是否修改
bool SysParam_IsModified(void);

// 获取写入次数
uint32_t SysParam_GetWriteCount(void);
```

### 快捷宏

```c
// 读取音量
uint8_t vol = SYSPARAM_AUDIO()->guitar_volume;

// 修改并保存
SYSPARAM_AUDIO()->guitar_volume = 80;
SysParam_Save();
```

## 注意事项

1. **立即保存**: 由于设备是直接断电关机，修改参数后必须立即调用保存命令
2. **写入次数**: Flash 有擦写寿命限制（约 10 万次），避免频繁保存
3. **版本兼容**: 参数结构变化时需要处理版本迁移
4. **数据校验**: 系统自动进行 CRC 校验，校验失败会加载默认值

## 故障排除

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 参数不保存 | Flash 写保护 | 检查 Flash 保护状态 |
| 加载失败 | CRC 错误 | 使用 `param -d` 重置 |
| 参数丢失 | 未调用保存 | 修改后执行 `-S` 保存 |

## 文件列表

- `sys_param.h` - 参数结构定义和 API 声明
- `sys_param.c` - 参数存储实现
- `param_def.h` - Flash 地址和配置定义
- `shell_cmd_param.c/h` - Shell 命令模块
