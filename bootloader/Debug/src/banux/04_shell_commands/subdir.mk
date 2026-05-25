################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/04_shell_commands/bg_shell.c \
../src/banux/04_shell_commands/bg_shell_commands.c \
../src/banux/04_shell_commands/shell_cmd_param.c \
../src/banux/04_shell_commands/shell_cmd_soundbank.c \
../src/banux/04_shell_commands/shell_cmd_sysmon.c \
../src/banux/04_shell_commands/shell_fs.c \
../src/banux/04_shell_commands/shell_io_ble.c \
../src/banux/04_shell_commands/shell_io_manager.c 

OBJS += \
./src/banux/04_shell_commands/bg_shell.o \
./src/banux/04_shell_commands/bg_shell_commands.o \
./src/banux/04_shell_commands/shell_cmd_param.o \
./src/banux/04_shell_commands/shell_cmd_soundbank.o \
./src/banux/04_shell_commands/shell_cmd_sysmon.o \
./src/banux/04_shell_commands/shell_fs.o \
./src/banux/04_shell_commands/shell_io_ble.o \
./src/banux/04_shell_commands/shell_io_manager.o 

C_DEPS += \
./src/banux/04_shell_commands/bg_shell.d \
./src/banux/04_shell_commands/bg_shell_commands.d \
./src/banux/04_shell_commands/shell_cmd_param.d \
./src/banux/04_shell_commands/shell_cmd_soundbank.d \
./src/banux/04_shell_commands/shell_cmd_sysmon.d \
./src/banux/04_shell_commands/shell_fs.d \
./src/banux/04_shell_commands/shell_io_ble.d \
./src/banux/04_shell_commands/shell_io_manager.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/04_shell_commands/%.o: ../src/banux/04_shell_commands/%.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/usb/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


