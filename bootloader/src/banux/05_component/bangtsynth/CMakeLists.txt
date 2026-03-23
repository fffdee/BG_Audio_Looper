cmake_minimum_required(VERSION 3.10)
project(BanGTsynth C)

# ============================================
# 编译器设置
# ============================================
set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_STANDARD 11)

# ============================================
# 平台配置 (在这里选择平台)
# ============================================
# 支持的平台: linux, stm32, esp32
set(PLATFORM "linux" CACHE STRING "Target platform")

# ============================================
# 功能模块裁剪 (通过 bg_config.h 控制)
# ============================================
# 所有功能开关、参数配置在 components/bg_config.h 中定义
# 修改配置后重新编译即可生效
# 可使用图形化配置工具: python components/config_tool.py

# ============================================
# 编译选项
# ============================================
add_compile_options(-Wall -O2)

# ============================================
# 头文件目录
# ============================================
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/components
    ${CMAKE_SOURCE_DIR}/components/BG_HAL
    ${CMAKE_SOURCE_DIR}/components/BG_HAL/include
    ${CMAKE_SOURCE_DIR}/components/BG_err_handle
    ${CMAKE_SOURCE_DIR}/include
)

# ============================================
# 核心源文件
# ============================================
set(CORE_SRC
    ${CMAKE_SOURCE_DIR}/src/main.c
)

# ============================================
# 硬件层源文件
# ============================================
set(HARDWARE_SRC
    ${CMAKE_SOURCE_DIR}/hardware/play/src/play.c
)

# 添加硬件接口层
set(COMPONENT_INTERFACE_SRC
    ${CMAKE_SOURCE_DIR}/components/hardware_interfance.c
)

# 添加硬件层头文件
list(APPEND INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/hardware/play/include
)

# ============================================
# 平台 HAL 源文件
# ============================================
if(PLATFORM STREQUAL "linux")
    set(HAL_SRC 
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/bg_hal_linux.c
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/src/bg_log.c
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/src/bg_storage.c
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/src/bg_storage_linux.c
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/src/bg_download_port_linux.c
    )
    set(PLATFORM_LIBS ev asound m)
elseif(PLATFORM STREQUAL "stm32")
    set(HAL_SRC 
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/bg_hal_stm32.c
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/src/bg_log.c
    )
    # 添加 STM32 HAL 库路径
elseif(PLATFORM STREQUAL "esp32")
    set(HAL_SRC 
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/bg_hal_esp32.c
        ${CMAKE_SOURCE_DIR}/components/BG_HAL/src/bg_log.c
    )
    # 添加 ESP-IDF 配置
else()
    message(FATAL_ERROR "Unsupported platform: ${PLATFORM}")
endif()

# ============================================
# 组件源文件 (根据功能裁剪)
# ============================================
set(COMPONENT_SRC)

# 音源管理器 (BG格式 + SF2格式)
file(GLOB SOUNDBANK_SRC ${CMAKE_SOURCE_DIR}/components/BG_Soundbank/src/*.c)
list(APPEND COMPONENT_SRC ${SOUNDBANK_SRC})
list(APPEND INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/components/BG_Soundbank/include)
message(STATUS "Soundbank Manager: ENABLED (BG + SF2)")

# MIDI 控制器 (通过 bg_config.h 中的 ENABLE_MIDI_CONTROLLER 控制)
file(GLOB MIDI_SRC ${CMAKE_SOURCE_DIR}/components/BG_Midi_Controller/src/*.c)
list(APPEND COMPONENT_SRC ${MIDI_SRC})
list(APPEND INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/components/BG_Midi_Controller/include)

# 混音器 (通过 bg_config.h 中的 ENABLE_MIXER 控制)
file(GLOB MIXER_SRC ${CMAKE_SOURCE_DIR}/components/BG_Mixer/src/*.c)
list(APPEND COMPONENT_SRC ${MIXER_SRC})
list(APPEND INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/components/BG_Mixer/include)

# 音频处理器 (通过 bg_config.h 中的 ENABLE_AUDIO_PROCESSOR 控制)
file(GLOB AUDIO_PROC_SRC 
    ${CMAKE_SOURCE_DIR}/components/BG_Audio_Processor/src/*.c
    ${CMAKE_SOURCE_DIR}/components/BG_Audio_Processor/src/effects/*.c
)
list(APPEND COMPONENT_SRC ${AUDIO_PROC_SRC})
list(APPEND INCLUDE_DIRS 
    ${CMAKE_SOURCE_DIR}/components/BG_Audio_Processor/include
    ${CMAKE_SOURCE_DIR}/components/BG_Audio_Processor/include/effects
)

# 包络生成器 (通用模块,供 SF2 和 BGS 使用)
file(GLOB ENVELOPE_SRC ${CMAKE_SOURCE_DIR}/components/BG_Envelope_Generator/src/*.c)
list(APPEND COMPONENT_SRC ${ENVELOPE_SRC})
list(APPEND INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/components/BG_Envelope_Generator/include)
message(STATUS "Envelope Generator: ENABLED")



# 键盘输入 (通过 bg_config.h 中的 ENABLE_KEYBOARD_INPUT 控制)
file(GLOB INPUT_SRC ${CMAKE_SOURCE_DIR}/hardware/input_handle/src/*.c)
list(APPEND HARDWARE_SRC ${INPUT_SRC})
list(APPEND INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/hardware/input_handle/include)

# ============================================
# 生成可执行文件
# ============================================
set(ALL_SRC ${CORE_SRC} ${HARDWARE_SRC} ${COMPONENT_INTERFACE_SRC} ${HAL_SRC} ${COMPONENT_SRC})

add_executable(${PROJECT_NAME} ${ALL_SRC})

# ============================================
# 设置头文件目录
# ============================================
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRS})

# ============================================
# 链接库
# ============================================
target_link_libraries(${PROJECT_NAME} ${PLATFORM_LIBS})

# ============================================
# 编译信息
# ============================================
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "========================================")
