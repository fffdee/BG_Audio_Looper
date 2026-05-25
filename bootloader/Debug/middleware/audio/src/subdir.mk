################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
E:/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/bits.c \
E:/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/libmp2dec.c \
E:/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/mvstdio.c 

OBJS += \
./middleware/audio/src/bits.o \
./middleware/audio/src/libmp2dec.o \
./middleware/audio/src/mvstdio.o 

C_DEPS += \
./middleware/audio/src/bits.d \
./middleware/audio/src/libmp2dec.d \
./middleware/audio/src/mvstdio.d 


# Each subdirectory must supply rules for building sources it contributes
middleware/audio/src/bits.o: /cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/bits.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/usb/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '

middleware/audio/src/libmp2dec.o: /cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/libmp2dec.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/usb/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '

middleware/audio/src/mvstdio.o: /cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/src/mvstdio.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/usb/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


