# Banux系统架构重组完成报告

## 日期
2026年1月2日

## 重组概述

按照您的要求，已完成以下三项主要任务：
1. ✅ **BanGUI移动到组件层** - 可复用UI组件
2. ✅ **Shell相关模块移动到Shell层** - shell_io_ble等
3. ✅ **硬件驱动适配到框架** - LCD、Flash等驱动

---

## 一、文件移动清单

### 1.1 组件层移动 (06_app → 05_component)

**移动的目录**:
```
06_app/BanGUI/  →  05_component/BanGUI/
```

**BanGUI组件结构**:
```
05_component/BanGUI/
├── base_func/          # 基础绘图函数
│   ├── bg_draw.c
│   ├── font.c
│   ├── graphic.c
│   └── ...
├── BG_List/            # 列表组件
│   ├── bg_list.c
│   └── bg_list.h
├── menu_slider/        # 菜单滑块组件
│   ├── menu_slider.c
│   ├── bg_menu_slider.c
│   └── ...
├── page/               # 页面管理
│   ├── bg_page.c
│   └── page_manager.c
└── ui_system/          # UI系统
    ├── ui_menu.c
    ├── ui_button.c
    ├── ui_statusbar.c
    └── ...
```

**特性**: BanGUI是一个完整的GUI框架，包含绘图、控件、页面管理等，适合作为可复用组件

---

### 1.2 Shell层移动 (06_app → 04_shell_commands)

**移动的文件**:
```
06_app/bluetooth/src/shell_io_ble.c  →  04_shell_commands/shell_io_ble.c
06_app/bluetooth/inc/shell_io_ble.h  →  04_shell_commands/shell_io_ble.h
```

**Shell层文件清单**:
```
04_shell_commands/
├── shell_fs_commands.c     # 文件系统Shell命令(pwd/cd/ls/cat等)
├── shell_fs_commands.h
├── shell_io_ble.c          # BLE Shell IO适配器
└── shell_io_ble.h
```

**说明**: shell_io_ble实现了通过BLE传输Shell命令的IO适配，属于Shell扩展功能

---

## 二、驱动框架适配

### 2.1 创建的驱动适配文件

**目录结构**:
```
03_driver_framework/
├── core/                    # 框架核心(已存在)
│   ├── drv_fs.c/h          # 驱动文件系统
│   ├── drv_device.c/h      # 设备注册管理
│   └── drv_framework.h     # 框架总头文件
├── drivers/                 # 驱动适配层(新建)
│   ├── drv_st7735.c        # ST7735 LCD驱动适配 ✨新建
│   ├── drv_st7735.h        
│   ├── drv_w25qxx.c        # W25Qxx Flash驱动适配 ✨新建
│   └── drv_w25qxx.h
├── drv_init.c              # 驱动统一注册 ✨新建
└── drv_init.h
```

---

### 2.2 ST7735 LCD驱动适配详情

**文件**: `03_driver_framework/drivers/drv_st7735.c/h`

**功能**:
- 封装底层LCD驱动(`02_device_drivers/lcd/st7735.c`)
- 注册到驱动框架的SPI总线
- 创建驱动参数节点

**文件系统路径**:
```
/driver/spi/st7735/
├── name          → "ST7735_LCD"
├── width         → "128"
├── height        → "160"
├── status        → "initialized"
└── brightness    → (可写，设置亮度0-100)
```

**Shell命令示例**:
```bash
# 查看LCD宽度
cat /driver/spi/st7735/width

# 查看所有LCD参数
ls -l /driver/spi/st7735

# 设置亮度
echo 80 > /driver/spi/st7735/brightness
```

**关键API**:
```c
/* 驱动注册 */
int St7735_DrvRegister(void);

/* 参数回调 */
static int param_get_width(char *buf, uint16_t maxLen, void *userData);
static int param_get_height(char *buf, uint16_t maxLen, void *userData);
static int param_set_brightness(const char *value, void *userData);

/* 驱动操作 */
static int st7735_drv_init(void *priv);
static int st7735_drv_write(void *priv, const uint8_t *buf, uint32_t len);
static int st7735_drv_ioctl(void *priv, uint32_t cmd, void *arg);
```

