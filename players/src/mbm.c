/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mbm.c
 *
 * MBM: MoonBlaster
 */

#include <string.h>

#include "support/inc/adpcm2pcm.h"

#include "player_interface.h"
#include "mbm.h"

bool load(const char * file_name)
{
  g_roboplay_interface->open(file_name, false);

  /* Check for USER or EDIT mode */
  bool edit_mode = false;
  g_roboplay_interface->read(&g_mbm_header.song_length, sizeof(g_mbm_header.song_length));
  if(g_mbm_header.song_length == 0xFF) edit_mode = true;

  if(edit_mode)
    g_roboplay_interface->read(&g_mbm_header, sizeof(MBM_HEADER));
  else
     g_roboplay_interface->read(&g_mbm_header.id, sizeof (MBM_HEADER) - sizeof(g_mbm_header.song_length));

  g_mbm_header.track_name[40] = '\0';

  g_roboplay_interface->read((void *)DATA_SEGMENT_BASE, DATA_SEGMENT_SIZE);

  g_position_table = (uint8_t *)DATA_SEGMENT_BASE;

  if(edit_mode)
    g_patterns = (uint8_t **)(DATA_SEGMENT_BASE + 201);
  else
    g_patterns = (uint8_t **)(DATA_SEGMENT_BASE + g_mbm_header.song_length + 1);

  g_roboplay_interface->close();

  /* Try to load a sample kit  */
  uint8_t load_segment = g_roboplay_interface->get_new_segment();
  g_roboplay_interface->set_segment(load_segment);

  adpcm2pcm_init();

  g_play_samples = load_sample_kit(file_name);

  g_roboplay_interface->set_segment(START_SEGMENT_INDEX);

  /* Start with refresh of 60Hz */
  g_refresh = 60.0;

  return true;
}

bool update(void)
{
    handle_psg();
    handle_frequency_mode();

    g_speed_count++;
    if(g_speed_count >= g_speed)
    {
        g_speed_count = 0;

        /* Play next step line */
        for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
        {
            if(g_step_buffer[i])
            {
                if(g_step_buffer[i] <= NOTE_ON)
                    note_on_event(i);
                else if(g_step_buffer[i] == NOTE_OFF)
                    note_off_event(i);
                else if(g_step_buffer[i] < VOLUME_CHANGE)
                    instrument_change_event(i);
                else if(g_step_buffer[i] < STEREO_SET)
                    volume_change_event(i);
                else if(g_step_buffer[i] < NOTE_LINK)
                   stereo_change_event(i);
                else if(g_step_buffer[i] < PITCH)
                   note_link_event(i);
                else if(g_step_buffer[i] < BRIGHTNESS_NEGATIVE)
                   pitch_event(i);
                else if(g_step_buffer[i] < REVERB)
                   brightness_negative_event(i);
                else if(g_step_buffer[i] < BRIGHTNESS_POSITIVE)
                   reverb_change_event(i);
                else if(g_step_buffer[i] < SUSTAIN)
                   brightness_positive_event(i);
                else if(g_step_buffer[i] == SUSTAIN)
                   sustain_event(i);
                else modulation_event(i);
            }
        }

        /* Play sample kit */
        if(g_play_samples)
        {
            uint8_t frequency = g_step_buffer[9];
            uint8_t volume = g_step_buffer[10];
            uint8_t sample_number = g_step_buffer[11] >> 4;

            play_sample(frequency, volume, sample_number);
        }

        /* Play FM / PSG drum */
        music_play_drum();

        /* Handle command channel */
        if(g_step_buffer[COMMAND_CHANNEL])
        {
            if(g_step_buffer[COMMAND_CHANNEL] <= COMMAND_TEMPO)
                tempo_command();
            else if(g_step_buffer[COMMAND_CHANNEL] == COMMAND_PATTERN_END)
                pattern_end_command();
            else if(g_step_buffer[COMMAND_CHANNEL] < COMMAND_STATUS_BYTE)
                drum_set_music_command();
            else if(g_step_buffer[COMMAND_CHANNEL] < COMMAND_TRANSPOSE)
                status_byte_command();
            else
                transpose_command();
        }
    }
    else
    {
        if(g_speed_count == (g_speed - 1))
        {
            g_step++;
            if(g_step > MAX_STEP)
            {
                g_step = 0;
                next_song_position();
            }

            /* Decrunch next step line */
            uint8_t c = 0;
            do
            {
                uint8_t value = *g_pattern_data++;
                if(value >= 0xF3)
                {
                    for(uint8_t j = 0; j < (value - 0xF2); j++)
                        g_step_buffer[c++] = 0;
                }
                else
                    g_step_buffer[c++] = value;
            } while(c < STEP_BUFFER_SIZE);


            for(uint8_t i = 0; i < STEP_BUFFER_SIZE; i++)
            {
                if(g_step_buffer[i] && g_step_buffer[i] <= NOTE_ON)
                {
                    g_mbm_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
                    if(is_audio_channel_active)
                        play_event(i, &g_mbm_status_table[i].audio);
                    if(is_music_channel_active)
                        play_event(i, &g_mbm_status_table[i].music);
                }
            }
        }
    }

    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    /* Set Mix control */
    g_roboplay_interface->opl4_write_wave(0xF8, 0x12);
    g_roboplay_interface->opl4_write_wave(0xF9, 0x1B);

    g_psg_count = 0;

    g_roboplay_interface->opl4_write_fm_1(0xBD, g_mbm_header.sustain & 0xC0);

    for(uint8_t i = 0; i < STEREO_SETTING_SIZE; i++)
        g_stereo_settings[i] = g_mbm_header.channel_chip_set[i];

    for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
    {
        g_mbm_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
        g_mbm_status_table[i].pitch_bend_value = 0;
        g_mbm_status_table[i].reverb_value = g_mbm_header.start_reverb[i];

        update_instrument_audio(i, g_mbm_header.start_instruments_audio[i]);
        if(i < FIRST_DRUM_CHANNEL || !(g_mbm_header.sustain & 0x20))
            update_instrument_music(i, g_mbm_header.start_instruments_music[i]);
    }

    if(g_mbm_header.sustain & 0x20)
    {
        for(uint8_t i = FIRST_DRUM_CHANNEL; i < NR_OF_CHANNELS; i++)
        {
            const uint8_t *instrument_data = g_music_drums[i - FIRST_DRUM_CHANNEL];
            for(uint8_t j = 0; j < AUDIO_INSTRUMENT_DATA_SIZE; j++)
            {
                g_roboplay_interface->opl4_write_fm_1(g_instrument_registers[i][j], instrument_data[j]);
            }  
        }

        /* Start with drum set 1 */
        update_drum_set_music(1);
    }

    if(g_play_samples)
    {
        /* Set sample values to default of MSX AUDIO */
        play_sample(49, 127, 0);
    }

    g_transpose_value = DEFAULT_TRANSPOSE;

    g_speed       = g_mbm_header.start_tempo;
    g_speed_count = 0;
    
    g_position = MAX_POSITION;
    g_step     = MAX_STEP;
}

