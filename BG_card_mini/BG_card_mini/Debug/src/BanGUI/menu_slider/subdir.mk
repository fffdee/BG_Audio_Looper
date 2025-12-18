################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/BanGUI/menu_slider/bg_menu_slider.c \
../src/BanGUI/menu_slider/bg_menu_slider_example.c \
../src/BanGUI/menu_slider/menu_slider.c \
../src/BanGUI/menu_slider/menu_slider_demo.c 

OBJS += \
./src/BanGUI/menu_slider/bg_menu_slider.o \
./src/BanGUI/menu_slider/bg_menu_slider_example.o \
./src/BanGUI/menu_slider/menu_slider.o \
./src/BanGUI/menu_slider/menu_slider_demo.o 

C_DEPS += \
./src/BanGUI/menu_slider/bg_menu_slider.d \
./src/BanGUI/menu_slider/bg_menu_slider_example.d \
./src/BanGUI/menu_slider/menu_slider.d \
./src/BanGUI/menu_slider/menu_slider_demo.d 


# Each subdirectory must supply rules for building sources it contributes
src/BanGUI/menu_slider/%.o: ../src/BanGUI/menu_slider/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/BG_List" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/page" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/audio" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_Lcd" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/base_func" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/IIC" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager/USB/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager/USB/src" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_Encoder" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_flash_manager" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/bluetooth/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/menu_slider" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/audio_looper" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/sys_param" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/ui_system" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