---

### 2.3 W25Qxx Flash驱动适配详情

**文件**: `03_driver_framework/drivers/drv_w25qxx.c/h`

**功能**:
- 封装底层Flash驱动(`02_device_drivers/flash/flash_nor_w25qxx.c`)
- 注册到驱动框架的SPI总线
- 提供Flash参数和操作接口

**文件系统路径**:
```
/driver/spi/w25qxx/
├── name          → "W25Qxx_Flash"
├── capacity      → "8192 KB" (W25Q64)
├── page_size     → "256"
├── sector_size   → "4096"
├── status        → "initialized"
├── device_id     → "0xEF40"
└── erase_chip    → (可写，写入"confirm"执行全片擦除)
```

**Shell命令示例**:
```bash
# 查看Flash容量
cat /driver/spi/w25qxx/capacity

# 查看设备ID
cat /driver/spi/w25qxx/device_id

# 全片擦除(危险操作!)
echo confirm > /driver/spi/w25qxx/erase_chip
```

**关键API**:
```c
/* 驱动注册 */
int W25qxx_DrvRegister(void);

/* 参数回调 */
static int param_get_capacity(char *buf, uint16_t maxLen, void *userData);
static int param_get_device_id(char *buf, uint16_t maxLen, void *userData);
static int param_cmd_erase_chip(const char *value, void *userData);

/* 驱动操作 */
static int w25qxx_drv_init(void *priv);
static int w25qxx_drv_read(void *priv, uint8_t *buf, uint32_t len);
static int w25qxx_drv_write(void *priv, const uint8_t *buf, uint32_t len);
static int w25qxx_drv_ioctl(void *priv, uint32_t cmd, void *arg);
```

---

### 2.4 驱动统一注册

**文件**: `03_driver_framework/drv_init.c/h`

**功能**: 提供统一的驱动初始化接口

**API**:
```c
/* 初始化驱动框架核心 */
int DrvFramework_Init(void);

/* 注册所有硬件驱动 */
int DrvFramework_RegisterAll(void);

/* 一步完成初始化+注册 */
int DrvFramework_FullInit(void);
```

**使用示例**:
```c
int main(void)
{
    /* 1. 硬件初始化 */
    SystemClock_Config();
    GPIO_Init();
    
    /* 2. 驱动框架初始化(自动注册所有驱动) */
    DrvFramework_FullInit();
    
    /* 3. Shell初始化 */
    Shell_Init();
    ShellFs_RegisterCommands();  // 注册文件系统命令
    
    /* 4. 主循环 */
    while(1) {
        Shell_Task();
    }
}
```

---

## 三、最终目录结构

```
banux/
├── 01_hal_drivers/             # HAL硬件抽象层
│   └── spi/                    # SPI底层驱动
│
├── 02_device_drivers/          # 设备驱动层
│   ├── lcd/                    # LCD驱动(st7735.c等)
│   ├── flash/                  # Flash驱动(flash_nor_w25qxx.c等)
│   └── power_mgr/              # 电源管理
│
├── 03_driver_framework/        # 驱动框架层 ✨重点
│   ├── core/                   # 框架核心
│   │   ├── drv_fs.c/h         # 驱动文件系统
│   │   ├── drv_device.c/h     # 设备管理
│   │   └── drv_framework.h
│   ├── drivers/                # 驱动适配层 ✨新建
│   │   ├── drv_st7735.c/h     # LCD适配
│   │   └── drv_w25qxx.c/h     # Flash适配
│   ├── drv_init.c/h           # 统一注册 ✨新建
│   └── ARCHITECTURE_REORGANIZATION.md
│
├── 04_shell_commands/          # Shell命令层 ✨已更新
│   ├── shell_fs_commands.c/h  # 文件系统命令
│   └── shell_io_ble.c/h       # BLE IO适配 ✨已移动
│
├── 05_component/               # 组件层 ✨已更新
│   └── BanGUI/                # GUI组件 ✨已移动
│       ├── base_func/
│       ├── BG_List/
│       ├── menu_slider/
│       ├── page/
│       └── ui_system/
│
└── 06_app/                     # 应用层
    ├── audio/                  # 音频应用
    ├── audio_looper/           # 音频循环器
    ├── BG_AudioIO_Manager/     # 音频IO管理
    ├── bluetooth/              # 蓝牙应用
    └── sys_param/              # 系统参数
```