void command(const uint8_t id)
{
   switch(id)
    {
        case 0x05:
            g_refresh = 50.0;
            g_roboplay_interface->update_refresh();
            break;
        case 0x06:
            g_refresh = 60.0;
            g_roboplay_interface->update_refresh();
            break;
    }
}

float get_refresh(void)
{
    return g_refresh;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "MoonBlaster 1.4 player V1.0 by RoboSoft Inc.";
}

char* get_title(void)
{
    return g_mbm_header.track_name;
}

char* get_author(void)
{
    return "-";
}

char* get_description(void)
{
    if(g_mbm_header.sustain & 0x20)
        return "9CH MSX-AUDIO + 6CH MSX-MUSIC + PERCUSSION";
    else
        return "9CH MSX-AUDIO + 9CH MSX-MUSIC";
}

void next_song_position(void)
{
    if(g_position < g_mbm_header.song_length)
    {
        g_position++;
    }
    else
    {
        if(g_mbm_header.loop_position != MAX_POSITION && g_position != MAX_POSITION)
            g_position = g_mbm_header.loop_position;
        else
            g_position = 0;
    }

    g_pattern_data = g_patterns[g_position_table[g_position] - 1] + DATA_SEGMENT_BASE - sizeof(MBM_HEADER);
}

bool is_audio_channel_active(uint8_t channel)
{
    bool active = (g_stereo_settings[channel] & CHANNEL_AUDIO);

    return active;
}

bool is_music_channel_active(uint8_t channel)
{
    bool active = (g_stereo_settings[channel] & CHANNEL_MUSIC);;

    if(g_mbm_header.sustain & 0x20)
        active = active && (channel < FIRST_DRUM_CHANNEL);

    return active;
}

void tempo_command(void)
{
    g_speed = (COMMAND_TEMPO + 2) - g_step_buffer[COMMAND_CHANNEL];
}

