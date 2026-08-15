# update_tool — BG Bootloader 上位机

USB CDC 固件升级工具（PyQt5 GUI），用于识别 BG Bootloader 设备并烧录 APP 固件。

## 依赖

```bash
pip install pyserial PyQt5
```

Python 3.8+ 推荐。

## 运行

```bash
cd update_tool
python bg_bootloader.py
```

默认波特率：**2000000**（USB CDC 实际速率由主机栈决定，下拉框可改）。

## USB 身份协议

上位机通过 USB **VID/PID** 识别 Bootloader 所属产品：

| 字段 | 值 | 含义 |
|------|-----|------|
| PID | `0x4247` | BG（`'B''G'`）Bootloader 家族 |
| VID | `0x0001` | BanBox |
| VID | `0x0002` | BanAirBundy |

本仓库 Bootloader 枚举为：`VID=0x0001` / `PID=0x4247` → **BanBox**。

固件侧常量见：`bootloader/src/usb_identity.h`。

## UI 说明

- **顶部 Banner**
  - 未连接：红色「未连接」
  - 已连接：绿色「已连接 · \<产品名\> (COMx)」
- **设备连接**：串口选择、刷新、自动扫描、「进入 Boot 模式」
- **固件操作**：选择 `.bin` → 升级；双分区模式额外支持握手/查询/擦除/跳转/重启
- **日志**：实时操作日志

## 典型流程

1. 设备进入 Bootloader（烧录 BL，或 APP 串口发 `boot` / 点「进入 Boot 模式」）
2. 打开本工具，自动扫描或手动选串口
3. Banner 显示绿色产品名后，选择固件并升级

## 目录结构

```
update_tool/
├── README.md           ← 本说明
├── bg_bootloader.py    ← GUI 入口
├── bl_core.py          ← CDC 升级协议 + USB 身份识别
└── worker.py           ← 后台扫描 / 升级线程
```

## 协议概要

- 帧头 SOF：`0xAA`
- 主要命令：SYNC / START / DATA / FINISH / JUMP / ERASE / QUERY_INFO / SET_PART / REBOOT
- 详情实现见 `bl_core.py`

## 与固件对应关系

| 工程 | 角色 |
|------|------|
| `bootloader` | USB CDC 升级模式，暴露身份 VID/PID |
| `boot_app` | 应用；可通过 Shell `boot` 写 burn flag 后复位进 BL |
| `update_tool` | 主机端识别产品并烧录 |

## 兼容说明

仍识别旧 ID `VID=0x8888 / PID=0x1722`（显示为 Legacy Bootloader），正式产品以 `PID=0x4247` 身份协议为准。
