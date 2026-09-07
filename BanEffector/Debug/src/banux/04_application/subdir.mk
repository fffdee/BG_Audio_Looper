################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/04_application/app_sys_handler.c \
../src/banux/04_application/main.c \
../src/banux/04_application/shell_cmd_battery_calib.c \
../src/banux/04_application/shell_cmd_effect.c \
../src/banux/04_application/shell_cmd_flash.c \
../src/banux/04_application/shell_cmd_mode.c \
../src/banux/04_application/shell_cmd_param.c \
../src/banux/04_application/shell_cmd_psram.c \
../src/banux/04_application/shell_cmd_sysmon.c 

OBJS += \
./src/banux/04_application/app_sys_handler.o \
./src/banux/04_application/main.o \
./src/banux/04_application/shell_cmd_battery_calib.o \
./src/banux/04_application/shell_cmd_effect.o \
./src/banux/04_application/shell_cmd_flash.o \
./src/banux/04_application/shell_cmd_mode.o \
./src/banux/04_application/shell_cmd_param.o \
./src/banux/04_application/shell_cmd_psram.o \
./src/banux/04_application/shell_cmd_sysmon.o 

C_DEPS += \
./src/banux/04_application/app_sys_handler.d \
./src/banux/04_application/main.d \
./src/banux/04_application/shell_cmd_battery_calib.d \
./src/banux/04_application/shell_cmd_effect.d \
./src/banux/04_application/shell_cmd_flash.d \
./src/banux/04_application/shell_cmd_mode.d \
./src/banux/04_application/shell_cmd_param.d \
./src/banux/04_application/shell_cmd_psram.d \
./src/banux/04_application/shell_cmd_sysmon.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/04_application/%.o: ../src/banux/04_application/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/00_core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/adc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/gpio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/sdio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/spi" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/library" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/library/legacy_flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/power_mgr" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/usb/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/command_line" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/command_parser" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework/vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/event" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/file_io" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/app_audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/BG_AudioIO_Manager" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/effect_graph" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/04_application" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/05_component/ble_app" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/05_component/firmware_upgrade" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/05_component/sys_led" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/05_component/sys_param" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/05_component/sys_state" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/startup" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


