/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * sng.c
 *
 * SNG: SCC Musixx
 */

#include <string.h>

#include "player_interface.h"
#include "sng.h"

bool load(const char* file_name)
{
    g_roboplay_interface->open(file_name, false);

    g_header = (SNG_HEADER *)(READ_BUFFER + SNG_PATTERN_SIZE);
    g_roboplay_interface->read((uint8_t *)g_header, sizeof(SNG_HEADER));

    uint8_t* destination = (uint8_t *)DATA_SEGMENT_BASE;
    uint8_t  current_segment = START_SEGMENT_INDEX;
    uint16_t page_left  = DATA_SEGMENT_SIZE;

    uint16_t bytes_read = 0;
    uint8_t  pattern_index = 0xFF;
    do
    {
        pattern_index++;
        g_pattern_data[pattern_index].segment = current_segment;
        g_pattern_data[pattern_index].data    = (SNG_ROW *)destination;

        /* It's not possible to read directly to non-primary mapper memory segments,
           so use a buffer inbetween. */
        bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, SNG_PATTERN_SIZE);
        memcpy(destination, (void*)READ_BUFFER, bytes_read);

        destination += bytes_read;
        page_left -= bytes_read;
        if(page_left < SNG_PATTERN_SIZE)
        {
            current_segment = g_roboplay_interface->get_new_segment();
            g_roboplay_interface->set_segment(current_segment);

            page_left = DATA_SEGMENT_SIZE;
            destination = (uint8_t *)DATA_SEGMENT_BASE;
        }

    } while(bytes_read);

    g_roboplay_interface->close();
    
    return true;
}

bool update(void)
{
    g_speed_count--;
    if(!g_speed_count)
    {
        do
        {
            play_note();
        }
        while(!handle_command());

        g_speed_count = g_speed;

        g_pattern_line++;
        if(g_pattern_line == SNG_NR_OF_ROWS) next_pattern();
    }

    /* Do commands */
    for(uint8_t i = 0; i < SNG_NR_OF_CHANNELS; i++)
    {
        switch(g_channel_data[i].command)
        {
            case SNG_COMMAND_SLIDE_DOWN:
            case SNG_COMMAND_SLIDE_UP:
                do_slide(i);
                break;
            case SNG_COMMAND_VOLUME_SLIDE:
                do_volume_slide(i);
                break;
        }
    }

    /* Do modulates */
    g_modulate_count--;
    if(!g_modulate_count)
    {
        g_modulate_count = SNG_MODULATE_COUNT;
        for(uint8_t i = 0; i < SNG_NR_OF_CHANNELS; i++)
        {          
            if(g_channel_data[i].modulate_work.value)
            {
                if(g_channel_data[i].modulate_work.direction == SNG_DIRECTION_DOWN)
                {
                    g_channel_data[i].frequency_work += g_channel_data[i].modulate_work.value;
                    g_channel_data[i].modulate_work.direction = SNG_DIRECTION_UP;
                }
                else
                {
                    g_channel_data[i].frequency_work -= g_channel_data[i].modulate_work.value;
                    g_channel_data[i].modulate_work.direction = SNG_DIRECTION_DOWN;                   
                }
            }
        }
    }

    /* Update to SCC */
    for(uint8_t i = 0; i < SNG_NR_OF_CHANNELS; i++)
    {
        g_roboplay_interface->scc_set_frequency(i, g_channel_data[i].frequency_work);
        g_roboplay_interface->scc_set_volume(i, g_channel_data[i].volume);
    }

    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    g_speed       = SNG_DEFAULT_SPEED;
    g_speed_count = 1;

    g_modulate_count = 1;

    g_pattern_line  = 0;
    g_position      = 0xFF;

    for(uint8_t i = 0; i < SNG_NR_OF_CHANNELS; i++)
    {
        g_channel_data[i].frequency_work = 0;
        g_channel_data[i].frequency_original = 0;

        g_channel_data[i].volume = 0;
        g_channel_data[i].instrument = 0;
        
        g_channel_data[i].transpose.direction = SNG_DIRECTION_DOWN;
        g_channel_data[i].transpose.value     = 0;

        g_channel_data[i].modulate_work.direction = SNG_DIRECTION_DOWN;
        g_channel_data[i].modulate_work.value     = 0;

        g_channel_data[i].modulate_original.direction = SNG_DIRECTION_DOWN;
        g_channel_data[i].modulate_original.value     = 0;

        g_channel_data[i].volume_slide.direction = SNG_DIRECTION_DOWN;
        g_channel_data[i].volume_slide.value     = 0;

        g_channel_data[i].frequency_slide.direction = SNG_DIRECTION_DOWN;
        g_channel_data[i].frequency_slide.value     = 0;

        g_channel_data[i].command = 0;
        g_channel_data[i].value   = 0;
    }

    /* Select start pattern */
    next_pattern();
}


