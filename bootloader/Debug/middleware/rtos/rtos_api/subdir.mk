################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
F:/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api/rtos_api.c 

OBJS += \
./middleware/rtos/rtos_api/rtos_api.o 

C_DEPS += \
./middleware/rtos/rtos_api/rtos_api.d 


# Each subdirectory must supply rules for building sources it contributes
middleware/rtos/rtos_api/rtos_api.o: /cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api/rtos_api.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/drivers/bluetooth/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/drivers/bluetooth" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/drivers/usb/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


