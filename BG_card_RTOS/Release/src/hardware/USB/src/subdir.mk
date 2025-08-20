################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hardware/USB/src/audio_api.c \
../src/hardware/USB/src/otg_device_audio.c \
../src/hardware/USB/src/otg_device_standard_request.c \
../src/hardware/USB/src/otg_device_stor.c \
../src/hardware/USB/src/usb_audio_api.c 

OBJS += \
./src/hardware/USB/src/audio_api.o \
./src/hardware/USB/src/otg_device_audio.o \
./src/hardware/USB/src/otg_device_standard_request.o \
./src/hardware/USB/src/otg_device_stor.o \
./src/hardware/USB/src/usb_audio_api.o 

C_DEPS += \
./src/hardware/USB/src/audio_api.d \
./src/hardware/USB/src/otg_device_audio.d \
./src/hardware/USB/src/otg_device_standard_request.d \
./src/hardware/USB/src/otg_device_stor.d \
./src/hardware/USB/src/usb_audio_api.d 


# Each subdirectory must supply rules for building sources it contributes
src/hardware/USB/src/%.o: ../src/hardware/USB/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -Os -mcmodel=medium -Wall -mcpu=d1088-spu -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


