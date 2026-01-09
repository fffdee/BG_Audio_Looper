# 蓝牙VFS集成指南

## 概述

蓝牙设备（BT和BLE）已成功挂载到VFS（虚拟文件系统），可通过Shell命令进行管理和调试。

## 目录结构

```
/
├── driver/
│   ├── bt/              # 经典蓝牙设备
│   │   ├── status       # 连接状态（只读）
│   │   ├── name         # 蓝牙名称（可读写）
│   │   ├── mac          # MAC地址（只读）
│   │   ├── volume       # 音量（可读写，0-127）
│   │   ├── connected_device  # 已连接设备（只读）
│   │   ├── rssi         # 信号强度（只读）
│   │   └── codec        # 编解码器类型（只读）
│   └── ble/             # 低功耗蓝牙设备
│       ├── status       # 连接状态（只读）
│       ├── name         # BLE广播名称（可读写）
│       ├── mac          # MAC地址（只读）
│       ├── advertising  # 广播状态（可读写）
│       ├── tx_power     # 发射功率（可读写）
│       ├── interval     # 连接间隔（只读）
│       └── mtu          # MTU大小（只读）
└── bin/
    ├── bt               # BT Shell命令
    └── ble              # BLE Shell命令
```

## Shell命令使用

### 基本命令

#### 1. 查看VFS树形结构
```bash
tree                    # 显示完整VFS树
```

#### 2. 查看驱动列表
```bash
drivers                 # 列出所有已注册的驱动
```

### BT（经典蓝牙）命令

#### 查看BT状态
```bash
bt -s                   # 显示蓝牙连接状态
```

#### VFS操作
```bash
# 进入BT目录
cd /driver/bt

# 列出所有BT参数
ls

# 读取参数
cat status              # 查看连接状态
cat name                # 查看蓝牙名称
cat mac                 # 查看MAC地址
cat volume              # 查看当前音量
cat connected_device    # 查看已连接设备
cat rssi                # 查看信号强度
cat codec               # 查看编解码器类型

# 写入参数（可写参数）
echo name BG_Audio      # 设置蓝牙名称
echo volume 80          # 设置音量（0-127）

# 或使用重定向语法
echo BG_Audio > name
echo 80 > volume
```

### BLE（低功耗蓝牙）命令

#### 查看BLE状态
```bash
ble -s                  # 显示BLE状态
```

#### VFS操作
```bash
# 进入BLE目录
cd /driver/ble

# 列出所有BLE参数
ls

# 读取参数
cat status              # 查看连接状态
cat name                # 查看BLE广播名称
cat mac                 # 查看MAC地址
cat advertising         # 查看广播状态
cat tx_power            # 查看发射功率
cat interval            # 查看连接间隔
cat mtu                 # 查看MTU大小

# 写入参数（可写参数）
echo name BG_BLE        # 设置BLE名称
echo advertising On     # 开启BLE广播
echo advertising Off    # 关闭BLE广播
echo tx_power 4         # 设置发射功率

# 或使用重定向语法
echo BG_BLE > name
echo On > advertising
```

## 实现细节

### 文件结构

1. **驱动文件**
   - `bt_vfs_driver.h` - BT/BLE VFS驱动头文件
   - `bt_vfs_driver.c` - BT/BLE VFS驱动实现

2. **框架集成**
   - `drv_init.c` - 驱动框架初始化，在此挂载BT/BLE到VFS
   - `bg_shell_commands.c` - Shell命令注册

### 初始化流程

```c
main() 
  └─> DrvFramework_FullInit()
       ├─> DrvFramework_Init()
       │    ├─> Vfs_Init()           // 初始化VFS
       │    ├─> DrvFs_Init()         // 创建/driver目录
       │    └─> ShellFs_Init()       // 创建/bin目录
       └─> DrvFramework_RegisterAll()
            ├─> BtVfs_Init()         // 初始化BT驱动
            ├─> BtVfs_Mount()        // 挂载到/driver/bt
            ├─> BleVfs_Init()        // 初始化BLE驱动
            └─> BleVfs_Mount()       // 挂载到/driver/ble
```

