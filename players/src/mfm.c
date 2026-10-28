/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mfm.c
 *
 * MFM: MoonBlaster FM
 */

#include <string.h>

#include "data/inc/waves.h"

#include "player_interface.h"
#include "mfm.h"

static uint8_t **g_waves_table;

static uint8_t   g_own_tone_info[MAX_OWN_TONES];
static OWN_PATCH g_own_patches[MAX_OWN_PATCHES];

static MFM_HEADER  *g_mfm_header;
static uint8_t     *g_position_table;
static uint8_t     *g_patterns[MAX_PATTERN + 1];

static uint8_t g_song_segments[MAX_SONG_SEGMENTS];
static uint8_t g_current_song_segment;
static uint8_t g_waves_segment;

static uint8_t g_speed;
static uint8_t g_speed_count;
static int8_t  g_transpose_value;
static uint8_t g_position;
static uint8_t g_step;

static uint8_t *g_pattern_data;

static uint8_t g_nr_of_2op_channels;

static uint8_t g_step_buffer[STEP_BUFFER_SIZE];

static char    g_song_name[SONG_NAME_LENGTH + 1];
static char    g_wave_kit_name[WAVE_KIT_NAME_LENGTH + 1];

static MFM_FM_STATUS_TABLE   g_2op_status_table[MAX_FM_CHANNELS];
static MFM_FM_STATUS_TABLE   g_4op_status_table[MAX_4OP_CHANNELS];
static MFM_WAVE_STATUS_TABLE g_wave_status_table[NR_WAVE_CHANNELS];

bool g_raw_mode;

bool load(const char *file_name)
{
    if(!load_song_data(file_name)) return false;;

    g_waves_segment = g_roboplay_interface->get_new_segment();

    if(!load_wave_kit(file_name)) return false;

    g_roboplay_interface->set_segment(g_waves_segment);
    g_waves_table = (uint8_t **)DATA_SEGMENT_BASE;

    g_roboplay_interface->open("WAVES.DAT", true);
    g_roboplay_interface->read(g_waves_table, DATA_SEGMENT_SIZE);
    g_roboplay_interface->close();

    g_roboplay_interface->set_segment(g_song_segments[0]);

    return true;
}

