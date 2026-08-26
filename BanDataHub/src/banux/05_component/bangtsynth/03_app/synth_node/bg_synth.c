/**
 * @file bg_synth.c
 * @brief 最小公开 API 实现：包装启动序列与 synth_node
 */
#include "bg_config.h"

#if BANGTSYNTH_EN

#include "bg_synth.h"
#include "bangtsynth_node.h"
#include "bg_storage.h"
#include "soundbank_manager.h"
#include "err_handle.h"

#if SYNTH_SD_NAND_PSRAM_EN
#include "synth_sdnandpsram.h"
#endif
#if defined(BANDATAHUB)
#include "bg_sf2_sd.h"
#endif

int bg_synth_init(void)
{
    int node_ok;

#if SYNTH_SD_NAND_PSRAM_EN
    if (!SYNTH_StartupSequence()) {
#if BG_CFG_EMBEDDED_SF2
        BG_Storage.SetDriver(&bg_storage_driver_embedded);
        (void)soundbank_manager.Init(0);
#endif
    }
#else
    BG_Storage.SetDriver(&bg_storage_driver_embedded);
    (void)soundbank_manager.Init(0);
#endif

    node_ok = BanGTsynth_Node_Init();
    return node_ok;
}

void bg_synth_deinit(void)
{
    BanGTsynth_Node_DeInit();
}

void bg_synth_render(int16_t *out, uint32_t frames)
{
    if (!out || frames == 0) {
        return;
    }
    (void)BanGTsynth_RenderS16(out, frames);
}

void bg_synth_note_on(uint8_t channel, uint8_t note, uint8_t velocity, uint8_t program)
{
    (void)BanGTsynth_TriggerNoteOn(note, velocity, program, channel);
}

void bg_synth_note_off(uint8_t channel, uint8_t note, uint8_t program)
{
    (void)BanGTsynth_TriggerNoteOff(note, program, channel);
}

void bg_synth_midi(const uint8_t *data, uint8_t len)
{
    BanGTsynth_SendMIDI((uint8_t *)data, len);
}

void bg_synth_set_volume(uint8_t volume_0_100)
{
    BanGTsynth_SetVolume(volume_0_100);
}

uint8_t bg_synth_get_volume(void)
{
    return BanGTsynth_GetVolume();
}

uint8_t bg_synth_is_ready(void)
{
    return BanGTsynth_IsInitialized();
}

int bg_synth_load_file(const char *filename)
{
#if defined(BANDATAHUB)
    BG_ERR ret;

    if (BanGTsynth_IsInitialized()) {
        soundbank_manager.DeInit();
    }
    ret = bg_sf2_sd_load_psram(filename);
    if (ret != SUCCESS) {
        return -1;
    }
    if (soundbank_manager.Init(0) != SUCCESS) {
        return -2;
    }
    return 0;
#else
    (void)filename;
    return -1;
#endif
}

#endif /* BANGTSYNTH_EN */
