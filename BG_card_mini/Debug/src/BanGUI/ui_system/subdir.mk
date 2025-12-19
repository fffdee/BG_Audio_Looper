################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/BanGUI/ui_system/about.c \
../src/BanGUI/ui_system/acc_chart.c \
../src/BanGUI/ui_system/black.c \
../src/BanGUI/ui_system/icon_about_32x32.c \
../src/BanGUI/ui_system/icon_game_32x32.c \
../src/BanGUI/ui_system/icon_music_32x32.c \
../src/BanGUI/ui_system/icon_setting_32x32.c \
../src/BanGUI/ui_system/setting.c \
../src/BanGUI/ui_system/ui_bootscreen.c \
../src/BanGUI/ui_system/ui_button.c \
../src/BanGUI/ui_system/ui_menu.c \
../src/BanGUI/ui_system/ui_menu_def.c \
../src/BanGUI/ui_system/ui_statusbar.c \
../src/BanGUI/ui_system/ui_system.c \
../src/BanGUI/ui_system/vacal_setting.c 

OBJS += \
./src/BanGUI/ui_system/about.o \
./src/BanGUI/ui_system/acc_chart.o \
./src/BanGUI/ui_system/black.o \
./src/BanGUI/ui_system/icon_about_32x32.o \
./src/BanGUI/ui_system/icon_game_32x32.o \
./src/BanGUI/ui_system/icon_music_32x32.o \
./src/BanGUI/ui_system/icon_setting_32x32.o \
./src/BanGUI/ui_system/setting.o \
./src/BanGUI/ui_system/ui_bootscreen.o \
./src/BanGUI/ui_system/ui_button.o \
./src/BanGUI/ui_system/ui_menu.o \
./src/BanGUI/ui_system/ui_menu_def.o \
./src/BanGUI/ui_system/ui_statusbar.o \
./src/BanGUI/ui_system/ui_system.o \
./src/BanGUI/ui_system/vacal_setting.o 

C_DEPS += \
./src/BanGUI/ui_system/about.d \
./src/BanGUI/ui_system/acc_chart.d \
./src/BanGUI/ui_system/black.d \
./src/BanGUI/ui_system/icon_about_32x32.d \
./src/BanGUI/ui_system/icon_game_32x32.d \
./src/BanGUI/ui_system/icon_music_32x32.d \
./src/BanGUI/ui_system/icon_setting_32x32.d \
./src/BanGUI/ui_system/setting.d \
./src/BanGUI/ui_system/ui_bootscreen.d \
./src/BanGUI/ui_system/ui_button.d \
./src/BanGUI/ui_system/ui_menu.d \
./src/BanGUI/ui_system/ui_menu_def.d \
./src/BanGUI/ui_system/ui_statusbar.d \
./src/BanGUI/ui_system/ui_system.d \
./src/BanGUI/ui_system/vacal_setting.d 


# Each subdirectory must supply rules for building sources it contributes
src/BanGUI/ui_system/%.o: ../src/BanGUI/ui_system/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/project_and_dataset/project/bp1048_sdk_v0.1.12-master/bp1048_sdk_v0.1.12-master/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/BG_List" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/page" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/audio" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_Lcd" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/base_func" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/IIC" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager/USB/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager/USB/src" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_Encoder" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/BG_flash_manager" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/bluetooth/inc" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/menu_slider" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/audio_looper" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/hardware/sys_param" -I"/cygdrive/F/project_and_dataset/project/BG_card_mini/BG_card_mini/src/BanGUI/ui_system" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


