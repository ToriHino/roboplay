/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mod.c
 *
 * MOD: Amiga MOD player
 */

#include <string.h>

#include "player_interface.h"
#include "mod.h"

bool load(const char *file_name)
{  
    /* Set OPL4 to memory access mode */
    g_roboplay_interface->opl4_write_wave(0x02, 0x11);

    g_roboplay_interface->open(file_name,false);
    g_roboplay_interface->read(&g_mod_header, sizeof(MOD_HEADER));

    for(uint8_t i = 0; i < sizeof(g_file_formats) / sizeof(MOD_FILE_FORMAT); i++)
    {
        if(g_mod_header.module_type[0] == g_file_formats[i].id[0] && 
           g_mod_header.module_type[1] == g_file_formats[i].id[1] &&
           g_mod_header.module_type[2] == g_file_formats[i].id[2] &&
           g_mod_header.module_type[3] == g_file_formats[i].id[3])
        {
            g_module_type = i;
            break;
        }
    }

    uint8_t max_pattern = 0;
    for(uint8_t pattern = 0; pattern < 128; pattern++)
    {
        if(g_mod_header.pattern_table[pattern] > max_pattern) max_pattern = g_mod_header.pattern_table[pattern];
    }

    read_patterns(max_pattern+1);
    read_samples();

    g_roboplay_interface->close();

    /* Set OPL4 to sound generation mode */
    g_roboplay_interface->opl4_write_wave(0x02, 0x10);

    return true;
}

bool update(void)
{
    g_speed_step--;

    if(!g_speed_step)
    {
        g_speed_step = g_speed;
      
        if(g_command_EE)
        {
            g_command_EE--;
            return true;
        }

        next_pattern_step();

        for(uint8_t channel = 0; channel < g_file_formats[g_module_type].nr_of_channels; channel++)
        {
            g_command_E5 = 0;
            set_sample(channel);
            handle_command(channel);

            if(!( (g_step_data[channel].effect_command == 0x0E) && (g_step_data[channel].extended_command == 0x0D) ))
            {
                new_note(channel);

                if(g_step_data[channel].effect_command == 0x0E)
                {
                    switch(g_step_data[channel].extended_command)
                    {
                        case 0x01:
                            g_channel_info[channel].period -= g_step_data[channel].effect_data;
                            if(g_channel_info[channel].period < 108) g_channel_info[channel].period = 108;
                            set_frequency(channel, g_channel_info[channel].period);              
                            break;
                        case 0x02:
                            g_channel_info[channel].period += g_step_data[channel].effect_data;
                            if(g_channel_info[channel].period > 907) g_channel_info[channel].period = 907;
                            set_frequency(channel, g_channel_info[channel].period);               
                            break;
                    }
                }
            }

            set_total_level(channel, g_channel_info[channel].volume);
            set_key_on(channel, true);
        }
    }
    else
    {
        g_arpeggio = (g_arpeggio < 2) ? g_arpeggio + 1 : 0;

        for(uint8_t channel = 0; channel < g_file_formats[g_module_type].nr_of_channels; channel++)
        {
            handle_off_note_command(channel);
        }
    }

    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    for(uint8_t channel = 0; channel < g_file_formats[g_module_type].nr_of_channels; channel++)
    {
        g_channel_info[channel].sample_number = 0;
        g_channel_info[channel].old_sample_number = 0;
        g_channel_info[channel].note_number = 0;
        g_channel_info[channel].period = 0;
        g_channel_info[channel].volume = 0;
        g_channel_info[channel].temp_command = 0;
        g_channel_info[channel].pitch_bend_speed = 0;
        g_channel_info[channel].vibrato_command = 0;
        g_channel_info[channel].vibrato_table_position = 0;
        g_channel_info[channel].tremolo_command = 0;
        g_channel_info[channel].tremolo_table_position = 0;
        g_channel_info[channel].wave_control = 0;
        g_channel_info[channel].pattern_loop_start = 0;
        g_channel_info[channel].pattern_loop_count = 0;
    }

    g_bpm = 125;
    g_speed = 6;
    g_speed_step = 1;

    g_song_position = 255;
    g_pattern_step = MAX_PATTERN_STEP;

    g_arpeggio = 0;
    g_command_E5 = 0;
    g_command_EE = 0;
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{ 
    float refresh = g_bpm * 0.4;
    return refresh;

}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "Amiga Module player V1.2 by RoboSoft Inc.";
}

