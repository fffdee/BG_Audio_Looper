# EQ Filter Params 深层修复说明

## 问题描述
用户报告：
- **mic_out_eq (ADC EQ)**: 开启超过2个节点就无声，关闭第3个频段能恢复
- **music_out_eq**: 设置一个频段就发出吱吱噪声，关闭也无法恢复

## 根本原因分析

### 问题1: filter_params是压缩数组
EQ系统有两个参数数组：
- **`eq_params[10]`** - 完整的10个频段配置（包括禁用的）
- **`filter_params[]`** - **压缩的**参数数组（只包含启用的频段）

底层`eq_configure_filters()`使用`filter_params`来配置滤波器系数。

**错误代码**：直接用band索引访问filter_params
```c
// 错误：如果只有3个频段启用，filter_params只有3个有效元素
target_eq->filter_params[band].gain = ...;  // band可能是5、6、7等，越界！
```

### 问题2: filter_count计算方式错误
**正确做法**（参考`communication.c`）：
```c
p->filter_count = 0;  // 先重置为0
for (i = 0; i < 10; i++) {
    if (p->eq_params[i].enable) {
        p->filter_params[p->filter_count].Q = p->eq_params[i].Q;
        // ... 复制其他参数
        p->filter_count++;  // 递增计数
    }
}
```

**我们之前的错误**：
- filter_count没有重置为0
- filter_count = max_enabled_band + 1（错误逻辑）

### 问题3: 使用了错误的配置函数
- **`AudioEffectEQFilterConfig()`** - 只配置滤波器系数
- **`AudioEffectEQFilterClearBufConfig()`** - **清除delay buffer** + 配置滤波器

`communication.c`中使用的是`ClearBuf`版本，因为**不清除delay buffer会导致噪声或异常**！

## 修复方案

### 1. 创建统一的EQ重建辅助函数
```c
static void RebuildAndApplyEQFilter(EQUnit *target_eq, EffectNode_t *node)
{
    // 关键步骤1: 重置filter_count为0
    target_eq->filter_count = 0;
    
    // 关键步骤2: 遍历所有频段，重建压缩的filter_params数组
    for (i = 0; i < 10; i++) {
        if (target_eq->eq_params[i].enable) {
            target_eq->filter_params[target_eq->filter_count].Q = ...;
            target_eq->filter_count++;
        }
    }
    
    // 关键步骤3: 调用AudioEffectEQFilterClearBufConfig
    AudioEffectEQFilterClearBufConfig(target_eq, sample_rate);
}
```

### 2. 修改所有EQ参数处理使用此辅助函数

## 修改文件
1. **[shell_cmd_graph.c](BanBox/src/banux/05_component/effect_graph/shell_cmd_graph.c)**
   - 添加`RebuildAndApplyEQFilter()`辅助函数
   - 修改所有EQ band参数处理使用新函数
   - 使用`AudioEffectEQFilterClearBufConfig`替代`AudioEffectEQFilterConfig`

2. **[chain_graph_apply.c](BanBox/src/banux/05_component/effect_graph/chain_graph_apply.c)**
   - 修改图应用时的EQ同步
   - 正确重置并累加filter_count
   - 使用`AudioEffectEQFilterClearBufConfig`

## 关键修复点总结
| 问题 | 错误做法 | 正确做法 |
|------|---------|---------|
| filter_params索引 | 直接用band索引 | 用压缩后的filter_count作为索引 |
| filter_count计算 | max_band + 1 | 循环累加enable的频段数 |
| 配置函数 | AudioEffectEQFilterConfig | AudioEffectEQFilterClearBufConfig |
| filter_count初始化 | 不重置 | 先重置为0再累加 |

## 测试建议
1. ✓ mic_out_eq 开启3个以上频段，验证无声问题解决
2. ✓ music_out_eq 添加频段，验证噪声问题解决
3. ✓ 频段enable/disable快速切换
4. ✓ 参数实时修改无异常

## 日期
2026-02-02 (深层修复)
