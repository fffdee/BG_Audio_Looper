################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/BanGUI/page/bg_page.c \
../src/BanGUI/page/page_manager.c 

OBJS += \
./src/BanGUI/page/bg_page.o \
./src/BanGUI/page/page_manager.o 

C_DEPS += \
./src/BanGUI/page/bg_page.d \
./src/BanGUI/page/page_manager.d 


# Each subdirectory must supply rules for building sources it contributes
src/BanGUI/page/%.o: ../src/BanGUI/page/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


