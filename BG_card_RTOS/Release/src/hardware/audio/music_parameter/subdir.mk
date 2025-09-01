################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hardware/audio/music_parameter/Music.c 

OBJS += \
./src/hardware/audio/music_parameter/Music.o 

C_DEPS += \
./src/hardware/audio/music_parameter/Music.d 


# Each subdirectory must supply rules for building sources it contributes
src/hardware/audio/music_parameter/%.o: ../src/hardware/audio/music_parameter/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


