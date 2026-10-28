/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * dro.c
 *
 * DRO: DOSBox Raw OPL player
 */

#include <string.h>

#include "player_interface.h"
#include "dro.h"

bool load(char *const file_name)
{  
    g_roboplay_interface->open(file_name, false);

    g_roboplay_interface->read(&g_dro_header, sizeof(DRO_HEADER));
    if(strncmp(g_dro_header.signature, "DBRAWOPL", sizeof(g_dro_header.signature)))
    {
        g_roboplay_interface->close();
        return false;
    }

    if(g_dro_header.version_major == 0 && g_dro_header.version_minor == 1)
    {
        g_dro_type = DRO_TYPE_0_1;
    }
    else if(g_dro_header.version_major == 2 && g_dro_header.version_minor == 0)
    {
        g_dro_type = DRO_TYPE_2_0;
    }
    else
    {
        g_roboplay_interface->close();
        return false;
    }

    g_segment_index = 0;
    g_segment_list[g_segment_index++] = START_SEGMENT_INDEX;

    uint32_t data_size = 0;
    uint16_t page_left = DATA_SEGMENT_SIZE;
    uint8_t* destination = (uint8_t*)DATA_SEGMENT_BASE;

    if(g_dro_type == DRO_TYPE_0_1)
    {
        g_roboplay_interface->read(&g_dro1_header, sizeof(DRO1_HEADER));

        data_size = g_dro1_header.length_bytes;
        if(g_dro1_header.hardware_extra[0] != 0x00 || g_dro1_header.hardware_extra[1] != 0x00 || g_dro1_header.hardware_extra[2] != 0x00)
        {
            // Early version of DRO 0.1 only used single uint8_t for hardware_type
            *destination++ = g_dro1_header.hardware_extra[0];
            *destination++ = g_dro1_header.hardware_extra[1];
            *destination++ = g_dro1_header.hardware_extra[2];

            data_size -= 3;
            page_left -= 3;
        }
    }
    else
    {
        g_roboplay_interface->read(&g_dro2_header, sizeof(DRO2_HEADER));
        g_roboplay_interface->read(g_code_map, g_dro2_header.code_map_length);

        data_size = g_dro2_header.length_pairs * 2;
    }

    uint16_t read_size = 0;
    uint16_t bytes_read = 0;
    while(data_size > 0)
    {
        read_size = (data_size < READ_BUFFER_SIZE) ? data_size : READ_BUFFER_SIZE;
        read_size = (read_size < page_left) ? read_size : page_left;

        /* It's not possible to read directly to non-primary mapper memory segments,
           so use a buffer inbetween. */
        bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, read_size);
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
        if(g_dro_type == DRO_TYPE_0_1)
        {
            if(g_index_pointer > g_dro1_header.length_bytes)
                return false;

            handle_dro1_update();
        }
        else
        {
            g_index_pointer++;
            if(g_index_pointer > g_dro2_header.length_pairs)
                return false;

            handle_dro2_update();
        }
    }
    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    /* Start with standard OPL2 mode */
    g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL2);

    if(g_dro_type == DRO_TYPE_0_1)
    {
        if(g_dro1_header.hardware_type == DRO1_HARDWARE_TYPE_OPL3 || g_dro1_header.hardware_type == DRO1_HARDWARE_TYPE_DUAL_OPL2)
            g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL3);
    }
    else
    {
        if(g_dro2_header.hardware_type == DRO2_HARDWARE_TYPE_OPL3 || g_dro2_header.hardware_type == DRO2_HARDWARE_TYPE_DUAL_OPL2)
            g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL3);
    }

    g_segment_index = 0;
    g_roboplay_interface->set_segment(g_segment_list[g_segment_index]);
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
    /* Fixed replay rate of 1000Hz */
    return 1000.0;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "DOSBox Raw OPL player V1.0 by RoboSoft Inc.";
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
    if(g_dro_type == DRO_TYPE_0_1)
    {
        switch(g_dro1_header.hardware_type)
        {
            case DRO1_HARDWARE_TYPE_OPL2:
                return "DRO Version 0.1, OPL2";
            case DRO1_HARDWARE_TYPE_OPL3:
                return "DRO Version 0.1, OPL3";
            case DRO1_HARDWARE_TYPE_DUAL_OPL2:
                return "DRO Version 0.1, DUAL OPL2";
            default:
                return "DRO Version 0.1";
        }
    }
    else
    {
        switch(g_dro2_header.hardware_type)
        {
            case DRO2_HARDWARE_TYPE_OPL2:
                return "DRO Version 2.0, OPL2";
            case DRO2_HARDWARE_TYPE_OPL3:
                return "DRO Version 2.0, OPL3";
            case DRO2_HARDWARE_TYPE_DUAL_OPL2:
                return "DRO Version 2.0, DUAL OPL2";
            default:
                return "DRO Version 2.0";
        }
    }
}

uint8_t get_next_data_byte(void)
{
    uint8_t value = *g_song_data++;

    if(g_song_data == (void*)(DATA_SEGMENT_BASE + DATA_SEGMENT_SIZE))
    {
        g_song_data = (void*)DATA_SEGMENT_BASE;
        g_roboplay_interface->set_segment(g_segment_list[++g_segment_index]);
    }

    return value;
}

void handle_dro1_update(void)
{
    uint8_t data_value;

    uint8_t register_value = get_next_data_byte();
    g_index_pointer++;

    switch(register_value)
    {
        case 0x00:
            g_delay_counter = get_next_data_byte();
            g_index_pointer++;
            break;
        case 0x01:
            g_delay_counter = get_next_data_byte();
            g_index_pointer++;
            g_delay_counter += (256 * get_next_data_byte());
            g_index_pointer++;
            break;
        case 0x02:
            g_high_opl_chip = false;
            break;
        case 0x03:
            g_high_opl_chip = true;
            break;
        case 0x04:
            register_value = get_next_data_byte();
            g_index_pointer++;
        default:
            data_value = get_next_data_byte();
            g_index_pointer++;

            if(g_high_opl_chip)
                g_roboplay_interface->opl4_write_fm_2(register_value, data_value);
            else
                g_roboplay_interface->opl4_write_fm_1(register_value, data_value);
    }
}

void handle_dro2_update(void)
{
    uint8_t register_index = get_next_data_byte();
    uint8_t value = get_next_data_byte();

    if(register_index == g_dro2_header.short_delay_code)
    {
        g_delay_counter = value + 1;
    }
    else if(register_index == g_dro2_header.long_delay_code)
    {
        g_delay_counter = (value + 1) << 8;
    }
    else
    {
        uint8_t actual_register;

        if(register_index & 0x80)
        {
            register_index &= 0x7f;
            actual_register = g_code_map[register_index];
            g_roboplay_interface->opl4_write_fm_2(actual_register, value);
        }
        else
        {
            actual_register = g_code_map[register_index];
            g_roboplay_interface->opl4_write_fm_1(actual_register, value);
        }
    }
}
