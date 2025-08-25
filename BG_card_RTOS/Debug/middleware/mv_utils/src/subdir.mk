################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/src/debug.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/src/mcu_circular_buf.c 

OBJS += \
./middleware/mv_utils/src/debug.o \
./middleware/mv_utils/src/mcu_circular_buf.o 

C_DEPS += \
./middleware/mv_utils/src/debug.d \
./middleware/mv_utils/src/mcu_circular_buf.d 


# Each subdirectory must supply rules for building sources it contributes
middleware/mv_utils/src/debug.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/src/debug.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/mv_utils/src/mcu_circular_buf.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/src/mcu_circular_buf.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


