################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/bits.c \
C:/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/libmp2dec.c \
C:/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/mvstdio.c 

OBJS += \
./middleware/audio/src/bits.o \
./middleware/audio/src/libmp2dec.o \
./middleware/audio/src/mvstdio.o 

C_DEPS += \
./middleware/audio/src/bits.d \
./middleware/audio/src/libmp2dec.d \
./middleware/audio/src/mvstdio.d 


# Each subdirectory must supply rules for building sources it contributes
middleware/audio/src/bits.o: /cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/bits.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/audio/src/libmp2dec.o: /cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/libmp2dec.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/audio/src/mvstdio.o: /cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/mvstdio.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/BanGO/Desktop/BG_Audio_Looper/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


