# BanBox VFS 架构重构说明

## 概述

将原来的 `drv_fs` 模块重构为独立的虚拟文件系统（VFS），实现更清晰的分层架构。

## 新架构

```
banux/
├── 01_vfs/                       # 独立的虚拟文件系统模块
│   ├── vfs.h                     # VFS核心接口
│   └── vfs.c                     # VFS核心实现
│
├── 02_device_drivers/            # 硬件驱动（不变）
│
├── 03_driver_framework/          # 驱动框架
│   ├── core/
│   │   ├── drv_fs.h              # 驱动FS适配层（封装VFS）
│   │   ├── drv_fs.c              # 创建/driver目录结构
│   │   └── ...
│   └── drv_init.c                # 框架初始化
│
├── 04_shell_commands/            # Shell命令
│   ├── shell_fs.h                # Shell FS接口
│   ├── shell_fs.c                # 创建/bin目录及系统命令
│   ├── bg_shell.c
│   └── bg_shell_commands.c
```

## 目录结构

```
/                          (VFS根目录)
├── bin/                   (由shell_fs.c创建)
│   └── sys/
│       ├── info          (系统信息)
│       ├── mem           (内存状态)
│       ├── tasks         (任务列表)
│       └── uptime        (运行时间)
│
└── driver/                (由drv_fs.c创建)
    ├── spi/
    │   ├── st7735/
    │   │   ├── name
    │   │   ├── width
    │   │   └── height
    │   └── w25qxx/
    ├── i2c/
    ├── i2s/
    ├── sdio/
    ├── power/
    │   └── battery/
    └── usb/
        └── cdc/
```

## 初始化流程

```c
DrvFramework_FullInit()
├── Vfs_Init()              // 创建VFS根节点 "/"
├── DrvFs_Init()            // 创建 /driver 及子目录
├── ShellFs_Init()          // 创建 /bin
├── DrvDevice_Init()        // 初始化设备管理
├── DrvFramework_RegisterAll()
│   ├── St7735_DrvRegister()    // 注册到 /driver/spi/st7735
│   ├── W25qxx_DrvRegister()    // 注册到 /driver/spi/w25qxx
│   ├── Battery_DrvRegister()   // 注册到 /driver/power/battery
│   ├── UsbCdc_DrvRegister()    // 注册到 /driver/usb/cdc
│   └── ShellFs_RegisterAllCommands()  // 注册 /bin/sys/*
```

## API映射

`drv_fs.h` 提供兼容层，将旧API映射到新VFS：

| 旧API | 新API |
|-------|-------|
| `DrvFs_GetRoot()` | `Vfs_GetRoot()` |
| `DrvFs_GetCwd()` | `Vfs_GetCwd()` |
| `DrvFs_Cd(path)` | `Vfs_Cd(path)` |
| `DrvFs_FindNode(path)` | `Vfs_FindNode(path)` |
| `DrvFs_CreateDir(p,n)` | `Vfs_CreateDir(p,n)` |
| `DrvFs_CreateParam(...)` | `Vfs_CreateParam(...)` |
| `FsNode_t` | `VfsNode_t` |
| `FS_NODE_DIR` | `VFS_NODE_DIR` |
| `FS_OK` | `VFS_OK` |

## Shell命令使用

```bash
# 查看根目录
ls /
# 输出: bin  driver

# 查看系统信息
cat /bin/sys/info

# 查看驱动
ls /driver
# 输出: spi  i2c  i2s  sdio  power  usb

# 切换目录
cd /driver/spi/st7735
ls
# 输出: name  width  height

# 读取参数
cat name
# 输出: ST7735 LCD Driver
```

## 优点

1. **清晰分层**: VFS作为基础层，驱动和Shell各自管理自己的目录
2. **易扩展**: 新模块只需在VFS下创建自己的目录即可
3. **向后兼容**: 通过宏定义保持旧API兼容
4. **UNIX风格**: `/bin` 放系统命令，`/driver` 放驱动，符合直觉
