################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/BanGUI/BG_List/bg_list.c 

OBJS += \
./src/BanGUI/BG_List/bg_list.o 

C_DEPS += \
./src/BanGUI/BG_List/bg_list.d 


# Each subdirectory must supply rules for building sources it contributes
src/BanGUI/BG_List/%.o: ../src/BanGUI/BG_List/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


