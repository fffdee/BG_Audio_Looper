################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hardware/USB/src/audio_api.c \
../src/hardware/USB/src/otg_device_audio.c \
../src/hardware/USB/src/otg_device_standard_request.c \
../src/hardware/USB/src/otg_device_stor.c \
../src/hardware/USB/src/usb_audio_api.c 

OBJS += \
./src/hardware/USB/src/audio_api.o \
./src/hardware/USB/src/otg_device_audio.o \
./src/hardware/USB/src/otg_device_standard_request.o \
./src/hardware/USB/src/otg_device_stor.o \
./src/hardware/USB/src/usb_audio_api.o 

C_DEPS += \
./src/hardware/USB/src/audio_api.d \
./src/hardware/USB/src/otg_device_audio.d \
./src/hardware/USB/src/otg_device_standard_request.d \
./src/hardware/USB/src/otg_device_stor.d \
./src/hardware/USB/src/usb_audio_api.d 


# Each subdirectory must supply rules for building sources it contributes
src/hardware/USB/src/%.o: ../src/hardware/USB/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


