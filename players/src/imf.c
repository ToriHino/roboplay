/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * imf.c
 *
 * IMF: id Software Music Format player
 */

#include <string.h>

#include "player_interface.h"
#include "imf.h"

bool load(const char *file_name)
{  
    const char* extension = &file_name[strlen(file_name)-3];
    
    if(!strcmp(extension, "WLF"))
        /* For .WLF use refresh rate of 700Hz */
        g_refresh = 700.0;
    else
        /* Default for IMF is 560Hz */
        g_refresh = 560.0;

    g_roboplay_interface->open(file_name, false);

    g_roboplay_interface->read(&g_imf_data_size, sizeof(g_imf_data_size));
    g_imf_type = (g_imf_data_size == 0x0) ? IMF_TYPE_0 : IMF_TYPE_1;

    g_segment_index = 0;

    uint32_t data_size = g_imf_data_size;
    uint16_t page_left = DATA_SEGMENT_SIZE;
    uint8_t* destination = (uint8_t*)DATA_SEGMENT_BASE;

    if(g_imf_type == IMF_TYPE_0)
    {
        *destination++ = 0;
        *destination++ = 0;
        page_left -= 2;
    }

    uint16_t read_size = 0;
    uint16_t bytes_read = 0;
    while(data_size > 0 || g_imf_type == IMF_TYPE_0)
    {
        if(g_imf_type == IMF_TYPE_0) data_size = READ_BUFFER_SIZE;

        read_size = (data_size < READ_BUFFER_SIZE) ? data_size : READ_BUFFER_SIZE;
        read_size = (read_size < page_left) ? read_size : page_left;

        /* It's not possible to read directly to non-primary mapper memory segments,
           so use a buffer inbetween. */
        bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, read_size);

        if(!bytes_read) break;

        if(g_imf_type == IMF_TYPE_0) g_imf_data_size += bytes_read;
        memcpy(destination, (void*)READ_BUFFER, bytes_read);

        data_size -= bytes_read;
        destination += bytes_read;
        page_left -= bytes_read;
        if(page_left == 0)
        {
            g_segment_list[g_segment_index] = g_roboplay_interface->get_new_segment();
            g_roboplay_interface->set_segment(g_segment_list[g_segment_index++]);

            page_left = DATA_SEGMENT_SIZE;
            destination = (uint8_t*)DATA_SEGMENT_BASE;
        }
    }

    g_roboplay_interface->close();

    return true;
}

bool update(void)
{
    
    if(g_delay_counter)
    {
        g_delay_counter--;
    }

    while(!g_delay_counter)
    {
        if(g_index_pointer > g_imf_data_size)
            return false;

        handle_imf_update();
    }

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
    g_song_data = (void*)DATA_SEGMENT_BASE;

    g_delay_counter = 0;
    g_index_pointer = 0;
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
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
    return "id Software Music Format player V1.0 by RoboSoft Inc.";
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
    if(g_imf_type == IMF_TYPE_0)
        return "IMF type 0 file";
    else
        return "IMF type 1 file";
}

uint8_t get_next_data_byte(void)
{
    uint8_t value = *g_song_data++;
    g_index_pointer++;

    if(g_song_data == (void*)(DATA_SEGMENT_BASE + DATA_SEGMENT_SIZE))
    {
        g_song_data = (void*)DATA_SEGMENT_BASE;
        g_roboplay_interface->set_segment(g_segment_list[g_segment_index++]);
    }

    return value;
}

void handle_imf_update(void)
{
    uint8_t register_value = get_next_data_byte(); 
    uint8_t data_value = get_next_data_byte();

    g_delay_counter = get_next_data_byte();
    g_delay_counter += 256 * get_next_data_byte();

    switch(register_value)
    {
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x53:
        case 0x54:
        case 0x55:
            data_value = (data_value & 0xC0) + ((data_value & 0x3F) >> 1);
    }

    g_roboplay_interface->opl4_write_fm_1(register_value, data_value);
}