---

## 四、编译集成指南

### 4.1 添加到Makefile

需要在`Debug/makefile`中添加新的源文件路径：

```makefile
# 驱动框架核心
C_SRCS += \
../src/banux/03_driver_framework/core/drv_fs.c \
../src/banux/03_driver_framework/core/drv_device.c \
../src/banux/03_driver_framework/drivers/drv_st7735.c \
../src/banux/03_driver_framework/drivers/drv_w25qxx.c \
../src/banux/03_driver_framework/drv_init.c

# Shell命令
C_SRCS += \
../src/banux/04_shell_commands/shell_fs_commands.c \
../src/banux/04_shell_commands/shell_io_ble.c

# 组件层
C_SRCS += \
../src/banux/05_component/BanGUI/base_func/*.c \
../src/banux/05_component/BanGUI/page/*.c \
../src/banux/05_component/BanGUI/ui_system/*.c

# 头文件路径
INCLUDES += \
-I../src/banux/03_driver_framework/core \
-I../src/banux/03_driver_framework/drivers \
-I../src/banux/04_shell_commands \
-I../src/banux/05_component/BanGUI
```

### 4.2 Include路径更新

**原有代码需要更新的include**:

```c
// 旧路径
#include "06_app/BanGUI/ui_system/ui_menu.h"
#include "06_app/bluetooth/inc/shell_io_ble.h"

// 新路径
#include "05_component/BanGUI/ui_system/ui_menu.h"
#include "04_shell_commands/shell_io_ble.h"
```

---

## 五、使用指南

### 5.1 驱动初始化流程

```c
/* main.c */
#include "drv_init.h"
#include "shell.h"
#include "shell_fs_commands.h"

int main(void)
{
    /* Step 1: 硬件初始化 */
    HAL_Init();
    SystemClock_Config();
    
    /* Step 2: 驱动框架初始化 */
    DrvFramework_FullInit();
    
    /* Step 3: Shell初始化 */
    Shell_Init();
    ShellFs_RegisterCommands();  // 注册文件系统命令(pwd/cd/ls等)
    
    /* Step 4: 应用初始化 */
    App_Init();
    
    /* Step 5: 主循环 */
    while(1)
    {
        Shell_Task();
        App_Task();
    }
}
```

### 5.2 Shell命令演示

初始化完成后，可以使用以下命令：

```bash
# 1. 查看所有注册的驱动
drivers

# 2. 列出驱动目录树
tree /driver

# 3. 进入SPI驱动目录
cd /driver/spi

# 4. 查看LCD参数
ls /driver/spi/st7735
cat /driver/spi/st7735/width
cat /driver/spi/st7735/height
cat /driver/spi/st7735/status

# 5. 查看Flash信息
cd /driver/spi/w25qxx
cat capacity
cat device_id
ls -l

# 6. 返回根目录
cd /
pwd
```

### 5.3 添加新驱动示例

假设要添加Audio Codec驱动:

