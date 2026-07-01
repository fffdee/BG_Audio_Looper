################################################################################
# Automatically-generated file. Do not edit!
################################################################################

ifndef ANDESIGHT_ROOT
ANDESIGHT_ROOT=/cygdrive/C/Andestech/AndeSight300RDS
endif

ifndef CROSS_COMPILE
CROSS_COMPILE=nds32le-elf-
endif

ifndef SECONDARY_OUTPUT_PATH
SECONDARY_OUTPUT_PATH=output
endif

$(shell mkdir -p $(SECONDARY_OUTPUT_PATH))

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include driver/driver_api/src/subdir.mk
-include middleware/audio/src/subdir.mk
-include middleware/mv_utils/src/subdir.mk
-include middleware/rtos/freertos/src/subdir.mk
-include middleware/rtos/rtos_api/subdir.mk
-include src/banux/01_hal_drivers/adc/subdir.mk
-include src/banux/01_hal_drivers/gpio/subdir.mk
-include src/banux/01_hal_drivers/sdio/subdir.mk
-include src/banux/01_vfs/subdir.mk
-include src/banux/02_device_drivers/USB/src/subdir.mk
-include src/banux/02_device_drivers/bluetooth/subdir.mk
-include src/banux/02_device_drivers/bluetooth/src/subdir.mk
-include src/banux/02_device_drivers/flash/subdir.mk
-include src/banux/02_device_drivers/lcd/subdir.mk
-include src/banux/02_device_drivers/power_mgr/subdir.mk
-include src/banux/02_device_drivers/remind_sound/subdir.mk
-include src/banux/02_device_drivers/sdcard/subdir.mk
-include src/banux/03_driver_framework/core/subdir.mk
-include src/banux/03_driver_framework/drivers/subdir.mk
-include src/banux/03_driver_framework/subdir.mk
-include src/banux/03_driver_framework/event/subdir.mk
-include src/banux/04_shell_commands/subdir.mk
-include src/banux/05_component/bangtsynth/01_hal/subdir.mk
-include src/banux/05_component/bangtsynth/01_hal/port/bandatahub/subdir.mk
-include src/banux/05_component/bangtsynth/01_hal/port/bp10/subdir.mk
-include src/banux/05_component/bangtsynth/01_hal/port/embedded/subdir.mk
-include src/banux/05_component/bangtsynth/02_core/envelope/subdir.mk
-include src/banux/05_component/bangtsynth/02_core/fat32/subdir.mk
-include src/banux/05_component/bangtsynth/02_core/midi/subdir.mk
-include src/banux/05_component/bangtsynth/02_core/nand_store/subdir.mk
-include src/banux/05_component/bangtsynth/02_core/psram_buffer/subdir.mk
-include src/banux/05_component/bangtsynth/02_core/sampler/subdir.mk
-include src/banux/05_component/bangtsynth/02_core/soundbank/subdir.mk
-include src/banux/05_component/bangtsynth/02_core/synth_integration/subdir.mk
-include src/banux/05_component/bangtsynth/03_app/drum_machine/subdir.mk
-include src/banux/05_component/bangtsynth/03_app/synth_node/subdir.mk
-include src/banux/05_component/bangtsynth/BG_Audio_Processor/subdir.mk
-include src/banux/05_component/bangtsynth/BG_Audio_Processor/effects/subdir.mk
-include src/banux/05_component/bangtsynth/BG_Envelope_Generator/subdir.mk
-include src/banux/05_component/bangtsynth/BG_HAL/subdir.mk
-include src/banux/05_component/bangtsynth/BG_Midi_Controller/subdir.mk
-include src/banux/05_component/bangtsynth/BG_Soundbank/subdir.mk
-include src/banux/05_component/bangtsynth/subdir.mk
-include src/banux/05_component/bangtsynth/data/drum_data/subdir.mk
-include src/banux/05_component/bangtsynth/data/soundbank_data/subdir.mk
-include src/banux/05_component/bangtsynth/drum_machine/subdir.mk
-include src/banux/05_component/bangtsynth/durm_data/subdir.mk
-include src/banux/05_component/bangtsynth/soundbank_data/subdir.mk
-include src/banux/05_component/fat32/subdir.mk
-include src/banux/05_component/sys_param/subdir.mk
-include src/banux/06_app/BG_AudioIO_Manager/subdir.mk
-include src/banux/06_app/subdir.mk
-include src/banux/06_app/audio/subdir.mk
-include src/banux/06_app/audio/effect_parameter/subdir.mk
-include src/banux/06_app/audio/music_parameter/subdir.mk
-include src/device/subdir.mk
-include src/subdir.mk
-include startup/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
BUILD_ARTIFACT_FILE_BASE_NAME = BanDataHub