void pattern_end_command(void)
{
    g_step = MAX_STEP;
}

void drum_set_music_command(void)
{
    uint8_t value = g_step_buffer[COMMAND_CHANNEL] - COMMAND_DRUM_SET_MUSIC + 1;
    update_drum_set_music(value);
}

void status_byte_command(void)
{
    /* Status byte updates not used */
}

void transpose_command(void)
{
    g_transpose_value = g_step_buffer[COMMAND_CHANNEL] - (55 - 48);
}

void handle_psg(void)
{
    if(g_psg_count)
    {
        g_psg_count--;
        if(!g_psg_count)
        {
            g_psg_volume = 0;
            g_roboplay_interface->psg_write(0x07, 0xBF);
            g_roboplay_interface->psg_write(0x08, 0x00);
        }
        else
        {
            g_psg_volume -= 2;
            g_roboplay_interface->psg_write(0x08, g_psg_volume);
        }
    }
}

void handle_frequency_mode(void)
{
    for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
    {      
        if(g_mbm_status_table[i].frequency_mode == FREQUENCY_MODE_PITCH_BEND)
            handle_pitch_bend(i);
        else if(g_mbm_status_table[i].frequency_mode >= FREQUENCY_MODE_MODULATION)
            handle_modulation(i);
    }
}

void handle_pitch_bend(const uint8_t channel)
{
    if(is_audio_channel_active(channel))
    {                      
        uint16_t frequency = (g_mbm_status_table[channel].audio.last_frequency_high << 8) + 
                          g_mbm_status_table[channel].audio.last_frequency_low;
    
        frequency += g_mbm_status_table[channel].pitch_bend_value;

        g_mbm_status_table[channel].audio.last_frequency_low = frequency & 0xFF;
        g_mbm_status_table[channel].audio.last_frequency_high = frequency >> 8;

        uint8_t reg  = g_audio_register_data[channel].frequency_register;
        g_roboplay_interface->opl4_write_fm_2(reg, g_mbm_status_table[channel].audio.last_frequency_low);

        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_2(reg, g_mbm_status_table[channel].audio.last_frequency_high);
    }

    if(is_music_channel_active(channel))
    {
        uint16_t frequency = (g_mbm_status_table[channel].music.last_frequency_high << 8) + 
                          g_mbm_status_table[channel].music.last_frequency_low;

        frequency += g_mbm_status_table[channel].pitch_bend_value;

        g_mbm_status_table[channel].music.last_frequency_low = frequency & 0xFF;
        g_mbm_status_table[channel].music.last_frequency_high = frequency >> 8;

        uint8_t reg  = g_audio_register_data[channel].frequency_register;
        g_roboplay_interface->opl4_write_fm_1(reg, g_mbm_status_table[channel].music.last_frequency_low);

        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_1(reg, g_mbm_status_table[channel].music.last_frequency_high);
    }   
}

void handle_modulation(const uint8_t channel)
{
    int16_t modulation_value = (g_modulation_values[g_mbm_status_table[channel].frequency_mode - FREQUENCY_MODE_MODULATION]);

    if(is_audio_channel_active(channel))
    {
        uint16_t frequency = (g_mbm_status_table[channel].audio.last_frequency_high << 8) + 
                              g_mbm_status_table[channel].audio.last_frequency_low;

        frequency += modulation_value;

        g_mbm_status_table[channel].audio.last_frequency_low = frequency & 0xFF;
        g_mbm_status_table[channel].audio.last_frequency_high = frequency >> 8;

        uint8_t reg  = g_audio_register_data[channel].frequency_register;
        g_roboplay_interface->opl4_write_fm_2(reg, g_mbm_status_table[channel].audio.last_frequency_low);

        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_2(reg, g_mbm_status_table[channel].audio.last_frequency_high);
    }

    if(is_music_channel_active(channel))
    {
        uint16_t frequency = (g_mbm_status_table[channel].music.last_frequency_high << 8) + 
                              g_mbm_status_table[channel].music.last_frequency_low;

        frequency += modulation_value;

        g_mbm_status_table[channel].music.last_frequency_low = frequency & 0xFF;
        g_mbm_status_table[channel].music.last_frequency_high = frequency >> 8;

        uint8_t reg  = g_audio_register_data[channel].frequency_register;
        g_roboplay_interface->opl4_write_fm_1(reg, g_mbm_status_table[channel].music.last_frequency_low);

        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_1(reg, g_mbm_status_table[channel].music.last_frequency_high);
    }

    g_mbm_status_table[channel].frequency_mode++;
    if(g_mbm_status_table[channel].frequency_mode >= FREQUENCY_MODE_MODULATION + sizeof(g_modulation_values))
        g_mbm_status_table[channel].frequency_mode = FREQUENCY_MODE_MODULATION;

}

