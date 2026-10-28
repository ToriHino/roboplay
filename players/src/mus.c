/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mus.c
 *
 * MUS: FAC SoundTracker
 */


#include <string.h>

#include "support/inc/adpcm2pcm.h"

#include "player_interface.h"
#include "mus.h"

bool load(const char *file_name)
{
    /* Set OPL4 to memory access mode */
    g_roboplay_interface->opl4_write_wave(0x02, 0x11);

    g_roboplay_interface->open(file_name, false);

    g_roboplay_interface->read((void *)DATA_SEGMENT_BASE, 7);
    g_roboplay_interface->read((void *)DATA_SEGMENT_BASE, DATA_SEGMENT_SIZE);

    g_roboplay_interface->close();

    g_mus_header = (void *)(DATA_SEGMENT_BASE + MAX_SONG_SIZE);

    if(g_mus_header->version == 0x03)
    {
        g_pattern_data = (uint8_t *)DATA_SEGMENT_BASE;

        uint8_t value = 0x00;
        while((uint16_t)g_pattern_data < (DATA_SEGMENT_BASE + MAX_SONG_SIZE))
        {
            *g_pattern_data ^= value;
            value++;
            g_pattern_data++;
        }
    }

    uint8_t length = (g_mus_header->version > 0x02) ? (SONG_NAME_LENGTH - 5) : SONG_NAME_LENGTH;
    strncpy(g_song_name, g_mus_header->song_name, length);
    g_song_name[length] = '\0';

    length = (g_mus_header->version > 0x02) ? AUTHOR_NAME_LENGTH - 5 : SONG_NAME_LENGTH;
    strncpy(g_author, g_mus_header->author, length);
    g_author[length] = '\0';

    uint16_t song_length = (g_mus_header->number_of_tracks + 1) * STEP_DATA_SIZE * PATTERN_SIZE;
    if(song_length > MAX_SONG_SIZE) song_length = MAX_SONG_SIZE;
    
    g_song_end = (uint8_t *)(DATA_SEGMENT_BASE + song_length);

    /* Load the sample kit when available */
    g_unpack_segment = g_roboplay_interface->get_new_segment();

    /* Set wave memory adress */
    uint32_t wave_address = OPL_WAVE_ADDRESS;
    g_roboplay_interface->opl4_write_wave(3, (wave_address >> 16) & 0x3F);
    g_roboplay_interface->opl4_write_wave(4, (wave_address >> 8)  & 0xFF);
    g_roboplay_interface->opl4_write_wave(5, wave_address & 0xFF);

    adpcm2pcm_init();

    g_play_samples = load_sample_kit(file_name, ".SM1");
    g_play_samples = g_play_samples && load_sample_kit(file_name, ".SM2");

    if(g_play_samples)
        write_sample_headers();   

    /* Set OPL4 to sound generation mode */
    g_roboplay_interface->opl4_write_wave(0x02, 0x10);

    return true;
}