### 参数访问接口

每个参数节点关联读写回调函数：

```c
// BT参数读取
int BtParam_Read(char *buf, uint16_t maxLen, void *userData);

// BT参数写入
int BtParam_Write(const char *buf, void *userData);

// BLE参数读取
int BleParam_Read(char *buf, uint16_t maxLen, void *userData);

// BLE参数写入
int BleParam_Write(const char *buf, void *userData);
```

## 待完善功能（TODO）

### BT参数
- [ ] `BT_PARAM_NAME` - 完善蓝牙名称修改后的协议栈同步
- [ ] `BT_PARAM_VOLUME` - 完善音量修改后的协议栈同步
- [ ] `BT_PARAM_CONNECTED_DEV` - 从btManager获取真实的远程设备名
- [ ] `BT_PARAM_RSSI` - 实现真实的RSSI信号强度读取
- [ ] `BT_PARAM_CODEC` - 从A2DP获取真实的编解码器类型

### BLE参数
- [ ] `BLE_PARAM_STATUS` - 从BLE管理器获取真实状态
- [ ] `BLE_PARAM_NAME` - 实现BLE名称读写
- [ ] `BLE_PARAM_MAC` - 从BLE管理器获取真实MAC地址
- [ ] `BLE_PARAM_ADVERTISING` - 实现BLE广播控制API调用
- [ ] `BLE_PARAM_TX_POWER` - 实现发射功率设置API调用
- [ ] `BLE_PARAM_INTERVAL` - 从BLE连接获取真实连接间隔
- [ ] `BLE_PARAM_MTU` - 从BLE连接获取真实MTU值

## 调试输出

驱动初始化时会输出调试信息：

```
[DrvInit] Initializing Bluetooth VFS drivers...
[BtVfs] Initializing BT VFS driver...
[BtVfs] Mounting BT device to VFS...
[BtVfs] BT device mounted successfully
[DrvInit] BT device mounted at /driver/bt
[BleVfs] Initializing BLE VFS driver...
[BleVfs] Mounting BLE device to VFS...
[BleVfs] BLE device mounted successfully
[DrvInit] BLE device mounted at /driver/ble
```

## 使用示例

### 示例1：查看蓝牙状态
```bash
$ cd /driver/bt
$ cat status
Connected
$ cat volume
80
$ cat mac
12:34:56:78:9A:BC
```

### 示例2：修改蓝牙参数
```bash
$ cd /driver/bt
$ echo BG_Card > name
OK
$ echo 100 > volume
OK
```

### 示例3：控制BLE广播
```bash
$ cd /driver/ble
$ cat advertising
Off
$ echo On > advertising
OK
$ cat advertising
On
```

## 注意事项

1. **权限限制**
   - 只读参数（status, mac, rssi等）无法通过echo写入
   - 尝试写入只读参数会返回错误

2. **参数范围**
   - BT音量：0-127
   - BLE功率：根据芯片规格确定

3. **系统集成**
   - 蓝牙驱动在系统启动时自动挂载
   - 无需手动初始化
   - 可通过Shell实时查看和控制

4. **扩展性**
   - 可根据需要添加更多参数节点
   - 参数读写回调函数可轻松扩展

## 相关文件

- `BanBox/src/banux/02_device_drivers/bluetooth/bt_vfs_driver.h`
- `BanBox/src/banux/02_device_drivers/bluetooth/bt_vfs_driver.c`
- `BanBox/src/banux/03_driver_framework/drv_init.c`
- `BanBox/src/banux/04_shell_commands/bg_shell_commands.c`

## 版本历史

- **V1.0.0** (2026-01-07)
  - 初始版本
  - 实现BT和BLE基础VFS挂载
  - 支持基本参数读写
  - 集成到驱动框架和Shell系统