char* get_title(void)
{
    return g_mod_header.song_name;
}

char* get_author(void)
{
    return "-";
}

char* get_description(void)
{
    return g_file_formats[g_module_type].description;
}

void read_patterns(uint8_t nr_of_patterns)
{
    uint8_t* file_data = (uint8_t*)DATA_SEGMENT_BASE;
    uint16_t page_left = DATA_SEGMENT_SIZE;
    uint8_t  current_segment = START_SEGMENT_INDEX;

    g_pattern_size = g_file_formats[g_module_type].nr_of_channels * STEP_DATA_SIZE * (MAX_PATTERN_STEP+1);

    for(uint8_t i = 0; i < nr_of_patterns; i++)
    {
        g_pattern_info[i].segment = current_segment;
        g_pattern_info[i].address = file_data;

        /* It's not possible to read directly to non-primary mapper memory segments,
           so use a buffer inbetween. */
        g_roboplay_interface->read((void*)READ_BUFFER, g_pattern_size);
        memcpy(file_data, (void*)READ_BUFFER, g_pattern_size);

        file_data += g_pattern_size;
        page_left = (page_left > g_pattern_size) ? page_left - g_pattern_size : 0;

        if(!page_left)
        {
            current_segment = g_roboplay_interface->get_new_segment();
            g_roboplay_interface->set_segment(current_segment);

            page_left = DATA_SEGMENT_SIZE;
            file_data = (uint8_t*)DATA_SEGMENT_BASE;
        }
    }
}

void read_samples(void)
{
    uint16_t bytes_left, bytes_read, read_size;

    uint32_t wave_address = OPL_WAVE_MEMORY + 384 * 12;
    uint16_t header_address = 0;
    
    uint32_t start;
    uint16_t loop, end;

    for(uint8_t i = 0; i < NR_OF_SAMPLES; i++)
    {
        g_mod_header.sample_info[i].sample_length = ((g_mod_header.sample_info[i].sample_length << 8) | (g_mod_header.sample_info[i].sample_length >> 8)) * 2;
        g_mod_header.sample_info[i].sample_repeat_point = ((g_mod_header.sample_info[i].sample_repeat_point << 8) | (g_mod_header.sample_info[i].sample_repeat_point >> 8)) * 2;
        g_mod_header.sample_info[i].sample_repeat_length = ((g_mod_header.sample_info[i].sample_repeat_length << 8) | (g_mod_header.sample_info[i].sample_repeat_length >> 8)) * 2;

        start = wave_address;
        loop  = g_mod_header.sample_info[i].sample_length - 1;
        end   = (g_mod_header.sample_info[i].sample_length - 1) ^ 0xFFFF;

        if(g_mod_header.sample_info[i].sample_repeat_point > 0 || g_mod_header.sample_info[i].sample_repeat_length > 4)
        {
            loop = g_mod_header.sample_info[i].sample_repeat_point;
            end = (loop + g_mod_header.sample_info[i].sample_repeat_length - 1) ^ 0xFFFF;
        }

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

        /* Set wave memory adress */
        g_roboplay_interface->opl4_write_wave(3, (wave_address >> 16) & 0x3F);
        g_roboplay_interface->opl4_write_wave(4, (wave_address >> 8)  & 0xFF);
        g_roboplay_interface->opl4_write_wave(5, wave_address & 0xFF);

        bytes_left = g_mod_header.sample_info[i].sample_length;
        while(bytes_left)
        {
            read_size = (bytes_left > READ_BUFFER_SIZE) ? READ_BUFFER_SIZE : bytes_left;
            bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, read_size);

            g_roboplay_interface->opl4_write_wave_data((uint8_t*)READ_BUFFER, bytes_read);

            wave_address += bytes_read;
            bytes_left -= bytes_read;
        }
    }
}