bool update(void)
{
    handle_frequency_mode();

    g_speed_count++;
    if(g_speed_count >= g_speed)
    {
        g_speed_count = 0;

        play_waves();

        /* Play next step line: FM 4OP */
        for(uint8_t i = 0; i < g_mfm_header->nr_of_4op_channels; i++)
        {
            if(g_step_buffer[i])
            {
                if(g_step_buffer[i] <= FM_NOTE_ON)
                    fm_note_on_event(i, g_port_data_4op, g_4op_status_table);
                else if(g_step_buffer[i] == FM_NOTE_OFF)
                    fm_note_off_event(i, g_port_data_4op, g_4op_status_table);
                else if(g_step_buffer[i] < FM_VOLUME_CHANGE)
                    fm_4op_instrument_change_event(i);
                else if(g_step_buffer[i] < FM_STEREO_SET)
                    fm_4op_volume_change_event(i, g_step_buffer[i]);
                else if(g_step_buffer[i] < FM_NOTE_LINK)
                    fm_4op_stereo_change_event(i, g_step_buffer[i]);
                else if(g_step_buffer[i] < FM_PITCH_BEND)
                    fm_note_link_event(i, g_port_data_4op, g_4op_status_table, g_step_buffer[i]);
                else if(g_step_buffer[i] < FM_BRIGHTNESS)
                    fm_pitch_bend_event(i, g_4op_status_table, g_step_buffer[i]);
                else if(g_step_buffer[i] < FM_DETUNE)
                    fm_4op_brightness_event(i, g_step_buffer[i]);
                else if(g_step_buffer[i] < FM_MODULATION)
                    fm_detune_event(i, g_4op_status_table, g_step_buffer[i]);
                else if(g_step_buffer[i] < FM_NONE)
                    fm_modulation_event(i, g_4op_status_table, g_step_buffer[i]);
            }
        }

        /* Play next step line: FM 2OP */
        for(uint8_t i = 0; i < g_nr_of_2op_channels; i++)
        {
            uint8_t index = i + g_mfm_header->nr_of_4op_channels;
            if(g_step_buffer[index])
            {
                if(g_step_buffer[index] <= FM_NOTE_ON)
                    fm_note_on_event(i, g_port_data_2op, g_2op_status_table);
                else if(g_step_buffer[index] == FM_NOTE_OFF)
                    fm_note_off_event(i, g_port_data_2op, g_2op_status_table);
                else if(g_step_buffer[index] < FM_VOLUME_CHANGE)
                    fm_2op_instrument_change_event(i);
                else if(g_step_buffer[index] < FM_STEREO_SET)
                    fm_2op_volume_change_event(i, g_step_buffer[index]);
                else if(g_step_buffer[index] < FM_NOTE_LINK)
                    fm_2op_stereo_change_event(i, g_step_buffer[index]);
                else if(g_step_buffer[index] < FM_PITCH_BEND)
                    fm_note_link_event(i, g_port_data_2op, g_2op_status_table, g_step_buffer[index]);
                else if(g_step_buffer[index] < FM_BRIGHTNESS)
                    fm_pitch_bend_event(i, g_2op_status_table, g_step_buffer[index]);
                else if(g_step_buffer[index] < FM_DETUNE)
                    fm_2op_brightness_event(i, g_step_buffer[index]);
                else if(g_step_buffer[index] < FM_MODULATION)
                    fm_detune_event(i, g_2op_status_table, g_step_buffer[index]);
                else if(g_step_buffer[index] < FM_NONE)
                    fm_modulation_event(i, g_2op_status_table, g_step_buffer[index]);
            }
        }

        /* Play next step line: WAVE */
        for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
        {
            if(g_step_buffer[i + MAX_FM_CHANNELS])
            {
                if(g_step_buffer[i + MAX_FM_CHANNELS] <= WAVE_NOTE_ON)
                    wave_note_on_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] == WAVE_NOTE_OFF)
                    wave_note_off_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] < WAVE_VOLUME_CHANGE)
                    wave_change_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] < WAVE_STEREO_SET)
                    wave_volume_change_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] < WAVE_NOTE_LINK)
                    wave_stereo_change_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] < WAVE_PITCH_BEND)
                    wave_note_link_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] < WAVE_DETUNE)
                    wave_pitch_bend_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] < WAVE_MODULATION)
                    wave_detune_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] < WAVE_DAMP)
                    wave_modulation_event(i);
                else if(g_step_buffer[i + MAX_FM_CHANNELS] < WAVE_NONE)
                    wave_damp_event(i);
            }
        }

        /* Handle command channel */
        if(g_step_buffer[COMMAND_CHANNEL])
        {
            if(g_step_buffer[COMMAND_CHANNEL] <= COMMAND_TEMPO)
                tempo_command();
            else if(g_step_buffer[COMMAND_CHANNEL] == COMMAND_PATTERN_END)
                pattern_end_command();
            else if(g_step_buffer[COMMAND_CHANNEL] < COMMAND_TRANSPOSE)
                status_byte_command();
            else if(g_step_buffer[COMMAND_CHANNEL] < COMMAND_NONE)
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
                if(!next_song_position()) return false;
            }

            g_roboplay_interface->set_segment(g_current_song_segment);

            if(g_raw_mode)
            {
                for(uint8_t i = 0; i < STEP_BUFFER_SIZE; i++)
                    g_step_buffer[i] = *g_pattern_data++;
            }
            else
            {
                uint8_t value = *g_pattern_data++;
                if(value == 0xFF)
                {
                    /* Empty line */
                    for(uint8_t n = 0; n < STEP_BUFFER_SIZE; n++)
                        g_step_buffer[n] = 0;
                }
                else
                {
                    /* Decrunch next step line */
                    uint8_t c = 0;
                    g_step_buffer[c++] = value;

                    uint8_t data[3];
                    data[0] = *g_pattern_data++;
                    data[1] = *g_pattern_data++;
                    data[2] = *g_pattern_data++;
                    for(uint8_t i = 0; i < 3; i++)
                    {
                        for(uint8_t j = 0; j < 8; j++)
                        {
                            if(data[i] & 0x80)
                                g_step_buffer[c++] = *g_pattern_data++;
                            else
                                g_step_buffer[c++] = 0x00;
                            data[i] <<= 1;
                        }
                    }
                }
            }

            for(uint8_t i = 0; i < g_mfm_header->nr_of_4op_channels; i++)
            {
                if(g_step_buffer[i] && g_step_buffer[i] <= FM_NOTE_ON)
                {
                    uint8_t note = g_step_buffer[i] + g_transpose_value;

                    g_4op_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
                    g_4op_status_table[i].last_note = note;

                    uint16_t frequency = g_frequency_table[note - 1];
                    frequency += g_4op_status_table[i].detune_value;

                    g_4op_status_table[i].next_frequency_low  = (frequency & 0xFF);
                    g_4op_status_table[i].next_frequency_high = frequency >> 8;
                }
            }

            for(uint8_t i = 0; i < g_nr_of_2op_channels; i++)
            {
                if(g_step_buffer[i + g_mfm_header->nr_of_4op_channels] && g_step_buffer[i + g_mfm_header->nr_of_4op_channels] <= FM_NOTE_ON)
                {
                    uint8_t note = g_step_buffer[i + g_mfm_header->nr_of_4op_channels] + g_transpose_value;

                    g_2op_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
                    g_2op_status_table[i].last_note = note;

                    uint16_t frequency = g_frequency_table[note - 1];
                    frequency += g_2op_status_table[i].detune_value;

                    g_2op_status_table[i].next_frequency_low  = (frequency & 0xFF);
                    g_2op_status_table[i].next_frequency_high = frequency >> 8;
               }
            }

            g_roboplay_interface->set_segment(g_waves_segment);
            for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
            {
                if(g_step_buffer[i + MAX_FM_CHANNELS] && g_step_buffer[i + MAX_FM_CHANNELS] <= WAVE_NOTE_ON)
                {
                    g_wave_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
                    calculate_wave(i);
                }
            }
            g_roboplay_interface->set_segment(g_song_segments[0]);
        }
    }  

    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    g_roboplay_interface->set_segment(g_song_segments[0]);

    if(g_mfm_header->nr_of_4op_channels)
        g_roboplay_interface->opl4_write_fm_2(0x04,  g_table_4op_on[g_mfm_header->nr_of_4op_channels - 1]);

    g_nr_of_2op_channels = MAX_FM_CHANNELS - (2 * g_mfm_header->nr_of_4op_channels);

    for(uint8_t i = 0; i < g_mfm_header->nr_of_4op_channels; i++)
    {    
        g_4op_status_table[i].last_note = 0;
        g_4op_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
        g_4op_status_table[i].modulation_index = 0;
        g_4op_status_table[i].modulation_count = 0;
        g_4op_status_table[i].detune_value = g_mfm_header->fm_detune[i];
        g_4op_status_table[i].next_frequency_low = 0;
        g_4op_status_table[i].next_frequency_high = 0;
        g_4op_status_table[i].last_frequency_low = 0;
        g_4op_status_table[i].last_frequency_high = 0;

        /* Initial instrument */
        update_instrument_4op(i, g_mfm_header->fm_start_voices[i]);

        /* Start pan setting */
        update_pan_setting_4op(i, g_mfm_header->fm_stereo[i]);
    }

    for(uint8_t i = 0; i < g_nr_of_2op_channels; i++)
    {
        g_2op_status_table[i].last_note = 0;
        g_2op_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
        g_2op_status_table[i].modulation_index = 0;
        g_2op_status_table[i].modulation_count = 0;
        g_2op_status_table[i].detune_value = g_mfm_header->fm_detune[i + g_mfm_header->nr_of_4op_channels];
        g_2op_status_table[i].next_frequency_low = 0;
        g_2op_status_table[i].next_frequency_high = 0;
        g_2op_status_table[i].last_frequency_low = 0;
        g_2op_status_table[i].last_frequency_high = 0;

        /* Initial instrument */
        update_instrument_2op(i, g_mfm_header->fm_start_voices[i + g_mfm_header->nr_of_4op_channels]);

        /* Start pan setting */
        update_pan_setting_2op(i, g_mfm_header->fm_stereo[i + g_mfm_header->nr_of_4op_channels]);
    }

    for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
    {
        g_wave_status_table[i].last_note = 0;
        g_wave_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
        g_wave_status_table[i].modulation_index = 0;
        g_wave_status_table[i].modulation_count = 0;
        g_wave_status_table[i].pitch_bend_speed = 0;
        g_wave_status_table[i].detune_value = g_mfm_header->wave_detune[i] << 1;
        g_wave_status_table[i].next_tone_low = 0;
        g_wave_status_table[i].next_tone_high = 0;
        g_wave_status_table[i].next_frequency = 0;
        g_wave_status_table[i].pseudo_reverb = REVERB_DISABLED;
        g_wave_status_table[i].frequency_table = 0;
        g_wave_status_table[i].header_bytes = 0;

        /* Initial wave number */
        g_wave_status_table[i].current_wave = g_mfm_header->wave_numbers[g_mfm_header->start_waves[i] - 1];

        /* Start volume */
        uint8_t volume = g_mfm_header->wave_volumes[g_mfm_header->start_waves[i] - 1];

        uint8_t level_direct = (volume & 0x01);
        g_wave_status_table[i].volume = (4 * volume) | level_direct;

        g_roboplay_interface->opl4_write_wave(0x50 + i, g_wave_status_table[i].volume);

        /* Start stereo setting */
        g_wave_status_table[i].current_stereo = g_mfm_header->stereo[i] & 0x0F;
        g_roboplay_interface->opl4_write_wave(0x68 + i, g_wave_status_table[i].current_stereo);
    }

    g_speed       = g_mfm_header->tempo;
    g_speed_count = g_speed - 3;

    g_transpose_value = 0;
    
    g_current_song_segment = 0;

    g_position = 255;
    g_step     = MAX_STEP;
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    if(g_mfm_header->hz_equalizer)
        return 50.0;
    else
        return 60.0;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "MoonBlaster FM player V1.0 by RoboSoft Inc.";
}

