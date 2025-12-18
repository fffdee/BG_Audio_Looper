################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hardware/bluetooth/src/audio_decoder_api.c \
../src/hardware/bluetooth/src/ble_app_callback.c \
../src/hardware/bluetooth/src/ble_app_func.c \
../src/hardware/bluetooth/src/bt_a2dp_app.c \
../src/hardware/bluetooth/src/bt_app_func.c \
../src/hardware/bluetooth/src/bt_app_interface.c \
../src/hardware/bluetooth/src/bt_avrcp_app.c \
../src/hardware/bluetooth/src/bt_ddb_flash.c \
../src/hardware/bluetooth/src/bt_hfp_app.c \
../src/hardware/bluetooth/src/bt_hid_app.c \
../src/hardware/bluetooth/src/bt_manager.c \
../src/hardware/bluetooth/src/bt_mfi_app.c \
../src/hardware/bluetooth/src/bt_pbap_app.c \
../src/hardware/bluetooth/src/bt_platform_interface.c \
../src/hardware/bluetooth/src/bt_spp_app.c \
../src/hardware/bluetooth/src/bt_stack_service.c \
../src/hardware/bluetooth/src/shell_io_ble.c 

OBJS += \
./src/hardware/bluetooth/src/audio_decoder_api.o \
./src/hardware/bluetooth/src/ble_app_callback.o \
./src/hardware/bluetooth/src/ble_app_func.o \
./src/hardware/bluetooth/src/bt_a2dp_app.o \
./src/hardware/bluetooth/src/bt_app_func.o \
./src/hardware/bluetooth/src/bt_app_interface.o \
./src/hardware/bluetooth/src/bt_avrcp_app.o \
./src/hardware/bluetooth/src/bt_ddb_flash.o \
./src/hardware/bluetooth/src/bt_hfp_app.o \
./src/hardware/bluetooth/src/bt_hid_app.o \
./src/hardware/bluetooth/src/bt_manager.o \
./src/hardware/bluetooth/src/bt_mfi_app.o \
./src/hardware/bluetooth/src/bt_pbap_app.o \
./src/hardware/bluetooth/src/bt_platform_interface.o \
./src/hardware/bluetooth/src/bt_spp_app.o \
./src/hardware/bluetooth/src/bt_stack_service.o \
./src/hardware/bluetooth/src/shell_io_ble.o 

C_DEPS += \
./src/hardware/bluetooth/src/audio_decoder_api.d \
./src/hardware/bluetooth/src/ble_app_callback.d \
./src/hardware/bluetooth/src/ble_app_func.d \
./src/hardware/bluetooth/src/bt_a2dp_app.d \
./src/hardware/bluetooth/src/bt_app_func.d \
./src/hardware/bluetooth/src/bt_app_interface.d \
./src/hardware/bluetooth/src/bt_avrcp_app.d \
./src/hardware/bluetooth/src/bt_ddb_flash.d \
./src/hardware/bluetooth/src/bt_hfp_app.d \
./src/hardware/bluetooth/src/bt_hid_app.d \
./src/hardware/bluetooth/src/bt_manager.d \
./src/hardware/bluetooth/src/bt_mfi_app.d \
./src/hardware/bluetooth/src/bt_pbap_app.d \
./src/hardware/bluetooth/src/bt_platform_interface.d \
./src/hardware/bluetooth/src/bt_spp_app.d \
./src/hardware/bluetooth/src/bt_stack_service.d \
./src/hardware/bluetooth/src/shell_io_ble.d 


# Each subdirectory must supply rules for building sources it contributes
src/hardware/bluetooth/src/%.o: ../src/hardware/bluetooth/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/BG_List" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/page" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/audio" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_Lcd" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/base_func" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/IIC" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager/USB/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager/USB/src" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_Encoder" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_flash_manager" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/bluetooth/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/menu_slider" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/audio_looper" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/sys_param" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/ui_system" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