void next_pattern_step(void)
{
    g_pattern_step++;

    if(g_pattern_step > MAX_PATTERN_STEP)
    {
        g_song_position = (g_song_position < (g_mod_header.song_length-1)) ? g_song_position + 1 : 0;
        g_pattern_step = 0;

        set_pattern_segment();
    }

    update_step_data();
}

void set_pattern_segment(void)
{
    g_roboplay_interface->set_segment(g_pattern_info[g_mod_header.pattern_table[g_song_position]].segment);
}

void update_step_data(void)
{
    uint8_t* current_step = g_pattern_info[g_mod_header.pattern_table[g_song_position]].address + (g_pattern_step * (g_file_formats[g_module_type].nr_of_channels * STEP_DATA_SIZE));
    for(uint8_t channel = 0; channel < g_file_formats[g_module_type].nr_of_channels; channel++)
    {
        g_step_data[channel].sample_number  = (current_step[0] & 0xF0) + ((current_step[2] >> 4) & 0x0F);
        g_step_data[channel].period         = ((current_step[0] & 0x0F) << 8) + current_step[1];
        g_step_data[channel].effect_command = current_step[2] & 0x0F;
        g_step_data[channel].effect_data    = current_step[3];
        if(g_step_data[channel].effect_command == 0x0E)
        {
            g_step_data[channel].extended_command = g_step_data[channel].effect_data >> 4;
            g_step_data[channel].effect_data &= 0x0F;
        }
        current_step += STEP_DATA_SIZE;
    }
}

void handle_command(uint8_t channel)
{
    switch(g_step_data[channel].effect_command)
    {
        case 0x00:
            g_arpeggio = 0;
            set_frequency(channel, g_channel_info[channel].period);
            break;
        case 0x04:
            g_channel_info[channel].temp_command = g_channel_info[channel].period;

            if(g_step_data[channel].effect_data & 0xF0)
            {
                g_channel_info[channel].vibrato_command &= 0x0F;
                g_channel_info[channel].vibrato_command |= (g_step_data[channel].effect_data & 0xF0);
            }
            if(g_step_data[channel].effect_data & 0x0F)
            {
                g_channel_info[channel].vibrato_command &= 0xF0;
                g_channel_info[channel].vibrato_command |= (g_step_data[channel].effect_data & 0x0F);
            }
            break;
        case 0x07:
            if(g_step_data[channel].effect_data & 0xF0)
            {
                g_channel_info[channel].tremolo_command &= 0x0F;
                g_channel_info[channel].tremolo_command |= (g_step_data[channel].effect_data & 0xF0);
            }
            if(g_step_data[channel].effect_data & 0x0F)
            {
                g_channel_info[channel].tremolo_command &= 0xF0;
                g_channel_info[channel].tremolo_command |= (g_step_data[channel].effect_data & 0x0F);
            }
            break;
        case 0x0C:
            g_channel_info[channel].volume = (g_step_data[channel].effect_data > 64) ? 64 : g_step_data[channel].effect_data;
            set_total_level(channel, g_channel_info[channel].volume);
            break;
        case 0x0E:
            switch(g_step_data[channel].extended_command)
            {
                case 0x04:
                    g_channel_info[channel].wave_control &= 0xFC;
                    g_channel_info[channel].wave_control |= g_step_data[channel].effect_data;
                    g_channel_info[channel].vibrato_table_position = 0;
                    break;
                case 0x05:
                    g_command_E5 = g_step_data[channel].effect_data;
                    break;
                case 0x07:
                    g_channel_info[channel].wave_control &= 0xF3;
                    g_channel_info[channel].wave_control |= (g_step_data[channel].effect_data << 2);
                    g_channel_info[channel].tremolo_table_position = 0;
                    break;
                case 0x09:
                    g_channel_info[channel].temp_command = g_step_data[channel].effect_data;
                    break;
                case 0x0A:
                    g_channel_info[channel].volume += g_step_data[channel].effect_data;
                    if(g_channel_info[channel].volume > MAX_VOLUME)
                        g_channel_info[channel].volume = MAX_VOLUME;
                    set_total_level(channel, g_channel_info[channel].volume);
                    break;
                case 0x0B:
                    if(g_channel_info[channel].volume >= g_step_data[channel].effect_data)
                        g_channel_info[channel].volume -= g_step_data[channel].effect_data;
                    else
                        g_channel_info[channel].volume = 0;              
                    set_total_level(channel, g_channel_info[channel].volume);
                    break;
                case 0x0C:
                    if(g_step_data[channel].effect_data)
                        g_channel_info[channel].temp_command = g_step_data[channel].effect_data;
                    else
                    {
                        g_channel_info[channel].volume = 0;
                        set_total_level(channel, g_channel_info[channel].volume);                        
                    }
                    break;
                case 0x0D:
                    if(g_step_data[channel].effect_data)
                        g_channel_info[channel].temp_command = g_step_data[channel].effect_data;                  
                    break;
                case 0x0E:
                    g_command_EE = g_step_data[channel].effect_data;
                    break;
                case 0x0F:
                    break;
            }
            break;
        case 0x0F:
            if(g_step_data[channel].effect_data < 32)
            {
                g_speed = g_step_data[channel].effect_data;
                g_speed_step = g_speed;
            }
            else
            {
                g_bpm = g_step_data[channel].effect_data;
                g_roboplay_interface->update_refresh();
            }
            break;
    }
}

