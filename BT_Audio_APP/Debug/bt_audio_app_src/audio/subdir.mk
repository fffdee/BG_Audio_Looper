################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../bt_audio_app_src/audio/audio_adjust.c \
../bt_audio_app_src/audio/audio_aec.c \
../bt_audio_app_src/audio/audio_common.c \
../bt_audio_app_src/audio/audio_effect.c \
../bt_audio_app_src/audio/audio_effect_process.c \
../bt_audio_app_src/audio/audio_vol.c \
../bt_audio_app_src/audio/beep.c \
../bt_audio_app_src/audio/communication.c \
../bt_audio_app_src/audio/ctrlvars.c \
../bt_audio_app_src/audio/eq_params.c 

OBJS += \
./bt_audio_app_src/audio/audio_adjust.o \
./bt_audio_app_src/audio/audio_aec.o \
./bt_audio_app_src/audio/audio_common.o \
./bt_audio_app_src/audio/audio_effect.o \
./bt_audio_app_src/audio/audio_effect_process.o \
./bt_audio_app_src/audio/audio_vol.o \
./bt_audio_app_src/audio/beep.o \
./bt_audio_app_src/audio/communication.o \
./bt_audio_app_src/audio/ctrlvars.o \
./bt_audio_app_src/audio/eq_params.o 

C_DEPS += \
./bt_audio_app_src/audio/audio_adjust.d \
./bt_audio_app_src/audio/audio_aec.d \
./bt_audio_app_src/audio/audio_common.d \
./bt_audio_app_src/audio/audio_effect.d \
./bt_audio_app_src/audio/audio_effect_process.d \
./bt_audio_app_src/audio/audio_vol.d \
./bt_audio_app_src/audio/beep.d \
./bt_audio_app_src/audio/communication.d \
./bt_audio_app_src/audio/ctrlvars.d \
./bt_audio_app_src/audio/eq_params.d 


# Each subdirectory must supply rules for building sources it contributes
bt_audio_app_src/audio/%.o: ../bt_audio_app_src/audio/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DFUNC_OS_EN=1 -DCFG_APP_CONFIG -DHAVE_CONFIG_H -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/md5" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/include/speex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/include" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/libspeex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc/otg" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/display" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/user" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ble" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/include" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/app" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/celt" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk/fixed" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk/float" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/xiaomi_ai" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


