################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/bluetooth/src/audio_decoder_api.c \
../src/bluetooth/src/ble_app_callback.c \
../src/bluetooth/src/ble_app_func.c \
../src/bluetooth/src/bt_a2dp_app.c \
../src/bluetooth/src/bt_app_func.c \
../src/bluetooth/src/bt_app_interface.c \
../src/bluetooth/src/bt_avrcp_app.c \
../src/bluetooth/src/bt_ddb_flash.c \
../src/bluetooth/src/bt_hfp_app.c \
../src/bluetooth/src/bt_hid_app.c \
../src/bluetooth/src/bt_manager.c \
../src/bluetooth/src/bt_mfi_app.c \
../src/bluetooth/src/bt_pbap_app.c \
../src/bluetooth/src/bt_platform_interface.c \
../src/bluetooth/src/bt_play_api.c \
../src/bluetooth/src/bt_spp_app.c \
../src/bluetooth/src/bt_stack_service.c 

OBJS += \
./src/bluetooth/src/audio_decoder_api.o \
./src/bluetooth/src/ble_app_callback.o \
./src/bluetooth/src/ble_app_func.o \
./src/bluetooth/src/bt_a2dp_app.o \
./src/bluetooth/src/bt_app_func.o \
./src/bluetooth/src/bt_app_interface.o \
./src/bluetooth/src/bt_avrcp_app.o \
./src/bluetooth/src/bt_ddb_flash.o \
./src/bluetooth/src/bt_hfp_app.o \
./src/bluetooth/src/bt_hid_app.o \
./src/bluetooth/src/bt_manager.o \
./src/bluetooth/src/bt_mfi_app.o \
./src/bluetooth/src/bt_pbap_app.o \
./src/bluetooth/src/bt_platform_interface.o \
./src/bluetooth/src/bt_play_api.o \
./src/bluetooth/src/bt_spp_app.o \
./src/bluetooth/src/bt_stack_service.o 

C_DEPS += \
./src/bluetooth/src/audio_decoder_api.d \
./src/bluetooth/src/ble_app_callback.d \
./src/bluetooth/src/ble_app_func.d \
./src/bluetooth/src/bt_a2dp_app.d \
./src/bluetooth/src/bt_app_func.d \
./src/bluetooth/src/bt_app_interface.d \
./src/bluetooth/src/bt_avrcp_app.d \
./src/bluetooth/src/bt_ddb_flash.d \
./src/bluetooth/src/bt_hfp_app.d \
./src/bluetooth/src/bt_hid_app.d \
./src/bluetooth/src/bt_manager.d \
./src/bluetooth/src/bt_mfi_app.d \
./src/bluetooth/src/bt_pbap_app.d \
./src/bluetooth/src/bt_platform_interface.d \
./src/bluetooth/src/bt_play_api.d \
./src/bluetooth/src/bt_spp_app.d \
./src/bluetooth/src/bt_stack_service.d 


# Each subdirectory must supply rules for building sources it contributes
src/bluetooth/src/%.o: ../src/bluetooth/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/hardware/audio_looper" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_RTOS/src/bluetooth/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