void update_instrument_audio(uint8_t channel, uint8_t instrument)
{
    g_mbm_status_table[channel].audio.last_instrument = instrument;

    uint8_t *instrument_data = g_mbm_header.voice_data_audio[instrument - 1];
    g_mbm_status_table[channel].audio.brightness = instrument_data[2];

    instrument_data[AUDIO_INSTRUMENT_DATA_SIZE - 1] = (instrument_data[AUDIO_INSTRUMENT_DATA_SIZE - 1] & 0x0F) | PAN_SETTING_RIGHT;
    for(uint8_t j = 0; j < AUDIO_INSTRUMENT_DATA_SIZE; j++)
    {
        uint8_t reg  = g_instrument_registers[channel][j];
        uint8_t data = instrument_data[j];

        if(reg > 0xC5 && (g_mbm_header.sustain & 0x20))
        {
            uint8_t feedback = (data & 0x0E) >> 1;

            if(feedback > 4) feedback = 4;
            data &= 0xF1;
            data |= (feedback << 1);
        }
        g_roboplay_interface->opl4_write_fm_2(reg, data);
    }  
}

void update_instrument_music(uint8_t channel, uint8_t instrument_index)
{
    uint8_t instrument = g_mbm_header.instrument_list_music[instrument_index - 1].instrument;
    uint8_t volume     = g_mbm_header.instrument_list_music[instrument_index - 1].volume << 1;
    uint8_t pan_preset = PAN_SETTING_LEFT;

    g_mbm_status_table[channel].music.last_instrument = instrument;

    uint8_t opl_instrument_preset[PRESET_DATA_SIZE];

    if(instrument > 15)
    {
        instrument -= 16;

        /* Original instrument */       
        opl_instrument_preset[0]  = g_mbm_header.original_instrument_data[instrument][0];                                     /* 0x20 + x */
        opl_instrument_preset[1]  = g_mbm_header.original_instrument_data[instrument][1];                                     /* 0x23 + x */
        opl_instrument_preset[2]  = g_mbm_header.original_instrument_data[instrument][2];                                     /* 0x40 + x */
        opl_instrument_preset[3]  = volume | 1;                                                                               /* 0x43 + x */
        opl_instrument_preset[4]  = g_mbm_header.original_instrument_data[instrument][4];                                     /* 0x60 + x */
        opl_instrument_preset[5]  = g_mbm_header.original_instrument_data[instrument][5];                                     /* 0x63 + x */
        opl_instrument_preset[6]  = g_mbm_header.original_instrument_data[instrument][6];                                     /* 0x80 + x */
        opl_instrument_preset[7]  = g_mbm_header.original_instrument_data[instrument][7];                                     /* 0x83 + x */
        opl_instrument_preset[8]  = ((g_mbm_header.original_instrument_data[instrument][3] & 0x07) << 1) | pan_preset;        /* 0xC0 + x */
        opl_instrument_preset[9]  = (g_mbm_header.original_instrument_data[instrument][3] & 0x10) >> 4;                       /* 0xE0 + x */
        opl_instrument_preset[10] = (g_mbm_header.original_instrument_data[instrument][3] & 0x08) >> 3;                       /* 0xE3 + x */
    }
    else
    {
        /* ROM instrument */
        opl_instrument_preset[0]  = g_music_voice_patches[instrument][0];                                     /* 0x20 + x */
        opl_instrument_preset[1]  = g_music_voice_patches[instrument][1];                                     /* 0x23 + x */
        opl_instrument_preset[2]  = g_music_voice_patches[instrument][2];                                     /* 0x40 + x */
        opl_instrument_preset[3]  = volume | 1;                                                               /* 0x43 + x */
        opl_instrument_preset[4]  = g_music_voice_patches[instrument][4];                                     /* 0x60 + x */
        opl_instrument_preset[5]  = g_music_voice_patches[instrument][5];                                     /* 0x63 + x */
        opl_instrument_preset[6]  = g_music_voice_patches[instrument][6];                                     /* 0x80 + x */
        opl_instrument_preset[7]  = g_music_voice_patches[instrument][7];                                     /* 0x83 + x */
        opl_instrument_preset[8]  = ((g_music_voice_patches[instrument][3] & 0x07) << 1) | pan_preset;        /* 0xC0 + x */
        opl_instrument_preset[9]  = (g_music_voice_patches[instrument][3] & 0x10) >> 4;                       /* 0xE0 + x */
        opl_instrument_preset[10] = (g_music_voice_patches[instrument][3] & 0x08) >> 3;                       /* 0xE3 + x */
    }

    g_mbm_status_table[channel].music.brightness = opl_instrument_preset[2];

    for(uint8_t i = 0; i < PRESET_DATA_SIZE; i++)
        g_roboplay_interface->opl4_write_fm_1(g_preset_registers[channel][i], opl_instrument_preset[i]);
    
}

