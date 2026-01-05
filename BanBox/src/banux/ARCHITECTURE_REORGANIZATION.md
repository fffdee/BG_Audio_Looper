# 工程架构重组说明

## 概述
已按照分层架构重新组织工程文件结构,使各层职责清晰,便于维护和扩展。

## 新的目录结构

```
hardware/
├── 01_hal_drivers/              # 硬件抽象层(通用复用层)
│   └── spi/                     # SPI底层驱动
│       └── (未来放置 spi_hal.c/h)
│
├── 02_device_drivers/           # 设备驱动层(差异化核心层)
│   ├── lcd/                     # LCD设备驱动
│   │   ├── st7735.c            # ST7735 LCD驱动实现
│   │   ├── st7735.h
│   │   ├── bg_lcd.c            # LCD适配层
│   │   ├── bg_lcd.h
│   │   ├── framebuffer.c       # 帧缓冲实现
│   │   ├── framebuffer.h
│   │   ├── framebuffer_adapter.h
│   │   └── font.h              # 字体定义
│   │
│   └── flash/                   # Flash设备驱动
│       ├── flash_nor_w25qxx.c  # W25Qxx Flash驱动
│       ├── flash_nor_w25qxx.h
│       ├── flash_bus.c         # Flash总线管理
│       ├── flash_bus.h
│       ├── flash_driver.c      # Flash驱动核心
│       ├── flash_driver.h
│       ├── flash_manager.c     # Flash管理器
│       ├── flash_manager.h
│       ├── flash_devices.c     # Flash设备配置
│       ├── flash_devices.h
│       ├── flash_api.h         # Flash API接口
│       ├── BG_FlashMgr.c       # Flash高级管理
│       ├── BG_FlashMgr.h
│       ├── bg_flash_manager.c  # Flash管理适配
│       ├── bg_flash_manager.h
│       ├── flash_test.c        # Flash测试代码
│       ├── flash_test.h
│       ├── flash_manager_example.h
│       └── BG_FLASHMGR_README.md
│
├── 03_driver_framework/         # 驱动框架层(驱动管理中心)
│   ├── core/                    # 框架核心
│   │   ├── drv_fs.c            # 虚拟文件系统
│   │   ├── drv_fs.h
│   │   ├── drv_device.c        # 设备注册管理
│   │   ├── drv_device.h
│   │   └── drv_framework.h     # 框架头文件
│   │
│   └── examples/                # 框架使用示例
│       ├── drv_st7735.c        # ST7735驱动框架示例
│       └── drv_st7735.h
│
└── 04_shell_commands/           # 应用层(Shell命令层)
    ├── shell_fs_commands.c     # 文件系统Shell命令
    └── shell_fs_commands.h     # (pwd, cd, ls, cat, echo等)

```

## 分层架构说明

### Layer 01: HAL驱动层 (01_hal_drivers/)
**职责**: 提供与硬件无关的通用接口
- **特点**: 高度可复用,跨平台移植
- **示例**: SPI/I2C/UART核心驱动
- **当前状态**: 目录已创建,待填充SPI HAL实现

### Layer 02: 设备驱动层 (02_device_drivers/)
**职责**: 特定设备的协议实现和硬件操作
- **特点**: 设备专属,封装硬件细节
- **LCD子系统**:
  - `st7735.c/h`: ST7735芯片驱动(寄存器操作、初始化序列)
  - `bg_lcd.c/h`: LCD适配层(统一接口)
  - `framebuffer.*`: 帧缓冲管理(双缓冲、DMA传输)
- **Flash子系统**:
  - `flash_nor_w25qxx.*`: W25Qxx系列Flash驱动
  - `flash_bus.*`: Flash总线管理(SPI+DMA)
  - `flash_manager.*`: 分区管理、读写保护
  - `BG_FlashMgr.*`: 高级功能(擦写平衡、坏块管理)

### Layer 03: 驱动框架层 (03_driver_framework/)
**职责**: 统一的驱动管理和虚拟文件系统
- **核心功能**:
  - `drv_fs.*`: 虚拟文件系统(VFS)
  - `drv_device.*`: 设备注册、查找、操作接口
  - `drv_framework.h`: 框架总头文件