char* get_title(void)
{
    return g_song_name;
}

char* get_author(void)
{
    return "-";
}

char* get_description(void)
{
    return g_wave_kit_name;
}

bool load_song_data(const char *file_name)
{
    g_roboplay_interface->open(file_name, false);

    uint8_t signature[6];
    g_roboplay_interface->read(&signature, 6);

    bool edit_mode = false;
    g_raw_mode = false;
    if(strncmp(signature, "MBMS\x10\x01", 6))
    {
        if(strncmp(signature, "MBMS\x10\x02", 6))
        {
            if(strncmp(signature, "MBMS\x10\x00", 6))
            {
                g_roboplay_interface->close();
                return false;
            }
            else edit_mode = true;
        }
        else g_raw_mode = true;
    }

    uint8_t *destination = (uint8_t *)DATA_SEGMENT_BASE;

    if(edit_mode || g_raw_mode)
    {
        /* Read position table */
        g_position_table = destination + sizeof(MFM_HEADER);
        g_roboplay_interface->read(g_position_table, MAX_POSITION + 1);

        /* Read header */
        g_mfm_header = (MFM_HEADER *)destination;
        destination += g_roboplay_interface->read(g_mfm_header, sizeof(MFM_HEADER));

        destination += MAX_POSITION + 1;
    }
    else
    {
        /* Read header */
        g_mfm_header = (MFM_HEADER *)destination;
        destination += g_roboplay_interface->read(g_mfm_header, sizeof(MFM_HEADER));
    }

    strncpy(g_song_name, g_mfm_header->song_name, SONG_NAME_LENGTH);
    g_song_name[SONG_NAME_LENGTH] = '\0';

    strncpy(g_wave_kit_name, g_mfm_header->wave_kit_name, WAVE_KIT_NAME_LENGTH);
    g_wave_kit_name[WAVE_KIT_NAME_LENGTH] = '\0';

    if(!edit_mode && !g_raw_mode)
    {
        /* Read position table */
        g_position_table = destination;
        destination += g_roboplay_interface->read(g_position_table, g_mfm_header->song_length + 1);
    }

    /* Read pattern addresses */
    uint8_t max_position = 0;

    if(edit_mode)
    {
        max_position = g_roboplay_interface->read(g_patterns, sizeof(uint8_t *) * (MAX_PATTERN + 1));
        destination = DATA_SEGMENT_BASE + g_patterns[0];
    }
    else if(g_raw_mode)
    {

        for(uint8_t i = 0; i < MAX_PATTERN + 1; i++)
            g_patterns[i] = (uint8_t *)(DATA_SEGMENT_SIZE + (i * PATTERNSIZE));

    }
    else
    {
        for(uint8_t i =0; i < g_mfm_header->song_length + 1; i++)
            if(g_position_table[i] > max_position) max_position = g_position_table[i];

        destination += g_roboplay_interface->read(g_patterns, sizeof(uint8_t *) * (max_position + 1));
    }

    /* Read pattern data */
    MFM_PATTERN_HEADER pattern_header;

    uint8_t segment_index = 0;
    g_song_segments[segment_index] = START_SEGMENT_INDEX;

    if(g_raw_mode)
    {
        destination = (uint8_t *)DATA_SEGMENT_BASE; 
        segment_index++;

        pattern_header.nr_of_patterns  = 40;
        pattern_header.size = DATA_SEGMENT_SIZE;
    }
    else
    {
        g_roboplay_interface->read(&pattern_header, sizeof(MFM_PATTERN_HEADER));
    }

    while(pattern_header.nr_of_patterns && segment_index < MAX_SONG_SEGMENTS)
    {
        if(segment_index)
        {
            destination = (uint8_t *)DATA_SEGMENT_BASE; 
            g_song_segments[segment_index] = g_roboplay_interface->get_new_segment();
            g_roboplay_interface->set_segment(g_song_segments[segment_index]);
        }

        g_roboplay_interface->read(destination, pattern_header.size);

        if(!g_raw_mode)
            g_roboplay_interface->read(&pattern_header, sizeof(MFM_PATTERN_HEADER));
        
        segment_index++;
    }

    g_roboplay_interface->close();

    return true;
}

