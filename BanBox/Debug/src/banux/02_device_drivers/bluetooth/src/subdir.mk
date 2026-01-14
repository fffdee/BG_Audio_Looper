################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/02_device_drivers/bluetooth/src/audio_decoder_api.c \
../src/banux/02_device_drivers/bluetooth/src/ble_app_callback.c \
../src/banux/02_device_drivers/bluetooth/src/ble_app_func.c \
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
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/device" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/adc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/gpio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/spi" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/flash" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/power_mgr" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/03_driver_framework/core" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/03_driver_framework" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/04_shell_commands" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/audio/music_parameter" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_looper" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/sys_param" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/BG_AudioIO_Manager" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_spectrum" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/03_driver_framework/drivers" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_vfs" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/effect_graph" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/components" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/core" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/resources" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui/views" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