void update_drum_set_music(uint8_t drum_set)
{
    for(uint8_t i = FIRST_DRUM_CHANNEL; i < NR_OF_CHANNELS; i++)
    {
        uint16_t frequency = 2 * g_mbm_header.drum_frequencies_music[drum_set - 1][i - FIRST_DRUM_CHANNEL];

        uint8_t reg  = g_audio_register_data[i].frequency_register;
        g_roboplay_interface->opl4_write_fm_1(reg, frequency & 0xFF);

        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_1(reg, (frequency >> 8) & 0x1F);
    }
}

void play_event(const uint8_t channel, MBM_NOTE_DATA *note_data)
{
    uint8_t note = g_step_buffer[channel] + g_transpose_value;
    if(note > NOTE_ON + DEFAULT_TRANSPOSE)
        note -= NOTE_ON;
    else if(note < DEFAULT_TRANSPOSE + 1)
        note += NOTE_ON;

    note -= DEFAULT_TRANSPOSE;

    note_data->last_note = note;

    uint16_t frequency = g_frequency_table[note - 1] << 1;

    note_data->next_frequency_low  = (frequency & 0xFF) + (g_mbm_status_table[channel].reverb_value << 1); 
    note_data->next_frequency_high = frequency >> 8;
}

void music_play_drum(void)
{
    uint8_t data = g_step_buffer[DRUM_CHANNEL] & 0x0F;

    if(data)
    {
        uint8_t value = g_mbm_header.drum_setup_music_psg[data - 1];

        if(value >= 0x20)
        {
            /* Play PSG drum */
            uint8_t index = (((value << 3) | (value >> (8 - 3))) & 0x07) - 1;

            g_psg_count = g_psg_drums[index].psg_count;

            for(uint8_t i = 0; i < g_psg_drums[index].data_size; i++)
            {
                g_roboplay_interface->psg_write(g_psg_drums[index].data[i][0], g_psg_drums[index].data[i][1]);
            }

            g_psg_volume = g_psg_drums[index].psg_volume;
            g_roboplay_interface->psg_write(0x08, g_psg_volume);
        }
        if(g_mbm_header.sustain & 0x20)
        {
            /* Play FM drum */
            value &= 0x1F;
            g_roboplay_interface->opl4_write_fm_1(0xBD, value);
            g_roboplay_interface->opl4_write_fm_1(0xBD, value | 0x20);
        }
    }
}

void note_on_event(const uint8_t channel)
{
    if(is_audio_channel_active(channel))
    {
        uint8_t reg  = g_audio_register_data[channel].frequency_register;
        uint8_t data = g_mbm_status_table[channel].audio.next_frequency_low;

        g_roboplay_interface->opl4_write_fm_2(reg, data);
        g_mbm_status_table[channel].audio.last_frequency_low = data;

        reg += 0x10;
        data = g_mbm_status_table[channel].audio.next_frequency_high & 0xDF;
        g_roboplay_interface->opl4_write_fm_2(reg, data);
        
        data = g_mbm_status_table[channel].audio.next_frequency_high | 0x20;
        g_roboplay_interface->opl4_write_fm_2(reg, data);
        g_mbm_status_table[channel].audio.last_frequency_high = data;
    }

    if(is_music_channel_active(channel))
    {
        uint8_t reg  = g_audio_register_data[channel].frequency_register;
        uint8_t data = g_mbm_status_table[channel].music.next_frequency_low;

        g_roboplay_interface->opl4_write_fm_1(reg, data);
        g_mbm_status_table[channel].music.last_frequency_low = data;

        reg += 0x10;
        data = g_mbm_status_table[channel].music.next_frequency_high & 0xDF;
        g_roboplay_interface->opl4_write_fm_1(reg, data);
        
        data = g_mbm_status_table[channel].music.next_frequency_high | 0x20;
        g_roboplay_interface->opl4_write_fm_1(reg, data);
        g_mbm_status_table[channel].music.last_frequency_high = data;
    }
}