bool update(void)
{
    g_speed_count++;
    if(g_speed_count == g_speed)
    {
        g_speed_count = 0;

        /* Play next step line */
        for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
        {
            uint8_t data = *g_pattern_data++;

            if(data && (g_mus_header->device_id == DEVICE_AUDIO || (g_mus_header->device_id == DEVICE_MUSIC && i < NR_OF_CHANNELS - NR_OF_DRUM_CHANNELS)))
            {
                if(data < NOTE_OFF)
                {
                    note_on(i, data);
                }
                else
                {
                    if(g_mus_header->version <= 1)
                    {
                        /* SoundTracker V1.0 */
                        if(data & 0x40) note_off(i);
                    }
                    else
                    {
                        /* SoundTracker V2.0 or higher */
                        if(data == NOTE_OFF) note_off(i);
                        else if(data == DETUNE_UP) detune_up(i);
                        else if(data == DETUNE_DOWN) detune_down(i);
                        else if(data < PITCH_BEND_DOWN) pitch_bend_up(i, data);
                        else if(data < TEMPO_CHANGE) pitch_bend_down(i, data);
                        else if(data < VOLUME_CHANGE) change_tempo(data);
                        else if(data  < BRIGHTNESS_CHANGE) change_volume(i, data);
                        else change_brightness(i, data);
                    }
                }
            }
        }

        if(g_mus_header->device_id == DEVICE_MUSIC)
        {
            /* Skip frequency */
            g_pattern_data ++;

            uint8_t volume = *g_pattern_data++;

            uint8_t data = *g_pattern_data++;
            if(g_mus_header->version >= 0x02) data >>= 4;

            if(data)
            {
              volume >>= 2;
              volume ^= 0x3f;
              
              g_roboplay_interface->opl4_write_fm_1(0x50, volume);
              g_roboplay_interface->opl4_write_fm_1(0x51, volume);
              g_roboplay_interface->opl4_write_fm_1(0x52, volume);
              g_roboplay_interface->opl4_write_fm_1(0x53, volume);
              g_roboplay_interface->opl4_write_fm_1(0x54, volume);
              g_roboplay_interface->opl4_write_fm_1(0x55, volume);

              g_roboplay_interface->opl4_write_fm_1(0xBD, 0x00);
              g_roboplay_interface->opl4_write_fm_1(0xBD, g_drum_presets[data - 1]);
            }
        }
        else
        {
            if(g_play_samples && *g_pattern_data)
            {
                uint8_t frequency = *g_pattern_data++;
                uint8_t volume = *g_pattern_data++;
                uint8_t sample_number = *g_pattern_data++;

                if(g_mus_header->version >= 0x02) sample_number &= 0x0F;

                play_sample(frequency, volume, sample_number);
            }
            else
            {
                /* Skip empty sample data */
                g_pattern_data += 3;
            }
        }
    }
    else
    {
        handle_pitch_bend();
    }

    if(g_pattern_data >= g_song_end) return false;

    return true;
}


void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    /* Set mix control */
    g_roboplay_interface->opl4_write_wave(0xF8, 0x00);
    g_roboplay_interface->opl4_write_wave(0xF9, 0x00);

    for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
    {
        if(g_mus_header->device_id == DEVICE_AUDIO)
        {
            for(uint8_t j = 0; j < INSTRUMENT_DATA_SIZE - 1; j++)
            {
                g_roboplay_interface->opl4_write_fm_1(g_instrument_registers[i][j], g_mus_header->voice_data_audio[i][j]);
                g_roboplay_interface->opl4_write_fm_2(g_instrument_registers[i][j], g_mus_header->voice_data_audio[i][j]);
            }
            g_roboplay_interface->opl4_write_fm_1(g_instrument_registers[i][INSTRUMENT_DATA_SIZE - 1], g_mus_header->voice_data_audio[i][INSTRUMENT_DATA_SIZE - 1] | PAN_SETTING_LEFT);
            g_roboplay_interface->opl4_write_fm_2(g_instrument_registers[i][INSTRUMENT_DATA_SIZE - 1], g_mus_header->voice_data_audio[i][INSTRUMENT_DATA_SIZE - 1] | PAN_SETTING_RIGHT);
        }
        else if(i < FIRST_DRUM_CHANNEL)           
        {
            uint8_t instrument = (g_mus_header->instrument_volume_music[i] & 0xF0) >> 4;

            if(instrument == 0x00)
            {
                /* Original instrument */
                opll_to_opl_fm_1(i, g_mus_header->original_data_music);
                opll_to_opl_fm_2(i, g_mus_header->original_data_music);
            }
            else
            {
                opll_to_opl_fm_1(i, g_music_voice_patches[instrument]);
                opll_to_opl_fm_2(i, g_music_voice_patches[instrument]);
            }
        }

        g_mus_status_table[i].note_active = false;
        g_mus_status_table[i].note_frequency.low  = 0;
        g_mus_status_table[i].note_frequency.high = 0;

        g_mus_status_table[i].pitch_bend_value_up   = 0;
        g_mus_status_table[i].pitch_bend_value_down = 0;
        g_mus_status_table[i].detune_value          = 0;
    }

    if(g_mus_header->device_id == DEVICE_MUSIC)
    {
        /* MSX-MUSIC always in percussion mode */
        for(uint8_t i = 0; i < 3; i++)
        {
            opll_to_opl_fm_1( i + FIRST_DRUM_CHANNEL, g_music_drum_patches[i]);
            g_roboplay_interface->opl4_write_fm_1(0xC0 + i + FIRST_DRUM_CHANNEL, PAN_SETTING_MID);
            g_roboplay_interface->opl4_write_fm_1(0xA0 + i + FIRST_DRUM_CHANNEL, 0x59);
            g_roboplay_interface->opl4_write_fm_1(0xB0 + i + FIRST_DRUM_CHANNEL, 0x0D);
        }
    }
    else
    {
      g_roboplay_interface->opl4_write_fm_1(0xBD, g_mus_header->sustain_audio);
    }

    if(g_play_samples)
    {
        /* Set sample values to default of MSX AUDIO */
        play_sample(49, 127, 0);
    }

    g_speed = g_mus_header->speed;
    g_speed_count = g_speed - 1;

    g_pattern_data = (uint8_t *)DATA_SEGMENT_BASE;
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    /* Fixed replay rate of 60Hz */
    return 60.0;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "FAC SoundTracker player V1.1 by RoboSoft Inc.";
}

