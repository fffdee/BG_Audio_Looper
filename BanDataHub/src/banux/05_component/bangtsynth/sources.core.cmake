# BanGTsynth 必选源（与 IDE 无关）。路径相对本文件所在目录。
set(BANGTSYNTH_CORE_SRCS
    01_hal/bg_log.c
    01_hal/bg_storage.c
    01_hal/hardware_interfance.c
    02_core/soundbank/soundbank_manager.c
    02_core/soundbank/sf2_parser.c
    02_core/soundbank/bgs_parser.c
    02_core/midi/midi_controller.c
    02_core/midi/standard_request_processing.c
    02_core/envelope/bg_envelope.c
    02_core/sampler/sampler.c
    02_core/psram_buffer/psram_buffer.c
    02_core/nand_store/nand_store.c
    02_core/synth_integration/synth_sdnandpsram.c
    02_core/synth_integration/synth_startup.c
    03_app/synth_node/bangtsynth_node.c
    03_app/synth_node/bg_synth.c
    03_app/drum_machine/drum_machine.c
)

set(BANGTSYNTH_PORT_BANDATAHUB_SRCS
    01_hal/port/bandatahub/bg_storage_bandatahub.c
    01_hal/port/bandatahub/bg_osal_freertos.c
    01_hal/port/bandatahub/bg_extmem_bandatahub.c
    01_hal/port/bandatahub/bg_mem_stdlib.c
    01_hal/port/bandatahub/bg_download_port_bandatahub.c
    01_hal/port/embedded/bg_storage_embedded.c
)

set(BANGTSYNTH_PORT_TEMPLATE_SRCS
    01_hal/port/template/bg_storage_template.c
    01_hal/port/template/bg_osal_baremetal.c
    01_hal/port/template/bg_extmem_template.c
    01_hal/port/template/bg_mem_arena.c
    01_hal/port/template/bg_download_port_template.c
)
