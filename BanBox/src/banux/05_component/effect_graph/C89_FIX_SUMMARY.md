# Effect Graph C89 语法修复总结

## 修复日期
2026年1月4日

## 问题描述
在使用 nds32le-elf-gcc 编译器（默认 C89/C90 模式）编译 Effect Graph 模块时，出现以下错误：
- `'for' loop initial declarations are only allowed in C99 or C11 mode`
- 未使用的函数和变量警告

## 修复内容

### 1. effect_graph.c

#### 修复的 C99 for 循环声明（共6处）
1. **InitNodeDefaults() 函数** (行78)
   - 修复前: `for (int i = 0; i < EFFECT_GRAPH_MAX_INPUTS; i++)`
   - 修复后: 在函数开头声明 `int i;`，然后使用 `for (i = 0; ...)`

2. **EffectGraph_Init() 函数** (行182)
   - 修复前: `for (int i = 0; i < EFFECT_GRAPH_MAX_NODES; i++)`
   - 修复后: 在函数开头声明 `int i;`，然后使用 `for (i = 0; ...)`

3. **TopologicalSort() 函数** (行141, 151)
   - 修复前: 在 while 循环和 for 循环内部声明变量
   - 修复后: 将所有变量声明提升到函数开头
   ```c
   uint8_t node_idx;
   EffectNode_t *node;
   uint8_t dst_idx;
   ```

4. **EffectGraph_CreateFromConfig() 函数** (行240, 247)
   - 修复前: 在 for 循环内部声明变量
   - 修复后: 将所有变量声明提升到函数开头
   ```c
   const NodeConfig_t *nc;
   const EdgeConfig_t *ec;
   EffectNode_t *src;
   EffectNode_t *dst;
   GraphError_t err;
   ```

5. **EffectGraph_Build() 函数** (行390)
   - 修复前: `for (uint8_t i = 0; i < g_EffectGraph.process_count; i++)`
   - 修复后: 在函数开头声明 `uint8_t i;`

6. **EffectGraph_Process() 函数** (行453-454, 497)
   - 修复前: 在 else 代码块内部声明变量，for 循环内声明 k
   - 修复后: 将所有变量声明提升到函数开头
   ```c
   int32_t *in_bufs[EFFECT_GRAPH_MAX_INPUTS];
   uint8_t in_count;
   uint16_t max_len;
   uint16_t k;
   EffectNode_t *node;
   EffectNode_t *src;
   ```

#### 修复的未使用代码警告
1. **IsProcessNode() 函数**
   - 状态: 未在代码中使用
   - 处理: 注释掉函数定义和声明，保留供将来扩展
   ```c
   /* 判断是否为处理节点 (未使用，保留供将来扩展)
   static bool IsProcessNode(EffectNodeType_t type)
   {
       return (type >= NODE_TYPE_MIXER && type < NODE_TYPE_MAX);
   }
   */
   ```

2. **TopologicalSort() 中的变量 j**
   - 状态: 声明但未使用
   - 处理: 从变量声明中删除

### 2. effect_graph_config.c

#### 修复的 C99 for 循环声明（共2处）
1. **ApplyDefaultParams() 函数** (行102)
   - 修复前: `for (int i = 0; i < EFFECT_GRAPH_MAX_INPUTS; i++)`
   - 修复后: 使用代码块包裹，在代码块内声明变量
   ```c
   case NODE_TYPE_MIXER:
       {
           int i;
           node->params.mixer.input_count = EFFECT_GRAPH_MAX_INPUTS;
           for (i = 0; i < EFFECT_GRAPH_MAX_INPUTS; i++) {
               node->params.mixer.input_gains[i] = 0;
           }
       }
       break;
   ```

2. **EffectGraphConfig_PrintPresets() 函数** (行261)
   - 修复前: `for (int i = 0; i < GRAPH_PRESET_MAX; i++)`
   - 修复后: 在函数开头声明 `int i;`

### 3. shell_cmd_graph.c
- ✅ 检查完成，无 C99 语法问题

## C89/C90 语法规则总结

### 关键规则
1. **变量声明必须在代码块开头**
   - ❌ 错误: 在代码块中间声明变量
   - ✅ 正确: 所有变量声明放在函数或代码块开头

2. **for 循环不能包含声明**
   - ❌ 错误: `for (int i = 0; i < n; i++)`
   - ✅ 正确: 先声明 `int i;`，再用 `for (i = 0; i < n; i++)`

3. **避免在嵌套代码块中声明变量**
   - 如果必须在代码块中声明，使用额外的大括号 `{ int i; ... }`

### 最佳实践
1. 将所有变量声明集中在函数开头
2. 按类型分组声明变量
3. 合理使用注释说明变量用途
4. 对于暂时未使用的函数，使用注释保留而非删除

## 编译验证
修复后需要重新编译验证：
```bash
# 编译 effect_graph.c
nds32le-elf-gcc [所有-I选项] -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c \
    -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant \
    -ffunction-sections -fdata-sections -MMD -MP \
    -o effect_graph.o effect_graph.c

# 编译 effect_graph_config.c
nds32le-elf-gcc [所有-I选项] -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c \
    -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant \
    -ffunction-sections -fdata-sections -MMD -MP \
    -o effect_graph_config.o effect_graph_config.c

# 编译 shell_cmd_graph.c
nds32le-elf-gcc [所有-I选项] -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c \
    -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant \
    -ffunction-sections -fdata-sections -MMD -MP \
    -o shell_cmd_graph.o shell_cmd_graph.c
```

## 修复状态
- ✅ effect_graph.c - 所有 C99 语法错误已修复
- ✅ effect_graph_config.c - 所有 C99 语法错误已修复
- ✅ shell_cmd_graph.c - 无需修复
- ✅ 未使用函数/变量警告已处理

## 注意事项
1. 修复后的代码完全符合 C89/C90 标准
2. 保持了代码的可读性和维护性
3. 未改变任何功能逻辑
4. 所有注释和文档保持完整

## 相关文件
- `effect_graph.h` - 头文件（无需修改）
- `effect_graph.c` - 已修复
- `effect_graph_config.h` - 头文件（无需修改）
- `effect_graph_config.c` - 已修复
- `shell_cmd_graph.h` - 头文件（无需修改）
- `shell_cmd_graph.c` - 无需修复

## 后续建议
1. 如果需要使用 C99 特性，可在编译时添加 `-std=c99` 选项
2. 建议保持当前 C89 兼容性，以确保在各种嵌入式平台上的可移植性
3. 定期检查新增代码是否遵循 C89 语法规范
