################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/04_shell_commands/bg_shell.c \
../src/banux/04_shell_commands/bg_shell_commands.c \
../src/banux/04_shell_commands/shell_fs.c \
../src/banux/04_shell_commands/shell_io_ble.c \
../src/banux/04_shell_commands/shell_io_manager.c 

OBJS += \
./src/banux/04_shell_commands/bg_shell.o \
./src/banux/04_shell_commands/bg_shell_commands.o \
./src/banux/04_shell_commands/shell_fs.o \
./src/banux/04_shell_commands/shell_io_ble.o \
./src/banux/04_shell_commands/shell_io_manager.o 

C_DEPS += \
./src/banux/04_shell_commands/bg_shell.d \
./src/banux/04_shell_commands/bg_shell_commands.d \
./src/banux/04_shell_commands/shell_fs.d \
./src/banux/04_shell_commands/shell_io_ble.d \
./src/banux/04_shell_commands/shell_io_manager.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/04_shell_commands/%.o: ../src/banux/04_shell_commands/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/device" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/adc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/gpio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_hal_drivers/spi" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/flash" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/lcd" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/power_mgr" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/03_driver_framework/core" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/03_driver_framework" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/04_shell_commands" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/base_func" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/BG_List" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/menu_slider" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/page" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/audio" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/audio/effect_parameter" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/audio/music_parameter" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_looper" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/sys_param" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/BG_AudioIO_Manager" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/02_device_drivers/USB/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/06_app/bluetooth/inc" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/BanGUI/ui_system" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/audio_spectrum" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/03_driver_framework/drivers" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/01_vfs" -I"/cygdrive/C/Users/BanGO/Desktop/BanGO_prj/BG_Audio_Looper/BanBox/src/banux/05_component/effect_graph" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