void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{ 
    /* Fixed replay rate of 50Hz */
    return 50.0;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "SCC Musixx player V1.0 by RoboSoft Inc.";
}

char* get_title(void)
{
    return "-";
}

char* get_author(void)
{
    return "-";
}

char* get_description(void)
{
    return "-";
}

void do_slide(uint8_t channel)
{
    uint8_t value = g_channel_data[channel].frequency_slide.value;
    if(g_channel_data[channel].frequency_slide.rest)
    {
        value++;
        g_channel_data[channel].frequency_slide.rest--;
    }

    if(g_channel_data[channel].command == SNG_COMMAND_SLIDE_DOWN)
    {
        g_channel_data[channel].frequency_work     += value;
        g_channel_data[channel].frequency_original += value;
    }
    else
    {
        g_channel_data[channel].frequency_work     -= value;
        g_channel_data[channel].frequency_original -= value;       
    }
}

void do_volume_slide(uint8_t channel)
{
    uint8_t value = g_channel_data[channel].volume_slide.value;
    if(g_channel_data[channel].volume_slide.rest)
    {
        value++;
        g_channel_data[channel].volume_slide.rest--;
    }

    if(g_channel_data[channel].command == SNG_COMMAND_SLIDE_DOWN)
    {
        if(g_channel_data[channel].volume > value)
            g_channel_data[channel].volume -= value;
        else
            g_channel_data[channel].volume = 0; 
    }
    else
    {
        g_channel_data[channel].volume += value;
        if(g_channel_data[channel].volume > SNG_MAX_VOLUME)
            g_channel_data[channel].volume = SNG_MAX_VOLUME;
    }
}

void next_pattern(void)
{
    g_position++;
    if(g_position >= g_header->song_length) g_position = 0;
    g_pattern_number = g_header->patterns[g_position];
    g_pattern_line = 0;

    g_roboplay_interface->set_segment(g_pattern_data[g_pattern_number].segment);
}

void play_note(void)
{
     SNG_ROW *row = &g_pattern_data[g_pattern_number].data[g_pattern_line];

    /* Handle frequency */
    for(uint8_t i = 0; i < SNG_NR_OF_CHANNELS; i++)
    {
        uint16_t frequency = (i == 4) ? row->channel_5.frequency : row->channels[i].frequency;
        if(frequency)
        {
            if(g_channel_data[i].transpose.direction == SNG_DIRECTION_DOWN)
                frequency += g_channel_data[i].transpose.value;
            else
                frequency -= g_channel_data[i].transpose.value;

            g_channel_data[i].frequency_original = frequency;
            g_channel_data[i].frequency_work     = frequency;

            g_channel_data[i].modulate_work.direction = g_channel_data[i].modulate_original.direction;
            g_channel_data[i].modulate_work.value     = g_channel_data[i].modulate_original.value;
       }
    }

    /* Handle volume */
    for(uint8_t i = 0; i < SNG_NR_OF_CHANNELS; i++)
    {
        uint8_t volume = (i == 4) ? row->channel_5.volume : row->channels[i].volume;
        volume = volume >> 4;

        g_channel_data[i].volume = volume;
    }

    /* Handle instrument */
    for(uint8_t i = 0; i < SNG_NR_OF_CHANNELS - 1; i++)
    {
        uint8_t instrument = row->channels[i].instrument;

        if(instrument && (instrument != g_channel_data[i].instrument))
        {
            g_channel_data[i].instrument = instrument;
            g_roboplay_interface->scc_set_waveform(i, g_header->instruments[instrument - 1].wave);
        }
    }
}

