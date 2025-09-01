################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hardware/audio/audio_effect.c \
../src/hardware/audio/communication.c \
../src/hardware/audio/ctrlvars.c \
../src/hardware/audio/eq_params.c 

OBJS += \
./src/hardware/audio/audio_effect.o \
./src/hardware/audio/communication.o \
./src/hardware/audio/ctrlvars.o \
./src/hardware/audio/eq_params.o 

C_DEPS += \
./src/hardware/audio/audio_effect.d \
./src/hardware/audio/communication.d \
./src/hardware/audio/ctrlvars.d \
./src/hardware/audio/eq_params.d 


# Each subdirectory must supply rules for building sources it contributes
src/hardware/audio/%.o: ../src/hardware/audio/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


