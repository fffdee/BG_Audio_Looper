# Effect Graph 集成修复总结

## 修复日期
2026年1月4日

## 问题分析

### 编译错误清单
1. **API 名称不匹配**
   - ❌ `EffectGraph_LoadPreset` 
   - ✅ `EffectGraphConfig_LoadPreset`

2. **常量名称错误**
   - ❌ `PRESET_DEFAULT`
   - ✅ `GRAPH_PRESET_DEFAULT`

3. **Shell 命令注册函数错误**
   - ❌ `ShellCmd_RegisterGraphCommands()`
   - ✅ `ShellCmd_GraphRegister()`

4. **类型名称错误**
   - ❌ `EffectGraphNode*`
   - ✅ `EffectNode_t*`

5. **查找函数错误**
   - ❌ `EffectGraph_FindNode()`
   - ✅ `EffectGraph_FindNodeByName()`

6. **节点成员访问错误**
   - ❌ `node->source_callback`
   - ❌ `node->sink_callback`
   - ✅ `node->func.source`
   - ✅ `node->func.sink`

7. **回调函数签名不匹配**
   - ❌ 原签名: `int32_t func(void* user_data, int16_t* buffer, uint32_t frame_count)`
   - ✅ 正确签名:
     - Source: `uint16_t func(EffectNode_t *node, int32_t *out_buf, uint16_t max_len)`
     - Sink: `void func(EffectNode_t *node, int32_t *in_buf, uint16_t len)`

8. **蓝牙解码函数调用错误**
   - ❌ `audio_decoder_decode(buffer, len, handle)`
   - ✅ `audio_decoder_decode()` (无参数)

## 修复内容

### 1. bg_audio_io_manager.c - 函数声明修复

```c
// 修复前
static int32_t ADC0_ReadGuitarData(void* user_data, int16_t* buffer, uint32_t frame_count);

// 修复后
static uint16_t ADC0_ReadGuitarData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len);
```

### 2. BG_audio_Init() 函数修复

```c
// 修复前
if (EffectGraph_LoadPreset(PRESET_DEFAULT) != 0) {
    ...
}
ShellCmd_RegisterGraphCommands();

// 修复后
if (EffectGraphConfig_LoadPreset(GRAPH_PRESET_DEFAULT) != 0) {
    ...
}
ShellCmd_GraphRegister();
```

### 3. SetupEffectGraphCallbacks() 函数修复

```c
// 修复前
EffectGraphNode* node = NULL;
node = EffectGraph_FindNode("guitar_in");
if (node) {
    node->source_callback = ADC0_ReadGuitarData;
}

// 修复后
EffectNode_t* node = NULL;
node = EffectGraph_FindNodeByName("guitar_in");
if (node) {
    node->func.source = ADC0_ReadGuitarData;
}
```

### 4. 回调函数实现修复

#### ADC0_ReadGuitarData
```c
// 修复前
static int32_t ADC0_ReadGuitarData(void* user_data, int16_t* buffer, uint32_t frame_count)
{
    AudioADC_DataGet(ADC0_MODULE, (void*)buffer, samples_to_read);
    return (int32_t)(samples_to_read / 2);
}

// 修复后
static uint16_t ADC0_ReadGuitarData(EffectNode_t *node, int32_t *out_buf, uint16_t max_len)
{
    uint32_t temp_buf[256];
    AudioADC_DataGet(ADC0_MODULE, temp_buf, samples_to_read);
    
    // 转换为 int32_t 格式
    for (i = 0; i < samples_to_read; i++) {
        out_buf[i] = (int32_t)temp_buf[i];
    }
    return samples_to_read;
}
```

#### DAC0_WriteSpeakerData
```c
// 修复前
static int32_t DAC0_WriteSpeakerData(void* user_data, const int16_t* buffer, uint32_t frame_count)
{
    AudioDAC_DataSet(DAC0, (void*)buffer, samples_to_write);
    return (int32_t)(samples_to_write / 2);
}

// 修复后
static void DAC0_WriteSpeakerData(EffectNode_t *node, int32_t *in_buf, uint16_t len)
{
    uint32_t temp_buf[256];
    
    // 转换为 uint32_t 格式
    for (i = 0; i < samples_to_write; i++) {
        temp_buf[i] = (uint32_t)in_buf[i];
    }
    
    AudioDAC_DataSet(DAC0, temp_buf, samples_to_write);
}
```

#### BT_ReadAudioData
```c
// 修复前
int32_t decoded_samples = audio_decoder_decode(
    (int16_t*)buffer, 
    frame_count * 2,
    &SBC_MemHandle
);

// 修复后
if (audio_decoder_decode() != RT_SUCCESS) {
    return 0;
}

// 然后从 audio_decoder->song_info 获取 PCM 数据
pcm_data = audio_decoder->song_info->pcm_addr;
pcm_len = audio_decoder->song_info->pcm_data_length;
```

## 关键修改点总结

### 1. 数据类型转换
- Effect Graph 内部使用 `int32_t` 数据格式
- 硬件接口使用 `uint32_t` 数据格式
- 需要在回调函数中进行类型转换

### 2. 函数签名规范
| 节点类型 | 函数签名 | 返回值 |
|---------|---------|--------|
| Source | `uint16_t func(EffectNode_t*, int32_t*, uint16_t)` | 实际读取的样本数 |
| Sink | `void func(EffectNode_t*, int32_t*, uint16_t)` | 无 |
| Process | `void func(EffectNode_t*, int32_t**, uint8_t, int32_t*, uint16_t)` | 无 |

### 3. API 命名规范
| 模块 | 前缀 | 示例 |
|------|------|------|
| Effect Graph 核心 | `EffectGraph_` | `EffectGraph_Init()` |
| Effect Graph 配置 | `EffectGraphConfig_` | `EffectGraphConfig_LoadPreset()` |
| Shell 命令 | `ShellCmd_Graph` | `ShellCmd_GraphRegister()` |

### 4. 节点访问方式
```c
// 联合体成员访问
typedef union {
    NodeSourceFunc_t  source;     // 源节点
    NodeSinkFunc_t    sink;       // 输出节点
    NodeProcessFunc_t process;    // 处理节点
} NodeFunc_t;

// 使用方式
node->func.source = my_source_func;
node->func.sink = my_sink_func;
node->func.process = my_process_func;
```

## 编译验证

修复后应该解决以下编译错误：
- ✅ implicit declaration 警告
- ✅ undeclared identifier 错误
- ✅ incompatible pointer type 警告
- ✅ too many arguments 错误
- ✅ unknown type name 错误
- ✅ request for member in something not a structure 错误

## 后续注意事项

1. **数据格式一致性**
   - Effect Graph 内部统一使用 int32_t
   - 与硬件接口交互时需要类型转换

2. **缓冲区大小限制**
   - 临时缓冲区设置为 256 样本
   - 避免栈溢出风险

3. **错误处理**
   - Source 函数返回 0 表示无数据
   - Sink 函数无返回值，静默失败

4. **性能优化**
   - 减少不必要的数据拷贝
   - 考虑使用 DMA 直接传输（未来优化）

## 相关文档
- `EFFECT_GRAPH_README.md` - Effect Graph 使用说明
- `EFFECT_GRAPH_INTEGRATION_GUIDE.md` - 集成指南
- `C89_FIX_SUMMARY.md` - C89 语法修复总结