void note_off_event(const uint8_t channel)
{
    if(is_audio_channel_active(channel))
    {
        uint8_t reg = g_audio_register_data[channel].frequency_register;
        uint8_t data = g_mbm_status_table[channel].audio.last_frequency_low;

        g_roboplay_interface->opl4_write_fm_2(reg, data);

        reg += 0x10;
        data = g_mbm_status_table[channel].audio.next_frequency_high & 0xDF;

        g_roboplay_interface->opl4_write_fm_2(reg, data);
        g_mbm_status_table[channel].audio.last_frequency_high = data;
    }

    if(is_music_channel_active(channel))
    {
        uint8_t reg = g_audio_register_data[channel].frequency_register;
        uint8_t data = g_mbm_status_table[channel].music.last_frequency_low;

        g_roboplay_interface->opl4_write_fm_1(reg, data);

        reg += 0x10;
        data = g_mbm_status_table[channel].music.next_frequency_high & 0xDF;

        g_roboplay_interface->opl4_write_fm_1(reg, data);
        g_mbm_status_table[channel].music.last_frequency_high = data;
    }
}

void instrument_change_event(const uint8_t channel)
{
    g_mbm_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    uint8_t instrument = g_step_buffer[channel] - INSTRUMENT_CHANGE + 1;

    if(is_audio_channel_active(channel))
    {
        update_instrument_audio(channel, instrument);
    }

    if(is_music_channel_active(channel))
    {
        update_instrument_music(channel, instrument);
    }
}

void volume_change_event(const uint8_t channel)
{
    uint8_t volume = (g_step_buffer[channel] - VOLUME_CHANGE) << 1;

    if(is_audio_channel_active(channel))
    {
        g_roboplay_interface->opl4_write_fm_2(g_audio_register_data[channel].volume_register, volume | 1);
    }
        

    if(is_music_channel_active(channel))
    {
        g_roboplay_interface->opl4_write_fm_1(g_audio_register_data[channel].volume_register, volume | 1);
    }
}

void stereo_change_event(const uint8_t channel)
{
    g_mbm_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    g_stereo_settings[channel] = g_step_buffer[channel] - STEREO_SET + 1;

    if(!is_audio_channel_active(channel))
    {
        uint8_t reg = g_audio_register_data[channel].frequency_register;
        g_roboplay_interface->opl4_write_fm_2(reg, 0x00);
        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_2(reg, 0x00);
    }

    if(!is_music_channel_active(channel))
    {
        uint8_t reg = g_audio_register_data[channel].frequency_register;
        g_roboplay_interface->opl4_write_fm_1(reg, 0x00);
        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_1(reg, 0x00);
    }
}

void note_link_event(const uint8_t channel)
{
    int8_t link = g_step_buffer[channel] - 189;
    g_mbm_status_table[channel].audio.last_note += link;
    g_mbm_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    if(is_audio_channel_active(channel))
    {
        uint8_t reg = g_audio_register_data[channel].frequency_register;
        uint16_t frequency = 2 * g_frequency_table[g_mbm_status_table[channel].audio.last_note - 1];

        uint8_t data = frequency & 0xFF;
        data += g_mbm_status_table[channel].reverb_value;
        data += g_mbm_status_table[channel].reverb_value;

        g_roboplay_interface->opl4_write_fm_2(reg, data);
        g_mbm_status_table[channel].audio.last_frequency_low = data;

        data = frequency >> 8;
        data |= 0x20;

        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_2(reg, data);
        g_mbm_status_table[channel].audio.last_frequency_high = data;
    }

    if(is_music_channel_active(channel))
    {
        uint8_t reg = g_audio_register_data[channel].frequency_register;
        uint16_t frequency = 2 * g_frequency_table[g_mbm_status_table[channel].music.last_note - 1];

        uint8_t data = frequency & 0xFF;
        data += g_mbm_status_table[channel].reverb_value;
        data += g_mbm_status_table[channel].reverb_value;

        g_roboplay_interface->opl4_write_fm_1(reg, data);
        g_mbm_status_table[channel].music.last_frequency_low = data;

        data = frequency >> 8;
        data |= 0x20;

        reg += 0x10;
        g_roboplay_interface->opl4_write_fm_1(reg, data);
        g_mbm_status_table[channel].music.last_frequency_high = data;
    }

}

