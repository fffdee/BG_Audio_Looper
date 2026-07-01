################################################################################
# BanGTsynth 合成器模块 - BanDataHub SD+PSRAM 移植版
################################################################################

C_SRCS += \
../src/banux/05_component/bangtsynth/bangtsynth_node.c \
../src/banux/05_component/bangtsynth/hardware_interfance.c \
../src/banux/05_component/bangtsynth/01_hal/bg_storage.c \
../src/banux/05_component/bangtsynth/01_hal/bg_log.c \
../src/banux/05_component/bangtsynth/01_hal/port/bandatahub/bg_storage_bandatahub.c \
../src/banux/05_component/bangtsynth/01_hal/port/bandatahub/bg_osal_freertos.c \
../src/banux/05_component/bangtsynth/02_core/envelope/bg_envelope.c \
../src/banux/05_component/bangtsynth/02_core/midi/midi_controller.c \
../src/banux/05_component/bangtsynth/02_core/midi/standard_request_processing.c \
../src/banux/05_component/bangtsynth/02_core/sampler/sampler.c \
../src/banux/05_component/bangtsynth/02_core/soundbank/sf2_parser.c \
../src/banux/05_component/bangtsynth/02_core/soundbank/bgs_parser.c \
../src/banux/05_component/bangtsynth/02_core/soundbank/soundbank_manager.c \
../src/banux/05_component/bangtsynth/03_app/drum_machine/drum_machine.c \
../src/banux/05_component/bangtsynth/02_core/synth_integration/synth_sdnandpsram.c \
../src/banux/05_component/bangtsynth/02_core/synth_integration/synth_startup.c \
../src/banux/05_component/bangtsynth/02_core/synth_integration/synth_integration_test.c \
../src/banux/05_component/bangtsynth/02_core/psram_buffer/psram_buffer.c

OBJS += \
./src/banux/05_component/bangtsynth/bangtsynth_node.o \
./src/banux/05_component/bangtsynth/hardware_interfance.o \
./src/banux/05_component/bangtsynth/01_hal/bg_storage.o \
./src/banux/05_component/bangtsynth/01_hal/bg_log.o \
./src/banux/05_component/bangtsynth/01_hal/port/bandatahub/bg_storage_bandatahub.o \
./src/banux/05_component/bangtsynth/01_hal/port/bandatahub/bg_osal_freertos.o \
./src/banux/05_component/bangtsynth/02_core/envelope/bg_envelope.o \
./src/banux/05_component/bangtsynth/02_core/midi/midi_controller.o \
./src/banux/05_component/bangtsynth/02_core/midi/standard_request_processing.o \
./src/banux/05_component/bangtsynth/02_core/sampler/sampler.o \
./src/banux/05_component/bangtsynth/02_core/soundbank/sf2_parser.o \
./src/banux/05_component/bangtsynth/02_core/soundbank/bgs_parser.o \
./src/banux/05_component/bangtsynth/02_core/soundbank/soundbank_manager.o \
./src/banux/05_component/bangtsynth/03_app/drum_machine/drum_machine.o \
./src/banux/05_component/bangtsynth/02_core/synth_integration/synth_sdnandpsram.o \
./src/banux/05_component/bangtsynth/02_core/synth_integration/synth_startup.o \
./src/banux/05_component/bangtsynth/02_core/synth_integration/synth_integration_test.o \
./src/banux/05_component/bangtsynth/02_core/psram_buffer/psram_buffer.o

C_DEPS += \
./src/banux/05_component/bangtsynth/bangtsynth_node.d \
./src/banux/05_component/bangtsynth/hardware_interfance.d \
./src/banux/05_component/bangtsynth/01_hal/bg_storage.d \
./src/banux/05_component/bangtsynth/01_hal/bg_log.d \
./src/banux/05_component/bangtsynth/01_hal/port/bandatahub/bg_storage_bandatahub.d \
./src/banux/05_component/bangtsynth/01_hal/port/bandatahub/bg_osal_freertos.d \
./src/banux/05_component/bangtsynth/02_core/envelope/bg_envelope.d \
./src/banux/05_component/bangtsynth/02_core/midi/midi_controller.d \
./src/banux/05_component/bangtsynth/02_core/midi/standard_request_processing.d \
./src/banux/05_component/bangtsynth/02_core/sampler/sampler.d \
./src/banux/05_component/bangtsynth/02_core/soundbank/sf2_parser.d \
./src/banux/05_component/bangtsynth/02_core/soundbank/bgs_parser.d \
./src/banux/05_component/bangtsynth/02_core/soundbank/soundbank_manager.d \
./src/banux/05_component/bangtsynth/03_app/drum_machine/drum_machine.d \
./src/banux/05_component/bangtsynth/02_core/synth_integration/synth_sdnandpsram.d \
./src/banux/05_component/bangtsynth/02_core/synth_integration/synth_startup.d \
./src/banux/05_component/bangtsynth/02_core/synth_integration/synth_integration_test.d \
./src/banux/05_component/bangtsynth/02_core/psram_buffer/psram_buffer.d

# Build rule - shared include paths
src/banux/05_component/bangtsynth/%.o: ../src/banux/05_component/bangtsynth/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/device" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/adc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/gpio" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/spi" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_hal_drivers/sdio" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/flash" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/lcd" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/power_mgr" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/sdcard" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/remind_sound" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/core" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/03_driver_framework/event" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/04_shell_commands" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/06_app/audio/music_parameter" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/sys_param" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/06_app/BG_AudioIO_Manager" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/01_vfs" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/fat32" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/01_hal" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/common" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/soundbank" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/midi" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/envelope" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/sampler" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/BanDataHub/src/banux/05_component/bangtsynth/02_core/synth_integration" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
