################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../bt_audio_app_src/apps/browser_parallel.c \
../bt_audio_app_src/apps/browser_tree.c \
../bt_audio_app_src/apps/bt_hf_api.c \
../bt_audio_app_src/apps/bt_hf_mode.c \
../bt_audio_app_src/apps/bt_obex_upgrade.c \
../bt_audio_app_src/apps/bt_play_api.c \
../bt_audio_app_src/apps/bt_play_mode.c \
../bt_audio_app_src/apps/bt_record_api.c \
../bt_audio_app_src/apps/bt_record_mode.c \
../bt_audio_app_src/apps/hdmi_in_api.c \
../bt_audio_app_src/apps/hdmi_in_mode.c \
../bt_audio_app_src/apps/i2sin_mode.c \
../bt_audio_app_src/apps/linein_mode.c \
../bt_audio_app_src/apps/main_task.c \
../bt_audio_app_src/apps/media_play_api.c \
../bt_audio_app_src/apps/media_play_mode.c \
../bt_audio_app_src/apps/mode_switch_api.c \
../bt_audio_app_src/apps/otg_device_audio.c \
../bt_audio_app_src/apps/radio_mode.c \
../bt_audio_app_src/apps/rest_mode.c \
../bt_audio_app_src/apps/spdif_mode.c \
../bt_audio_app_src/apps/usb_audio_mode.c \
../bt_audio_app_src/apps/waiting_mode.c 

OBJS += \
./bt_audio_app_src/apps/browser_parallel.o \
./bt_audio_app_src/apps/browser_tree.o \
./bt_audio_app_src/apps/bt_hf_api.o \
./bt_audio_app_src/apps/bt_hf_mode.o \
./bt_audio_app_src/apps/bt_obex_upgrade.o \
./bt_audio_app_src/apps/bt_play_api.o \
./bt_audio_app_src/apps/bt_play_mode.o \
./bt_audio_app_src/apps/bt_record_api.o \
./bt_audio_app_src/apps/bt_record_mode.o \
./bt_audio_app_src/apps/hdmi_in_api.o \
./bt_audio_app_src/apps/hdmi_in_mode.o \
./bt_audio_app_src/apps/i2sin_mode.o \
./bt_audio_app_src/apps/linein_mode.o \
./bt_audio_app_src/apps/main_task.o \
./bt_audio_app_src/apps/media_play_api.o \
./bt_audio_app_src/apps/media_play_mode.o \
./bt_audio_app_src/apps/mode_switch_api.o \
./bt_audio_app_src/apps/otg_device_audio.o \
./bt_audio_app_src/apps/radio_mode.o \
./bt_audio_app_src/apps/rest_mode.o \
./bt_audio_app_src/apps/spdif_mode.o \
./bt_audio_app_src/apps/usb_audio_mode.o \
./bt_audio_app_src/apps/waiting_mode.o 

C_DEPS += \
./bt_audio_app_src/apps/browser_parallel.d \
./bt_audio_app_src/apps/browser_tree.d \
./bt_audio_app_src/apps/bt_hf_api.d \
./bt_audio_app_src/apps/bt_hf_mode.d \
./bt_audio_app_src/apps/bt_obex_upgrade.d \
./bt_audio_app_src/apps/bt_play_api.d \
./bt_audio_app_src/apps/bt_play_mode.d \
./bt_audio_app_src/apps/bt_record_api.d \
./bt_audio_app_src/apps/bt_record_mode.d \
./bt_audio_app_src/apps/hdmi_in_api.d \
./bt_audio_app_src/apps/hdmi_in_mode.d \
./bt_audio_app_src/apps/i2sin_mode.d \
./bt_audio_app_src/apps/linein_mode.d \
./bt_audio_app_src/apps/main_task.d \
./bt_audio_app_src/apps/media_play_api.d \
./bt_audio_app_src/apps/media_play_mode.d \
./bt_audio_app_src/apps/mode_switch_api.d \
./bt_audio_app_src/apps/otg_device_audio.d \
./bt_audio_app_src/apps/radio_mode.d \
./bt_audio_app_src/apps/rest_mode.d \
./bt_audio_app_src/apps/spdif_mode.d \
./bt_audio_app_src/apps/usb_audio_mode.d \
./bt_audio_app_src/apps/waiting_mode.d 


# Each subdirectory must supply rules for building sources it contributes
bt_audio_app_src/apps/%.o: ../bt_audio_app_src/apps/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DFUNC_OS_EN=1 -DCFG_APP_CONFIG -DHAVE_CONFIG_H -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/md5" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/include/speex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/include" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/libspeex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc/otg" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/display" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/user" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ble" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/include" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/app" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/celt" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk/fixed" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk/float" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/xiaomi_ai" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