void pitch_event(const uint8_t channel)
{
    int8_t value = g_step_buffer[channel] - 208;
    g_mbm_status_table[channel].frequency_mode = FREQUENCY_MODE_PITCH_BEND;
    g_mbm_status_table[channel].pitch_bend_value = value;
}

void brightness_negative_event(const uint8_t channel)
{
    uint8_t value = g_step_buffer[channel] - 224;

    if(is_audio_channel_active(channel))
    {
        value += (g_mbm_status_table[channel].audio.brightness & 0x3F);
        value += (g_mbm_status_table[channel].audio.brightness & 0xC0);

        g_mbm_status_table[channel].audio.brightness = value;

        g_roboplay_interface->opl4_write_fm_2(g_audio_register_data[channel].volume_register - 3, value);
    }

    if(is_music_channel_active(channel) && g_mbm_status_table[channel].music.last_instrument > 15)
    {
        value += (g_mbm_status_table[channel].music.brightness & 0x3F);
        value += (g_mbm_status_table[channel].music.brightness & 0xC0);

        g_mbm_status_table[channel].music.brightness = value;

        g_roboplay_interface->opl4_write_fm_1(g_audio_register_data[channel].volume_register - 3, value);
    }
}

void reverb_change_event(const uint8_t channel)
{
    g_mbm_status_table[channel].reverb_value = g_step_buffer[channel] - 227;
}

void brightness_positive_event(const uint8_t channel)
{
    uint8_t value = g_step_buffer[channel] - 230;

    if(is_audio_channel_active(channel))
    {
        value += (g_mbm_status_table[channel].audio.brightness & 0x3F);
        value += (g_mbm_status_table[channel].audio.brightness & 0xC0);

        g_mbm_status_table[channel].audio.brightness = value;

        g_roboplay_interface->opl4_write_fm_2(g_audio_register_data[channel].volume_register - 3, value);
    }

    if(is_music_channel_active(channel) && g_mbm_status_table[channel].music.last_instrument > 15)
    {
        value += (g_mbm_status_table[channel].music.brightness & 0x3F);
        value += (g_mbm_status_table[channel].music.brightness & 0xC0);

        g_mbm_status_table[channel].music.brightness = value;

        g_roboplay_interface->opl4_write_fm_1(g_audio_register_data[channel].volume_register - 3, value);
    }
}

void sustain_event(const uint8_t channel)
{
    g_mbm_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    if(is_audio_channel_active(channel))
    {
        uint8_t reg  = g_audio_register_data[channel].frequency_register;
        uint8_t data = g_mbm_status_table[channel].audio.last_frequency_low;

        g_roboplay_interface->opl4_write_fm_2(reg, data);

        reg += 0x10;
        data = g_mbm_status_table[channel].audio.next_frequency_high & 0xDF;

        g_roboplay_interface->opl4_write_fm_2(reg, data);
        g_mbm_status_table[channel].audio.last_frequency_high = data;
    }

    if(is_music_channel_active(channel))
    {
        uint8_t reg  = g_audio_register_data[channel].frequency_register;
        uint8_t data = g_mbm_status_table[channel].music.last_frequency_low;

        g_roboplay_interface->opl4_write_fm_1(reg, data);

        reg += 0x10;
        data = g_mbm_status_table[channel].audio.next_frequency_high & 0xDF;

        g_roboplay_interface->opl4_write_fm_1(reg, data);
        g_mbm_status_table[channel].music.last_frequency_high = data;
    }
}

void modulation_event(const uint8_t channel)
{
    g_mbm_status_table[channel].frequency_mode = FREQUENCY_MODE_MODULATION;
}

void play_sample(uint8_t frequency, uint8_t volume, uint8_t sample_number)
{
    if(volume)
    {
        volume = (127 - volume) / 2;
        g_roboplay_interface->opl4_write_wave(0x50, (volume << 1) | 0x01);
    }

    if(frequency)
    {
        frequency--;
        g_roboplay_interface->opl4_write_wave(0x38, g_pitch_table[frequency][0]);
        g_roboplay_interface->opl4_write_wave(0x20, g_pitch_table[frequency][1] | 0x01);
    }

    if(sample_number)
    {
        g_roboplay_interface->opl4_write_wave(0x68, 0x00);

        g_roboplay_interface->opl4_write_wave(0x08, 0x7F + sample_number);
        g_roboplay_interface->opl4_wait_for_load();

        g_roboplay_interface->opl4_write_wave(0x68, 0x80);
    }
}

