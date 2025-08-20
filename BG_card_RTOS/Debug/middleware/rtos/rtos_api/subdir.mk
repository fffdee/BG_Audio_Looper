################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/BanGO/Desktop/BG_card_RTOS/MVsB1_Base_SDK/middleware/rtos/rtos_api/rtos_api.c 

OBJS += \
./middleware/rtos/rtos_api/rtos_api.o 

C_DEPS += \
./middleware/rtos/rtos_api/rtos_api.d 


# Each subdirectory must supply rules for building sources it contributes
middleware/rtos/rtos_api/rtos_api.o: /cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/MVsB1_Base_SDK/middleware/rtos/rtos_api/rtos_api.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_card_RTOS/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/BanGO/Desktop/BG_card_RTOS/BG_card_RTOS/src/BanGUI/page" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