bool load_wave_kit(const char* file_name)
{
    bool result = false;

    char* wave_kit_name = (char *)READ_BUFFER;

    strcpy(wave_kit_name, file_name);
    uint8_t found = strlen(wave_kit_name) - 1;
    while(wave_kit_name[found] != '\\' && wave_kit_name[found] != ':' && found > 0) found--;
    if(found) 
        wave_kit_name[found + 1] = '\0';
    else
        wave_kit_name[found] = '\0';

    uint8_t l = strlen(wave_kit_name);
    uint8_t c = 0;

    while(g_mfm_header->wave_kit_name[c] != ' ' && c < 8)
        wave_kit_name[l + c] = g_mfm_header->wave_kit_name[c++];
    wave_kit_name[l + c] = '\0';

    strcat(wave_kit_name, ".MWK");

    if(!g_roboplay_interface->exists(wave_kit_name))
    {
        /* Try to load the wave kit based on the file name */
        strcpy(wave_kit_name, file_name);
        wave_kit_name[strlen(wave_kit_name) - 4] = '\0';
        strcat(wave_kit_name, ".MWK");
    }

    if(g_roboplay_interface->exists(wave_kit_name))
    {      
        result = true;

        g_roboplay_interface->open(wave_kit_name, false);

        uint8_t signature[6];
        g_roboplay_interface->read(&signature, 6);

        bool edit_mode = false;
        if(strncmp(signature, "MBMS\x10\x0D", 6))
        {
            if(strncmp(signature, "MBMS\x10\x0C", 6))
            {
                g_roboplay_interface->close();
                return false;
            }
            edit_mode = true;
        }

        uint32_t total_sample_size;
        uint8_t  nr_of_waves;
        g_roboplay_interface->read(&total_sample_size, 3);
        g_roboplay_interface->read(&nr_of_waves, 1);
        g_roboplay_interface->read(g_own_tone_info, sizeof(g_own_tone_info));

        g_roboplay_interface->read(g_own_patches, nr_of_waves * sizeof(OWN_PATCH));

        uint8_t name_buffer[16];
        if(edit_mode)
        {
            for(uint8_t i = 0; i < nr_of_waves; i++)
                g_roboplay_interface->read(name_buffer, sizeof(name_buffer));
        }

        /* Set OPL4 to memory access mode */
        g_roboplay_interface->opl4_write_wave(0x02, 0x11);

        uint32_t header_address = 0x200000;
        uint32_t sample_address = 0x200300;
        for(uint8_t i = 0; i < MAX_OWN_TONES; i++)
        {
            uint8_t sample_header[13];
            if(g_own_tone_info[i] & 0x01)
            {
                if(edit_mode)
                    g_roboplay_interface->read(name_buffer, sizeof(name_buffer));           

                g_roboplay_interface->read(sample_header, 11 + 2);

                /* Write header to OPL memory */
                g_roboplay_interface->opl4_write_wave(3, header_address >> 16);
                g_roboplay_interface->opl4_write_wave(4, (header_address >> 8) & 0xFF);
                g_roboplay_interface->opl4_write_wave(5, header_address & 0xFF);

                if(g_own_tone_info[i] & 0x20)
                {
                    /* Start address */
                    g_roboplay_interface->opl4_write_wave(6, sample_header[12] | (g_own_tone_info[i] & 0xC0));
                    g_roboplay_interface->opl4_write_wave(6, (sample_address >> 8) & 0xFF);
                    g_roboplay_interface->opl4_write_wave(6, sample_address & 0xFF);
                }
                else
                {
                    /* Start address */
                    g_roboplay_interface->opl4_write_wave(6, ((sample_address >> 16) & 0x3F) | (g_own_tone_info[i] & 0xC0));
                    g_roboplay_interface->opl4_write_wave(6, (sample_address >> 8) & 0xFF);
                    g_roboplay_interface->opl4_write_wave(6, sample_address & 0xFF);
                }

                /* Loop address */
                g_roboplay_interface->opl4_write_wave(6, sample_header[2]);
                g_roboplay_interface->opl4_write_wave(6, sample_header[3]);

                /* End address */
                g_roboplay_interface->opl4_write_wave(6, sample_header[4]);
                g_roboplay_interface->opl4_write_wave(6, sample_header[5]);


                g_roboplay_interface->opl4_write_wave(6, sample_header[6]);       /* LFO, VIB                */
                g_roboplay_interface->opl4_write_wave(6, sample_header[7]);       /* AR, D1R                 */
                g_roboplay_interface->opl4_write_wave(6, sample_header[8]);       /* DL, D2R                 */
                g_roboplay_interface->opl4_write_wave(6, sample_header[9]);       /* Rate correction , RR    */
                g_roboplay_interface->opl4_write_wave(6, sample_header[10]);      /* AM                      */

                if(!(g_own_tone_info[i] & 0x20))
                {
                    /* Set wave memory adress */
                    g_roboplay_interface->opl4_write_wave(3, (sample_address >> 16) & 0x3F);
                    g_roboplay_interface->opl4_write_wave(4, (sample_address >> 8)  & 0xFF);
                    g_roboplay_interface->opl4_write_wave(5, sample_address & 0xFF);

                    uint16_t bytes_left = sample_header[11] + 256 * sample_header[12];
                    while(bytes_left)
                    {
                        uint16_t read_size = (bytes_left > READ_BUFFER_SIZE) ? READ_BUFFER_SIZE : bytes_left;
                        uint16_t bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, read_size);

                        g_roboplay_interface->opl4_write_wave_data((uint8_t*)READ_BUFFER, bytes_read);

                        sample_address += bytes_read;
                        bytes_left -= bytes_read;
                    }
                }
            }
            header_address += 12;
        }

        /* Set OPL4 to sound generation mode */
        g_roboplay_interface->opl4_write_wave(0x02, 0x10);

        g_roboplay_interface->close();
    }

    return true;
}

void write_opl(const bool second_opl_port, const uint8_t reg, const uint8_t value)
{
    if(second_opl_port)
        g_roboplay_interface->opl4_write_fm_2(reg, value);
    else
        g_roboplay_interface->opl4_write_fm_1(reg, value);
}