LINKER_OUTPUTS = BanDataHub.adx

GCOV_OUT += \
$(foreach subdir,$(SUBDIRS),$(wildcard $(subdir)/*.gcda) $(wildcard $(subdir)/*.gcno)) \

GPROF_OUT += \
../gmon.sum ../gmon.out \

SYMBOL_OUTPUTS += \
$(SECONDARY_OUTPUT_PATH)/symbol.txt \

READELF_OUTPUTS += \
$(SECONDARY_OUTPUT_PATH)/readelf.txt \

OBJDUMP_OUTPUTS += \
$(SECONDARY_OUTPUT_PATH)/objdump.txt \

OBJCOPY_OUTPUTS += \
$(SECONDARY_OUTPUT_PATH)/$(BUILD_ARTIFACT_FILE_BASE_NAME).bin \

SIZE_OUTPUTS += \
$(SECONDARY_OUTPUT_PATH)/.PHONY.size \


# All Target
all: BanDataHub.adx secondary-outputs

# Tool invocations
BanDataHub.adx: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Andes C Linker'
	$(CROSS_COMPILE)gcc -Og -nostartfiles -static -L"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver" -L"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio" -L"/cygdrive/E/project_and_dataset/project/BG_Audio_Looper/BanDataHub/src/banux/02_device_drivers/bluetooth" -mcmodel=medium -ldsp -mext-dsp -T"../nds32-ae210p.ld" -Wl,--gc-sections -mvh -o "BanDataHub.adx" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

$(foreach subdir,$(SUBDIRS),$(wildcard $(subdir)/*.gcda) $(wildcard $(subdir)/*.gcno)): BanDataHub.adx $(OBJS) $(USER_OBJS)
../gmon.sum ../gmon.out: BanDataHub.adx $(OBJS) $(USER_OBJS)

$(SECONDARY_OUTPUT_PATH)/symbol.txt: $(LINKER_OUTPUTS)
	@echo 'Invoking: NM (symbol listing)'
	$(CROSS_COMPILE)nm -n -l -C "BanDataHub.adx" > $(SECONDARY_OUTPUT_PATH)/symbol.txt
	@echo 'Finished building: $@'
	@echo ' '

$(SECONDARY_OUTPUT_PATH)/readelf.txt: $(LINKER_OUTPUTS)
	@echo 'Invoking: Readelf (ELF info listing)'
	$(CROSS_COMPILE)readelf -a "BanDataHub.adx" > $(SECONDARY_OUTPUT_PATH)/readelf.txt
	@echo 'Finished building: $@'
	@echo ' '

$(SECONDARY_OUTPUT_PATH)/objdump.txt: $(LINKER_OUTPUTS)
	@echo 'Invoking: Objdump (disassembly)'
	$(CROSS_COMPILE)objdump -x -d -C "BanDataHub.adx" > $(SECONDARY_OUTPUT_PATH)/objdump.txt
	@echo 'Finished building: $@'
	@echo ' '

$(SECONDARY_OUTPUT_PATH)/$(BUILD_ARTIFACT_FILE_BASE_NAME).bin: $(LINKER_OUTPUTS)
	@echo 'Invoking: Objcopy (object content copy)'
	$(CROSS_COMPILE)objcopy -S -O binary "BanDataHub.adx" $(SECONDARY_OUTPUT_PATH)/$(BUILD_ARTIFACT_FILE_BASE_NAME).bin
	@echo 'Finished building: $@'
	@echo ' '

$(SECONDARY_OUTPUT_PATH)/.PHONY.size: $(LINKER_OUTPUTS)
	@echo 'Invoking: Size (section size listing)'
	$(CROSS_COMPILE)size  "BanDataHub.adx" | tee $(SECONDARY_OUTPUT_PATH)/.PHONY.size
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJCOPY_OUTPUTS)$(OBJS)$(C_DEPS)$(GCOV_OUT)$(SYMBOL_OUTPUTS)$(OBJDUMP_OUTPUTS)$(READELF_OUTPUTS)$(GPROF_OUT)$(SIZE_OUTPUTS)$(EXECUTABLES)$(S_UPPER_DEPS) BanDataHub.adx
	-@echo ' '

secondary-outputs: $(SYMBOL_OUTPUTS) $(READELF_OUTPUTS) $(OBJDUMP_OUTPUTS) $(OBJCOPY_OUTPUTS) $(SIZE_OUTPUTS)

.PHONY: all clean dependents
.SECONDARY:
.NOTPARALLEL: pre-build

-include ../makefile.targets