void handle_off_note_command(uint8_t channel)
{
    switch(g_step_data[channel].effect_command)
    {
        case 0x00:
            if(g_step_data[channel].effect_data)
            {
                uint8_t note_number, new_note_number;

                if(!g_arpeggio) 
                {
                    set_frequency(channel, g_channel_info[channel].period);
                }
                else
                {
                    note_number = g_channel_info[channel].note_number;

                    if(g_arpeggio == 1)
                        new_note_number = note_number + (g_step_data[channel].effect_data >> 4);
                    else
                        new_note_number = note_number + (g_step_data[channel].effect_data & 0x0F);

                    set_frequency(channel, tune_period(channel, g_periods[new_note_number]));
                    g_channel_info[channel].note_number = note_number;
                }
            }
            break;
        case 0x01:
            g_channel_info[channel].period -= g_step_data[channel].effect_data;
            if(g_channel_info[channel].period < 108) g_channel_info[channel].period = 108;
            set_frequency(channel, g_channel_info[channel].period);
            break;
        case 0x02:
            g_channel_info[channel].period += g_step_data[channel].effect_data;
            if(g_channel_info[channel].period > 907) g_channel_info[channel].period = 907;
            set_frequency(channel, g_channel_info[channel].period);
            break;
        case 0x03:
            tone_portamento(channel);
            break;
        case 0x04:
            vibrato(channel);
            break;
        case 0x05:
            tone_portamento(channel);
            volume_slide(channel);
            break;
        case 0x06:
            vibrato(channel);
            volume_slide(channel);
            break;
        case 0x07:
            tremolo(channel);
            break;
        case 0x0A:
            volume_slide(channel);
            break;
        case 0x0B:
            if(g_speed_step == 1)
            {
                g_song_position = g_step_data[channel].effect_data - 1;
                g_pattern_step = MAX_PATTERN_STEP;
            }
            break;
        case 0x0D:
            if(g_speed_step == 1)
            {
                g_song_position++;
                g_pattern_step = g_step_data[channel].effect_data - 1;
                set_pattern_segment();
            }
            break;
        case 0x0E:
            switch(g_step_data[channel].extended_command)
            {
                case 0x06:
                    if(g_speed_step == 1)
                    {
                        if(!g_step_data[channel].effect_data)
                        {
                            g_channel_info[channel].pattern_loop_start = g_pattern_step - 1;
                        }
                        else
                        {
                            if(g_channel_info[channel].pattern_loop_count < g_step_data[channel].effect_data)
                            {
                                g_channel_info[channel].pattern_loop_count++;
                                g_pattern_step = g_channel_info[channel].pattern_loop_start;
                            }
                            else
                                g_channel_info[channel].pattern_loop_count = 0;
                        }
                    }
                    break;
                case 0x09:
                    new_note(channel);
                    break;
                case 0x0C:
                    if(g_channel_info[channel].temp_command)
                    {
                        g_channel_info[channel].temp_command--;
                        if(!g_channel_info[channel].temp_command)
                        {
                            g_channel_info[channel].volume = 0;
                            set_total_level(channel, g_channel_info[channel].volume);                          
                        }
                    }
                    break;
                case 0x0D:
                    if(g_channel_info[channel].temp_command)
                    {
                        g_channel_info[channel].temp_command--;
                        if(!g_channel_info[channel].temp_command)
                            new_note(channel);
                    }
                    break;
            }
            break;
    }
}

