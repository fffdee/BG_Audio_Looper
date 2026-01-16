# Echo命令使用指南

## 概述

`echo` 命令用于向VFS参数节点写入值，是修改效果图参数和驱动参数的关键命令。

## 语法

### 方法1：简化语法（推荐）

```bash
echo <parameter> <value>
```

### 方法2：重定向语法

```bash
echo <value> > <parameter>
```

## 使用示例

### 1. 修改效果图参数

#### 调整DRC阈值

```bash
$ cd /audio/graph0/nodes/6_drc
$ cat threshold
-20

$ echo threshold -30
OK

$ cat threshold
-30
```

#### 启用/禁用节点

```bash
$ cd /audio/graph0/nodes/8_reverb
$ cat enabled
1

$ echo enabled 0
OK

$ cat enabled
0
```

#### 调整EQ频段

```bash
$ cd /audio/graph0/nodes/7_eq
$ cat band0
0

$ echo band0 6
OK

$ cat band0
6
```

### 2. 修改驱动参数

#### 调整LCD亮度

```bash
$ cd /driver/spi/st7735
$ cat brightness
100

$ echo brightness 50
OK

$ cat brightness
50
```

#### 刷新电池状态

```bash
$ cd /driver/adc/battery
$ echo refresh 1
OK

$ cat voltage
3720
```

### 3. 批量修改（使用脚本）

```bash
# 配置完整的DRC
$ cd /audio/graph0/nodes/6_drc
$ echo threshold -25
$ echo ratio 6
$ echo attack 5
$ echo release 150

# 配置Reverb
$ cd /audio/graph0/nodes/8_reverb
$ echo room 70
$ echo damp 60
$ echo wet 40
```

### 4. 使用重定向语法

```bash
$ echo -20 > /audio/graph0/nodes/6_drc/threshold
OK

$ echo 50 > /driver/spi/st7735/brightness
OK
```

## 参数说明

### `<parameter>`

参数文件的路径，可以是：
- 相对路径：`threshold`（当前目录下）
- 绝对路径：`/audio/graph0/nodes/6_drc/threshold`

### `<value>`

要写入的值，支持：
- 整数：`-20`, `100`, `0`
- 字符串：对于某些参数（如预设名称）

## 错误处理

### 参数不存在

```bash
$ echo noexist 100
echo: noexist: No such file or directory
```

### 参数只读

```bash
$ echo type drc
echo: type: Read-only parameter
```

### 不是参数文件

```bash
$ echo nodes 100
echo: nodes: Not a parameter file
```

## 与cat命令配合使用

### 查看当前值

```bash
$ cat threshold
-20
```

### 修改值

```bash
$ echo threshold -30
OK
```

### 确认修改

```bash
$ cat threshold
-30
```

## 常用参数列表

### 效果图参数

#### DRC（动态范围压缩）
```bash
/audio/graph0/nodes/6_drc/
  ├── threshold    # 阈值 dB (-60~0)
  ├── ratio        # 压缩比 (1~20)
  ├── attack       # 启动时间 ms (1~500)
  └── release      # 释放时间 ms (10~2000)
```

#### Reverb（混响）
```bash
/audio/graph0/nodes/8_reverb/
  ├── room         # 房间大小 (0~100)
  ├── damp         # 阻尼 (0~100)
  └── wet          # 干湿比 (0~100)
```

#### EQ（均衡器）
```bash
/audio/graph0/nodes/7_eq/
  ├── band0        # 频段0增益 dB (-12~+12)
  ├── band1        # 频段1增益 dB (-12~+12)
  ├── ...
  └── band9        # 频段9增益 dB (-12~+12)
```

#### Expander（扩展器）
```bash
/audio/graph0/nodes/5_expander/
  ├── threshold    # 阈值 dB (-80~0)
  └── ratio        # 比率 (1~10)
```

### 节点通用参数

```bash
/audio/graph0/nodes/<node>/
  ├── enabled      # 启用 (0/1)
  ├── bypass       # 旁路 (0/1)
  └── type         # 类型 (只读)
```

