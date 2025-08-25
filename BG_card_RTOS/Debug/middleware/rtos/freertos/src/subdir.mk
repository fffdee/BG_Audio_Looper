################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/croutine.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/event_groups.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/heap_5s.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/list.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/port.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/portISR.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/queue.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/tasks.c \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/timers.c 

S_UPPER_SRCS += \
C:/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/os_cpu_a.S 

OBJS += \
./middleware/rtos/freertos/src/croutine.o \
./middleware/rtos/freertos/src/event_groups.o \
./middleware/rtos/freertos/src/heap_5s.o \
./middleware/rtos/freertos/src/list.o \
./middleware/rtos/freertos/src/os_cpu_a.o \
./middleware/rtos/freertos/src/port.o \
./middleware/rtos/freertos/src/portISR.o \
./middleware/rtos/freertos/src/queue.o \
./middleware/rtos/freertos/src/tasks.o \
./middleware/rtos/freertos/src/timers.o 

C_DEPS += \
./middleware/rtos/freertos/src/croutine.d \
./middleware/rtos/freertos/src/event_groups.d \
./middleware/rtos/freertos/src/heap_5s.d \
./middleware/rtos/freertos/src/list.d \
./middleware/rtos/freertos/src/port.d \
./middleware/rtos/freertos/src/portISR.d \
./middleware/rtos/freertos/src/queue.d \
./middleware/rtos/freertos/src/tasks.d \
./middleware/rtos/freertos/src/timers.d 

S_UPPER_DEPS += \
./middleware/rtos/freertos/src/os_cpu_a.d 


# Each subdirectory must supply rules for building sources it contributes
middleware/rtos/freertos/src/croutine.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/croutine.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/event_groups.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/event_groups.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/heap_5s.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/heap_5s.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/list.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/list.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/os_cpu_a.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/os_cpu_a.S
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/port.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/port.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/portISR.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/portISR.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/queue.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/queue.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/tasks.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/tasks.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

middleware/rtos/freertos/src/timers.o: /cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/src/timers.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/USB/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/base_func" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/BG_List" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/BanGUI/page" -I"/cygdrive/C/Users/Hasee/Desktop/BG_Audio_Looper2/BG_card_RTOS/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