void set_sample(uint8_t channel)
{
    if(g_step_data[channel].sample_number)
    {
        g_channel_info[channel].sample_number = g_step_data[channel].sample_number;
        g_channel_info[channel].volume = g_mod_header.sample_info[g_step_data[channel].sample_number - 1].volume;
        g_channel_info[channel].vibrato_table_position = 0;
    }
}

void new_note(uint8_t channel)
{
    if(g_channel_info[channel].sample_number)
    {
        if(g_step_data[channel].effect_command == 0x03 || g_step_data[channel].effect_command == 0x05)
        {
            if(g_step_data[channel].effect_command == 0x03 && g_step_data[channel].effect_data)
                g_channel_info[channel].pitch_bend_speed = g_step_data[channel].effect_data;

            if(g_step_data[channel].period)
                g_channel_info[channel].temp_command = tune_period(channel, g_step_data[channel].period);
        }
        else if(g_step_data[channel].period)
        {          
            set_total_level(channel, 0);
            set_key_on(channel, false);

            g_channel_info[channel].period = tune_period(channel, g_step_data[channel].period);
            set_frequency(channel, g_channel_info[channel].period);

            if(g_channel_info[channel].sample_number != g_channel_info[channel].old_sample_number)
            {
                g_channel_info[channel].old_sample_number = g_channel_info[channel].sample_number;
                set_sample_number(channel, g_channel_info[channel].sample_number);
            }           
        }
    }
}

uint16_t tune_period(uint8_t channel, uint16_t period)
{
    uint8_t i;
    for(i = 0; i < NOTE_NUMBERS; i++)
    {
        if(g_periods[i] == period) break;
    }
    g_channel_info[channel].note_number = i;

    if(g_command_E5)
    {
        period += g_finetune_table[g_command_E5 - 1][g_channel_info[channel].note_number];
    }
    else if((g_mod_header.sample_info[g_channel_info[channel].sample_number - 1].finetune & 0x0F)) 
    {  
        uint8_t finetune = (g_mod_header.sample_info[g_channel_info[channel].sample_number - 1].finetune & 0x0F);
        period += g_finetune_table[finetune - 1][g_channel_info[channel].note_number];
    }

    return period;
}

void tone_portamento(uint8_t channel)
{
    if(g_channel_info[channel].period > g_channel_info[channel].temp_command)
    {
        if(g_channel_info[channel].period >= g_channel_info[channel].pitch_bend_speed)
            g_channel_info[channel].period -= g_channel_info[channel].pitch_bend_speed;
        else
            g_channel_info[channel].period = 0;

        if(g_channel_info[channel].period < g_channel_info[channel].temp_command)
            g_channel_info[channel].period = g_channel_info[channel].temp_command;
    }
    if(g_channel_info[channel].period < g_channel_info[channel].temp_command)
    {
        g_channel_info[channel].period += g_channel_info[channel].pitch_bend_speed;   
        if(g_channel_info[channel].period > g_channel_info[channel].temp_command)
            g_channel_info[channel].period = g_channel_info[channel].temp_command;
    }

     set_frequency(channel, g_channel_info[channel].period);
}

