################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/02_device_drivers/USB/src/audio_api.c \
../src/banux/02_device_drivers/USB/src/otg_device_audio.c \
../src/banux/02_device_drivers/USB/src/otg_device_cdc.c \
../src/banux/02_device_drivers/USB/src/otg_device_standard_request.c \
../src/banux/02_device_drivers/USB/src/otg_device_stor.c \
../src/banux/02_device_drivers/USB/src/shell_io_cdc.c \
../src/banux/02_device_drivers/USB/src/usb_audio_api.c 

OBJS += \
./src/banux/02_device_drivers/USB/src/audio_api.o \
./src/banux/02_device_drivers/USB/src/otg_device_audio.o \
./src/banux/02_device_drivers/USB/src/otg_device_cdc.o \
./src/banux/02_device_drivers/USB/src/otg_device_standard_request.o \
./src/banux/02_device_drivers/USB/src/otg_device_stor.o \
./src/banux/02_device_drivers/USB/src/shell_io_cdc.o \
./src/banux/02_device_drivers/USB/src/usb_audio_api.o 

C_DEPS += \
./src/banux/02_device_drivers/USB/src/audio_api.d \
./src/banux/02_device_drivers/USB/src/otg_device_audio.d \
./src/banux/02_device_drivers/USB/src/otg_device_cdc.d \
./src/banux/02_device_drivers/USB/src/otg_device_standard_request.d \
./src/banux/02_device_drivers/USB/src/otg_device_stor.d \
./src/banux/02_device_drivers/USB/src/shell_io_cdc.d \
./src/banux/02_device_drivers/USB/src/usb_audio_api.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/02_device_drivers/USB/src/%.o: ../src/banux/02_device_drivers/USB/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/device" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/adc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/gpio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/spi" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/lcd" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/power_mgr" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/04_shell_commands" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/music_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/sys_param" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/BG_AudioIO_Manager" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/remind_sound" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/sdio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/event" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/sdcard" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/fat32" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


