################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/01_driver/library/drv_battery.c \
../src/banux/01_driver/library/drv_psram.c \
../src/banux/01_driver/library/drv_sdcard.c \
../src/banux/01_driver/library/drv_usb_cdc.c \
../src/banux/01_driver/library/drv_w25n02.c \
../src/banux/01_driver/library/drv_w25qxx.c 

OBJS += \
./src/banux/01_driver/library/drv_battery.o \
./src/banux/01_driver/library/drv_psram.o \
./src/banux/01_driver/library/drv_sdcard.o \
./src/banux/01_driver/library/drv_usb_cdc.o \
./src/banux/01_driver/library/drv_w25n02.o \
./src/banux/01_driver/library/drv_w25qxx.o 

C_DEPS += \
./src/banux/01_driver/library/drv_battery.d \
./src/banux/01_driver/library/drv_psram.d \
./src/banux/01_driver/library/drv_sdcard.d \
./src/banux/01_driver/library/drv_usb_cdc.d \
./src/banux/01_driver/library/drv_w25n02.d \
./src/banux/01_driver/library/drv_w25qxx.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/01_driver/library/%.o: ../src/banux/01_driver/library/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/00_core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/adc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/gpio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/sdio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/spi" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/library" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/library/legacy_flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/power_mgr" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/usb/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/command_line" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/command_parser" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework/vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/event" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/file_io" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/app_audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/BG_AudioIO_Manager" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/effect_graph" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/04_application" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/ble_app" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/firmware_upgrade" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/sys_led" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/sys_param" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/sys_state" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/startup" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


