################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/04_shell_commands/bg_shell.c \
../src/banux/04_shell_commands/bg_shell_commands.c \
../src/banux/04_shell_commands/shell_cmd_effect.c \
../src/banux/04_shell_commands/shell_cmd_metronome.c \
../src/banux/04_shell_commands/shell_cmd_param.c \
../src/banux/04_shell_commands/shell_cmd_sysmon.c \
../src/banux/04_shell_commands/shell_cmd_ui.c \
../src/banux/04_shell_commands/shell_fs.c \
../src/banux/04_shell_commands/shell_io_ble.c \
../src/banux/04_shell_commands/shell_io_manager.c 

OBJS += \
./src/banux/04_shell_commands/bg_shell.o \
./src/banux/04_shell_commands/bg_shell_commands.o \
./src/banux/04_shell_commands/shell_cmd_effect.o \
./src/banux/04_shell_commands/shell_cmd_metronome.o \
./src/banux/04_shell_commands/shell_cmd_param.o \
./src/banux/04_shell_commands/shell_cmd_sysmon.o \
./src/banux/04_shell_commands/shell_cmd_ui.o \
./src/banux/04_shell_commands/shell_fs.o \
./src/banux/04_shell_commands/shell_io_ble.o \
./src/banux/04_shell_commands/shell_io_manager.o 

C_DEPS += \
./src/banux/04_shell_commands/bg_shell.d \
./src/banux/04_shell_commands/bg_shell_commands.d \
./src/banux/04_shell_commands/shell_cmd_effect.d \
./src/banux/04_shell_commands/shell_cmd_metronome.d \
./src/banux/04_shell_commands/shell_cmd_param.d \
./src/banux/04_shell_commands/shell_cmd_sysmon.d \
./src/banux/04_shell_commands/shell_cmd_ui.d \
./src/banux/04_shell_commands/shell_fs.d \
./src/banux/04_shell_commands/shell_io_ble.d \
./src/banux/04_shell_commands/shell_io_manager.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/04_shell_commands/%.o: ../src/banux/04_shell_commands/%.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/device" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/adc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/gpio" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/spi" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/flash" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/lcd" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/power_mgr" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/03_driver_framework/core" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/03_driver_framework" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/04_shell_commands" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/base_func" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/06_app/audio" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/06_app/audio/music_parameter" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/audio_looper" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/sys_param" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/06_app/BG_AudioIO_Manager" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/audio_spectrum" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/03_driver_framework/drivers" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/01_vfs" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/effect_graph" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/components" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/core" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/resources" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/views" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/BanBox/src/banux/05_component/metronome" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


