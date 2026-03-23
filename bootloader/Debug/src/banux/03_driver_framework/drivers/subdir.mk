################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/03_driver_framework/drivers/drv_usb_cdc.c \
../src/banux/03_driver_framework/drivers/drv_w25qxx.c 

OBJS += \
./src/banux/03_driver_framework/drivers/drv_usb_cdc.o \
./src/banux/03_driver_framework/drivers/drv_w25qxx.o 

C_DEPS += \
./src/banux/03_driver_framework/drivers/drv_usb_cdc.d \
./src/banux/03_driver_framework/drivers/drv_w25qxx.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/03_driver_framework/drivers/%.o: ../src/banux/03_driver_framework/drivers/%.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/flash" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/core" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/03_driver_framework" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/04_shell_commands" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/06_app/audio" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/06_app/audio/music_parameter" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth/BG_Audio_Processor" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth/BG_Audio_Processor/effects" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth/BG_Envelope_Generator" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth/BG_err_handle" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth/BG_HAL" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth/BG_Midi_Controller" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth/BG_Soundbank" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth" -I"/cygdrive/F/project_and_dataset/project/BG_Audio_Looper/BG_Audio_Looper/bootloader/src/banux/05_component/bangtsynth/soundbank_data" -Og -mcmodel=medium -g3 -Wall -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


