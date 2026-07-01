################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/06_app/audio/effect_parameter/AECBuf.c \
../src/banux/06_app/audio/effect_parameter/DianYin.c \
../src/banux/06_app/audio/effect_parameter/HanMai.c \
../src/banux/06_app/audio/effect_parameter/HunXiang.c \
../src/banux/06_app/audio/effect_parameter/MoYin.c \
../src/banux/06_app/audio/effect_parameter/NanBianNv.c \
../src/banux/06_app/audio/effect_parameter/NvBianNan.c \
../src/banux/06_app/audio/effect_parameter/UsbAecBuf.c \
../src/banux/06_app/audio/effect_parameter/WaWaYin.c \
../src/banux/06_app/audio/effect_parameter/YuanSheng.c 

OBJS += \
./src/banux/06_app/audio/effect_parameter/AECBuf.o \
./src/banux/06_app/audio/effect_parameter/DianYin.o \
./src/banux/06_app/audio/effect_parameter/HanMai.o \
./src/banux/06_app/audio/effect_parameter/HunXiang.o \
./src/banux/06_app/audio/effect_parameter/MoYin.o \
./src/banux/06_app/audio/effect_parameter/NanBianNv.o \
./src/banux/06_app/audio/effect_parameter/NvBianNan.o \
./src/banux/06_app/audio/effect_parameter/UsbAecBuf.o \
./src/banux/06_app/audio/effect_parameter/WaWaYin.o \
./src/banux/06_app/audio/effect_parameter/YuanSheng.o 

C_DEPS += \
./src/banux/06_app/audio/effect_parameter/AECBuf.d \
./src/banux/06_app/audio/effect_parameter/DianYin.d \
./src/banux/06_app/audio/effect_parameter/HanMai.d \
./src/banux/06_app/audio/effect_parameter/HunXiang.d \
./src/banux/06_app/audio/effect_parameter/MoYin.d \
./src/banux/06_app/audio/effect_parameter/NanBianNv.d \
./src/banux/06_app/audio/effect_parameter/NvBianNan.d \
./src/banux/06_app/audio/effect_parameter/UsbAecBuf.d \
./src/banux/06_app/audio/effect_parameter/WaWaYin.d \
./src/banux/06_app/audio/effect_parameter/YuanSheng.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/06_app/audio/effect_parameter/%.o: ../src/banux/06_app/audio/effect_parameter/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio/music_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/04_shell_commands" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