bool handle_command(void)
{
    bool result = true;

     SNG_ROW *row = &g_pattern_data[g_pattern_number].data[g_pattern_line];

    for(uint8_t i = 0; i < SNG_NR_OF_CHANNELS; i++)
    {
        uint8_t command = (i == 4) ? row->channel_5.volume : row->channels[i].volume;
        uint8_t value = (i == 4) ? row->channel_5.value : row->channels[i].value;

        command = command & 0x0F;
        g_channel_data[i].command = command;
        g_channel_data[i].value   = value;

        switch(command)
        {
            case SNG_COMMAND_CHANGE_SPEED:
                if(value)
                {
                    g_speed = (value == 1) ? 2 : value;
                }
                break;
            case SNG_COMMAND_BYTE:
                break;
            case SNG_COMMAND_CAPS:
                break;
            case  SNG_COMMAND_GOTO:
                g_position = value - 1;
                next_pattern();
                result = false;
                break;
            case SNG_COMMAND_SKIP_ROWS:
                next_pattern();
                result = false;
                break;
            case SNG_COMMAND_REGISTER:
                break;
            case SNG_COMMAND_TRANSPOSE_DOWN:
                handle_transpose(i, SNG_DIRECTION_DOWN, value);
                break;
            case SNG_COMMAND_TRANSPOSE_UP:
                handle_transpose(i, SNG_DIRECTION_UP, value);
                break;
            case SNG_COMMAND_VOLUME_SLIDE:
                handle_volume_slide(i, value);
                break;
            case SNG_COMMAND_MODULATE_DOWN:
                handle_modulate(i, SNG_DIRECTION_DOWN, value);
                break;
            case SNG_COMMAND_MODULATE_UP:
                handle_modulate(i, SNG_DIRECTION_UP, value);
                break;
            case SNG_COMMAND_SLIDE_DOWN:
                handle_slide(i, SNG_DIRECTION_DOWN, value);
                break;
            case SNG_COMMAND_SLIDE_UP:
                handle_slide(i, SNG_DIRECTION_UP, value);
                break;
        }
    }

    return result;
}

void handle_transpose(uint8_t channel, SNG_DIRECTION direction, uint8_t value)
{
    g_channel_data[channel].transpose.direction = direction;
    g_channel_data[channel].transpose.value     = value;

    if(direction == SNG_DIRECTION_DOWN)
        g_channel_data[channel].frequency_work += value;
    else
        g_channel_data[channel].frequency_work -= value;
}

void handle_volume_slide(uint8_t channel, uint8_t value)
{
    if(value & 0xF0)
        g_channel_data[channel].volume_slide.direction = SNG_DIRECTION_DOWN;
    else
        g_channel_data[channel].volume_slide.direction = SNG_DIRECTION_UP;

    value = value & 0x0F;
    g_channel_data[channel].volume_slide.value     = value / g_speed;
    g_channel_data[channel].volume_slide.rest      = value % g_speed;
}

void handle_modulate(uint8_t channel, SNG_DIRECTION direction, uint8_t value)
{
    g_channel_data[channel].modulate_work.direction = direction;
    g_channel_data[channel].modulate_work.value     = value;

    g_channel_data[channel].modulate_original.direction = direction;
    g_channel_data[channel].modulate_original.value     = value;

    g_channel_data[channel].frequency_work = g_channel_data[channel].frequency_original;
}

void handle_slide(uint8_t channel, SNG_DIRECTION direction, uint8_t value)
{
    g_channel_data[channel].frequency_slide.direction = direction;
    g_channel_data[channel].frequency_slide.value     = value / g_speed;
    g_channel_data[channel].frequency_slide.rest      = value % g_speed;
}