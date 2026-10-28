/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mwm.c
 *
 * MWM: MoonBlaster Wave
 */

#include <string.h>

#include "data/inc/waves.h"

#include "player_interface.h"
#include "mwm.h"

bool load(const char *file_name)
{
    if(!load_song_data(file_name)) return false;;
    if(!load_wave_kit(file_name)) return false;

    g_waves_segment = g_roboplay_interface->get_new_segment();
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

        /* Play next step line */
        for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
        {
            if(g_step_buffer[i])
            {
                if(g_step_buffer[i] <= NOTE_ON)
                    note_on_event(i);
                else if(g_step_buffer[i] == NOTE_OFF)
                    note_off_event(i);
                else if(g_step_buffer[i] < VOLUME_CHANGE)
                    wave_change_event(i);
                else if(g_step_buffer[i] < STEREO_SET)
                    volume_change_event(i);
                else if(g_step_buffer[i] < NOTE_LINK)
                    stereo_change_event(i);
                else if(g_step_buffer[i] < PITCH_BEND)
                    note_link_event(i);
                else if(g_step_buffer[i] < DETUNE)
                    pitch_bend_event(i);
                else if(g_step_buffer[i] < MODULATION)
                    detune_event(i);
                else if(g_step_buffer[i] < REVERB_ON)
                    modulation_event(i);
                else if(g_step_buffer[i] == REVERB_ON)
                    reverb_on_event(i);
                else if(g_step_buffer[i] < LFO)
                    damp_event(i);
                else if(g_step_buffer[i] < REVERB_OFF)
                    lfo_event(i);
                else if(g_step_buffer[i] == REVERB_OFF)
                    reverb_off_event(i);
                else if(g_step_buffer[i] < NO_EVENT)
                    extra_lfo_event(i);
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
            else if(g_step_buffer[COMMAND_CHANNEL] < COMMAND_FREQUENCY)
                transpose_command();
            else
                frequency_command();
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

            g_roboplay_interface->set_segment(g_current_song_segment);

            uint8_t value = *g_pattern_data;
            g_pattern_data++;
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

            g_roboplay_interface->set_segment(g_waves_segment);
            for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
            {
                if(g_step_buffer[i] && g_step_buffer[i] <= NOTE_ON)
                {
                    g_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
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

    for (uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
    {
        g_status_table[i].last_note = 0;
        g_status_table[i].frequency_mode = FREQUENCY_MODE_NORMAL;
        g_status_table[i].pitch_bend_speed = 0;
        g_status_table[i].modulation_index = 0;
        g_status_table[i].modulation_count = 0;
        g_status_table[i].detune_value = g_mwm_header->detune[i] << 1;
        g_status_table[i].next_tone_low = 0;
        g_status_table[i].next_tone_high = 0;
        g_status_table[i].next_frequency = 0;
        g_status_table[i].pseudo_reverb = REVERB_DISABLED;
        g_status_table[i].frequency_table = 0;
        g_status_table[i].pitch_frequency = 0;
        g_status_table[i].header_bytes = 0;

        /* Initial wave number */
        g_status_table[i].current_wave = g_mwm_header->wave_numbers[g_mwm_header->start_waves[i] - 1];

        /* Start volume */
        uint8_t volume = g_mwm_header->wave_volumes[g_mwm_header->start_waves[i] - 1];

        uint8_t level_direct = (volume & 0x01);
        g_status_table[i].volume = (4 * volume) | level_direct;

        g_roboplay_interface->opl4_write_wave(0x50 + i, g_status_table[i].volume);

        /* Start stereo setting */
        g_status_table[i].current_stereo = g_mwm_header->stereo[i] & 0x0F;
        g_roboplay_interface->opl4_write_wave(0x68 + i, g_status_table[i].current_stereo);
    }

    g_base_frequency = g_mwm_header->base_frequency;

    g_speed       = g_mwm_header->tempo;
    g_speed_count = g_speed - 3;

    g_transpose_value = 0;
    
    g_current_song_segment = 0;

    g_position = MAX_POSITION;
    g_step     = MAX_STEP;
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    if(g_base_frequency == 0)
        return 60.0;
    else if(g_base_frequency == 1)
        return 50.0;

    float s = g_base_frequency * 0.0000808;
    return 1.0/s + 0.5;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "MoonBlaster Wave player V1.0 by RoboSoft Inc.";
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
    if(strncmp(signature, "MBMS\x10\x08", 6))
    {
        if(strncmp(signature, "MBMS\x10\x07", 6))
        {
            g_roboplay_interface->close();
            return false;
        }
        edit_mode = true;
    }

    uint8_t *destination = (uint8_t *)DATA_SEGMENT_BASE;

    if(edit_mode)
    {
        /* Read position table */
        g_position_table = destination + sizeof(MWM_HEADER);
        g_roboplay_interface->read(g_position_table, MAX_POSITION + 1);

        /* Read header */
        g_mwm_header = (MWM_HEADER *)destination;
        destination += g_roboplay_interface->read(g_mwm_header, sizeof(MWM_HEADER));
        destination += g_mwm_header->song_length + 1;
    }
    else
    {
        /* Read header */
        g_mwm_header = (MWM_HEADER *)destination;
        destination += g_roboplay_interface->read(g_mwm_header, sizeof(MWM_HEADER));
    }

    strncpy(g_song_name, g_mwm_header->song_name, SONG_NAME_LENGTH);
    g_song_name[SONG_NAME_LENGTH] = '\0';

    strncpy(g_wave_kit_name, g_mwm_header->wave_kit_name, WAVE_KIT_NAME_LENGTH);
    g_wave_kit_name[WAVE_KIT_NAME_LENGTH] = '\0';

    if(!edit_mode)
    {
        /* Read position table */
        g_position_table = destination;
        destination += g_roboplay_interface->read(g_position_table, g_mwm_header->song_length + 1);
    }

    /* Read pattern addresses */
    uint8_t max_position = 0;
    for(uint8_t i =0; i < g_mwm_header->song_length + 1; i++)
        if(g_position_table[i] > max_position) max_position = g_position_table[i];

    g_patterns = destination;
    destination += g_roboplay_interface->read(g_patterns, sizeof(uint8_t *) * (max_position + 1));

    /* Read pattern data */
    MWM_PATTERN_HEADER pattern_header;

    uint8_t segment_index = 0;
    g_song_segments[segment_index] = START_SEGMENT_INDEX;

    g_roboplay_interface->read(&pattern_header, sizeof(MWM_PATTERN_HEADER));
    while(pattern_header.nr_of_patterns && segment_index < MAX_SONG_SEGMENTS)
    {
        if(segment_index)
        {
            destination = (uint8_t *)DATA_SEGMENT_BASE; 
            g_song_segments[segment_index] = g_roboplay_interface->get_new_segment();
            g_roboplay_interface->set_segment(g_song_segments[segment_index]);
        }

        g_roboplay_interface->read(destination, pattern_header.size);
        g_roboplay_interface->read(&pattern_header, sizeof(MWM_PATTERN_HEADER));
        segment_index++;
    }

    /* Read XLFO data */
    g_roboplay_interface->read(&signature, 4);

    if(!strncmp(signature, "XLFO", 4))
        g_roboplay_interface->read(g_xlfo, sizeof(g_xlfo));

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

    while(g_mwm_header->wave_kit_name[c] != ' ' && c < 8)
        wave_kit_name[l + c] = g_mwm_header->wave_kit_name[c++];
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

        uint16_t header_address = 0;
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
                g_roboplay_interface->opl4_write_wave(3, 0x20);
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

void next_song_position(void)
{
    if(g_position < g_mwm_header->song_length)
    {
        g_position++;
    }
    else
    {
        if(g_mwm_header->loop_position != MAX_POSITION && g_position != MAX_POSITION)
            g_position = g_mwm_header->loop_position;
        else
            g_position = 0;
    }

    uint16_t pattern_data = (uint16_t)g_patterns[g_position_table[g_position]];
    g_current_song_segment = pattern_data >> 14;
    pattern_data = pattern_data & 0x3FFF;

    g_pattern_data = (uint8_t *)(pattern_data + DATA_SEGMENT_BASE);
}

void calculate_wave(const uint8_t channel)
{
    uint16_t frequency;
    uint8_t note = g_step_buffer[channel] - 1;

    if((g_status_table[channel].current_wave == 175) && (note > 35))
    {
        /* GM Drum patch */
        if(note > 89) note = 89;
        note -= 36;

        GM_DRUM_PATCH *patch = (GM_DRUM_PATCH *)(g_waves_table[g_status_table[channel].current_wave] + sizeof(PATCH));
        g_status_table[channel].next_tone_low  = patch[note].tone;
        g_status_table[channel].next_tone_high = 0;
        g_status_table[channel].header_bytes = patch[note].header_bytes;
        frequency = patch[note].frequency;
    }
    else if(g_status_table[channel].current_wave > 175)
    {
        /* Own wave */
        OWN_PATCH      *patch = &g_own_patches[g_status_table[channel].current_wave - 176];
        OWN_PATCH_PART *patch_part = patch->patch_part;
        if(patch->transpose) note += g_transpose_value;
        g_status_table[channel].header_bytes = 0;

        uint8_t min_note = 0;
        while(note >= patch_part->next_patch_note)
        {
            min_note = patch_part->next_patch_note;
            patch_part++;
        }

        g_status_table[channel].next_tone_low  = patch_part->tone + 128;
        g_status_table[channel].next_tone_high = 1;

        note = patch_part->tone_note + note - min_note;
        g_status_table[channel].last_note = note;

        uint8_t type = (g_own_tone_info[patch_part->tone] & 0x06) >> 1;

        if(type == 0x00)
            g_status_table[channel].frequency_table = g_waves_table[FRQ_TAB_AMIGA];
        else if(type == 0x01)
            g_status_table[channel].frequency_table = g_waves_table[FRQ_TAB_PC];
        else
            g_status_table[channel].frequency_table = g_waves_table[FRQ_TAB_TURBO];

        frequency = g_status_table[channel].frequency_table[note];
    }
    else
    {
        /* Regular wave */
        PATCH      *patch = (PATCH *)g_waves_table[g_status_table[channel].current_wave];
        PATCH_PART *patch_part = &patch->patch_part;

        if(patch->transpose) note += g_transpose_value;
        g_status_table[channel].header_bytes = patch->header_bytes;

        uint8_t min_note = 0;       
        while(note >= patch_part->next_patch_note)
        {
            min_note = patch_part->next_patch_note;
            patch_part++;
        }

        g_status_table[channel].next_tone_low  = patch_part->tone_low;
        g_status_table[channel].next_tone_high = patch_part->tone_high & 0x01;

        note = note + (patch_part->tone_high >> 1) - min_note;
        g_status_table[channel].last_note = note;
        g_status_table[channel].frequency_table = patch_part->frequency_table;

        uint16_t octave = g_tabdiv12[note][0] << 8;
        uint8_t  index  = g_tabdiv12[note][1] >> 1;

        frequency = patch_part->frequency_table[index];
        frequency = (frequency << 1) + octave;
    }

    int detune_value = g_status_table[channel].detune_value * 2;
    if(detune_value >= 0) detune_value += 0x0800;
    frequency += detune_value;
    frequency &= 0xF7FF;

    g_status_table[channel].next_frequency = frequency;
}

void play_waves(void)
{
    for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
    {
        if(g_step_buffer[i] && g_step_buffer[i] <= NOTE_ON)
        {
            g_status_table[i].pitch_frequency = g_status_table[i].next_frequency;

            g_roboplay_interface->opl4_write_wave(0x68 + i, 0x00);   /* Off */
            g_roboplay_interface->opl4_write_wave(0x50 + i, 0xFF);   /* Volume to 0 */

            g_roboplay_interface->opl4_write_wave(0x20 + i, (g_status_table[i].next_frequency & 0xFF) | g_status_table[i].next_tone_high);
            g_roboplay_interface->opl4_write_wave(0x38 + i, (g_status_table[i].next_frequency >> 8) | g_status_table[i].pseudo_reverb);
            g_roboplay_interface->opl4_write_wave(0x08 + i, g_status_table[i].next_tone_low);
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

void frequency_command(void)
{
    g_base_frequency = -(g_step_buffer[COMMAND_CHANNEL] - COMMAND_FREQUENCY);
    g_roboplay_interface->update_refresh();
}

void handle_frequency_mode(void)
{
    for(uint8_t i = 0; i < NR_WAVE_CHANNELS; i++)
    {      
        if(g_status_table[i].frequency_mode == FREQUENCY_MODE_PITCH_BEND)
            handle_pitch_bend(i);
        else if(g_status_table[i].frequency_mode >= FREQUENCY_MODE_MODULATION)
            handle_modulation(i);
    }
}

void handle_pitch_bend(const uint8_t channel)
{
    uint16_t frequency = g_status_table[channel].pitch_frequency + g_status_table[channel].pitch_bend_speed;

    if(frequency & 0x0800)
    {
        if(g_status_table[channel].pitch_bend_speed < 0)
            frequency = frequency & 0xF7FF;
        else
            frequency += 0x0800;
    }

    g_status_table[channel].pitch_frequency = frequency;

    g_roboplay_interface->opl4_write_wave(0x20 + channel, (frequency & 0xFF) | g_status_table[channel].next_tone_high);
    g_roboplay_interface->opl4_write_wave(0x38 + channel, (frequency >> 8) | g_status_table[channel].pseudo_reverb);
}

void handle_modulation(const uint8_t channel)
{
    uint16_t frequency = g_status_table[channel].pitch_frequency;

    uint8_t count = g_status_table[channel].modulation_count;
    int8_t  value = g_mwm_header->modulation[g_status_table[channel].modulation_index][count];
    frequency = frequency + (4 * value);

    if(frequency & 0x0800)
    {
        if(value < 0)
            frequency = frequency & 0xF7FF;
        else
            frequency += 0x0800;
    }

    g_status_table[channel].pitch_frequency = frequency;
    
    count = (count + 1) % MODULATION_WAVE_LENGTH;
    g_status_table[channel].modulation_count = (g_status_table[channel].modulation_count + 1) % MODULATION_WAVE_LENGTH;
    if(g_mwm_header->modulation[g_status_table[channel].modulation_index][count] == 10) count = 0;
    g_status_table[channel].modulation_count = count;

    g_roboplay_interface->opl4_write_wave(0x20 + channel, (frequency & 0xFF) | g_status_table[channel].next_tone_high);
    g_roboplay_interface->opl4_write_wave(0x38 + channel, (frequency >> 8) | g_status_table[channel].pseudo_reverb);   
}

void note_on_event(const uint8_t channel)
{
    if(g_status_table[channel].header_bytes)
    {
        g_roboplay_interface->set_segment(g_waves_segment);
        
        uint8_t index = 0;

        uint8_t reg = 0x80;
        uint8_t value = g_status_table[channel].header_bytes[index++];
        do
        {
            g_roboplay_interface->opl4_write_wave(reg + channel, value);
            reg   = g_status_table[channel].header_bytes[index++];
            value = g_status_table[channel].header_bytes[index++];
        } while(reg != 0xFF);

        g_roboplay_interface->set_segment(g_song_segments[0]);       
    }

    g_roboplay_interface->opl4_write_wave(0x50 + channel, g_status_table[channel].volume | 1);
    g_roboplay_interface->opl4_write_wave(0x68 + channel, 0x80 | g_status_table[channel].current_stereo);
}

void note_off_event(const uint8_t channel)
{
    g_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    uint8_t value = g_roboplay_interface->opl4_read_wave_register(0x68 + channel);
    g_roboplay_interface->opl4_write_wave(0x68 + channel, value & 0x7F);

}

void wave_change_event(const uint8_t channel)
{   
    g_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    uint8_t wave = g_step_buffer[channel] - WAVE_CHANGE;
    g_status_table[channel].current_wave = g_mwm_header->wave_numbers[wave];

    uint8_t volume = g_mwm_header->wave_volumes[wave];
    uint8_t level_direct = (g_status_table[channel].volume & 1);
    g_status_table[channel].volume = (4 * volume) | level_direct;
}

void volume_change_event(const uint8_t channel)
{
    uint8_t volume = g_step_buffer[channel] - VOLUME_CHANGE;
    volume = (volume ^ 0x1F) << 1;

    uint8_t level_direct = (g_status_table[channel].volume & 0x01);
    g_status_table[channel].volume = (4 * volume) | level_direct;

    g_roboplay_interface->opl4_write_wave(0x50 + channel, g_status_table[channel].volume);
}

void stereo_change_event(const uint8_t channel)
{
    g_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    g_status_table[channel].current_stereo = (g_step_buffer[channel] - (STEREO_SET + 7)) & 0x0F;

    uint8_t value = g_roboplay_interface->opl4_read_wave_register(0x68 + channel) & 0xF0;
    g_roboplay_interface->opl4_write_wave(0x68 + channel, value & g_status_table[channel].current_stereo);
}

void note_link_event(const uint8_t channel)
{
    g_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;

    uint8_t note = g_status_table[channel].last_note + g_step_buffer[channel] - (NOTE_LINK + 9);
    g_status_table[channel].last_note = note;

    uint16_t frequency;
    if((g_status_table[channel].next_tone_high == 1) && (g_status_table[channel].next_tone_low >= 128))
    {
        /* Link own wave */
        g_roboplay_interface->set_segment(g_waves_segment);
        frequency = g_status_table[channel].frequency_table[note];
        g_roboplay_interface->set_segment(g_song_segments[0]);
    }
    else
    {
        uint16_t octave = g_tabdiv12[note][0] << 8;
        uint8_t  index  = g_tabdiv12[note][1] >> 1;

        g_roboplay_interface->set_segment(g_waves_segment);
        frequency = g_status_table[channel].frequency_table[index];
        g_roboplay_interface->set_segment(g_song_segments[0]);

        frequency = (frequency << 1) + octave;
    }

    int detune_value = g_status_table[channel].detune_value * 2;
    if(detune_value >= 0) detune_value += 0x0800;
    frequency += detune_value;
    frequency &= 0xF7FF;

    g_status_table[channel].pitch_frequency = frequency;

    g_roboplay_interface->opl4_write_wave(0x20 + channel, frequency & 0xFF | g_status_table[channel].next_tone_high);
    g_roboplay_interface->opl4_write_wave(0x38 + channel, frequency >> 8 | g_status_table[channel].pseudo_reverb);
}

void pitch_bend_event(const uint8_t channel)
{
    int8_t value = g_step_buffer[channel] - (PITCH_BEND + 9);
    g_status_table[channel].frequency_mode = FREQUENCY_MODE_PITCH_BEND;
    g_status_table[channel].pitch_bend_speed = value * 4;
}

void detune_event(const uint8_t channel)
{
    int value = g_step_buffer[channel] - (DETUNE + 3);
    g_status_table[channel].detune_value = value * 4;
}

void modulation_event(const uint8_t channel)
{
    int8_t value = g_step_buffer[channel] - MODULATION;
    g_status_table[channel].pitch_bend_speed = 0;
    g_status_table[channel].frequency_mode = FREQUENCY_MODE_MODULATION;
    g_status_table[channel].modulation_index = value;
    g_status_table[channel].modulation_count = 0;
}

void reverb_on_event(const uint8_t channel)
{
    g_status_table[channel].pseudo_reverb = REVERB_ENABLED;
}

void reverb_off_event(const uint8_t channel)
{
    g_status_table[channel].pseudo_reverb = REVERB_DISABLED;
}

void damp_event(const uint8_t channel)
{
    g_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    
    uint8_t value = g_roboplay_interface->opl4_read_wave_register(0x68 + channel);
    g_roboplay_interface->opl4_write_wave(0x68 + channel, value | 0x40);
}

void lfo_event(const uint8_t channel)
{
    g_status_table[channel].frequency_mode = FREQUENCY_MODE_NORMAL;
    
    uint8_t value = g_roboplay_interface->opl4_read_wave_register(0x68 + channel);
    g_roboplay_interface->opl4_write_wave(0x68 + channel, value ^ 0x20);
}

void extra_lfo_event(const uint8_t channel)
{
    uint8_t index = g_step_buffer[channel] - XTRA_LFO;

    uint8_t value = g_roboplay_interface->opl4_read_wave_register(0x68 + channel);
    g_roboplay_interface->opl4_write_wave(0x68 + channel, value | 0x20);

    g_roboplay_interface->opl4_write_wave(0x80 + channel, g_xls_table[index][0]);
    g_roboplay_interface->opl4_write_wave(0xE0 + channel, g_xls_table[index][1]);

    g_roboplay_interface->opl4_write_wave(0x68 + channel, value & 0xDF);
}