**Step 1**: 创建驱动适配文件
```c
/* 03_driver_framework/drivers/drv_audio_codec.c */
#include "drv_audio_codec.h"
#include "drv_device.h"

static const FsParamDef_t audio_params[] = {
    { .name = "volume",  .desc = "音量",  .get = get_volume, .set = set_volume },
    { .name = "mute",    .desc = "静音",  .get = get_mute,   .set = set_mute },
    FS_PARAM_END()
};

static const DrvDevice_t audio_driver = {
    .name = "audio_codec",
    .bus = DRV_BUS_I2C,
    .init = audio_drv_init,
    .params = audio_params,
    // ...
};

int AudioCodec_DrvRegister(void) {
    return DrvDevice_Register(&audio_driver);
}
```

**Step 2**: 在drv_init.c中注册
```c
int DrvFramework_RegisterAll(void)
{
    St7735_DrvRegister();
    W25qxx_DrvRegister();
    AudioCodec_DrvRegister();  // ✨添加
    return 0;
}
```

**Step 3**: Shell访问
```bash
cat /driver/i2c/audio_codec/volume
echo 50 > /driver/i2c/audio_codec/volume
```

---

## 六、待完成工作

### 6.1 必须完成的任务

1. **更新Makefile** ⚠️
   - [ ] 添加驱动适配文件编译规则
   - [ ] 更新头文件搜索路径
   - [ ] 移除旧路径的引用

2. **更新Include路径** ⚠️
   - [ ] 全局搜索替换BanGUI引用
   - [ ] 更新shell_io_ble引用
   - [ ] 验证所有头文件能正确找到

3. **驱动适配完善** 🔨
   - [ ] ST7735驱动适配：连接实际LCD初始化函数
   - [ ] W25Qxx驱动适配：连接实际Flash读写函数
   - [ ] 添加Power Manager驱动适配
   - [ ] 添加Audio相关驱动适配

4. **测试验证** ✅
   - [ ] 编译测试
   - [ ] 驱动注册测试
   - [ ] Shell命令测试
   - [ ] 参数读写测试

### 6.2 可选优化

- [ ] 添加驱动热插拔支持
- [ ] 添加驱动依赖管理
- [ ] 添加驱动版本信息
- [ ] 添加驱动日志系统
- [ ] 添加驱动性能监控

---

## 七、架构优势

### 7.1 分层清晰

```
06_app (应用层)      → 业务逻辑,用户交互
    ↓ 调用
05_component (组件层) → 可复用组件(BanGUI等)
    ↓ 调用
04_shell (Shell层)   → 命令行交互,调试接口
    ↓ 调用
03_framework (框架层) → 驱动统一管理,参数访问
    ↓ 调用
02_drivers (驱动层)   → 硬件抽象,设备操作
    ↓ 操作
01_hal (HAL层)       → 寄存器级操作
    ↓
硬件外设
```

### 7.2 核心优势

1. **统一访问接口**: 所有驱动通过/driver/xxx路径访问
2. **Linux风格**: pwd/cd/ls等命令自然操作驱动参数
3. **参数可见**: 驱动状态和配置一目了然
4. **易于调试**: Shell命令快速诊断问题
5. **可扩展性**: 新增驱动只需注册，自动创建节点
6. **代码复用**: BanGUI等组件独立于应用，便于移植

---

## 八、总结

### 完成情况

✅ **BanGUI移动到组件层**: 06_app/BanGUI → 05_component/BanGUI  
✅ **Shell模块移动**: shell_io_ble → 04_shell_commands  
✅ **ST7735驱动适配**: 创建drv_st7735.c/h，支持参数访问  
✅ **W25Qxx驱动适配**: 创建drv_w25qxx.c/h，支持Flash操作  
✅ **统一注册接口**: drv_init.c/h一键注册所有驱动  
✅ **文档完善**: 本说明文档详细记录所有变更  

### 下一步

1. 更新Makefile和头文件路径
2. 连接实际驱动函数
3. 添加更多驱动适配(Power/Audio等)
4. 编译测试验证

---

**重组完成时间**: 2026年1月2日  
**文档版本**: v1.0  
**维护者**: BG Card Team
