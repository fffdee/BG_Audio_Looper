################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/drivers/usb/src/audio_api.c \
../src/drivers/usb/src/otg_device_audio.c \
../src/drivers/usb/src/otg_device_cdc.c \
../src/drivers/usb/src/otg_device_standard_request.c \
../src/drivers/usb/src/otg_device_stor.c \
../src/drivers/usb/src/usb_audio_api.c 

OBJS += \
./src/drivers/usb/src/audio_api.o \
./src/drivers/usb/src/otg_device_audio.o \
./src/drivers/usb/src/otg_device_cdc.o \
./src/drivers/usb/src/otg_device_standard_request.o \
./src/drivers/usb/src/otg_device_stor.o \
./src/drivers/usb/src/usb_audio_api.o 

C_DEPS += \
./src/drivers/usb/src/audio_api.d \
./src/drivers/usb/src/otg_device_audio.d \
./src/drivers/usb/src/otg_device_cdc.d \
./src/drivers/usb/src/otg_device_standard_request.d \
./src/drivers/usb/src/otg_device_stor.d \
./src/drivers/usb/src/usb_audio_api.d 


# Each subdirectory must supply rules for building sources it contributes
src/drivers/usb/src/%.o: ../src/drivers/usb/src/%.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/usb/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


