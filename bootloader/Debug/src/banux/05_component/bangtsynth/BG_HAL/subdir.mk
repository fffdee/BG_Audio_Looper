################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/banux/05_component/bangtsynth/BG_HAL/bg_download_port_bp10.c \
../src/banux/05_component/bangtsynth/BG_HAL/bg_download_port_linux.c \
../src/banux/05_component/bangtsynth/BG_HAL/bg_hal_filesystem_linux.c \
../src/banux/05_component/bangtsynth/BG_HAL/bg_hal_linux.c \
../src/banux/05_component/bangtsynth/BG_HAL/bg_log.c \
../src/banux/05_component/bangtsynth/BG_HAL/bg_storage.c \
../src/banux/05_component/bangtsynth/BG_HAL/bg_storage_bp10.c \
../src/banux/05_component/bangtsynth/BG_HAL/bg_storage_embedded.c \
../src/banux/05_component/bangtsynth/BG_HAL/bg_storage_linux.c 

OBJS += \
./src/banux/05_component/bangtsynth/BG_HAL/bg_download_port_bp10.o \
./src/banux/05_component/bangtsynth/BG_HAL/bg_download_port_linux.o \
./src/banux/05_component/bangtsynth/BG_HAL/bg_hal_filesystem_linux.o \
./src/banux/05_component/bangtsynth/BG_HAL/bg_hal_linux.o \
./src/banux/05_component/bangtsynth/BG_HAL/bg_log.o \
./src/banux/05_component/bangtsynth/BG_HAL/bg_storage.o \
./src/banux/05_component/bangtsynth/BG_HAL/bg_storage_bp10.o \
./src/banux/05_component/bangtsynth/BG_HAL/bg_storage_embedded.o \
./src/banux/05_component/bangtsynth/BG_HAL/bg_storage_linux.o 

C_DEPS += \
./src/banux/05_component/bangtsynth/BG_HAL/bg_download_port_bp10.d \
./src/banux/05_component/bangtsynth/BG_HAL/bg_download_port_linux.d \
./src/banux/05_component/bangtsynth/BG_HAL/bg_hal_filesystem_linux.d \
./src/banux/05_component/bangtsynth/BG_HAL/bg_hal_linux.d \
./src/banux/05_component/bangtsynth/BG_HAL/bg_log.d \
./src/banux/05_component/bangtsynth/BG_HAL/bg_storage.d \
./src/banux/05_component/bangtsynth/BG_HAL/bg_storage_bp10.d \
./src/banux/05_component/bangtsynth/BG_HAL/bg_storage_embedded.d \
./src/banux/05_component/bangtsynth/BG_HAL/bg_storage_linux.d 


# Each subdirectory must supply rules for building sources it contributes
src/banux/05_component/bangtsynth/BG_HAL/%.o: ../src/banux/05_component/bangtsynth/BG_HAL/%.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_hal_drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/03_driver_framework/drivers" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/banux/01_vfs" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth/inc" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/bluetooth" -I"/cygdrive/E/BanGO_prj/BG_Audio_Looper/bootloader/src/drivers/usb/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -fsingle-precision-constant -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