char* get_title(void)
{
    return g_song_name;
}

char* get_author(void)
{
    return g_author;
}

char* get_description(void)
{
    switch(g_mus_header->version)
    {
        case 0x00:
        case 0x01:
            if(g_mus_header->device_id == DEVICE_MUSIC)
                return "FAC SoundTracker v1.0  6CH MSX-MUSIC + PERCUSSION";
            else
                return "FAC SoundTracker v1.0  9CH MSX-AUDIO";
        case 0x02:
            if(g_mus_header->device_id == DEVICE_MUSIC)
                return "FAC SoundTracker v2.0  6CH MSX-MUSIC + PERCUSSION";
            else
                return "FAC SoundTracker v2.0  9CH MSX-AUDIO";
        case 0x03:
            if(g_mus_header->device_id == DEVICE_MUSIC)
                return "FAC SoundTracker PRO  6CH MSX-MUSIC + PERCUSSION";
            else
                return "FAC SoundTracker PRO  9CH MSX-AUDIO";
        default:
            return "Unknown tracker version";
    }
}

void write_sample_headers(void)
{
    uint16_t header_address = 0;

    for(int i = 0; i < NR_OF_SAMPLE_BLOCKS; i++)
    {
        uint32_t start = OPL_WAVE_ADDRESS + g_sample_blocks[i].start_address;
        uint16_t loop = g_sample_blocks[i].end_address - 1;
        uint16_t end = (g_sample_blocks[i].end_address - 1) ^ 0xFFFF;

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

bool load_sample_kit(const char* file_name, const char* extension)
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

    while(g_mus_header->sample_kit_name[c] != ' ' && c < 8)
        sample_kit_name[l + c] = g_mus_header->sample_kit_name[c++];
    sample_kit_name[l + c] = '\0';

    strcat(sample_kit_name, extension);

    if(!g_roboplay_interface->exists(sample_kit_name))
    {
        /* Try to load the drum kit based on the file name */
        strcpy(sample_kit_name, file_name);
        sample_kit_name[strlen(sample_kit_name) - 4] = '\0';
        strcat(sample_kit_name, extension);
    }

    if(g_roboplay_interface->exists(sample_kit_name))
    {
        result = true;

        g_roboplay_interface->set_segment(g_unpack_segment);

        g_roboplay_interface->open(sample_kit_name, false);
        g_roboplay_interface->read((void *)READ_BUFFER, 7);

        for(int i = 0; i < 4; i++)
        {
            g_roboplay_interface->read((uint8_t *)READ_BUFFER, READ_BUFFER_SIZE);
            adpcm2pcm();
            g_roboplay_interface->opl4_write_wave_data((uint8_t*)DATA_SEGMENT_BASE, 2 * READ_BUFFER_SIZE);
        }
        g_roboplay_interface->close();

        g_roboplay_interface->set_segment(START_SEGMENT_INDEX);
    }

    return result;
}

void play_sample(uint8_t frequency, uint8_t volume, uint8_t sample_number)
{
    volume = 128 - (volume >> 1);
    g_roboplay_interface->opl4_write_wave(0x50, (volume << 1) | 0x01);

    frequency--;
    g_roboplay_interface->opl4_write_wave(0x38, g_pitch_table[frequency][0]);
    g_roboplay_interface->opl4_write_wave(0x20, g_pitch_table[frequency][1] | 0x01);

    g_roboplay_interface->opl4_write_wave(0x68, 0x00);

    g_roboplay_interface->opl4_write_wave(0x08, 0x7F + sample_number);
    g_roboplay_interface->opl4_wait_for_load();

    g_roboplay_interface->opl4_write_wave(0x68, 0x80);
}

void opll_instrument_to_opl_preset(uint8_t *opl_instrument_preset, const uint8_t *instrument_data, uint8_t volume, uint8_t pan_preset)
{
    opl_instrument_preset[0]  = instrument_data[0];                                     /* 0x20 + x */
    opl_instrument_preset[1]  = instrument_data[1];                                     /* 0x23 + x */
    opl_instrument_preset[2]  = instrument_data[2];                                     /* 0x40 + x */
    opl_instrument_preset[3]  = (instrument_data[3] & 0xC0) | volume;                   /* 0x43 + x */
    opl_instrument_preset[4]  = instrument_data[4];                                     /* 0x60 + x */
    opl_instrument_preset[5]  = instrument_data[5];                                     /* 0x63 + x */
    opl_instrument_preset[6]  = instrument_data[6];                                     /* 0x80 + x */
    opl_instrument_preset[7]  = instrument_data[7];                                     /* 0x83 + x */
    opl_instrument_preset[8]  = ((instrument_data[3] & 0x07) << 1) | pan_preset;        /* 0xC0 + x */
    opl_instrument_preset[9]  = (instrument_data[3] & 0x10) >> 4;                       /* 0xE0 + x */
    opl_instrument_preset[10] = (instrument_data[3] & 0x08) >> 3;                       /* 0xE3 + x */
}

void opll_to_opl_fm_1(const uint8_t channel, const uint8_t *instrument_data)
{
    uint8_t opl_instrument_preset[PRESET_DATA_SIZE];

    uint8_t volume = (g_mus_header->instrument_volume_music[channel] & 0x0F) * 4;
    opll_instrument_to_opl_preset(opl_instrument_preset, instrument_data, volume, PAN_SETTING_LEFT);

    g_mus_volume_table[channel] = opl_instrument_preset[3];

    for(uint8_t i = 0; i < PRESET_DATA_SIZE; i++)
        g_roboplay_interface->opl4_write_fm_1(g_preset_registers[channel][i], opl_instrument_preset[i]);
}

void opll_to_opl_fm_2(const uint8_t channel, const uint8_t *instrument_data)
{
    uint8_t opl_instrument_preset[PRESET_DATA_SIZE];

    uint8_t volume = (g_mus_header->instrument_volume_music[channel] & 0x0F) * 4;
    opll_instrument_to_opl_preset(opl_instrument_preset, instrument_data, volume, PAN_SETTING_RIGHT);

    for(uint8_t i = 0; i < PRESET_DATA_SIZE; i++)
        g_roboplay_interface->opl4_write_fm_2(g_preset_registers[channel][i], opl_instrument_preset[i]);
}

void frequency_up(uint8_t channel, uint8_t value)
{
    if(!(g_mus_status_table[channel].note_frequency.high & 0x01))
    {
        g_mus_status_table[channel].note_frequency.low += value;
        if(g_mus_status_table[channel].note_frequency.low > 0xB2)
        {
            g_mus_status_table[channel].note_frequency.low -= 0xB2;
            g_mus_status_table[channel].note_frequency.low += 0x59;
            g_mus_status_table[channel].note_frequency.high += 0x03;
        }
    }
    else
    {
        if(g_mus_status_table[channel].note_frequency.low + value > 255)
            g_mus_status_table[channel].note_frequency.high += 0x01;

        g_mus_status_table[channel].note_frequency.low += value;
    }

    g_roboplay_interface->opl4_write_fm_1(0xA0 + channel, g_mus_status_table[channel].note_frequency.low);
    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);

    g_roboplay_interface->opl4_write_fm_2(0xA0 + channel, g_mus_status_table[channel].note_frequency.low + 1);
    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);
}

