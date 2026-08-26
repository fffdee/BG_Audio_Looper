################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/02_device_drivers/bluetooth/src/audio_decoder_api.c \
../src/banux/02_device_drivers/bluetooth/src/ble_app_callback.c \
../src/banux/02_device_drivers/bluetooth/src/ble_app_func.c \
../src/banux/02_device_drivers/bluetooth/src/ble_protocol.c \
../src/banux/02_device_drivers/bluetooth/src/bt_a2dp_app.c \
../src/banux/02_device_drivers/bluetooth/src/bt_app_func.c \
../src/banux/02_device_drivers/bluetooth/src/bt_app_interface.c \
../src/banux/02_device_drivers/bluetooth/src/bt_avrcp_app.c \
../src/banux/02_device_drivers/bluetooth/src/bt_ddb_flash.c \
../src/banux/02_device_drivers/bluetooth/src/bt_hfp_app.c \
../src/banux/02_device_drivers/bluetooth/src/bt_hid_app.c \
../src/banux/02_device_drivers/bluetooth/src/bt_manager.c \
../src/banux/02_device_drivers/bluetooth/src/bt_mfi_app.c \
../src/banux/02_device_drivers/bluetooth/src/bt_pbap_app.c \
../src/banux/02_device_drivers/bluetooth/src/bt_platform_interface.c \
../src/banux/02_device_drivers/bluetooth/src/bt_spp_app.c \
../src/banux/02_device_drivers/bluetooth/src/bt_stack_service.c 

OBJS += \
./src/banux/02_device_drivers/bluetooth/src/audio_decoder_api.o \
./src/banux/02_device_drivers/bluetooth/src/ble_app_callback.o \
./src/banux/02_device_drivers/bluetooth/src/ble_app_func.o \
./src/banux/02_device_drivers/bluetooth/src/ble_protocol.o \
./src/banux/02_device_drivers/bluetooth/src/bt_a2dp_app.o \
./src/banux/02_device_drivers/bluetooth/src/bt_app_func.o \
./src/banux/02_device_drivers/bluetooth/src/bt_app_interface.o \
./src/banux/02_device_drivers/bluetooth/src/bt_avrcp_app.o \
./src/banux/02_device_drivers/bluetooth/src/bt_ddb_flash.o \
./src/banux/02_device_drivers/bluetooth/src/bt_hfp_app.o \
./src/banux/02_device_drivers/bluetooth/src/bt_hid_app.o \
./src/banux/02_device_drivers/bluetooth/src/bt_manager.o \
./src/banux/02_device_drivers/bluetooth/src/bt_mfi_app.o \
./src/banux/02_device_drivers/bluetooth/src/bt_pbap_app.o \
./src/banux/02_device_drivers/bluetooth/src/bt_platform_interface.o \
./src/banux/02_device_drivers/bluetooth/src/bt_spp_app.o \
./src/banux/02_device_drivers/bluetooth/src/bt_stack_service.o 

C_DEPS += \
./src/banux/02_device_drivers/bluetooth/src/audio_decoder_api.d \
./src/banux/02_device_drivers/bluetooth/src/ble_app_callback.d \
./src/banux/02_device_drivers/bluetooth/src/ble_app_func.d \
./src/banux/02_device_drivers/bluetooth/src/ble_protocol.d \
./src/banux/02_device_drivers/bluetooth/src/bt_a2dp_app.d \
./src/banux/02_device_drivers/bluetooth/src/bt_app_func.d \
./src/banux/02_device_drivers/bluetooth/src/bt_app_interface.d \
./src/banux/02_device_drivers/bluetooth/src/bt_avrcp_app.d \
./src/banux/02_device_drivers/bluetooth/src/bt_ddb_flash.d \
./src/banux/02_device_drivers/bluetooth/src/bt_hfp_app.d \
./src/banux/02_device_drivers/bluetooth/src/bt_hid_app.d \
./src/banux/02_device_drivers/bluetooth/src/bt_manager.d \
./src/banux/02_device_drivers/bluetooth/src/bt_mfi_app.d \
./src/banux/02_device_drivers/bluetooth/src/bt_pbap_app.d \
./src/banux/02_device_drivers/bluetooth/src/bt_platform_interface.d \
./src/banux/02_device_drivers/bluetooth/src/bt_spp_app.d \
./src/banux/02_device_drivers/bluetooth/src/bt_stack_service.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/02_device_drivers/bluetooth/src/%.o: ../src/banux/02_device_drivers/bluetooth/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/device" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/adc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/gpio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/spi" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/lcd" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/power_mgr" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/04_shell_commands" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/startup" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/music_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/sys_param" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/06_app/BG_AudioIO_Manager" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Audio_Processor" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Audio_Processor/effects" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Envelope_Generator" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_err_handle" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_HAL" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Midi_Controller" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/BG_Soundbank" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/01_hal" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/drum_machine" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/03_app/drum_machine" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/03_app/synth_node" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/midi" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/soundbank" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/synth_integration" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/psram_buffer" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/nand_store" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/fat32" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/sampler" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/envelope" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/soundbank_data" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/remind_sound" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/sdio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/event" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/sdcard" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/05_component/fat32" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


