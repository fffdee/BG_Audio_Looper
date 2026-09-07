# RAM 优化方案

> 已完成：
> - 方案1（删除LCD相关代码）— LCD驱动本来就被 HW_DRV_LCD_EN=0 禁用，未实际释放RAM
> - **方案5（减小Effect Graph缓冲池粒度）— 已实施，节省~6.8KB**
>   - `EFFECT_GRAPH_BUFFER_SIZE` 从200降到128，`BT_DECODED_BUFFER_SIZE` 从256降到128
>   - 通过 `REVERB_RAM_OPTIMIZE` 宏开关控制，设0可撤回
>   - 128足够SBC单帧最大128样本和ADC帧48样本

## 背景

当前内部RAM可用约51KB，ReverbContext需要57.5KB，分配失败。
已通过减小Effect Graph缓冲池粒度释放~6.8KB，ReverbContext应可分配成功。

---

## 方案2：Effect Graph 节点缓冲池移至 PSRAM

**预计收益：约17.6KB**

Effect Graph 的节点缓冲池（`g_node_buf_pool[22][128]`）是在内部RAM中静态分配的大块缓冲区。
可将其移至PSRAM，由 `psram_malloc()` 分配。

**涉及文件：**
- `src/banux/05_component/effect_graph/effect_graph.c` - 缓冲池分配逻辑

**风险：** PSRAM通过SPI访问（非内存映射），Effect Graph是实时音频路径，延迟可能影响处理。
需实测确认PSRAM带宽是否满足需求。

---

## 方案3：提示音解码器缓冲区移至 PSRAM

**预计收益：约4KB**

提示音模块的WAV/MP3解码器缓冲区（`decoder_buf[4096]`）当前在内部RAM。
可改为PSRAM分配，播放结束后释放。

**涉及文件：**
- `src/banux/06_app/BG_AudioIO_Manager/bg_audio_io_manager.c` - `decoder_buf`

**风险：** 低。提示音解码非实时路径，PSRAM延迟可接受。

---

## 方案4：Reverb 延迟线移至 PSRAM

**预计收益：约50KB+（最大单项优化）**

ReverbContext 占用 57.5KB，其中绝大部分是延迟线缓冲区。
但 `reverb_apply()` 来自预编译库 `libAudioEffectLibrary.a`，需要直接指针访问延迟线，
而PSRAM是SPI接口不能内存映射，无法直接当RAM用。

**可行但复杂的方案：**
1. 每帧处理前将ReverbContext从PSRAM读入内部RAM临时缓冲区，处理后写回
2. 57KB SPI读写约2.3ms，超过1.09ms/帧的实时预算，音频会断续
3. 需要SDK提供可修改的reverb源码才能只拆分延迟线到PSRAM

**风险：** 高。实时性约束下无法直接实施，需要SDK支持。

---

## 方案6：Shell 历史缓冲区缩减

**预计收益：约1-2KB**

Shell模块的命令历史缓冲区和输出缓冲区占用一定RAM。

**涉及文件：**
- `src/banux/04_shell_commands/bg_shell.h` - `SHELL_CMD_MAX_LEN`, `SHELL_MODULE_MAX`, `SHELL_OUT_BUF_SIZE`

**风险：** 低。仅影响长命令/输出，功能无损。

---

## 实施优先级建议

| 优先级 | 方案 | 预计收益 | 风险 | 备注 |
|--------|------|----------|------|------|
| 1 | **方案5（已实施）**：减小缓冲池粒度 | ~6.8KB | 低 | REVERB_RAM_OPTIMIZE宏控制 |
| 2 | 方案2：Effect Graph缓冲池→PSRAM | ~11KB | 中 | 实时路径需实测 |
| 3 | 方案3：提示音解码缓冲→PSRAM | ~4KB | 低 | 非实时路径 |
| 4 | 方案6：Shell缓冲区缩减 | ~1-2KB | 低 | 简单快速 |
| 5 | 方案4：Reverb延迟线→PSRAM | ~50KB | 高 | 需SDK源码支持 |
