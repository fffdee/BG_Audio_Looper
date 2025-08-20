################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hardware/audio/effect_parameter/AECBuf.c \
../src/hardware/audio/effect_parameter/DianYin.c \
../src/hardware/audio/effect_parameter/HanMai.c \
../src/hardware/audio/effect_parameter/HunXiang.c \
../src/hardware/audio/effect_parameter/MoYin.c \
../src/hardware/audio/effect_parameter/NanBianNv.c \
../src/hardware/audio/effect_parameter/NvBianNan.c \
../src/hardware/audio/effect_parameter/UsbAecBuf.c \
../src/hardware/audio/effect_parameter/WaWaYin.c \
../src/hardware/audio/effect_parameter/YuanSheng.c 

OBJS += \
./src/hardware/audio/effect_parameter/AECBuf.o \
./src/hardware/audio/effect_parameter/DianYin.o \
./src/hardware/audio/effect_parameter/HanMai.o \
./src/hardware/audio/effect_parameter/HunXiang.o \
./src/hardware/audio/effect_parameter/MoYin.o \
./src/hardware/audio/effect_parameter/NanBianNv.o \
./src/hardware/audio/effect_parameter/NvBianNan.o \
./src/hardware/audio/effect_parameter/UsbAecBuf.o \
./src/hardware/audio/effect_parameter/WaWaYin.o \
./src/hardware/audio/effect_parameter/YuanSheng.o 

C_DEPS += \
./src/hardware/audio/effect_parameter/AECBuf.d \
./src/hardware/audio/effect_parameter/DianYin.d \
./src/hardware/audio/effect_parameter/HanMai.d \
./src/hardware/audio/effect_parameter/HunXiang.d \
./src/hardware/audio/effect_parameter/MoYin.d \
./src/hardware/audio/effect_parameter/NanBianNv.d \
./src/hardware/audio/effect_parameter/NvBianNan.d \
./src/hardware/audio/effect_parameter/UsbAecBuf.d \
./src/hardware/audio/effect_parameter/WaWaYin.d \
./src/hardware/audio/effect_parameter/YuanSheng.d 


# Each subdirectory must supply rules for building sources it contributes
src/hardware/audio/effect_parameter/%.o: ../src/hardware/audio/effect_parameter/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/BanGUI/page" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


