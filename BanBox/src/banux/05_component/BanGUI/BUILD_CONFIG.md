# BanGUI 编译配置

## 新增编译文件

在 Eclipse/Andes IDE 中添加以下源文件到项目:

### 核心层 (ui/core/)
- `../src/banux/05_component/BanGUI/ui/core/bg_ui.c`
- `../src/banux/05_component/BanGUI/ui/core/ui_page.c`
- `../src/banux/05_component/BanGUI/ui/core/bg_page_compat.c`

### 组件层 (ui/components/)
- `../src/banux/05_component/BanGUI/ui/components/comp_statusbar.c`
- `../src/banux/05_component/BanGUI/ui/components/comp_popup.c`

### 视图层 (ui/views/)
- `../src/banux/05_component/BanGUI/ui/views/view_home.c`
- `../src/banux/05_component/BanGUI/ui/views/view_menu.c`
- `../src/banux/05_component/BanGUI/ui/views/view_looper.c`
- `../src/banux/05_component/BanGUI/ui/views/app_pages.c`

## 需要移除的编译文件（重要！）

从编译列表中移除以下文件以避免冲突:

### 旧页面系统（已被 ui/core 替代）
- `../src/banux/05_component/BanGUI/page/bg_page.c` ⚠️ **必须移除** (与 bg_page_compat.c 冲突)
- `../src/banux/05_component/BanGUI/page/page_manager.c` (旧页面管理器)

### 未使用的 UI 文件
- `../src/banux/05_component/BanGUI/ui_system/audio_spectrum_simple.c`
- `../src/banux/05_component/BanGUI/ui_system/vacal_setting.c`

## 新增包含路径

添加以下路径到编译器包含路径 (-I):

```
-I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui"
-I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/core"
-I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/components"
-I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/views"
-I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/resources"
```

## subdir.mk 示例

为 `ui/core/`, `ui/components/`, `ui/views/` 创建 subdir.mk:

### ui/core/subdir.mk

```makefile
# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/05_component/BanGUI/ui/core/bg_ui.c 

OBJS += \
./src/banux/05_component/BanGUI/ui/core/bg_ui.o 

C_DEPS += \
./src/banux/05_component/BanGUI/ui/core/bg_ui.d 

# Each subdirectory must supply rules for building sources it contributes
src/banux/05_component/BanGUI/ui/core/%.o: ../src/banux/05_component/BanGUI/ui/core/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc $(INCLUDES) $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
```

### ui/components/subdir.mk

```makefile
C_SRCS += \
../src/banux/05_component/BanGUI/ui/components/comp_statusbar.c \
../src/banux/05_component/BanGUI/ui/components/comp_popup.c 

OBJS += \
./src/banux/05_component/BanGUI/ui/components/comp_statusbar.o \
./src/banux/05_component/BanGUI/ui/components/comp_popup.o 

C_DEPS += \
./src/banux/05_component/BanGUI/ui/components/comp_statusbar.d \
./src/banux/05_component/BanGUI/ui/components/comp_popup.d 
```

### ui/views/subdir.mk

```makefile
C_SRCS += \
../src/banux/05_component/BanGUI/ui/views/view_home.c \
../src/banux/05_component/BanGUI/ui/views/view_menu.c \
../src/banux/05_component/BanGUI/ui/views/view_looper.c \
../src/banux/05_component/BanGUI/ui/views/app_pages.c

OBJS += \
./src/banux/05_component/BanGUI/ui/views/view_home.o \
./src/banux/05_component/BanGUI/ui/views/view_menu.o \
./src/banux/05_component/BanGUI/ui/views/view_looper.o \
./src/banux/05_component/BanGUI/ui/views/app_pages.o

C_DEPS += \
./src/banux/05_component/BanGUI/ui/views/view_home.d \
./src/banux/05_component/BanGUI/ui/views/view_menu.d \
./src/banux/05_component/BanGUI/ui/views/view_looper.d \
./src/banux/05_component/BanGUI/ui/views/app_pages.d
```

### ui/core/subdir.mk (完整版)

```makefile
C_SRCS += \
../src/banux/05_component/BanGUI/ui/core/bg_ui.c \
../src/banux/05_component/BanGUI/ui/core/ui_page.c \
../src/banux/05_component/BanGUI/ui/core/bg_page_compat.c

OBJS += \
./src/banux/05_component/BanGUI/ui/core/bg_ui.o \
./src/banux/05_component/BanGUI/ui/core/ui_page.o \
./src/banux/05_component/BanGUI/ui/core/bg_page_compat.o

C_DEPS += \
./src/banux/05_component/BanGUI/ui/core/bg_ui.d \
./src/banux/05_component/BanGUI/ui/core/ui_page.d \
./src/banux/05_component/BanGUI/ui/core/bg_page_compat.d
```

## 在 Eclipse/Andes IDE 中操作

1. **添加新源文件:**
   - 右键项目 → Properties → C/C++ Build → Settings
   - 或者刷新项目让 IDE 自动检测新文件

2. **添加包含路径:**
   - 右键项目 → Properties → C/C++ Build → Settings → Tool Settings
   - Andes C Compiler → Includes → 添加新路径

3. **移除未使用的文件:**
   - 在 Project Explorer 中右键文件 → Exclude from Build

4. **清理并重新构建:**
   - Project → Clean...
   - Project → Build Project
