# Query命令功能汇总

## 概述

为了方便APP到特定页面获取参数，已为所有主要模块添加了JSON格式的query命令。

## 已实现的Query命令

### 1. Graph模块 (效果图)

**命令格式**: `graph query <target>`

**支持的target**:
- `all` - 查询所有节点
- `node <id>` - 查询单个节点
- `volume` - 查询音量参数
- `system` - 查询系统参数
- `eq` - 查询EQ详细参数

**示例**:
```bash
graph query all
graph query node 7
graph query volume
```

**响应格式**: JSON
```json
{"status":"ok","node_count":14,"nodes":[...]}
```

---

### 2. Param模块 (系统参数)

**命令格式**: `param -q <target>` 或 `param query <target>`

**支持的target**:
- `all` / `system` - 查询系统启动参数
- `volume` - 查询音量设置
- `looper` - 查询循环器参数
- `bluetooth` - 查询蓝牙设置
- `lcd` - 查询LCD设置

**示例**:
```bash
param -q system
param -q volume
param -q looper
param -q bluetooth
param -q lcd
```

**响应示例**:
```json
// system
{"status":"ok","system":{"boot_count":5,"current_boot_status":1}}

// volume
{"status":"ok","volume":{"mic1":80,"mic2":75,"guitar1":70,"guitar2":70,"output":85}}

// looper
{"status":"ok","looper":{"loop_count":4,"overdub_mode":1,"quantize":1,"click_volume":50,"tempo":120,"time_signature":4,"fade_time":100,"max_loop_time":300000}}

// bluetooth
{"status":"ok","bluetooth":{"enabled":1,"discoverable":1,"auto_connect":1,"a2dp_volume":80,"device_name":"BG_Audio"}}

// lcd
{"status":"ok","lcd":{"contrast":50,"color_scheme":0,"screen_saver":1,"bg_color":0}}
```

---

### 3. Effect模块 (效果器)

**命令格式**: `effect query <id>`

**支持的效果器ID**:
- 0: REVERB (混响)
- 1: DRC (动态范围压缩)
- 2: EXPANDER (扩展器)
- 3: ECHO (回声)
- 4: EQ (均衡器-麦克风)
- 5: MUSIC_EQ (均衡器-音乐)
- 6: PLATE_REVERB (板式混响)

**示例**:
```bash
effect query 0  # 查询混响参数
effect query 1  # 查询DRC参数
effect query 3  # 查询回声参数
```

**响应示例**:
```json
// REVERB (id=0)
{"status":"ok","effect_id":0,"params":{"enable":1,"room":512,"damp":256,"wet":128}}

// DRC (id=1)
{"status":"ok","effect_id":1,"params":{"enable":1,"threshold":-2048,"ratio":4096,"attack":100,"release":500}}

// ECHO (id=3)
{"status":"ok","effect_id":3,"params":{"enable":1,"delay":24000,"feedback":512}}

// EQ (id=4)
{"status":"ok","effect_id":4,"params":{"enable":1,"filter_count":5}}

// PLATE_REVERB (id=6)
{"status":"ok","effect_id":6,"params":{"enable":1,"predelay":50,"diffusion":512,"decay":1024,"damping":256,"wetdrymix":128}}
```

---

### 4. UI模块 (界面状态)

**命令格式**: `ui -q` 或 `ui query`

**返回内容**:
- current_state: 当前UI状态
- previous_state: 上一个UI状态
- ready: 是否就绪

**UI状态值**:
- 0: BOOT (启动中)
- 1: IDLE (空闲)
- 2: MENU (菜单)
- 3: LOOPER (循环器)
- 4: SETTINGS (设置)

**示例**:
```bash
ui -q
```

**响应示例**:
```json
{"status":"ok","ui":{"current_state":2,"previous_state":1,"ready":true}}
```

---

### 5. Sysmon模块 (系统监控)

**命令格式**: `sysmon -q` 或 `sysmon query`

**返回内容**:
- memory: 内存使用情况
  - free: 当前空闲内存
  - min_free: 历史最小空闲内存
  - total: 总内存大小
  - used: 已使用内存
- tasks: 任务信息
  - count: 任务数量
  - tick_rate: 系统tick频率

**示例**:
```bash
sysmon -q
```

**响应示例**:
```json
{
  "status":"ok",
  "system":{
    "memory":{
      "free":45120,
      "min_free":38400,
      "total":65536,
      "used":20416
    },
    "tasks":{
      "count":8,
      "tick_rate":1000
    }
  }
}
```

---

## APP使用场景

### 场景1: 进入音量设置页面