void update_instrument_4op(const uint8_t channel, const uint8_t instrument)
{
    g_4op_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    g_4op_status_table[channel].last_instrument = instrument;

    uint8_t reg;
    uint8_t value;
    for(uint8_t i = 0; i < INSTRUMENT_SIZE_4OP - 2; i++)
    {
        reg   = g_4op_registers[channel][i];
        value = g_mfm_header->fm_voice_data_4op[instrument - 1][i];

        write_opl(g_port_data_4op[channel].second_opl_port, reg , value);
    }

    reg   = g_4op_registers[channel][INSTRUMENT_SIZE_4OP - 2];
    value = g_mfm_header->fm_voice_data_4op[instrument - 1][INSTRUMENT_SIZE_4OP - 2];

    uint8_t pan_setting = read_opl(g_port_data_4op[channel].second_opl_port, reg) & 0x30;
    if(pan_setting ^ 0x30) pan_setting ^= 0x30;

    write_opl(g_port_data_4op[channel].second_opl_port, reg , value | pan_setting);

    reg   = g_4op_registers[channel][INSTRUMENT_SIZE_4OP - 1];
    value = g_mfm_header->fm_voice_data_4op[instrument - 1][INSTRUMENT_SIZE_4OP - 1];

    write_opl(g_port_data_4op[channel].second_opl_port, reg , value | pan_setting);
}

void update_instrument_2op(const uint8_t channel, const uint8_t instrument)
{
    g_2op_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    g_2op_status_table[channel].last_instrument = instrument;

    uint8_t reg = g_2op_registers[channel][0];
    uint8_t value;
    for(uint8_t i = 0; i < INSTRUMENT_SIZE_2OP - 3; i++)
    {
        value = g_mfm_header->fm_voice_data_2op[instrument - 1][i++];

        write_opl(g_port_data_2op[channel].second_opl_port, reg, value);

        reg += 0x03;
        value = g_mfm_header->fm_voice_data_2op[instrument - 1][i];

        write_opl(g_port_data_2op[channel].second_opl_port, reg, value);

        reg += 0x1D;
    }

    reg = g_2op_registers[channel][1];
    value = g_mfm_header->fm_voice_data_2op[instrument - 1][INSTRUMENT_SIZE_2OP - 3];

    write_opl(g_port_data_2op[channel].second_opl_port, reg, value);

    reg = g_2op_registers[channel][2];
    value = g_mfm_header->fm_voice_data_2op[instrument - 1][INSTRUMENT_SIZE_2OP - 2];

    write_opl(g_port_data_2op[channel].second_opl_port, reg, value);

    uint8_t pan_setting = read_opl(g_port_data_2op[channel].second_opl_port, reg) & 0x30;
    if(pan_setting ^ 0x30) pan_setting ^= 0x30;

    reg   = g_2op_registers[channel][3];
    value = g_mfm_header->fm_voice_data_2op[instrument - 1][INSTRUMENT_SIZE_2OP - 1];

    write_opl(g_port_data_2op[channel].second_opl_port, reg, value | pan_setting);
}

uint8_t read_opl(const bool second_opl_port, const uint8_t reg)
{
    if(second_opl_port)
        return g_roboplay_interface->opl4_read_fm_2(reg);
    else
        return g_roboplay_interface->opl4_read_fm_1(reg);
}

void update_pan_setting_4op(const uint8_t channel, const uint8_t pan_setting)
{
    uint8_t reg = g_port_data_4op[channel].frequency_register + 0x20;

    uint8_t value = read_opl(g_port_data_4op[channel].second_opl_port, reg) & 0x0F;
    write_opl(g_port_data_4op[channel].second_opl_port, reg, value | (pan_setting << 4));

    reg += 0x03;

    value = read_opl(g_port_data_4op[channel].second_opl_port, reg) & 0x0F;
    write_opl(g_port_data_4op[channel].second_opl_port, reg, value | (pan_setting << 4));
}

void update_pan_setting_2op(const uint8_t channel, const uint8_t pan_setting)
{
    uint8_t reg = g_port_data_2op[channel].frequency_register + 0x20;

    uint8_t value = read_opl(g_port_data_2op[channel].second_opl_port, reg) & 0x0F;
    write_opl(g_port_data_2op[channel].second_opl_port, reg, value | (pan_setting << 4));
}

bool next_song_position(void)
{
    g_position++;

    if(g_position > g_mfm_header->song_length)
    {
        if(g_mfm_header->loop_position > MAX_POSITION)
            return false;
        else
            g_position = g_mfm_header->loop_position;
    }

    uint16_t pattern_data = (uint16_t)g_patterns[g_position_table[g_position]];
    g_current_song_segment = pattern_data >> 14;
    pattern_data = pattern_data & 0x3FFF;

    g_pattern_data = (uint8_t *)(pattern_data + DATA_SEGMENT_BASE);

    return true;
}

void calculate_wave(const uint8_t channel)
{
    uint16_t frequency;
    uint8_t note = g_step_buffer[channel + MAX_FM_CHANNELS] - 1;

    if((g_wave_status_table[channel].current_wave == 175) && (note > 35))
    {
        /* GM Drum patch */
        if(note > 89) note = 89;
        note -= 36;

        GM_DRUM_PATCH *patch = (GM_DRUM_PATCH *)(g_waves_table[g_wave_status_table[channel].current_wave] + sizeof(PATCH));
        g_wave_status_table[channel].next_tone_low  = patch[note].tone;
        g_wave_status_table[channel].next_tone_high = 0;
        g_wave_status_table[channel].header_bytes = patch[note].header_bytes;
        frequency = patch[note].frequency;
    }
    else if(g_wave_status_table[channel].current_wave > 175)
    {
        /* Own wave */
        OWN_PATCH      *patch = &g_own_patches[g_wave_status_table[channel].current_wave - 176];
        OWN_PATCH_PART *patch_part = patch->patch_part;
        if(patch->transpose) note += g_transpose_value;
        g_wave_status_table[channel].header_bytes = 0;

        uint8_t min_note = 0;
        while(note >= patch_part->next_patch_note)
        {
            min_note = patch_part->next_patch_note;
            patch_part++;
        }

        g_wave_status_table[channel].next_tone_low  = patch_part->tone + 128;
        g_wave_status_table[channel].next_tone_high = 1;

        note = patch_part->tone_note + note - min_note;
        g_wave_status_table[channel].last_note = note;

        uint8_t type = (g_own_tone_info[patch_part->tone] & 0x06) >> 1;

        if(type == 0x00)
            g_wave_status_table[channel].frequency_table = g_waves_table[FRQ_TAB_AMIGA];
        else if(type == 0x01)
            g_wave_status_table[channel].frequency_table = g_waves_table[FRQ_TAB_PC];
        else
            g_wave_status_table[channel].frequency_table = g_waves_table[FRQ_TAB_TURBO];

        frequency = g_wave_status_table[channel].frequency_table[note];
    }
    else
    {
        /* Regular wave */
        PATCH      *patch = (PATCH *)g_waves_table[g_wave_status_table[channel].current_wave];
        PATCH_PART *patch_part = &patch->patch_part;

        if(patch->transpose) note += g_transpose_value;
        g_wave_status_table[channel].header_bytes = patch->header_bytes;

        uint8_t min_note = 0;       
        while(note >= patch_part->next_patch_note)
        {
            min_note = patch_part->next_patch_note;
            patch_part++;
        }

        g_wave_status_table[channel].next_tone_low  = patch_part->tone_low;
        g_wave_status_table[channel].next_tone_high = patch_part->tone_high & 0x01;

        note = note + (patch_part->tone_high >> 1) - min_note;
        g_wave_status_table[channel].last_note = note;
        g_wave_status_table[channel].frequency_table = patch_part->frequency_table;

        uint16_t octave = g_tabdiv12[note][0] << 8;
        uint8_t  index  = g_tabdiv12[note][1] >> 1;

        frequency = patch_part->frequency_table[index];
        frequency = (frequency << 1) + octave;
    }

    int detune_value = g_wave_status_table[channel].detune_value;
    if(detune_value >= 0) detune_value += 0x0800;
    frequency += detune_value;
    frequency &= 0xF7FF;

    g_wave_status_table[channel].next_frequency = frequency;
}

