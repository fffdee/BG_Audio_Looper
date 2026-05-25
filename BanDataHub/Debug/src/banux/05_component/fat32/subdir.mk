################################################################################
# �Զ����ɵ��ļ�����Ҫ�༭��
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/05_component/fat32/fat32_diskio.c \
../src/banux/05_component/fat32/fat32_nand.c \
../src/banux/05_component/fat32/fat32_reader.c \
../src/banux/05_component/fat32/psram_heap.c 

OBJS += \
./src/banux/05_component/fat32/fat32_diskio.o \
./src/banux/05_component/fat32/fat32_nand.o \
./src/banux/05_component/fat32/fat32_reader.o \
./src/banux/05_component/fat32/psram_heap.o 

C_DEPS += \
./src/banux/05_component/fat32/fat32_diskio.d \
./src/banux/05_component/fat32/fat32_nand.d \
./src/banux/05_component/fat32/fat32_reader.d \
./src/banux/05_component/fat32/psram_heap.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/05_component/fat32/%.o: ../src/banux/05_component/fat32/%.c
	@echo '���ڹ����ļ��� $<'
	@echo '���ڵ��ã� Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/device" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/adc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/gpio" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/spi" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/flash" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/lcd" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/power_mgr" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/core" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/04_shell_commands" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/music_parameter" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/sys_param" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/06_app/BG_AudioIO_Manager" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_vfs" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/remind_sound" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/sdio" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/event" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/sdcard" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/fat32" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '�ѽ��������� $<'
	@echo ' '


