# APP通信协议说明

## 概述

本文档描述了APP与下位机之间的通信格式和命令协议，用于参数同步和远程控制。

## 通信方式

- **接口**: BLE SPP (Bluetooth Low Energy Serial Port Profile)
- **MTU**: 23字节（数据自动分包）
- **格式**: 文本命令 + JSON响应

## 修复记录

### Bug 1: 烧录完第一次开机不使用默认参数

**问题**: 烧录完第一次开机使用flash的空参数而不是默认值

**原因**: `SysParam_Init()` 加载flash参数后，没有应用到音频系统 (`gCtrlVars`)

**解决方案**:
1. 在 `sys_param.c` 中添加 `SysParam_ApplyToAudio()` 函数
2. 在 `main.c` 的初始化流程中调用该函数

```c
// main.c
SysParam_Status_t param_status = SysParam_Init();
SysParam_ApplyToAudio();  // 应用参数到音频系统
```

### Bug 2: APP命令行接收异常

**问题**: APP发送命令后无法收到返回信息

**原因**: BLE接收数据和Shell处理是异步的，Shell可能使用CDC接口发送响应

**解决方案**: 在 `ShellIO_BLE_OnDataReceived()` 中：
1. 强制切换到BLE IO接口
2. 立即处理命令
3. 确保响应通过BLE发送

```c
// shell_io_ble.c
void ShellIO_BLE_OnDataReceived(uint8_t *data, uint16_t len) {
    // ... 数据放入缓冲区 ...
    ShellIOManager_UpdateActivity(SHELL_IO_BLE);
    ShellIOManager_SwitchIO(SHELL_IO_BLE);  // 强制切换
    Shell_Process();  // 立即处理
}
```

### Bug 3: APP与下位机通信格式

**新增功能**: JSON格式查询命令

## 命令格式

### 查询命令 (JSON响应)

#### Graph模块 (效果图命令)

| 命令 | 说明 | 示例响应 |
|------|------|----------|
| `graph query all` | 查询所有节点参数 | `{"status":"ok","node_count":14,"nodes":[...]}` |
| `graph query node <id>` | 查询单个节点 | `{"status":"ok","node":{...}}` |
| `graph query volume` | 查询音量参数 | `{"status":"ok","volume":{...}}` |
| `graph query system` | 查询系统参数 | `{"status":"ok","system":{...}}` |
| `graph query eq` | 查询EQ详细参数 | `{"status":"ok","eq":{...}}` |

#### Param模块 (系统参数)

| 命令 | 说明 | 示例响应 |
|------|------|----------|
| `param -q all` | 查询所有系统参数 | `{"status":"ok","system":{...}}` |
| `param -q system` | 查询系统启动参数 | `{"status":"ok","system":{"boot_count":5,"current_boot_status":1}}` |
| `param -q volume` | 查询音量设置 | `{"status":"ok","volume":{"mic1":80,"mic2":75,...}}` |
| `param -q looper` | 查询循环器参数 | `{"status":"ok","looper":{"loop_count":4,"overdub_mode":1,...}}` |
| `param -q bluetooth` | 查询蓝牙设置 | `{"status":"ok","bluetooth":{"enabled":1,"discoverable":1,...}}` |
| `param -q lcd` | 查询LCD设置 | `{"status":"ok","lcd":{"contrast":50,"color_scheme":0,...}}` |

#### Effect模块 (效果器)

| 命令 | 说明 | 示例响应 |
|------|------|----------|
| `effect query <id>` | 查询指定效果器参数 | `{"status":"ok","effect_id":0,"params":{...}}` |

支持的效果器ID：
- 0: REVERB (混响)
- 1: DRC (动态范围压缩)
- 2: EXPANDER (扩展器)
- 3: ECHO (回声)
- 4: EQ (均衡器-麦克风)
- 5: MUSIC_EQ (均衡器-音乐)
- 6: PLATE_REVERB (板式混响)

#### UI模块 (界面状态)

| 命令 | 说明 | 示例响应 |
|------|------|----------|
| `ui -q` | 查询UI状态 | `{"status":"ok","ui":{"current_state":2,"previous_state":1,"ready":true}}` |

UI状态值:
- 0: BOOT (启动中)
- 1: IDLE (空闲)
- 2: MENU (菜单)
- 3: LOOPER (循环器)
- 4: SETTINGS (设置)

#### Sysmon模块 (系统监控)

| 命令 | 说明 | 示例响应 |
|------|------|----------|
| `sysmon -q` | 查询系统运行状态 | `{"status":"ok","system":{"memory":{...},"tasks":{...}}}` |

### 参数设置命令

| 命令 | 说明 |
|------|------|
| `fx <id> <param> <value>` | 设置效果参数 |
| `graph set <id> <key> <val>` | 设置节点参数 |
| `param -s` | 保存参数到flash |