void play_waves(void)
{
    for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
    {
        if(g_step_buffer[i + MAX_FM_CHANNELS] && g_step_buffer[i + MAX_FM_CHANNELS] <= WAVE_NOTE_ON)
        {
            g_wave_status_table[i].pitch_frequency = g_wave_status_table[i].next_frequency;

            g_roboplay_interface->opl4_write_wave(0x68 + i, 0x00);   /* Off */
            g_roboplay_interface->opl4_write_wave(0x50 + i, 0xFF);   /* Volume to 0 */

            g_roboplay_interface->opl4_write_wave(0x20 + i, (g_wave_status_table[i].next_frequency & 0xFF) | g_wave_status_table[i].next_tone_high);
            g_roboplay_interface->opl4_write_wave(0x08 + i, g_wave_status_table[i].next_tone_low);
            g_roboplay_interface->opl4_write_wave(0x38 + i, (g_wave_status_table[i].next_frequency >> 8) | g_wave_status_table[i].pseudo_reverb);
            g_roboplay_interface->opl4_wait_for_load();
        }
    }
}

void tempo_command(void)
{
    g_speed = (COMMAND_TEMPO + 2) - g_step_buffer[COMMAND_CHANNEL];
}

void pattern_end_command(void)
{
    g_step = MAX_STEP;
}

void status_byte_command(void)
{

}

void transpose_command(void)
{
    g_transpose_value = g_step_buffer[COMMAND_CHANNEL] - (COMMAND_TRANSPOSE + 24);
}

void handle_frequency_mode(void)
{
    for(uint8_t i = 0; i < g_mfm_header->nr_of_4op_channels; i++)
    {
        if(g_4op_status_table[i].frequency_mode == FREQUENCY_MODE_PITCH_BEND)
            fm_handle_pitch_bend(i, g_port_data_4op, g_4op_status_table);
        else if(g_4op_status_table[i].frequency_mode == FREQUENCY_MODE_MODULATION)
            fm_handle_modulation(i, g_port_data_4op, g_4op_status_table);
    }

    for(uint8_t i = 0; i < g_nr_of_2op_channels; i++)
    {
        if(g_2op_status_table[i].frequency_mode == FREQUENCY_MODE_PITCH_BEND)
            fm_handle_pitch_bend(i, g_port_data_2op, g_2op_status_table);
        else if(g_2op_status_table[i].frequency_mode == FREQUENCY_MODE_MODULATION)
            fm_handle_modulation(i, g_port_data_2op, g_2op_status_table);
    }

    for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
    {      
        if(g_wave_status_table[i].frequency_mode == FREQUENCY_MODE_PITCH_BEND)
            wave_handle_pitch_bend(i);
        else if(g_wave_status_table[i].frequency_mode >= FREQUENCY_MODE_MODULATION)
            wave_handle_modulation(i);
    }
}

void fm_handle_pitch_bend(const uint8_t channel, MFM_FM_PORT_DATA *port_data, MFM_FM_STATUS_TABLE *status_table)
{
    uint16_t frequency = (status_table[channel].last_frequency_high << 8) + 
                          status_table[channel].last_frequency_low;

    frequency += status_table[channel].pitch_bend_speed;

    status_table[channel].last_frequency_low = frequency & 0xFF;
    status_table[channel].last_frequency_high = frequency >> 8;

    uint8_t reg  = port_data[channel].frequency_register;
    write_opl(port_data[channel].second_opl_port, reg, status_table[channel].last_frequency_low);

    reg += 0x10;
    write_opl(port_data[channel].second_opl_port, reg, status_table[channel].last_frequency_high);
}

void fm_handle_modulation(const uint8_t channel, MFM_FM_PORT_DATA *port_data, MFM_FM_STATUS_TABLE *status_table)
{
    uint8_t count = status_table[channel].modulation_count;
    int8_t  value = g_mfm_header->modulation[status_table[channel].modulation_index][count];

    uint16_t frequency = (status_table[channel].last_frequency_high << 8) + 
                          status_table[channel].last_frequency_low;

    frequency += value;

    status_table[channel].last_frequency_low = frequency & 0xFF;
    status_table[channel].last_frequency_high = frequency >> 8;

    uint8_t reg  = port_data[channel].frequency_register;
    write_opl(port_data[channel].second_opl_port, reg, status_table[channel].last_frequency_low);

    reg += 0x10;
    write_opl(port_data[channel].second_opl_port, reg, status_table[channel].last_frequency_high);

    count = (count + 1) % MODULATION_WAVE_LENGTH;
    if(g_mfm_header->modulation[status_table[channel].modulation_index][count] == 10) count = 0;
    status_table[channel].modulation_count = count;
}