void frequency_down(uint8_t channel, uint8_t value)
{
    if(g_mus_status_table[channel].note_frequency.high & 0x01)
    {
        g_mus_status_table[channel].note_frequency.low -= value;
        if(g_mus_status_table[channel].note_frequency.low < 0x46)
        {
            g_mus_status_table[channel].note_frequency.low -= 0x45;
            g_mus_status_table[channel].note_frequency.low += 0x8B;
            g_mus_status_table[channel].note_frequency.high -= 0x03;
        }
    }
    else
    {
        if(g_mus_status_table[channel].note_frequency.low < value)
            g_mus_status_table[channel].note_frequency.high -= 0x01;

        g_mus_status_table[channel].note_frequency.low -= value;
    }

    g_roboplay_interface->opl4_write_fm_1(0xA0 + channel, g_mus_status_table[channel].note_frequency.low);
    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);

    g_roboplay_interface->opl4_write_fm_2(0xA0 + channel, g_mus_status_table[channel].note_frequency.low - 1);
    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);
}

void handle_pitch_bend(void)
{
    for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
    {
        if(g_mus_status_table[i].pitch_bend_value_up)
            frequency_up(i, g_mus_status_table[i].pitch_bend_value_up);

        if(g_mus_status_table[i].pitch_bend_value_down)
            frequency_down(i, g_mus_status_table[i].pitch_bend_value_down);
    }
}

