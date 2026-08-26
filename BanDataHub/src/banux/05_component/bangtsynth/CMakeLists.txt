cmake_minimum_required(VERSION 3.16)
# 独立编译参考。BanDataHub Eclipse 工程不使用本文件。
#   cmake -DBANGTSYNTH_PORT=bandatahub ...
#   cmake -DBANGTSYNTH_PORT=template -DBG_PORT_TEMPLATE=1 ...

project(bangtsynth C)

set(BANGTSYNTH_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include(${BANGTSYNTH_DIR}/sources.core.cmake)

if(NOT BANGTSYNTH_PORT)
    set(BANGTSYNTH_PORT bandatahub)
endif()

set(BANGTSYNTH_SRCS ${BANGTSYNTH_CORE_SRCS})
if(BANGTSYNTH_PORT STREQUAL "bandatahub")
    list(APPEND BANGTSYNTH_SRCS ${BANGTSYNTH_PORT_BANDATAHUB_SRCS})
elseif(BANGTSYNTH_PORT STREQUAL "template")
    list(APPEND BANGTSYNTH_SRCS ${BANGTSYNTH_PORT_TEMPLATE_SRCS})
    add_compile_definitions(BG_PORT_TEMPLATE=1)
else()
    message(FATAL_ERROR "Unknown BANGTSYNTH_PORT=${BANGTSYNTH_PORT}")
endif()

add_library(bangtsynth STATIC ${BANGTSYNTH_SRCS})

target_include_directories(bangtsynth PUBLIC
    ${BANGTSYNTH_DIR}
    ${BANGTSYNTH_DIR}/01_hal
    ${BANGTSYNTH_DIR}/02_core
    ${BANGTSYNTH_DIR}/02_core/soundbank
    ${BANGTSYNTH_DIR}/02_core/midi
    ${BANGTSYNTH_DIR}/02_core/envelope
    ${BANGTSYNTH_DIR}/02_core/sampler
    ${BANGTSYNTH_DIR}/02_core/psram_buffer
    ${BANGTSYNTH_DIR}/02_core/nand_store
    ${BANGTSYNTH_DIR}/02_core/synth_integration
    ${BANGTSYNTH_DIR}/03_app/synth_node
    ${BANGTSYNTH_DIR}/03_app/drum_machine
    ${BANGTSYNTH_DIR}/BG_err_handle
)