### 驱动参数示例

#### LCD驱动
```bash
/driver/spi/st7735/
  └── brightness   # 亮度 (0~100)
```

#### 电池驱动
```bash
/driver/adc/battery/
  ├── refresh      # 刷新状态 (写1触发)
  ├── soc          # 电量 % (只读)
  └── voltage      # 电压 mV (只读)
```

#### USB CDC驱动
```bash
/driver/usb/cdc/
  ├── baudrate     # 波特率
  ├── flush        # 清空缓冲区 (写1触发)
  ├── rx_count     # 接收计数 (只读)
  └── tx_count     # 发送计数 (只读)
```

## 高级用法

### 1. 组合命令

```bash
# 快速查看和修改
$ cat threshold && echo threshold -30 && cat threshold
-20
OK
-30
```

### 2. 参数预设脚本

创建效果预设脚本：

```bash
# preset_guitar.sh
echo /audio/graph0/nodes/6_drc/threshold -25
echo /audio/graph0/nodes/6_drc/ratio 6
echo /audio/graph0/nodes/8_reverb/room 60
echo /audio/graph0/nodes/8_reverb/wet 30
echo /audio/graph0/nodes/7_eq/band0 3
echo /audio/graph0/nodes/7_eq/band1 6
```

### 3. 实时调试

```bash
# 边调整边听效果
$ cd /audio/graph0/nodes/8_reverb
$ while true; do
>   echo room 50
>   sleep 1
>   echo room 70
>   sleep 1
> done
```

## 注意事项

1. **参数范围**：确保输入的值在允许范围内
2. **立即生效**：大多数参数修改后立即生效
3. **只读参数**：某些参数（如type）不能修改
4. **路径格式**：使用正确的路径分隔符 `/`

## 故障排除

### 问题1：echo命令不存在

**症状**：
```bash
$ echo threshold -20
ERROR: Unknown command 'echo'
```

**解决**：
1. 确保已编译并烧录最新代码
2. 检查 `bg_shell_commands.c` 中是否注册了echo命令
3. 重启设备

### 问题2：无法写入参数

**症状**：
```bash
$ echo threshold -20
echo: threshold: Write error
```

**原因**：
- 参数只读
- 参数值超出范围
- 系统未初始化

**解决**：
1. 使用 `cat` 检查参数是否可读
2. 确认参数值在有效范围内
3. 检查效果图是否已挂载

### 问题3：修改不生效

**症状**：参数写入成功，但音频效果没变化

**原因**：
- 节点被bypass
- 节点未启用
- 效果图未运行

**解决**：
```bash
$ cat enabled
0
$ echo enabled 1

$ cat bypass
1
$ echo bypass 0
```

## 测试用例

### 测试1：基本读写

```bash
$ cd /audio/graph0/nodes/6_drc
$ cat threshold
-20
$ echo threshold -30
OK
$ cat threshold
-30
✅ 通过
```

### 测试2：参数范围

```bash
$ echo threshold -100
echo: threshold: Write error
✅ 正确拒绝超范围值
```

### 测试3：只读参数

```bash
$ echo type test
echo: type: Read-only parameter
✅ 正确拒绝只读参数
```

### 测试4：绝对路径

```bash
$ echo /audio/graph0/nodes/6_drc/threshold -25
OK
$ cat /audio/graph0/nodes/6_drc/threshold
-25
✅ 绝对路径工作正常
```

### 测试5：重定向语法

```bash
$ echo -20 > threshold
OK
$ cat threshold
-20
✅ 重定向语法工作正常
```

## 总结

`echo` 命令是修改VFS参数的核心工具，结合 `cat`、`ls`、`cd` 等命令，可以方便地：

- ✅ 实时调整效果图参数
- ✅ 配置驱动参数
- ✅ 创建参数预设脚本
- ✅ 进行交互式调试

**下一步**：编译并烧录代码，然后就可以使用完整的VFS命令行系统了！
