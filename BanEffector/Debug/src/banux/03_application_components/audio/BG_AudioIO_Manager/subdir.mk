################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_detection.c \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_init.c \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_io_core.c \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_io_manager.c \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_loop.c \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_effects.c \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_io.c \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_setup.c \
../src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_low_power.c 

OBJS += \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_detection.o \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_init.o \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_io_core.o \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_io_manager.o \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_loop.o \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_effects.o \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_io.o \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_setup.o \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_low_power.o 

C_DEPS += \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_detection.d \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_init.d \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_io_core.d \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_io_manager.d \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_audio_loop.d \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_effects.d \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_io.d \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_graph_setup.d \
./src/banux/03_application_components/audio/BG_AudioIO_Manager/bg_low_power.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/03_application_components/audio/BG_AudioIO_Manager/%.o: ../src/banux/03_application_components/audio/BG_AudioIO_Manager/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/00_core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/adc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/gpio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/sdio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/hal/spi" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/library" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/library/legacy_flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/power_mgr" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/usb/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/01_driver/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/command_line" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/command_parser" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/driver_framework/vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/event" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/02_system_components/file_io" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/app_audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/BG_AudioIO_Manager" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/audio/effect_graph" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/04_application" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/ble_app" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/firmware_upgrade" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/sys_led" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/sys_param" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src/banux/03_application_components/sys_state" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanEffector/startup" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


