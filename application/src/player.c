/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * player.c
 */

#include <string.h>

#include "support/inc/printf_simple.h"
#include "players/inc/player_interface.h"

#include "file.h"
#include "memory.h"
#include "device.h"
#include "player.h"

extern char *g_current_player_name;
extern char *g_extension;

#ifdef __DEBUG
void debug_string(char *const str)
{
    printf_simple("%s", str);
}

void debug_hex(const uint16_t num)
{
    printf_simple("%x", num);
}
#endif

void find_player_name(void)
{
    /* Find player name based on extension of current song name */
    get_program_path(g_current_player_name);
    strcat(g_current_player_name, g_extension);
    strcat(g_current_player_name, ".PLY");
}

void init_player(void)
{
    load_player(g_current_player_name);

    printf_simple("%s\n\r", g_roboplay_interface->get_player_info());

    g_roboplay_interface->open   = &file_open;
    g_roboplay_interface->read   = &file_read;
    g_roboplay_interface->close  = &file_close;
    g_roboplay_interface->exists = &file_exists;
    g_roboplay_interface->seek   = &file_seek;

    g_roboplay_interface->get_new_segment     = &get_free_ram_segment;
    g_roboplay_interface->set_segment         = &set_active_ram_segment;

    g_roboplay_interface->update_refresh = &set_refresh;

    g_roboplay_interface->opl4_set_mode = &set_opl4_mode;

    g_roboplay_interface->opl4_write_fm_1 = &write_opl4_fm_1;
    g_roboplay_interface->opl4_write_fm_2 = &write_opl4_fm_2;

    g_roboplay_interface->opl4_write_wave = &opl4_write_wave_register;
    g_roboplay_interface->opl4_write_wave_data = &opl4_write_wave_data;

    g_roboplay_interface->opl4_read_status = &opl4_read_status_register;
    g_roboplay_interface->opl4_read_fm_1 = &opl4_read_fm_register_array_1;
    g_roboplay_interface->opl4_read_fm_2 = &opl4_read_fm_register_array_2;
    g_roboplay_interface->opl4_read_wave_register = &opl4_read_wave_register;

    g_roboplay_interface->opl4_wait_for_load = &opl4_wait_for_ld;

    if(darky_detect())
    {
      g_roboplay_interface->psg_write = &epsg1_write;
      g_roboplay_interface->psg_read = &epsg1_read;
      g_roboplay_interface->psg2_write = &epsg2_write;
      g_roboplay_interface->psg2_read = &epsg2_read;
    }
    else
    {
      g_roboplay_interface->psg_write = &psg_write;
      g_roboplay_interface->psg_read = &psg_read;
      g_roboplay_interface->psg2_write = &psg2_write;
      g_roboplay_interface->psg2_read = &psg2_read;
    }

    g_roboplay_interface->scc_set_waveform = &scc_set_waveform;
    g_roboplay_interface->scc_set_frequency = &scc_set_frequency;
    g_roboplay_interface->scc_set_volume = &scc_set_volume;
    g_roboplay_interface->scc_write_register = &scc_write_register;

    g_roboplay_interface->opm_write = &write_opm_fm;
    g_roboplay_interface->opm_read_status = &opm_read_status_register;

    if(msx_midi_detect())
    {
      g_roboplay_interface->midi_send_data_1 = &msx_midi_send_data;
      g_roboplay_interface->midi_send_data_2 = &msx_midi_send_data_2;
      g_roboplay_interface->midi_send_data_3 = &msx_midi_send_data_3;
    }
    else
    {
      g_roboplay_interface->midi_send_data_1 = &midi_pac_send_data;
      g_roboplay_interface->midi_send_data_2 = &midi_pac_send_data_2;
      g_roboplay_interface->midi_send_data_3 = &midi_pac_send_data_3;
    }

    g_roboplay_interface->soundstar_write = &soundstar_write;

    g_roboplay_interface->msx_audio_write = &write_msx_audio_fm;
    g_roboplay_interface->msx_audio_read_status = &msx_audio_read_status_register;
    g_roboplay_interface->msx_audio_write_adpcm_data = &msx_audio_write_adpcm_data;

    g_roboplay_interface->msx_music_write = &write_msx_music_fm;

    if(sid_detect())
    {
      g_roboplay_interface->sid_write_register = &sid_write;
      g_roboplay_interface->sid_read_register = &sid_read;
    }

#ifdef __DEBUG
    g_roboplay_interface->debug_string = &debug_string;
    g_roboplay_interface->debug_hex = &debug_hex;
#endif    
}

