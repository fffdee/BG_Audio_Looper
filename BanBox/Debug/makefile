################################################################################
# Automatically-generated file. Do not edit!
################################################################################

ifndef ANDESIGHT_ROOT
ANDESIGHT_ROOT=/cygdrive/D/app/Andestech/AndeSight300RDS
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
-include src/banux/01_vfs/subdir.mk
-include src/banux/02_device_drivers/USB/src/subdir.mk
-include src/banux/02_device_drivers/flash/subdir.mk
-include src/banux/02_device_drivers/lcd/subdir.mk
-include src/banux/02_device_drivers/power_mgr/subdir.mk
-include src/banux/03_driver_framework/core/subdir.mk
-include src/banux/03_driver_framework/drivers/subdir.mk
-include src/banux/03_driver_framework/subdir.mk
-include src/banux/04_shell_commands/subdir.mk
-include src/banux/05_component/BanGUI/BG_List/subdir.mk
-include src/banux/05_component/BanGUI/base_func/subdir.mk
-include src/banux/05_component/BanGUI/menu_slider/subdir.mk
-include src/banux/05_component/BanGUI/page/subdir.mk
-include src/banux/05_component/BanGUI/ui_system/subdir.mk
-include src/banux/05_component/audio_looper/subdir.mk
-include src/banux/05_component/effect_graph/subdir.mk
-include src/banux/05_component/sys_param/subdir.mk
-include src/banux/06_app/BG_AudioIO_Manager/subdir.mk
-include src/banux/06_app/audio/subdir.mk
-include src/banux/06_app/audio/effect_parameter/subdir.mk
-include src/banux/06_app/audio/music_parameter/subdir.mk
-include src/banux/06_app/bluetooth/src/subdir.mk
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
BUILD_ARTIFACT_FILE_BASE_NAME = BanBox

LINKER_OUTPUTS = BanBox.adx

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
all: BanBox.adx secondary-outputs

# Tool invocations
BanBox.adx: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Andes C Linker'
	$(CROSS_COMPILE)gcc -Og -nostartfiles -static -L"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver" -L"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio" -L"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/bluetooth" -mcmodel=medium -ldsp -mext-dsp -T"../nds32-ae210p.ld" -Wl,--gc-sections -mvh -o "BanBox.adx" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

$(foreach subdir,$(SUBDIRS),$(wildcard $(subdir)/*.gcda) $(wildcard $(subdir)/*.gcno)): BanBox.adx $(OBJS) $(USER_OBJS)
../gmon.sum ../gmon.out: BanBox.adx $(OBJS) $(USER_OBJS)

$(SECONDARY_OUTPUT_PATH)/symbol.txt: $(LINKER_OUTPUTS)
	@echo 'Invoking: NM (symbol listing)'
	$(CROSS_COMPILE)nm -n -l -C "BanBox.adx" > $(SECONDARY_OUTPUT_PATH)/symbol.txt
	@echo 'Finished building: $@'
	@echo ' '

$(SECONDARY_OUTPUT_PATH)/readelf.txt: $(LINKER_OUTPUTS)
	@echo 'Invoking: Readelf (ELF info listing)'
	$(CROSS_COMPILE)readelf -a "BanBox.adx" > $(SECONDARY_OUTPUT_PATH)/readelf.txt
	@echo 'Finished building: $@'
	@echo ' '

$(SECONDARY_OUTPUT_PATH)/objdump.txt: $(LINKER_OUTPUTS)
	@echo 'Invoking: Objdump (disassembly)'
	$(CROSS_COMPILE)objdump -x -d -C "BanBox.adx" > $(SECONDARY_OUTPUT_PATH)/objdump.txt
	@echo 'Finished building: $@'
	@echo ' '

$(SECONDARY_OUTPUT_PATH)/$(BUILD_ARTIFACT_FILE_BASE_NAME).bin: $(LINKER_OUTPUTS)
	@echo 'Invoking: Objcopy (object content copy)'
	$(CROSS_COMPILE)objcopy -S -O binary "BanBox.adx" $(SECONDARY_OUTPUT_PATH)/$(BUILD_ARTIFACT_FILE_BASE_NAME).bin
	@echo 'Finished building: $@'
	@echo ' '

$(SECONDARY_OUTPUT_PATH)/.PHONY.size: $(LINKER_OUTPUTS)
	@echo 'Invoking: Size (section size listing)'
	$(CROSS_COMPILE)size  "BanBox.adx" | tee $(SECONDARY_OUTPUT_PATH)/.PHONY.size
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJCOPY_OUTPUTS)$(OBJS)$(C_DEPS)$(GCOV_OUT)$(SYMBOL_OUTPUTS)$(OBJDUMP_OUTPUTS)$(READELF_OUTPUTS)$(GPROF_OUT)$(SIZE_OUTPUTS)$(EXECUTABLES)$(S_UPPER_DEPS) BanBox.adx
	-@echo ' '

secondary-outputs: $(SYMBOL_OUTPUTS) $(READELF_OUTPUTS) $(OBJDUMP_OUTPUTS) $(OBJCOPY_OUTPUTS) $(SIZE_OUTPUTS)

.PHONY: all clean dependents
.SECONDARY:
.NOTPARALLEL: pre-build

-include ../makefile.targets