void vibrato(uint8_t channel)
{
    uint8_t vib_index;
    uint16_t vib_value;

    vib_index = (g_channel_info[channel].vibrato_table_position >> 2) & 0x1F;
    vib_value = g_vibrato_table[vib_index] * (g_channel_info[channel].vibrato_command & 0x0F);
    vib_value = vib_value >> 7;

    if(g_channel_info[channel].vibrato_table_position > 127)
        g_channel_info[channel].temp_command = g_channel_info[channel].period - vib_value;
    else
        g_channel_info[channel].temp_command = g_channel_info[channel].period + vib_value;

    g_channel_info[channel].vibrato_table_position += ((g_channel_info[channel].vibrato_command >> 2) & 0x3C);

    set_frequency(channel, g_channel_info[channel].temp_command);
}

void tremolo(uint8_t channel)
{
    uint8_t  tremolo_index, volume;
    uint16_t tremolo_value;

    tremolo_index = (g_channel_info[channel].tremolo_table_position >> 2) & 0x1F;
    tremolo_value = g_vibrato_table[tremolo_index] * (g_channel_info[channel].tremolo_command & 0x0F);
    tremolo_value = tremolo_value >> 6;

    if(g_channel_info[channel].tremolo_table_position > 127)
    {
        volume = (g_channel_info[channel].volume >= tremolo_value) ?  g_channel_info[channel].volume - tremolo_value : 0;
    }
    else
    {
        volume = g_channel_info[channel].volume + tremolo_value;
        if(volume > 64) volume = 64;
    }

    g_channel_info[channel].tremolo_table_position += ((g_channel_info[channel].tremolo_command >> 2) & 0x3C);

    set_total_level(channel, volume);
}

void volume_slide(uint8_t channel)
{
    if(g_step_data[channel].effect_data < 0x10)
    {
        if(g_channel_info[channel].volume >= g_step_data[channel].effect_data & 0x0F)
            g_channel_info[channel].volume -= g_step_data[channel].effect_data & 0x0F;
        else
            g_channel_info[channel].volume = 0;              
    }
    else
    {
        g_channel_info[channel].volume += ((g_step_data[channel].effect_data >> 4) & 0x0F);
        if(g_channel_info[channel].volume > MAX_VOLUME)
            g_channel_info[channel].volume = MAX_VOLUME;
    }

    set_total_level(channel, g_channel_info[channel].volume);
}

void set_sample_number(uint8_t channel, uint8_t sample_number)
{
    g_roboplay_interface->opl4_write_wave(0x08 + channel, 0x7F + sample_number);
    g_roboplay_interface->opl4_wait_for_load();
}

void set_frequency(uint8_t channel, uint16_t period)
{
    uint16_t pitch_index = 2 * (period - 108);
    g_roboplay_interface->opl4_write_wave(0x38 + channel, g_pitch_table[pitch_index + 1]);
    g_roboplay_interface->opl4_write_wave(0x20 + channel, g_pitch_table[pitch_index] | 0x01);
}

void set_total_level(uint8_t channel, uint8_t volume)
{
    g_roboplay_interface->opl4_write_wave(0x50 + channel, (g_volume_table[volume] << 1) | 0x01);
}

void set_key_on(uint8_t channel, bool key_on)
{
    if(!key_on)
        g_roboplay_interface->opl4_write_wave(0x68 + channel, 0x7F & g_pan_setting[channel]);
    else
        g_roboplay_interface->opl4_write_wave(0x68 + channel, 0x80 | g_pan_setting[channel]);
 }