- **示例代码**:
  - `drv_st7735.*`: 演示如何将ST7735注册到框架

### Layer 04: Shell命令层 (04_shell_commands/)
**职责**: 用户交互和业务逻辑
- **特点**: Linux风格命令实现
- **当前命令**:
  - `pwd`: 显示当前工作目录
  - `cd <dir>`: 切换目录
  - `ls [-l] [path]`: 列出文件(支持-l详细模式)
  - `cat <file>`: 显示文件内容
  - `echo <text>`: 输出文本
  - `tree [path]`: 树形显示目录结构
  - `drivers`: 显示已注册驱动

## 文件移动清单

### 已完成的文件迁移:

#### 从 drv_framework/ → 03_driver_framework/core/
- ✅ drv_fs.c
- ✅ drv_fs.h
- ✅ drv_device.c
- ✅ drv_device.h
- ✅ drv_framework.h

#### 从 drv_framework/ → 03_driver_framework/examples/
- ✅ drv_st7735.c
- ✅ drv_st7735.h

#### 从 drv_framework/ → 04_shell_commands/
- ✅ shell_fs_commands.c
- ✅ shell_fs_commands.h

#### 从 BG_Lcd/ → 02_device_drivers/lcd/
- ✅ st7735.c, st7735.h
- ✅ bg_lcd.c, bg_lcd.h
- ✅ framebuffer.c, framebuffer.h
- ✅ framebuffer_adapter.h
- ✅ font.h

#### 从 BG_flash_manager/ → 02_device_drivers/flash/
- ✅ flash_nor_w25qxx.c/h
- ✅ flash_bus.c/h
- ✅ flash_driver.c/h
- ✅ flash_manager.c/h
- ✅ flash_devices.c/h
- ✅ BG_FlashMgr.c/h
- ✅ bg_flash_manager.c/h
- ✅ flash_test.c/h
- ✅ flash_api.h
- ✅ flash_manager_example.h
- ✅ BG_FLASHMGR_README.md

## 后续工作

### 1. 更新构建系统
需要修改以下文件以适应新的目录结构:
- `Debug/makefile`
- `Debug/sources.mk`
- `Debug/subdir.mk`

示例路径更新:
```makefile
# 旧路径
INCLUDES += -I../src/hardware/drv_framework
INCLUDES += -I../src/hardware/BG_Lcd

# 新路径
INCLUDES += -I../src/hardware/03_driver_framework/core
INCLUDES += -I../src/hardware/02_device_drivers/lcd
```

### 2. 更新源文件#include路径
需要批量替换头文件包含路径:
```c
// 旧路径
#include "drv_framework/drv_fs.h"
#include "BG_Lcd/st7735.h"

// 新路径
#include "03_driver_framework/core/drv_fs.h"
#include "02_device_drivers/lcd/st7735.h"
```

### 3. 测试编译
```bash
cd BG_card_mini/Debug
make clean
make all
```

### 4. 功能验证
编译通过后,测试以下功能:
- Shell命令是否正常工作
- LCD显示是否正常
- Flash读写是否正常
- 驱动注册是否成功

## 架构优势

1. **职责清晰**: 每一层都有明确的职责边界
2. **易于维护**: 模块化设计,修改不影响其他层
3. **可扩展性**: 添加新设备只需在Layer 02添加驱动
4. **可移植性**: Layer 01可跨平台复用
5. **规范统一**: 数字前缀强制执行依赖顺序

## 依赖关系

```
04_shell_commands (应用层)
        ↓ 调用
03_driver_framework (框架层)
        ↓ 调用
02_device_drivers (设备驱动层)
        ↓ 调用
01_hal_drivers (HAL层)
        ↓ 操作
    硬件外设
```

## 注意事项

- ⚠️ 原始目录(drv_framework/, BG_Lcd/, BG_flash_manager/)中的文件已复制但未删除
- ⚠️ 需要更新构建系统才能成功编译
- ⚠️ 需要更新所有源文件中的#include路径
- ⚠️ 建议在修改后进行完整的功能测试

## 日期
重组完成时间: 2024年
