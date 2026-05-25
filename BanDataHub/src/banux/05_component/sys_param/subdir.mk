################################################################################
# �Զ����ɵ��ļ�����Ҫ�༭��
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/05_component/sys_param/sys_param.c

OBJS += \
./src/banux/05_component/sys_param/sys_param.o

C_DEPS += \
./src/banux/05_component/sys_param/sys_param.d


# Each subdirectory must supply rules for building sources it contributes
src/banux/05_component/sys_param/%.o: ../src/banux/05_component/sys_param/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	nds32le-elf-gcc -D__NO_INLINE__ -DDEBUG -D__STACK_SIZE=0x800 -D__HEAP_SIZE=0x800 -DUSE_FREERTOS -DUSE_SHELL -DUSE_AUDIO -DUSE_BT -DUSE_USB -DUSE_FLASH -DUSE_LCD -DUSE_ADC -DUSE_DAC -DUSE_SPI -DUSE_I2C -DUSE_GPIO -DUSE_TIMER -DUSE_WDT -DUSE_DMA -DUSE_UART -DUSE_CLK -DUSE_REMAP -DUSE_CHIP_INFO -DUSE_DELAY -DUSE_DEBUG -DUSE_TIMEOUT -DUSE_WATCHDOG -DUSE_SPI_FLASH -DUSE_AUDIO_ADC -DUSE_DAC_INTERFACE -DUSE_SPIM_INTERFACE -DUSE_SPIM -DUSE_BG_FLASH_MANAGER -DUSE_BG_FLASHMGR -DUSE_FLASH_TEST -DUSE_INTERNAL_FLASH_TEST -DUSE_BG_LCD -DUSE_MATH -DUSE_STRING -DUSE_STDIO -DUSE_STDLIB -DUSE_BOOL -DUSE_STDINT -DUSE_NDS32_INTRINSIC -DUSE_FREERTOS -DUSE_TASK -DUSE_QUEUE -DUSE_DELAY -DUSE_CHIP_INFO -DUSE_AUDIO_ADC -DUSE_ADC_INTERFACE -DUSE_DAC_INTERFACE -DUSE_SPIM_INTERFACE -DUSE_SPIM -DUSE_BG_FLASH_MANAGER -DUSE_BG_FLASHMGR -DUSE_FLASH_TEST -DUSE_INTERNAL_FLASH_TEST -DUSE_BG_LCD -DUSE_MATH -DUSE_STRING -DUSE_STDIO -DUSE_STDLIB -DUSE_BOOL -DUSE_STDINT -DUSE_NDS32_INTRINSIC -I"../MVsB1_Base_SDK/driver/driver_api/inc" -I"../MVsB1_Base_SDK/middleware/audio/inc" -I"../MVsB1_Base_SDK/middleware/mv_utils/inc" -I"../MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"../MVsB1_Base_SDK/middleware/rtos/rtos_api/inc" -I"../src/banux/01_hal_drivers/adc" -I"../src/banux/01_vfs" -I"../src/banux/02_device_drivers/USB/inc" -I"../src/banux/02_device_drivers/bluetooth/inc" -I"../src/banux/02_device_drivers/flash" -I"../src/banux/02_device_drivers/lcd" -I"../src/banux/02_device_drivers/power_mgr" -I"../src/banux/03_driver_framework/core" -I"../src/banux/03_driver_framework/drivers" -I"../src/banux/03_driver_framework" -I"../src/banux/04_shell_commands" -I"../src/banux/05_component/BanGUI/base_func" -I"../src/banux/05_component/BanGUI/ui/components" -I"../src/banux/05_component/BanGUI/ui/core" -I"../src/banux/05_component/BanGUI/ui/views" -I"../src/banux/05_component/audio_looper" -I"../src/banux/05_component/effect_graph" -I"../src/banux/05_component/sys_param" -I"../src/banux/06_app/BG_AudioIO_Manager" -I"../src/banux/06_app/audio" -I"../src/banux/06_app/audio/effect_parameter" -I"../src/banux/06_app/audio/music_parameter" -I"../src/device" -I"../src" -I"../startup" -O0 -g3 -Wall -c -fmessage-length=0 -mcmodel=large -mno-mac -mavg-align=4 -mforce-fp-as-gp -mext-dsp -mext-zol -ffunction-sections -fdata-sections -std=c99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '