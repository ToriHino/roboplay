/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * raw.c
 *
 * RAW: RAW OPL capture player
 */

#include <string.h>

#include "player_interface.h"
#include "raw.h"

bool load(const char* file_name)
{
    g_roboplay_interface->open(file_name, false);

    g_roboplay_interface->read(&g_raw_header, sizeof(RAW_HEADER));
    if (strncmp(g_raw_header.signature, "RAWADATA", sizeof(g_raw_header.signature)))
    {
        g_roboplay_interface->close();
        return false;
    }

    g_segment_index = 0;

    uint16_t bytes_read = 0;
    uint8_t* destination = (uint8_t*)DATA_SEGMENT_BASE;
    do {
        destination = (uint8_t*)DATA_SEGMENT_BASE;
        for(uint8_t i = 0; i < DATA_SEGMENT_SIZE / READ_BUFFER_SIZE; i++)
        {
            /* It's not possible to read directly to non-primary mapper memory segments,
               so use a buffer inbetween. */
            bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, READ_BUFFER_SIZE);
            if(!bytes_read) break;

            memcpy(destination, (void*)READ_BUFFER, READ_BUFFER_SIZE);
            destination += READ_BUFFER_SIZE;
        }

        if(bytes_read)
        {
            g_segment_list[g_segment_index] = g_roboplay_interface->get_new_segment();
            g_roboplay_interface->set_segment(g_segment_list[g_segment_index++]);
        }
    } while (bytes_read);

    for(uint8_t i = 0; i < 2; i++)
    {
        if(destination >= (uint8_t*)DATA_SEGMENT_SIZE)
        {
            g_segment_list[g_segment_index] = g_roboplay_interface->get_new_segment();
            g_roboplay_interface->set_segment(g_segment_list[g_segment_index++]);
            destination = (uint8_t*)DATA_SEGMENT_BASE;
        }
        *destination++ = 0xff;
    }

    g_roboplay_interface->close();

    return true;
}

bool update(void)
{
    bool set_speed = false;

    if(g_delay_counter)
    {
        g_delay_counter--;
        return true;
    }

    do
    {
        set_speed = false;
        switch(g_song_data[g_position].command)
        {
            case RAW_DELAY:
                g_delay_counter = g_song_data[g_position].parameter - 1;
                break;

            case RAW_CONTROL:
                if(g_song_data[g_position].parameter == RAW_CONTROL_TYPE_CLOCK)
                {
                    set_speed = true;
                    next_position();

                    g_speed = g_song_data[g_position].parameter + ((uint16_t)g_song_data[g_position].command << 8);
                    g_roboplay_interface->update_refresh();
                }
                else
                {
                    g_second_opl = (g_song_data[g_position].parameter == RAW_CONTROL_TYPE_SECOND_OPL);
                }
                break;

            case 0xff:
                if(g_song_data[g_position].parameter == 0xff) return false;
                break;

            default:
                if(g_second_opl)
                {
                    if(g_song_data[g_position].command > 3)
                        g_roboplay_interface->opl4_write_fm_2(g_song_data[g_position].command, g_song_data[g_position].parameter);
                }
                else
                {
                    if(g_song_data[g_position].command > 4)
                        g_roboplay_interface->opl4_write_fm_1(g_song_data[g_position].command, g_song_data[g_position].parameter);

                }
        }
        next_position();
    } while (g_song_data[g_position].command || set_speed);

    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    /* Start with standard OPL2 mode */
    g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL2);

    g_segment_index = 0;

    g_roboplay_interface->set_segment(START_SEGMENT_INDEX);
    g_song_data = (RAW_DATA*)DATA_SEGMENT_BASE;

    g_position = 0;

    g_delay_counter = 0;
    g_speed = g_raw_header.clock;
    g_second_opl = false;
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{ 
    return 1193180.0 / (float)(g_speed ? g_speed : 0xffff);
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "Raw AdLib Capture player V1.0 by RoboSoft Inc.";
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

void next_position(void)
{
    g_position++;
    if(g_position >= DATA_SEGMENT_SIZE/2)
    {
        g_position = 0;
        g_roboplay_interface->set_segment(++g_segment_index);
    }
}