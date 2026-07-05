# RAM 优化方案

> 已完成：方案1（删除LCD相关代码），释放约41KB+ RAM
> 以下为待实施优化方案，按预计收益排序

## 背景

当前内部RAM可用约51KB，ReverbContext需要57.5KB，分配失败。
删除LCD后内部RAM已释放大量空间，但仍有以下优化空间可进一步提升可用RAM。

---

## 方案2：Effect Graph 节点缓冲池移至 PSRAM

**预计收益：约17.6KB**

Effect Graph 的节点缓冲池（`node_buffer_pool`）是在内部RAM中静态分配的大块缓冲区，
用于各音频节点的输入/输出缓冲。可将其移至PSRAM，由 `psram_malloc()` 分配。

**涉及文件：**
- `src/banux/05_component/effect_graph/effect_graph.c` - 缓冲池分配逻辑
- `src/banux/05_component/effect_graph/effect_graph.h` - 缓冲池大小配置

**实施方式：**
1. 将 `node_buffer_pool` 数组改为通过 `psram_malloc()` 动态分配
2. 节点注册/连接时从PSRAM池分配缓冲区
3. 需确保PSRAM已初始化后再创建Effect Graph

**风险：** PSRAM访问延迟略高于内部RAM，但对音频节点缓冲（非实时路径）影响极小。

---

## 方案3：提示音解码器缓冲区移至 PSRAM

**预计收益：约4-8KB**

提示音模块（`remind_sound`）的WAV/MP3解码器缓冲区当前在栈/堆上分配。
可改为PSRAM分配，播放结束后释放。

**涉及文件：**
- `src/banux/02_device_drivers/remind_sound/remind_sound.c` - 解码缓冲区

**实施方式：**
1. `RemindSound_Start()` 时通过 `psram_malloc()` 分配解码缓冲区
2. `RemindSound_Stop()` 时释放
3. 解码输出的小帧PCM（1-2ms）仍在内部RAM，确保混音实时性

**风险：** 低。提示音解码非实时路径，PSRAM延迟可接受。

---

## 方案4：Reverb 延迟线移至 PSRAM

**预计收益：约50KB+（最大单项优化）**

ReverbContext 占用 57.5KB，其中绝大部分是延迟线缓冲区（delay lines）。
这是RAM不足的直接原因。可将延迟线分配到PSRAM。

**涉及文件：**
- `src/banux/06_app/audio/audio_effect.c` - Reverb 初始化分配
- `src/banux/06_app/audio/ctrlvars.h` - ReverbContext 结构定义

**实施方式：**
1. 将 `ReverbContext` 中的延迟线缓冲区指针改为PSRAM地址
2. `osPortMallocFromEnd(REVERB_SIZE)` 改为 `psram_malloc(REVERB_SIZE)`
3. 或将延迟线拆分为独立PSRAM分配，结构体本身留在内部RAM

**风险：** 中。Reverb处理是实时音频路径，PSRAM延迟可能影响处理时序。
需实测确认PSRAM带宽是否满足44.1kHz立体声处理需求。
若有问题，可考虑将最常用的前几条延迟线保留在内部RAM。

---

## 方案5：减小 Effect Graph 缓冲区粒度

**预计收益：约2-4KB**

当前Effect Graph节点缓冲可能使用较大的帧大小（如256样本/帧）。
减半帧大小可降低缓冲区占用，但会增加处理频率。

**涉及文件：**
- `src/banux/05_component/effect_graph/effect_graph.h` - `EFFECT_GRAPH_FRAME_SIZE`

**实施方式：**
1. 将 `EFFECT_GRAPH_FRAME_SIZE` 从256减至128或64
2. 相应调整所有节点的处理回调

**风险：** 中。更小的帧大小意味着更频繁的中断/处理调用，可能增加CPU负载。

---

## 方案6：Shell 历史缓冲区缩减

**预计收益：约1-2KB**

Shell模块的命令历史缓冲区和输出缓冲区占用一定RAM。

**涉及文件：**
- `src/banux/04_shell_commands/bg_shell.h` - `SHELL_CMD_MAX_LEN`, `SHELL_MODULE_MAX`, `SHELL_OUT_BUF_SIZE`
- `src/banux/04_shell_commands/bg_shell.c` - `g_History`, `g_OutBuf`

**实施方式：**
1. `SHELL_CMD_MAX_LEN`: 128→64
2. `SHELL_OUT_BUF_SIZE`: 256→128
3. `SHELL_MODULE_MAX`: 40→25

**风险：** 低。仅影响长命令/输出，功能无损。

---

## 实施优先级建议

| 优先级 | 方案 | 预计收益 | 风险 | 备注 |
|--------|------|----------|------|------|
| 1 | 方案4：Reverb延迟线→PSRAM | ~50KB | 中 | 解决Reverb分配失败的根本方案 |
| 2 | 方案2：Effect Graph缓冲池→PSRAM | ~17.6KB | 低 | 大块静态缓冲移出 |
| 3 | 方案3：提示音解码缓冲→PSRAM | ~4-8KB | 低 | 非实时路径 |
| 4 | 方案6：Shell缓冲区缩减 | ~1-2KB | 低 | 简单快速 |
| 5 | 方案5：Effect Graph帧大小缩减 | ~2-4KB | 中 | 需全面测试 |

**推荐路径：** 先实施方案4（Reverb→PSRAM）解决核心问题，再按需实施方案2/3进一步优化。
