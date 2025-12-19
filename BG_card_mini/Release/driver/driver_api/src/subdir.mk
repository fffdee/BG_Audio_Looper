################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/adc_interface.c \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/backup_interface.c \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/dac_interface.c \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/i2c_host.c \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/i2c_interface.c \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/sadc_interface.c \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/sd_card.c \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/spim_interface.c \
I:/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/uarts_interface.c 

OBJS += \
./driver/driver_api/src/adc_interface.o \
./driver/driver_api/src/backup_interface.o \
./driver/driver_api/src/dac_interface.o \
./driver/driver_api/src/i2c_host.o \
./driver/driver_api/src/i2c_interface.o \
./driver/driver_api/src/sadc_interface.o \
./driver/driver_api/src/sd_card.o \
./driver/driver_api/src/spim_interface.o \
./driver/driver_api/src/uarts_interface.o 

C_DEPS += \
./driver/driver_api/src/adc_interface.d \
./driver/driver_api/src/backup_interface.d \
./driver/driver_api/src/dac_interface.d \
./driver/driver_api/src/i2c_host.d \
./driver/driver_api/src/i2c_interface.d \
./driver/driver_api/src/sadc_interface.d \
./driver/driver_api/src/sd_card.d \
./driver/driver_api/src/spim_interface.d \
./driver/driver_api/src/uarts_interface.d 


# Each subdirectory must supply rules for building sources it contributes
driver/driver_api/src/adc_interface.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/adc_interface.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver/driver_api/src/backup_interface.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/backup_interface.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver/driver_api/src/dac_interface.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/dac_interface.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver/driver_api/src/i2c_host.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/i2c_host.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver/driver_api/src/i2c_interface.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/i2c_interface.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver/driver_api/src/sadc_interface.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/sadc_interface.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver/driver_api/src/sd_card.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/sd_card.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver/driver_api/src/spim_interface.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/spim_interface.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver/driver_api/src/uarts_interface.o: /cygdrive/I/MVsB1_BT_Audio_SDK_v0.1.12+P05/MVsB1_Base_SDK/driver/driver_api/src/uarts_interface.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


