# 效果器参数调节 - 快速参考

## 常用命令速查表

```bash
# 列出所有效果器
effect list

# 查看效果器信息
effect info 0    # Reverb
effect info 1    # DRC (Mic)
effect info 3    # Expander

# 获取参数值
effect get 1 threshold
effect get 1 ratio

# 设置参数值
effect set 1 threshold -25
effect set 1 ratio 6

# 启用/禁用效果器
effect enable 1 on
effect enable 4 off

# 帮助
effect help
```

## 效果器编号速查

| 编号 | 效果器 | 快速调节命令 |
|:----:|--------|-------------|
| 0 | 混响 (Reverb) | `effect enable 0 on` |
| 1 | DRC (麦克风) | `effect set 1 threshold -25` |
| 2 | EQ (麦克风) | `effect set 2 band0 3` |
| 3 | 扩展器 | `effect set 3 threshold -60` |
| 4 | 回声 | `effect enable 4 on` |
| 5 | 啸叫抑制 | `effect enable 5 on` |
| 6 | 3D音效 | `effect enable 6 on` |
| 7 | 虚拟低音 | `effect enable 7 on` |
| 8 | 板式混响 | `effect enable 8 on` |
| 9 | DRC (音乐) | `effect set 9 threshold -20` |
| 10 | EQ (音乐) | `effect set 10 band0 5` |

## DRC (效果器1/9) 参数调节

```bash
# 基础调节
effect set 1 threshold -25      # 阈值 (越低越容易被压缩)
effect set 1 ratio 4            # 压缩比 (越大压缩越多)
effect set 1 attack 10          # 启动时间 (ms, 越小反应越快)
effect set 1 release 100        # 释放时间 (ms, 越大释放越慢)

# 常用预设
# 轻度压缩
effect set 1 threshold -20 && effect set 1 ratio 2 && effect set 1 attack 5 && effect set 1 release 50

# 中度压缩
effect set 1 threshold -25 && effect set 1 ratio 4 && effect set 1 attack 10 && effect set 1 release 100

# 重度压缩
effect set 1 threshold -30 && effect set 1 ratio 8 && effect set 1 attack 2 && effect set 1 release 200
```

## EQ (效果器2/10) 参数调节

```bash
# 频段定义 (近似)
# band0: 低频 (100Hz)
# band1-2: 低中频 (250Hz-500Hz)
# band3-5: 中频 (1kHz-4kHz)
# band6-8: 中高频 (8kHz-12kHz)
# band9: 高频 (16kHz+)

# 亮化音音质
effect set 2 band9 5            # 高频 +5dB
effect set 2 band6 2            # 中高频 +2dB

# 温暖音色
effect set 2 band0 3            # 低频 +3dB
effect set 2 band1 2            # 低中频 +2dB

# 清晰人声
effect set 2 band3 3            # 中频 +3dB
effect set 2 band0 -2           # 低频 -2dB

# 重低音
effect set 2 band0 8            # 低频 +8dB
effect set 2 band9 -3           # 高频 -3dB
```

## Expander (效果器3) 参数调节

```bash
# 基础设置
effect set 3 threshold -60      # 阈值 (低于此阈值信号被扩展)
effect set 3 ratio 2            # 扩展比 (信号有多少被扩展)

# 提高清晰度
effect set 3 threshold -55
effect set 3 ratio 3
```

## 混响 (效果器0) 参数调节

```bash
# 基础调节
effect set 0 room 50            # 房间大小 (0-100)
effect set 0 damp 70            # 阻尼 (0-100)
effect set 0 wet 30             # 干湿比 (0-100, 越小越干)

# 小房间混响
effect set 0 room 25 && effect set 0 damp 80 && effect set 0 wet 20

# 大厅混响
effect set 0 room 75 && effect set 0 damp 50 && effect set 0 wet 40

# 轻微混响
effect set 0 room 40 && effect set 0 damp 70 && effect set 0 wet 15
```

## 问题诊断命令序列

### 诊断1: 音频失真

```bash
# 1. 查看DRC状态
effect info 1
effect get 1 threshold
effect get 1 ratio

# 2. 尝试调节
effect set 1 threshold -30
effect set 1 ratio 6

# 3. 查看扩展器
effect info 3
effect set 3 threshold -60
```

### 诊断2: 音量太小

```bash
# 1. 检查EQ
effect info 2
effect set 2 band3 3            # 提升中频

# 2. 检查Reverb
effect info 0
effect enable 0 off             # 临时关闭混响
```

### 诊断3: 回声/残响问题

```bash
# 1. 禁用Reverb测试
effect enable 0 off

# 2. 禁用Echo测试
effect enable 4 off

# 3. 逐一启用以找出问题源
effect enable 0 on
effect enable 4 on
```

## 参数值对照表

### DRC Threshold (dB)
```
-60 dB  : 极低，几乎压缩所有声音
-40 dB  : 低，压缩大部分声音
-25 dB  : 中等，推荐值
-15 dB  : 高，仅压缩最大声音
-10 dB  : 极高，只处理极端情况
```

### DRC Ratio
```
1:1   : 无压缩
2:1   : 轻度压缩
4:1   : 中度压缩
8:1   : 重度压缩
16:1+ : 极端压缩（几乎相当于限幅）
```

### Attack Time (ms)
```
0-5 ms    : 非常快，可能有"pumping"效果
5-20 ms   : 快速反应
20-100 ms : 标准
100+ ms   : 缓慢反应
```

### Release Time (ms)
```
50 ms     : 快速释放
100 ms    : 标准
200-500 ms: 缓慢释放（音乐用）
1000+ ms  : 极慢释放
```

## 高级用法

### 组合多个效果器

```bash
# 启用完整效果链：DRC -> EQ -> Reverb
effect enable 1 on          # DRC
effect enable 2 on          # EQ
effect enable 0 on          # Reverb

# 调节参数
effect set 1 threshold -25
effect set 2 band3 3
effect set 0 wet 25
```

### A/B对比测试

```bash
# 打开配置A
effect set 1 threshold -20
effect set 1 ratio 2
effect set 0 wet 20

# 记录效果...

# 打开配置B
effect set 1 threshold -30
effect set 1 ratio 6
effect set 0 wet 40

# 对比效果...
```

## 注意事项

1. **实时调节**：修改在数毫秒内生效
2. **参数范围**：超出范围的值可能被截断或导致错误
3. **效果器顺序**：效果器的处理顺序是固定的（ADC -> Effects -> DAC）
4. **CPU占用**：启用更多效果器会增加CPU占用，监控 `sysmon -c`
5. **持久化**：参数修改在系统重启后丢失（除非实现了Flash保存）

## 常见配置预设

### 直播/卡拉OK
```bash
effect enable 1 on && effect set 1 threshold -25 && effect set 1 ratio 4
effect enable 2 on && effect set 2 band3 5
effect enable 0 on && effect set 0 wet 25
```

### 音乐播放
```bash
effect enable 9 on && effect set 9 threshold -20 && effect set 9 ratio 2
effect enable 10 on && effect set 10 band0 3 && effect set 10 band9 2
effect enable 8 on
```

### 清晰通话
```bash
effect enable 1 on && effect set 1 threshold -25 && effect set 1 ratio 6
effect enable 3 on && effect set 3 threshold -60
effect enable 2 on && effect set 2 band3 5
effect enable 0 off
```

---

**更多信息请参考：** [EFFECT_PARAMS_GUIDE.md](./EFFECT_PARAMS_GUIDE.md)
