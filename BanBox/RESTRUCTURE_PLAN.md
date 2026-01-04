# 工程文件重组计划

## 分层结构设计

```
BG_card_mini/src/hardware/
├── 01_hal_drivers/          # 底层：通用硬件抽象层（HAL）
│   ├── spi/                 # SPI核心驱动（spi_core_xxx）
│   ├── i2c/                 # I2C核心驱动
│   ├── uart/                # UART核心驱动
│   └── gpio/                # GPIO核心驱动
│
├── 02_device_drivers/       # 中间层：设备专属驱动
│   ├── lcd/                 # LCD设备驱动
│   │   ├── st7735.c/.h      # ST7735专属API（基于SPI HAL）
│   │   └── ...
│   ├── flash/               # Flash设备驱动
│   │   ├── w25qxx.c/.h      # W25Qxx专属API（基于SPI HAL）
│   │   └── ...
│   ├── audio/               # 音频设备驱动
│   └── sensor/              # 传感器设备驱动
│
├── 03_driver_framework/     # 系统层：驱动管理框架
│   ├── core/                # 框架核心
│   │   ├── drv_fs.c/.h      # 文件系统（树形结构）
│   │   ├── drv_device.c/.h  # 设备注册管理
│   │   └── drv_framework.h  # 统一头文件
│   └── examples/            # 示例驱动
│       └── drv_st7735.c/.h  # ST7735框架适配示例
│
└── 04_shell_commands/       # 应用层：Shell命令
    ├── shell_fs_commands.c/.h   # 文件系统命令（pwd/cd/ls/cat/echo）
    └── README.md                 # 命令使用说明
```

## 文件移动操作清单

### 1. 底层HAL驱动 → 01_hal_drivers/
暂无需移动（SDK层提供，路径：MVsB1_Base_SDK/driver/）

### 2. 设备驱动 → 02_device_drivers/

#### LCD驱动：
- `BG_Lcd/st7735.c` → `02_device_drivers/lcd/st7735.c`
- `BG_Lcd/st7735.h` → `02_device_drivers/lcd/st7735.h`
- `BG_Lcd/bg_lcd.c` → `02_device_drivers/lcd/bg_lcd.c`
- `BG_Lcd/bg_lcd.h` → `02_device_drivers/lcd/bg_lcd.h`
- `BG_Lcd/framebuffer.c` → `02_device_drivers/lcd/framebuffer.c`
- `BG_Lcd/framebuffer.h` → `02_device_drivers/lcd/framebuffer.h`

#### Flash驱动：
- `BG_flash_manager/flash_nor_w25qxx.c` → `02_device_drivers/flash/w25qxx.c`
- `BG_flash_manager/flash_nor_w25qxx.h` → `02_device_drivers/flash/w25qxx.h`
- `BG_flash_manager/flash_bus.c` → `02_device_drivers/flash/flash_bus.c`
- `BG_flash_manager/flash_bus.h` → `02_device_drivers/flash/flash_bus.h`
- `BG_flash_manager/flash_driver.c` → `02_device_drivers/flash/flash_driver.c`
- `BG_flash_manager/flash_driver.h` → `02_device_drivers/flash/flash_driver.h`

### 3. 驱动框架 → 03_driver_framework/

#### 框架核心：
- `drv_framework/drv_fs.c` → `03_driver_framework/core/drv_fs.c`
- `drv_framework/drv_fs.h` → `03_driver_framework/core/drv_fs.h`
- `drv_framework/drv_device.c` → `03_driver_framework/core/drv_device.c`
- `drv_framework/drv_device.h` → `03_driver_framework/core/drv_device.h`
- `drv_framework/drv_framework.h` → `03_driver_framework/core/drv_framework.h`
- `drv_framework/DRV_FRAMEWORK_README.md` → `03_driver_framework/README.md`

#### 示例驱动：
- `drv_framework/drv_st7735.c` → `03_driver_framework/examples/drv_st7735.c`
- `drv_framework/drv_st7735.h` → `03_driver_framework/examples/drv_st7735.h`

### 4. Shell命令 → 04_shell_commands/
- `drv_framework/shell_fs_commands.c` → `04_shell_commands/shell_fs_commands.c`
- `drv_framework/shell_fs_commands.h` → `04_shell_commands/shell_fs_commands.h`
- `drv_framework/LINUX_STYLE_COMMANDS_UPDATE.md` → `04_shell_commands/README.md`

## 保留的旧目录结构
以下目录保持不变（业务逻辑层）：
- `audio/` - 音频业务逻辑
- `audio_looper/` - 音频循环器
- `battery/` - 电池管理
- `BG_AudioIO_Manager/` - 音频IO管理器
- `BG_Encoder/` - 编码器
- `BG_flash_manager/` - Flash管理器（高层接口，保留不动）
- `bluetooth/` - 蓝牙
- `IIC/` - I2C业务接口
- `sys_param/` - 系统参数

## 执行后的目录结构

```
hardware/
├── 01_hal_drivers/          # ★ 底层（通用复用层）
│   └── spi/
├── 02_device_drivers/       # ★ 中间层（设备专属API）
│   ├── lcd/
│   │   ├── st7735.c/.h
│   │   ├── bg_lcd.c/.h
│   │   └── framebuffer.c/.h
│   └── flash/
│       ├── w25qxx.c/.h
│       ├── flash_bus.c/.h
│       └── flash_driver.c/.h
├── 03_driver_framework/     # ★ 系统层（驱动管理中心）
│   ├── core/
│   │   ├── drv_fs.c/.h
│   │   ├── drv_device.c/.h
│   │   └── drv_framework.h
│   ├── examples/
│   │   └── drv_st7735.c/.h
│   └── README.md
├── 04_shell_commands/       # ★ 应用层（Shell命令）
│   ├── shell_fs_commands.c/.h
│   └── README.md
├── audio/                   # 业务层（保持不变）
├── battery/
├── BG_flash_manager/        # Flash高层管理（保持）
├── bluetooth/
└── ...（其他业务模块）
```
