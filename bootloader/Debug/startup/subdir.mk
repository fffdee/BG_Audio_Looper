################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../startup/init-default.c \
../startup/interrupt.c \
../startup/retarget.c 

S_UPPER_SRCS += \
../startup/crt0.S 

OBJS += \
./startup/crt0.o \
./startup/init-default.o \
./startup/interrupt.o \
./startup/retarget.o 

C_DEPS += \
./startup/init-default.d \
./startup/interrupt.d \
./startup/retarget.d 

S_UPPER_DEPS += \
./startup/crt0.d 


# Each subdirectory must supply rules for building sources it contributes
startup/%.o: ../startup/%.S
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio/music_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/04_shell_commands" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

startup/%.o: ../startup/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/flash" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/core" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/06_app/audio/music_parameter" -I"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/bootloader/src/banux/04_shell_commands" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