void note_on(uint8_t channel, uint8_t data)
{
    g_mus_status_table[channel].note_active = true;
    g_mus_status_table[channel].pitch_bend_value_up = 0;
    g_mus_status_table[channel].pitch_bend_value_down = 0;

    g_mus_status_table[channel].note_frequency.low  = g_frequency_table[data - 1].low + g_mus_status_table[channel].detune_value;
    g_mus_status_table[channel].note_frequency.high = g_frequency_table[data - 1].high;

    g_roboplay_interface->opl4_write_fm_1(0xA0 + channel, g_mus_status_table[channel].note_frequency.low);

    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, g_mus_status_table[channel].note_frequency.high);
    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);

    g_roboplay_interface->opl4_write_fm_2(0xA0 + channel, g_mus_status_table[channel].note_frequency.low - 1);

    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, g_mus_status_table[channel].note_frequency.high);
    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);
}

void note_off(uint8_t channel)
{
    g_mus_status_table[channel].note_active = false;

    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, 0x00);
    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, 0x00);
}

void detune_up(uint8_t channel)
{
    g_mus_status_table[channel].detune_value++;
    if(g_mus_status_table[channel].note_active)
    {
        frequency_up(channel, 1);
    }
}

void detune_down(uint8_t channel)
{
    g_mus_status_table[channel].detune_value--;
    if(g_mus_status_table[channel].note_active)
    {
        frequency_down(channel, 1);
    }
}

void pitch_bend_up(uint8_t channel, uint8_t data)
{
    if(g_mus_status_table[channel].note_active)
    {
        uint8_t value = data - PITCH_BEND_UP;
        g_mus_status_table[channel].pitch_bend_value_up   = value;
        g_mus_status_table[channel].pitch_bend_value_down = 0;

        frequency_up(channel, g_mus_status_table[channel].pitch_bend_value_up);
    }
}

void pitch_bend_down(uint8_t channel, uint8_t data)
{
    if(g_mus_status_table[channel].note_active)
    {
        uint8_t value = data - PITCH_BEND_DOWN;
        g_mus_status_table[channel].pitch_bend_value_up   = 0;
        g_mus_status_table[channel].pitch_bend_value_down = value;

        frequency_down(channel, g_mus_status_table[channel].pitch_bend_value_down);
    }
}

void change_tempo(uint8_t data)
{
    g_speed = MAX_TEMPO - data;
}


void change_volume(uint8_t channel, uint8_t data)
{
    data &= 0x3F;
    data ^= 0x3F;

    uint8_t  value;
    if(g_mus_header->device_id == DEVICE_AUDIO)
        value = (g_mus_header->voice_data_audio[channel][VOLUME_REGISTER_INDEX] & 0xC0) | data;
    else
        value = (g_mus_volume_table[channel] & 0xC0) | data;

    g_roboplay_interface->opl4_write_fm_1(g_instrument_registers[channel][VOLUME_REGISTER_INDEX], value);
    g_roboplay_interface->opl4_write_fm_2(g_instrument_registers[channel][VOLUME_REGISTER_INDEX], value);
}

void change_brightness(uint8_t channel, uint8_t data)
{
    if(g_mus_header->device_id == DEVICE_AUDIO)
    {
        data &= 0x3F;
        data ^= 0x3F;

        uint8_t  value = (g_mus_header->voice_data_audio[channel][BRIGHTNESS_REGISTER_INDEX] & 0xC0) | data;
        g_roboplay_interface->opl4_write_fm_1(g_instrument_registers[channel][BRIGHTNESS_REGISTER_INDEX], value);
        g_roboplay_interface->opl4_write_fm_2(g_instrument_registers[channel][BRIGHTNESS_REGISTER_INDEX], value);
    }
}