void wave_handle_pitch_bend(const uint8_t channel)
{
    uint16_t frequency = g_wave_status_table[channel].pitch_frequency + g_wave_status_table[channel].pitch_bend_speed;

    if(frequency & 0x0800)
    {
        if(g_wave_status_table[channel].pitch_bend_speed < 0)
            frequency = frequency & 0xF7FF;
        else
            frequency += 0x0800;
    }

    g_wave_status_table[channel].pitch_frequency = frequency;

    g_roboplay_interface->opl4_write_wave(0x20 + channel, (frequency & 0xFF) | g_wave_status_table[channel].next_tone_high);
    g_roboplay_interface->opl4_write_wave(0x38 + channel, (frequency >> 8) | g_wave_status_table[channel].pseudo_reverb);
}

void wave_handle_modulation(const uint8_t channel)
{
    uint16_t frequency = g_wave_status_table[channel].pitch_frequency;

    uint8_t count = g_wave_status_table[channel].modulation_count;
    int8_t  value = g_mfm_header->modulation[g_wave_status_table[channel].modulation_index][count];
    frequency = frequency + (4 * value);

    if(frequency & 0x0800)
    {
        if(value < 0)
            frequency = frequency & 0xF7FF;
        else
            frequency += 0x0800;
    }

    g_wave_status_table[channel].pitch_frequency = frequency;
    
    count = (count + 1) % MODULATION_WAVE_LENGTH;
    if(g_mfm_header->modulation[g_wave_status_table[channel].modulation_index][count] == 10) count = 0;
    g_wave_status_table[channel].modulation_count = count;

    g_roboplay_interface->opl4_write_wave(0x20 + channel, (frequency & 0xFF) | g_wave_status_table[channel].next_tone_high);
    g_roboplay_interface->opl4_write_wave(0x38 + channel, (frequency >> 8) | g_wave_status_table[channel].pseudo_reverb);   
}

void fm_note_on_event(const uint8_t channel, MFM_FM_PORT_DATA *port_data, MFM_FM_STATUS_TABLE *status_table)
{
    uint8_t reg = port_data[channel].frequency_register;
    uint8_t data = status_table[channel].next_frequency_low;

    status_table[channel].last_frequency_low = data;

    write_opl(port_data[channel].second_opl_port, reg, data);

    reg += 0x10;
    data = status_table[channel].next_frequency_high & 0xDF;
    write_opl(port_data[channel].second_opl_port, reg, data);

    data = status_table[channel].next_frequency_high | 0x20;
    write_opl(port_data[channel].second_opl_port, reg, data);

    status_table[channel].last_frequency_high = data;       
}

void fm_note_off_event(const uint8_t channel, MFM_FM_PORT_DATA *port_data, MFM_FM_STATUS_TABLE *status_table)
{
    uint8_t reg = port_data[channel].frequency_register;
    uint8_t data = status_table[channel].next_frequency_low;

    status_table[channel].last_frequency_low = data;

    write_opl(port_data[channel].second_opl_port, reg, data);

    reg += 0x10;
    data = status_table[channel].next_frequency_high & 0xDF;

    write_opl(port_data[channel].second_opl_port, reg, data);

    status_table[channel].last_frequency_high = data;       
}

void fm_4op_instrument_change_event(const uint8_t channel)
{
    uint8_t instrument = g_step_buffer[channel] - FM_INSTRUMENT_CHANGE + 1;
    update_instrument_4op(channel, instrument);
}

void fm_2op_instrument_change_event(const uint8_t channel)
{
    uint8_t instrument = g_step_buffer[channel + g_mfm_header->nr_of_4op_channels] - FM_INSTRUMENT_CHANGE + 1;
    update_instrument_2op(channel, instrument);
}

void fm_4op_volume_change_event(const uint8_t channel, uint8_t volume_value)
{
    /* Not implemented */
    channel;
    volume_value;
}

void fm_2op_volume_change_event(const uint8_t channel, uint8_t volume_value)
{
    int8_t new_volume = volume_value - 122;

    uint8_t reg = g_port_data_2op[channel].volume_register;
    uint8_t value = read_opl(g_port_data_2op[channel].second_opl_port, reg) & 0xC0;
    value += new_volume;

    write_opl(g_port_data_2op[channel].second_opl_port, reg, value);
}

void fm_4op_stereo_change_event(const uint8_t channel, const uint8_t stereo_value)
{
    g_4op_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    uint8_t pan_setting = stereo_value - FM_STEREO_SET;

    update_pan_setting_4op(channel, pan_setting);
}

void fm_2op_stereo_change_event(const uint8_t channel, const uint8_t stereo_value)
{
    g_2op_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    uint8_t pan_setting = stereo_value - FM_STEREO_SET;

    update_pan_setting_2op(channel, pan_setting);
}

void fm_note_link_event(const uint8_t channel, MFM_FM_PORT_DATA *port_data, MFM_FM_STATUS_TABLE* status_table, const uint8_t link_value)
{
    int8_t link = link_value - 198;
    status_table[channel].last_note += link;
    status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    uint16_t frequency = g_frequency_table[status_table[channel].last_note - 1];
    frequency += status_table[channel].detune_value;

    status_table[channel].next_frequency_low  = (frequency & 0xFF);
    status_table[channel].next_frequency_high = frequency >> 8;

    uint8_t reg = port_data[channel].frequency_register;
    uint8_t data = status_table[channel].next_frequency_low;
    write_opl(port_data[channel].second_opl_port, reg, data);

    reg += 0x10;
    data = status_table[channel].next_frequency_high | 0x20;
    write_opl(port_data[channel].second_opl_port, reg, data);
}

void fm_pitch_bend_event(const uint8_t channel, MFM_FM_STATUS_TABLE* status_table, const uint8_t pitch_bend_value)
{
    status_table[channel].frequency_mode = FREQUENCY_MODE_PITCH_BEND;
    status_table[channel].pitch_bend_speed =  pitch_bend_value - 217;
}

void fm_4op_brightness_event(const uint8_t channel, const uint8_t brightness_value)
{
    /* Not implemented */
    channel;
    brightness_value;
}

void fm_2op_brightness_event(const uint8_t channel, const uint8_t brightness_value)
{
    int8_t new_brightness = brightness_value - 233;

    uint8_t reg = g_port_data_2op[channel].volume_register - 0x03;
    uint8_t value = read_opl(g_port_data_2op[channel].second_opl_port, reg);
    uint8_t ksl = value & 0xC0;
    
    value = (value & 0x3F) + new_brightness;
    
    write_opl(g_port_data_2op[channel].second_opl_port, reg, value | ksl);
}


