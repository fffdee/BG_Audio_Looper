################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/drivers/bluetooth/src/audio_decoder_api.c \
../src/drivers/bluetooth/src/ble_app_callback.c \
../src/drivers/bluetooth/src/ble_app_func.c \
../src/drivers/bluetooth/src/bt_a2dp_app.c \
../src/drivers/bluetooth/src/bt_app_func.c \
../src/drivers/bluetooth/src/bt_app_interface.c \
../src/drivers/bluetooth/src/bt_avrcp_app.c \
../src/drivers/bluetooth/src/bt_ddb_flash.c \
../src/drivers/bluetooth/src/bt_hfp_app.c \
../src/drivers/bluetooth/src/bt_hid_app.c \
../src/drivers/bluetooth/src/bt_manager.c \
../src/drivers/bluetooth/src/bt_mfi_app.c \
../src/drivers/bluetooth/src/bt_pbap_app.c \
../src/drivers/bluetooth/src/bt_platform_interface.c \
../src/drivers/bluetooth/src/bt_spp_app.c \
../src/drivers/bluetooth/src/bt_stack_service.c 

OBJS += \
./src/drivers/bluetooth/src/audio_decoder_api.o \
./src/drivers/bluetooth/src/ble_app_callback.o \
./src/drivers/bluetooth/src/ble_app_func.o \
./src/drivers/bluetooth/src/bt_a2dp_app.o \
./src/drivers/bluetooth/src/bt_app_func.o \
./src/drivers/bluetooth/src/bt_app_interface.o \
./src/drivers/bluetooth/src/bt_avrcp_app.o \
./src/drivers/bluetooth/src/bt_ddb_flash.o \
./src/drivers/bluetooth/src/bt_hfp_app.o \
./src/drivers/bluetooth/src/bt_hid_app.o \
./src/drivers/bluetooth/src/bt_manager.o \
./src/drivers/bluetooth/src/bt_mfi_app.o \
./src/drivers/bluetooth/src/bt_pbap_app.o \
./src/drivers/bluetooth/src/bt_platform_interface.o \
./src/drivers/bluetooth/src/bt_spp_app.o \
./src/drivers/bluetooth/src/bt_stack_service.o 

C_DEPS += \
./src/drivers/bluetooth/src/audio_decoder_api.d \
./src/drivers/bluetooth/src/ble_app_callback.d \
./src/drivers/bluetooth/src/ble_app_func.d \
./src/drivers/bluetooth/src/bt_a2dp_app.d \
./src/drivers/bluetooth/src/bt_app_func.d \
./src/drivers/bluetooth/src/bt_app_interface.d \
./src/drivers/bluetooth/src/bt_avrcp_app.d \
./src/drivers/bluetooth/src/bt_ddb_flash.d \
./src/drivers/bluetooth/src/bt_hfp_app.d \
./src/drivers/bluetooth/src/bt_hid_app.d \
./src/drivers/bluetooth/src/bt_manager.d \
./src/drivers/bluetooth/src/bt_mfi_app.d \
./src/drivers/bluetooth/src/bt_pbap_app.d \
./src/drivers/bluetooth/src/bt_platform_interface.d \
./src/drivers/bluetooth/src/bt_spp_app.d \
./src/drivers/bluetooth/src/bt_stack_service.d 


# Each subdirectory must supply rules for building sources it contributes
src/drivers/bluetooth/src/%.o: ../src/drivers/bluetooth/src/%.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/drivers/bluetooth/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/drivers/bluetooth" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/drivers/usb/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