```javascript
// APP进入音量页面时发送
ble.send("param -q volume\n");

// 收到响应
{
  "status":"ok",
  "volume":{
    "mic1":80,
    "mic2":75,
    "guitar1":70,
    "guitar2":70,
    "output":85
  }
}

// APP解析并显示滑块位置
```

### 场景2: 进入效果器页面

```javascript
// APP进入混响效果页面时发送
ble.send("effect query 0\n");

// 收到响应
{
  "status":"ok",
  "effect_id":0,
  "params":{
    "enable":1,
    "room":512,
    "damp":256,
    "wet":128
  }
}

// APP显示开关状态和参数旋钮
```

### 场景3: 进入循环器页面

```javascript
// APP进入循环器页面时发送
ble.send("param -q looper\n");

// 收到响应
{
  "status":"ok",
  "looper":{
    "loop_count":4,
    "overdub_mode":1,
    "quantize":1,
    "click_volume":50,
    "tempo":120,
    "time_signature":4,
    "fade_time":100,
    "max_loop_time":300000
  }
}

// APP显示所有循环器设置
```

### 场景4: 系统状态监控页面

```javascript
// APP进入系统监控页面时发送
ble.send("sysmon -q\n");

// 收到响应
{
  "status":"ok",
  "system":{
    "memory":{"free":45120,"total":65536,"used":20416},
    "tasks":{"count":8,"tick_rate":1000}
  }
}

// APP显示系统资源使用情况
```

---

## 实现细节

### 代码修改位置

1. **shell_cmd_param.c** (系统参数模块)
   - 新增 `param_query()` 函数
   - 支持6种查询目标: all/system/volume/looper/bluetooth/lcd

2. **shell_cmd_effect.c** (效果器模块)
   - 新增 `CmdQuery()` 函数
   - 支持7种效果器查询 (0-6)
   - 更新帮助信息

3. **shell_cmd_ui.c** (UI模块)
   - 新增 `ui_query()` 函数
   - 查询UI状态和就绪状态

4. **shell_cmd_sysmon.c** (系统监控模块)
   - 新增 `Opt_QueryJSON()` 函数
   - 查询内存和任务信息

5. **APP_COMMUNICATION_PROTOCOL.md** (协议文档)
   - 更新所有query命令说明
   - 添加详细的JSON响应示例

### 统一的错误处理

所有query命令遵循统一的错误格式:

```json
{"error":"错误描述"}
{"hint":"可用的选项提示"}
```

示例:
```json
{"error":"Unknown target: abc"}
{"hint":"Available: all, system, volume, looper, bluetooth, lcd"}
```

---

## 测试建议

### 1. 基本功能测试

通过BLE或CDC串口发送命令并验证响应:

```bash
# 测试param模块
param -q system
param -q volume
param -q looper

# 测试effect模块
effect query 0
effect query 1
effect query 3

# 测试ui模块
ui -q

# 测试sysmon模块
sysmon -q

# 测试graph模块
graph query all
graph query volume
```

### 2. 错误处理测试

```bash
# 无效的target
param -q invalid_target

# 无效的效果器ID
effect query 99

# 缺少参数
effect query
```

### 3. JSON格式验证

使用JSON解析工具验证所有响应都是有效的JSON格式。

### 4. BLE通信测试

- 验证响应能正确通过BLE发送
- 验证超过23字节的响应能正确分包
- 验证APP能完整接收并解析响应

---

## 未来扩展

如需为其他模块添加query功能，遵循以下模式:

1. 添加query函数:
```c
static int module_query(int argc, char *argv[]) {
    // 解析target参数
    const char *target = (argc >= 1) ? argv[0] : "all";
    
    // 输出JSON格式
    Shell_Printf("{\"status\":\"ok\",\"data\":{...}}\n");
    
    return 0;
}
```

2. 注册到ShellOpt_t数组:
```c
OPT("q", "query", "<target>", "Query params (JSON)", module_query)
```

3. 更新文档:
   - APP_COMMUNICATION_PROTOCOL.md
   - QUERY_COMMANDS_SUMMARY.md (本文档)

---

## 总结

✅ **已完成**: 为所有主要模块添加query命令
- param (6种查询目标)
- effect (7种效果器)
- ui (状态查询)
- sysmon (系统监控)
- graph (5种查询目标) [已存在]

✅ **统一格式**: 所有命令返回JSON格式
✅ **错误处理**: 统一的错误响应格式
✅ **文档更新**: 完整的协议文档和示例

🎯 **使用场景**: APP可根据当前页面发送对应query命令，实时获取下位机参数状态，无需主动轮询所有数据。
