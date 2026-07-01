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
# 头文件目录 (分层架构)
# ============================================
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/01_hal
    ${CMAKE_SOURCE_DIR}/02_core/soundbank
    ${CMAKE_SOURCE_DIR}/02_core/midi
    ${CMAKE_SOURCE_DIR}/02_core/envelope
    ${CMAKE_SOURCE_DIR}/03_app/synth_node
    ${CMAKE_SOURCE_DIR}/03_app/drum_machine
    ${CMAKE_SOURCE_DIR}/data/soundbank_data
    ${CMAKE_SOURCE_DIR}/data/drum_data
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

# ============================================
# 硬件接口层源文件 (已整合到 01_hal/)
# ============================================
set(COMPONENT_INTERFACE_SRC
    ${CMAKE_SOURCE_DIR}/01_hal/hardware_interfance.c
)

# 添加硬件层头文件
list(APPEND INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/hardware/play/include
)

# ============================================
# 平台 HAL 源文件 (01_hal/port/)
# ============================================
if(PLATFORM STREQUAL "linux")
    set(HAL_SRC 
        ${CMAKE_SOURCE_DIR}/01_hal/port/linux/bg_hal_linux.c
        ${CMAKE_SOURCE_DIR}/01_hal/bg_log.c
        ${CMAKE_SOURCE_DIR}/01_hal/bg_storage.c
        ${CMAKE_SOURCE_DIR}/01_hal/port/linux/bg_storage_linux.c
        ${CMAKE_SOURCE_DIR}/01_hal/port/linux/bg_download_port_linux.c
    )
    set(PLATFORM_LIBS ev asound m)
elseif(PLATFORM STREQUAL "stm32")
    set(HAL_SRC 
        ${CMAKE_SOURCE_DIR}/01_hal/bg_log.c
        ${CMAKE_SOURCE_DIR}/01_hal/bg_storage.c
    )
    # 添加 STM32 HAL 库路径
elseif(PLATFORM STREQUAL "esp32")
    set(HAL_SRC 
        ${CMAKE_SOURCE_DIR}/01_hal/bg_log.c
        ${CMAKE_SOURCE_DIR}/01_hal/bg_storage.c
    )
    # 添加 ESP-IDF 配置
else()
    message(FATAL_ERROR "Unsupported platform: ${PLATFORM}")
endif()

# ============================================
# 组件源文件 (分层架构)
# ============================================
set(COMPONENT_SRC)

# 02_core: 音源管理器 (BGS + SF2)
file(GLOB SOUNDBANK_SRC ${CMAKE_SOURCE_DIR}/02_core/soundbank/*.c)
list(APPEND COMPONENT_SRC ${SOUNDBANK_SRC})
message(STATUS "Soundbank Manager: ENABLED (BG + SF2)")

# 02_core: MIDI 控制器
file(GLOB MIDI_SRC ${CMAKE_SOURCE_DIR}/02_core/midi/*.c)
list(APPEND COMPONENT_SRC ${MIDI_SRC})

# 02_core: 包络生成器
file(GLOB ENVELOPE_SRC ${CMAKE_SOURCE_DIR}/02_core/envelope/*.c)
list(APPEND COMPONENT_SRC ${ENVELOPE_SRC})
message(STATUS "Envelope Generator: ENABLED")

# 03_app: 合成器节点
file(GLOB SYNTH_NODE_SRC ${CMAKE_SOURCE_DIR}/03_app/synth_node/*.c)
list(APPEND COMPONENT_SRC ${SYNTH_NODE_SRC})

# 03_app: 鼓机
file(GLOB DRUM_MACHINE_SRC ${CMAKE_SOURCE_DIR}/03_app/drum_machine/*.c)
list(APPEND COMPONENT_SRC ${DRUM_MACHINE_SRC})

# data: 嵌入音源数据
file(GLOB SOUNDBANK_DATA_SRC ${CMAKE_SOURCE_DIR}/data/soundbank_data/*.c)
file(GLOB DRUM_DATA_SRC ${CMAKE_SOURCE_DIR}/data/drum_data/*.c)
list(APPEND COMPONENT_SRC ${SOUNDBANK_DATA_SRC} ${DRUM_DATA_SRC})



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
