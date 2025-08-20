################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hardware/BG_Lcd/bg_lcd.c \
../src/hardware/BG_Lcd/st7735.c 

OBJS += \
./src/hardware/BG_Lcd/bg_lcd.o \
./src/hardware/BG_Lcd/st7735.o 

C_DEPS += \
./src/hardware/BG_Lcd/bg_lcd.d \
./src/hardware/BG_Lcd/st7735.d 


# Each subdirectory must supply rules for building sources it contributes
src/hardware/BG_Lcd/%.o: ../src/hardware/BG_Lcd/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


