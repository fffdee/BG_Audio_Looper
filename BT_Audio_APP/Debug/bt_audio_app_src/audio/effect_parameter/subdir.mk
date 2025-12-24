################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../bt_audio_app_src/audio/effect_parameter/AECBuf.c \
../bt_audio_app_src/audio/effect_parameter/DianYin.c \
../bt_audio_app_src/audio/effect_parameter/HanMai.c \
../bt_audio_app_src/audio/effect_parameter/HunXiang.c \
../bt_audio_app_src/audio/effect_parameter/MoYin.c \
../bt_audio_app_src/audio/effect_parameter/NanBianNv.c \
../bt_audio_app_src/audio/effect_parameter/NvBianNan.c \
../bt_audio_app_src/audio/effect_parameter/WaWaYin.c \
../bt_audio_app_src/audio/effect_parameter/YuanSheng.c 

OBJS += \
./bt_audio_app_src/audio/effect_parameter/AECBuf.o \
./bt_audio_app_src/audio/effect_parameter/DianYin.o \
./bt_audio_app_src/audio/effect_parameter/HanMai.o \
./bt_audio_app_src/audio/effect_parameter/HunXiang.o \
./bt_audio_app_src/audio/effect_parameter/MoYin.o \
./bt_audio_app_src/audio/effect_parameter/NanBianNv.o \
./bt_audio_app_src/audio/effect_parameter/NvBianNan.o \
./bt_audio_app_src/audio/effect_parameter/WaWaYin.o \
./bt_audio_app_src/audio/effect_parameter/YuanSheng.o 

C_DEPS += \
./bt_audio_app_src/audio/effect_parameter/AECBuf.d \
./bt_audio_app_src/audio/effect_parameter/DianYin.d \
./bt_audio_app_src/audio/effect_parameter/HanMai.d \
./bt_audio_app_src/audio/effect_parameter/HunXiang.d \
./bt_audio_app_src/audio/effect_parameter/MoYin.d \
./bt_audio_app_src/audio/effect_parameter/NanBianNv.d \
./bt_audio_app_src/audio/effect_parameter/NvBianNan.d \
./bt_audio_app_src/audio/effect_parameter/WaWaYin.d \
./bt_audio_app_src/audio/effect_parameter/YuanSheng.d 


# Each subdirectory must supply rules for building sources it contributes
bt_audio_app_src/audio/effect_parameter/%.o: ../bt_audio_app_src/audio/effect_parameter/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DFUNC_OS_EN=1 -DCFG_APP_CONFIG -DHAVE_CONFIG_H -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/md5" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/include/speex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/include" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex/libspeex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai/speex" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ai" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc/otg" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/display" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/user" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/ble" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/include" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/app" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/celt" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk/fixed" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/libopus/silk/float" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BT_Audio_APP/bt_audio_app_src/xiaomi_ai" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