void write_sample_headers(void)
{
    uint16_t *sample_kit_header = (uint8_t *)READ_BUFFER;

    uint16_t header_address = 0;
    for(int i = 0; i < NR_OF_SAMPLE_BLOCKS; i++)
    {
        uint32_t start = *sample_kit_header * 8;
        if(*sample_kit_header & 1) start += 7;
        sample_kit_header++;

        uint16_t end = *sample_kit_header * 8;
        if(*sample_kit_header & 1) end += 7;
        sample_kit_header++;

        end = end - start;
        uint16_t loop = end - 1;

        start += OPL_WAVE_ADDRESS;
        end ^= 0xFFFF;

        /* Write header to OPL memory */
        g_roboplay_interface->opl4_write_wave(3, 0x20);
        g_roboplay_interface->opl4_write_wave(4, (header_address >> 8) & 0xFF);
        g_roboplay_interface->opl4_write_wave(5, header_address & 0xFF);

        /* Start address */
        g_roboplay_interface->opl4_write_wave(6, (start >> 16) & 0x3F);
        g_roboplay_interface->opl4_write_wave(6, (start >> 8) & 0xFF);
        g_roboplay_interface->opl4_write_wave(6, start & 0xFF);

        /* Loop address */
        g_roboplay_interface->opl4_write_wave(6, (loop >> 8) & 0xFF);
        g_roboplay_interface->opl4_write_wave(6, loop & 0xFF);

        /* End address */
        g_roboplay_interface->opl4_write_wave(6, (end >> 8) & 0xFF);
        g_roboplay_interface->opl4_write_wave(6, end & 0xFF);


        g_roboplay_interface->opl4_write_wave(6, 0);          /* LFO, VIB                */
        g_roboplay_interface->opl4_write_wave(6, 0xF0);       /* AR, D1R                 */
        g_roboplay_interface->opl4_write_wave(6, 0xFF);       /* DL, D2R                 */
        g_roboplay_interface->opl4_write_wave(6, 0x0F);       /* Rate correction , RR    */
        g_roboplay_interface->opl4_write_wave(6, 0);          /* AM                      */

        header_address += 12;
    }
}

bool load_sample_kit(const char* file_name)
{
    bool result = false;

    char* sample_kit_name = (char *)READ_BUFFER;

    strcpy(sample_kit_name, file_name);
    uint8_t found = strlen(sample_kit_name) - 1;
    while(sample_kit_name[found] != '\\' && sample_kit_name[found] != ':' && found > 0) found--;
    if(found) 
        sample_kit_name[found + 1] = '\0';
    else
        sample_kit_name[found] = '\0';

    uint8_t l = strlen(sample_kit_name);
    uint8_t c = 0;

    while(g_mbm_header.sample_kit_name[c] != ' ' && c < 8)
        sample_kit_name[l + c] = g_mbm_header.sample_kit_name[c++];
    sample_kit_name[l + c] = '\0';

    strcat(sample_kit_name, ".MBK");

    if(!g_roboplay_interface->exists(sample_kit_name))
    {
        /* Try to load the sample kit based on the file name */
        strcpy(sample_kit_name, file_name);
        sample_kit_name[strlen(sample_kit_name) - 4] = '\0';
        strcat(sample_kit_name, ".MBK");
    }

    if(g_roboplay_interface->exists(sample_kit_name))
    {      
        result = true;

        /* Set OPL4 to memory access mode */
        g_roboplay_interface->opl4_write_wave(0x02, 0x11);

        g_roboplay_interface->open(sample_kit_name, false);
        g_roboplay_interface->read((uint8_t *)READ_BUFFER, SAMPLE_FILE_HEADER_SIZE);
        write_sample_headers();

        /* Set wave memory adress */
        uint32_t wave_address = OPL_WAVE_ADDRESS;
        g_roboplay_interface->opl4_write_wave(3, (wave_address >> 16) & 0x3F);
        g_roboplay_interface->opl4_write_wave(4, (wave_address >> 8)  & 0xFF);
        g_roboplay_interface->opl4_write_wave(5, wave_address & 0xFF);

        for(int i = 0; i < 8; i++)
        {
            g_roboplay_interface->read((uint8_t *)READ_BUFFER, READ_BUFFER_SIZE);
            adpcm2pcm();
            g_roboplay_interface->opl4_write_wave_data((uint8_t*)DATA_SEGMENT_BASE, 2 * READ_BUFFER_SIZE);
        }

        g_roboplay_interface->close();

        /* Set OPL4 to sound generation mode */
        g_roboplay_interface->opl4_write_wave(0x02, 0x10);
    }

    return result;
}