void fm_detune_event(const uint8_t channel, MFM_FM_STATUS_TABLE* status_table, const uint8_t detune_value)
{
    status_table[channel].detune_value = detune_value - 243;
}

void fm_modulation_event(const uint8_t channel, MFM_FM_STATUS_TABLE* status_table, const uint8_t modulation_value)
{
    status_table[channel].pitch_bend_speed = 0;
    status_table[channel].frequency_mode = FREQUENCY_MODE_MODULATION;
    status_table[channel].modulation_index = modulation_value - FM_MODULATION;
    status_table[channel].modulation_count = 0;
}

void wave_note_on_event(const uint8_t channel)
{
    if(g_wave_status_table[channel].header_bytes)
    {
        g_roboplay_interface->set_segment(g_waves_segment);
        
        uint8_t index = 0;

        uint8_t reg = 0x80;
        uint8_t value = g_wave_status_table[channel].header_bytes[index++];
        do
        {
            g_roboplay_interface->opl4_write_wave(reg + channel, value);
            reg   = g_wave_status_table[channel].header_bytes[index++];
            value = g_wave_status_table[channel].header_bytes[index++];
        } while(reg != 0xFF);

        g_roboplay_interface->set_segment(g_song_segments[0]);       
    }

    g_roboplay_interface->opl4_write_wave(0x50 + channel, g_wave_status_table[channel].volume | 1);
    g_roboplay_interface->opl4_write_wave(0x68 + channel, 0x80 | g_wave_status_table[channel].current_stereo);
}

void wave_note_off_event(const uint8_t channel)
{
    g_wave_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    uint8_t value = g_roboplay_interface->opl4_read_wave_register(0x68 + channel);
    g_roboplay_interface->opl4_write_wave(0x68 + channel, value & 0x7F);

}

void wave_change_event(const uint8_t channel)
{   
    g_wave_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    uint8_t wave = g_step_buffer[channel + MAX_FM_CHANNELS] - WAVE_CHANGE;
    g_wave_status_table[channel].current_wave = g_mfm_header->wave_numbers[wave];

    uint8_t volume = g_mfm_header->wave_volumes[wave];
    uint8_t level_direct = (g_wave_status_table[channel].volume & 1);
    g_wave_status_table[channel].volume = (4 * volume) | level_direct;
}

void wave_volume_change_event(const uint8_t channel)
{
    uint8_t volume = g_step_buffer[channel + MAX_FM_CHANNELS] - WAVE_VOLUME_CHANGE;
    volume = (volume ^ 0x1F) << 1;

    uint8_t level_direct = (g_wave_status_table[channel].volume & 0x01);
    g_wave_status_table[channel].volume = (4 * volume) | level_direct;

    g_roboplay_interface->opl4_write_wave(0x50 + channel, g_wave_status_table[channel].volume);
}

void wave_stereo_change_event(const uint8_t channel)
{
    g_wave_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    g_wave_status_table[channel].current_stereo = (g_step_buffer[channel + MAX_FM_CHANNELS] - (WAVE_STEREO_SET + 7)) & 0x0F;

    uint8_t value = g_roboplay_interface->opl4_read_wave_register(0x68 + channel) & 0xF0;
    g_roboplay_interface->opl4_write_wave(0x68 + channel, value & g_wave_status_table[channel].current_stereo);
}

void wave_note_link_event(const uint8_t channel)
{
    g_wave_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    uint8_t note = g_wave_status_table[channel].last_note + g_step_buffer[channel + MAX_FM_CHANNELS] - (WAVE_NOTE_LINK + 9);
    g_wave_status_table[channel].last_note = note;

    uint16_t frequency;
    if((g_wave_status_table[channel].next_tone_high == 1) && (g_wave_status_table[channel].next_tone_low >= 128))
    {
        /* Link own wave */
        g_roboplay_interface->set_segment(g_waves_segment);
        frequency = g_wave_status_table[channel].frequency_table[note];
        g_roboplay_interface->set_segment(g_song_segments[0]);
    }
    else
    {
        uint16_t octave = g_tabdiv12[note][0] << 8;
        uint8_t  index  = g_tabdiv12[note][1] >> 1;

        g_roboplay_interface->set_segment(g_waves_segment);
        frequency = g_wave_status_table[channel].frequency_table[index];
        g_roboplay_interface->set_segment(g_song_segments[0]);

        frequency = (frequency << 1) + octave;
    }

    int detune_value = g_wave_status_table[channel].detune_value * 2;
    if(detune_value >= 0) detune_value += 0x0800;
    frequency += detune_value;
    frequency &= 0xF7FF;

    g_wave_status_table[channel].pitch_frequency = frequency;

    g_roboplay_interface->opl4_write_wave(0x20 + channel, frequency & 0xFF | g_wave_status_table[channel].next_tone_high);
    g_roboplay_interface->opl4_write_wave(0x38 + channel, frequency >> 8 | g_wave_status_table[channel].pseudo_reverb);
}

void wave_pitch_bend_event(const uint8_t channel)
{
    int8_t value = g_step_buffer[channel + MAX_FM_CHANNELS] - (WAVE_PITCH_BEND + 9);
    g_wave_status_table[channel].frequency_mode = FREQUENCY_MODE_PITCH_BEND;
    g_wave_status_table[channel].pitch_bend_speed = value * 4;
}

void wave_detune_event(const uint8_t channel)
{
    int value = g_step_buffer[channel + MAX_FM_CHANNELS] - (WAVE_DETUNE + 3);
    g_wave_status_table[channel].detune_value = value * 4;
}

void wave_modulation_event(const uint8_t channel)
{
    int8_t value = g_step_buffer[channel + MAX_FM_CHANNELS] - WAVE_MODULATION;
    g_wave_status_table[channel].pitch_bend_speed = 0;
    g_wave_status_table[channel].frequency_mode = FREQUENCY_MODE_MODULATION;
    g_wave_status_table[channel].modulation_index = value;
    g_wave_status_table[channel].modulation_count = 0;
}

void wave_damp_event(const uint8_t channel)
{
    g_wave_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    
    uint8_t value = g_roboplay_interface->opl4_read_wave_register(0x68 + channel);
    g_roboplay_interface->opl4_write_wave(0x68 + channel, value | 0x40);
}