## JSON响应格式

### 节点参数 (graph query all)

```json
{
  "status": "ok",
  "node_count": 14,
  "nodes": [
    {
      "id": 0,
      "name": "adc0",
      "type": 0,
      "enabled": 1,
      "bypass": 0,
      "params": {}
    },
    {
      "id": 7,
      "name": "eq",
      "type": 7,
      "enabled": 1,
      "bypass": 0,
      "params": {
        "band_count": 5,
        "pregain": 0,
        "bands": [
          {"gain": 0, "type": 0, "f0": 100, "Q": 724, "en": 1},
          {"gain": 256, "type": 0, "f0": 500, "Q": 724, "en": 1}
        ]
      }
    }
  ]
}
```

### 音量参数 (graph query volume)

```json
{
  "status": "ok",
  "volume": {
    "mic1": 80,
    "mic2": 80,
    "guitar1": 80,
    "guitar2": 80,
    "output": 80
  }
}
```

### EQ参数 (graph query eq)

```json
{
  "status": "ok",
  "eq": {
    "mic_eq": {
      "enabled": 1,
      "band_count": 5,
      "pregain": 0,
      "bands": [...]
    },
    "music_eq": {
      "enabled": 1,
      "band_count": 5,
      "pregain": 0,
      "bands": [...]
    }
  }
}
```

### 系统参数 (param query system)

```json
{
  "status": "ok",
  "system": {
    "boot_count": 5,
    "current_boot_status": 1
  }
}
```

### 音量参数 (param query volume)

```json
{
  "status": "ok",
  "volume": {
    "mic1": 80,
    "mic2": 75,
    "guitar1": 70,
    "guitar2": 70,
    "output": 85
  }
}
```

### 循环器参数 (param query looper)

```json
{
  "status": "ok",
  "looper": {
    "loop_count": 4,
    "overdub_mode": 1,
    "quantize": 1,
    "click_volume": 50,
    "tempo": 120,
    "time_signature": 4,
    "fade_time": 100,
    "max_loop_time": 300000
  }
}
```

### 蓝牙参数 (param query bluetooth)

```json
{
  "status": "ok",
  "bluetooth": {
    "enabled": 1,
    "discoverable": 1,
    "auto_connect": 1,
    "a2dp_volume": 80,
    "device_name": "BG_Audio"
  }
}
```

### 效果器参数 (effect query <id>)

**混响效果 (id=0)**
```json
{
  "status": "ok",
  "effect_id": 0,
  "params": {
    "enable": 1,
    "room": 512,
    "damp": 256,
    "wet": 128
  }
}
```

**DRC动态压缩 (id=1)**
```json
{
  "status": "ok",
  "effect_id": 1,
  "params": {
    "enable": 1,
    "threshold": -2048,
    "ratio": 4096,
    "attack": 100,
    "release": 500
  }
}
```

**回声效果 (id=3)**
```json
{
  "status": "ok",
  "effect_id": 3,
  "params": {
    "enable": 1,
    "delay": 24000,
    "feedback": 512
  }
}
```

### UI状态 (ui query)

```json
{
  "status": "ok",
  "ui": {
    "current_state": 2,
    "previous_state": 1,
    "ready": true
  }
}
```

### 系统监控 (sysmon query)

```json
{
  "status": "ok",
  "system": {
    "memory": {
      "free": 45120,
      "min_free": 38400,
      "total": 65536,
      "used": 20416
    },
    "tasks": {
      "count": 8,
      "tick_rate": 1000
    }
  }
}
```

```json
{
  "status": "ok",
  "eq": {
    "adc": {
      "enable": 1,
      "filter_count": 5,
      "bands": [
        {"en": 1, "gain": 0, "f0": 100, "Q": 724, "type": 0},
        ...
      ]
    },
    "music": {
      "enable": 1,
      "filter_count": 0,
      "bands": [...]
    }
  }
}
```

## APP开发指南

### 同步流程

1. 连接BLE后发送 `graph query all\n`
2. 解析JSON响应获取所有节点状态
3. 发送 `graph query volume\n` 获取音量
4. 发送 `graph query eq\n` 获取EQ详细配置

### 参数修改

1. 发送设置命令: `fx 7 band0 512\n` (设置EQ band0增益为+2dB)
2. 检查响应确认成功
3. 发送 `param -s\n` 保存到flash

### 注意事项

- 每条命令以 `\n` 结尾
- 响应以 `\n` 结尾
- JSON中的数值均为整数
- gain值为Q8.8格式 (除以256得到dB值)
- Q值为Q6.10格式
- f0为Hz
- type: 0=PEAKING, 1=LOW_SHELF, 2=HIGH_SHELF, 3=LOW_PASS, 4=HIGH_PASS
