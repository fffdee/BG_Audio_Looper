################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hardware/BG_flash_manager/bg_flash_manager.c 

OBJS += \
./src/hardware/BG_flash_manager/bg_flash_manager.o 

C_DEPS += \
./src/hardware/BG_flash_manager/bg_flash_manager.d 


# Each subdirectory must supply rules for building sources it contributes
src/hardware/BG_flash_manager/%.o: ../src/hardware/BG_flash_manager/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


