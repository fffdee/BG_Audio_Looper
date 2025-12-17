################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/BanGUI/base_func/bg_shell.c \
../src/BanGUI/base_func/bg_shell_commands.c \
../src/BanGUI/base_func/gui_tool.c \
../src/BanGUI/base_func/shell_io_manager.c \
../src/BanGUI/base_func/shell_lcd_adapter.c 

OBJS += \
./src/BanGUI/base_func/bg_shell.o \
./src/BanGUI/base_func/bg_shell_commands.o \
./src/BanGUI/base_func/gui_tool.o \
./src/BanGUI/base_func/shell_io_manager.o \
./src/BanGUI/base_func/shell_lcd_adapter.o 

C_DEPS += \
./src/BanGUI/base_func/bg_shell.d \
./src/BanGUI/base_func/bg_shell_commands.d \
./src/BanGUI/base_func/gui_tool.d \
./src/BanGUI/base_func/shell_io_manager.d \
./src/BanGUI/base_func/shell_lcd_adapter.d 


# Each subdirectory must supply rules for building sources it contributes
src/BanGUI/base_func/%.o: ../src/BanGUI/base_func/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/BanGUI/BG_List" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/BanGUI/page" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/BG_Lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/IIC" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/BG_AudioIO_Manager/USB/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/BG_Encoder" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/BG_flash_manager" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/bluetooth/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/BanGUI/menu_slider" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BG_card_mini/BG_card_mini/src/hardware/audio_looper" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


