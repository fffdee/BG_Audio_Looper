################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/04_shell_commands/bg_shell.c \
../src/banux/04_shell_commands/bg_shell_commands.c \
../src/banux/04_shell_commands/shell_cmd_battery_calib.c \
../src/banux/04_shell_commands/shell_cmd_fat.c \
../src/banux/04_shell_commands/shell_cmd_flash.c \
../src/banux/04_shell_commands/shell_cmd_hwtest.c \
../src/banux/04_shell_commands/shell_cmd_lp.c \
../src/banux/04_shell_commands/shell_cmd_param.c \
../src/banux/04_shell_commands/shell_cmd_psram.c \
../src/banux/04_shell_commands/shell_cmd_soundbank.c \
../src/banux/04_shell_commands/shell_cmd_speedtest.c \
../src/banux/04_shell_commands/shell_cmd_sysmon.c \
../src/banux/04_shell_commands/shell_cmd_ui.c \
../src/banux/04_shell_commands/shell_fs.c \
../src/banux/04_shell_commands/shell_io_ble.c \
../src/banux/04_shell_commands/shell_io_manager.c 

OBJS += \
./src/banux/04_shell_commands/bg_shell.o \
./src/banux/04_shell_commands/bg_shell_commands.o \
./src/banux/04_shell_commands/shell_cmd_battery_calib.o \
./src/banux/04_shell_commands/shell_cmd_fat.o \
./src/banux/04_shell_commands/shell_cmd_flash.o \
./src/banux/04_shell_commands/shell_cmd_hwtest.o \
./src/banux/04_shell_commands/shell_cmd_lp.o \
./src/banux/04_shell_commands/shell_cmd_param.o \
./src/banux/04_shell_commands/shell_cmd_psram.o \
./src/banux/04_shell_commands/shell_cmd_soundbank.o \
./src/banux/04_shell_commands/shell_cmd_speedtest.o \
./src/banux/04_shell_commands/shell_cmd_sysmon.o \
./src/banux/04_shell_commands/shell_cmd_ui.o \
./src/banux/04_shell_commands/shell_fs.o \
./src/banux/04_shell_commands/shell_io_ble.o \
./src/banux/04_shell_commands/shell_io_manager.o 

C_DEPS += \
./src/banux/04_shell_commands/bg_shell.d \
./src/banux/04_shell_commands/bg_shell_commands.d \
./src/banux/04_shell_commands/shell_cmd_battery_calib.d \
./src/banux/04_shell_commands/shell_cmd_fat.d \
./src/banux/04_shell_commands/shell_cmd_flash.d \
./src/banux/04_shell_commands/shell_cmd_hwtest.d \
./src/banux/04_shell_commands/shell_cmd_lp.d \
./src/banux/04_shell_commands/shell_cmd_param.d \
./src/banux/04_shell_commands/shell_cmd_psram.d \
./src/banux/04_shell_commands/shell_cmd_soundbank.d \
./src/banux/04_shell_commands/shell_cmd_speedtest.d \
./src/banux/04_shell_commands/shell_cmd_sysmon.d \
./src/banux/04_shell_commands/shell_cmd_ui.d \
./src/banux/04_shell_commands/shell_fs.d \
./src/banux/04_shell_commands/shell_io_ble.d \
./src/banux/04_shell_commands/shell_io_manager.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/04_shell_commands/%.o: ../src/banux/04_shell_commands/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/device" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/adc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/gpio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/spi" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/lcd" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/power_mgr" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/04_shell_commands" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/music_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/sys_param" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/BG_AudioIO_Manager" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Audio_Processor/effects" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Envelope_Generator" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_err_handle" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_HAL" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Midi_Controller" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Soundbank" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/01_hal" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/soundbank_data" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/remind_sound" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/sdio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/event" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/sdcard" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/fat32" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


