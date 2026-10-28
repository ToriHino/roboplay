/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * tfc.c
 *
 * TFC: TurboFM Compiled
 */

#include <string.h>

#include "support/inc/opm_frequency2key.h"

#include "player_interface.h"
#include "tfc.h"

#define GET_UINT8_T       g_data[cursor++]
#define GET_UINT16_T      256 * g_data[cursor++] + g_data[cursor++]

bool load(char *const file_name)
{  
    g_roboplay_interface->open(file_name, false);

    uint8_t *destination = (uint8_t *)DATA_SEGMENT_BASE;
    g_roboplay_interface->read(destination, DATA_SEGMENT_SIZE);

    g_roboplay_interface->close();

    g_tfc_header = (TFC_HEADER *)DATA_SEGMENT_BASE;
    g_data       = (uint8_t *)DATA_SEGMENT_BASE;

    /* Check song signature */
    const uint8_t signature[] = { {'T', 'F', 'M', 'c', 'o', 'm'} };
    for(uint8_t i = 0; i < sizeof(signature); i++)
    {
        if(g_tfc_header->signature[i] != signature[i]) return false;
    }

    /* Get Title, Author and comment strings */
    g_title  = (char *)DATA_SEGMENT_BASE + sizeof(TFC_HEADER);
    g_author = g_title;
    while(*g_author) g_author++;
    g_author ++;
    g_comment = g_author;
    while(*g_comment) g_comment++;
    g_comment++;

    /* Move Log2 table to aligned address */
    memcpy((uint8_t *)READ_BUFFER, g_log2_table, sizeof(g_log2_table));

    return true;
}

bool update(void)
{
    for(g_channel = 0; g_channel < NR_OF_CHANNELS; g_channel++)
    {
        g_channel_offset = (g_channel < CHANNEL_SPLIT) ? 0 : CHANNEL_SPLIT;

        if(g_skip[g_channel]) g_skip[g_channel]--;

        if(!g_skip[g_channel])
        {
            if(g_cursor[g_channel] = parse_frame_control(g_cursor[g_channel]))
            {
                /* Parse frame commands for one channel */
                g_cursor[g_channel] = parse_frame_commands(g_cursor[g_channel]);
            }
            else
            {
                g_cursor[g_channel] = g_loop[g_channel];
            }
        }
    }
    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
    {
        g_cursor[i] = (uint16_t)g_tfc_header->offsets[i];
        g_frequency[i] = 0x0;
        g_skip[i] = 0x0;

        g_repeat_frames[i] = 0;
        g_return_address[i] = 0;

        g_loop[i] = g_cursor[i];
    }

    for(uint8_t i = 0; i < NR_OF_VOICES; i++)
    {
        g_rl_fb_connect[i] = 0;
        g_frequency_msb[i] = 0;
    }

    g_channel_offset = CHANNEL_SPLIT;
    set_register(0xB4, 0x40);                     /* Enable L/R */
    set_register(0xB5, 0x80);                     /* Enable L/R */
    set_register(0xB6, 0x40);                     /* Enable L/R */

    g_channel_offset = 0x0;
    set_register(0xB4, 0xC0);                     /* Enable L/R */
    set_register(0xB5, 0xC0);                     /* Enable L/R */
    set_register(0xB6, 0x80);                     /* Enable L/R */

    g_roboplay_interface->opm_write(0x19, 0x10);    /* AMD, rescales AMS */
    g_roboplay_interface->opm_write(0x19, 0xA8);    /* PMD, rescales PMS */
    g_roboplay_interface->opm_write(0x1B, 0x02);    /* W, triangle LFO waveform */
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    return g_tfc_header->frequency;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "TurboFM Compiled player V1.0 by RoboSoft Inc.";
}

char* get_title(void)
{
    return g_title;
}

char* get_author(void)
{
    return g_author;
}

char* get_description(void)
{
    return g_comment;
}

void set_register(uint8_t reg, uint8_t val)
{
    if(reg == 0x22)
    {
        g_roboplay_interface->opm_write(0x18, g_lfo_lut[val]);
        g_roboplay_interface->opm_write(0x08, (val & 0x07) | ((val & 0xF0) >> 1) );
    }
    if(reg == 0x28)
    {
        g_roboplay_interface->opm_write(0x08, (val & 0x07) | ((val & 0xF0) >> 1) );   
    }
    else if(reg < 0x30) return;
    else if(reg < 0x90) write_slots(reg, val);
    else if(reg >= 0xA0) write_channels(reg, val);
}

void write_slots(uint8_t reg, uint8_t val)
{
    uint8_t opm_reg = ((((reg - 0x30) & 0xFC) << 1) | (reg & 0x03)) + 0x40;

    g_roboplay_interface->opm_write( opm_reg + g_channel_offset, val );
}

void write_channels(uint8_t reg, uint8_t val)
{
    if(reg < 0xA4)
    {
        opm_frequency2key(g_frequency_msb[reg - 0xA0 + g_channel_offset], val);

        g_roboplay_interface->opm_write(reg - 0xA0 + 0x30 + g_channel_offset, g_key_fraction);
        g_roboplay_interface->opm_write(reg - 0xA0 + 0x28 + g_channel_offset, g_key_code);
    } 
    else if(reg < 0xA8) 
    {       
        g_frequency_msb[reg - 0xA4 + g_channel_offset] = val;
    }
    else if(reg < 0xB0) return;
    else if(reg < 0xB4)
    {
       /* Write FB connect */
       g_rl_fb_connect[reg - 0xB0 + g_channel_offset] = (g_rl_fb_connect[reg - 0xB0 + g_channel_offset] & 0xC0) | (val &0x3F);

       g_roboplay_interface->opm_write(reg - 0xB0 + 0x20 + g_channel_offset, g_rl_fb_connect[reg - 0xB0 + g_channel_offset]);
    }
    else if(reg < 0xB8)
    {
       /* Write PMS, AMS, LR */
       g_rl_fb_connect[reg - 0xB4 + g_channel_offset] = g_rl_fb_connect[reg - 0xB4 + g_channel_offset] & 0x3F;
  
       if(val & 0x40) g_rl_fb_connect[reg - 0xB4 + g_channel_offset] = g_rl_fb_connect[reg - 0xB4 + g_channel_offset] | 0x80;
       if(val & 0x80) g_rl_fb_connect[reg - 0xB4 + g_channel_offset] = g_rl_fb_connect[reg - 0xB4 + g_channel_offset] | 0x40;

       g_roboplay_interface->opm_write(reg - 0xB4 + 0x38 + g_channel_offset, val << 4);
       g_roboplay_interface->opm_write(reg - 0xB4 + 0x20 + g_channel_offset, g_rl_fb_connect[reg - 0xB4 + g_channel_offset]);
    }
}

uint16_t parse_frame_control(uint16_t cursor)
{
    /* Handle repeat frames */
    if(g_repeat_frames[g_channel] && !--g_repeat_frames[g_channel])
    {
        cursor = g_return_address[g_channel];
        g_return_address[g_channel] = 0;
    }

    for(;;)
    {
        uint8_t cmd = GET_UINT8_T;

        if(cmd == 0x7F)         /* %01111111 */
            return 0;
        else if(cmd == 0x7E)    /* %01111110 */
        {
            /* SetLoop */
            g_loop[g_channel] = cursor;
            continue;
        }
        else if(cmd == 0xD0)    /* %11010000 */
        {
            /* Repeat frames */
            g_repeat_frames[g_channel] = GET_UINT8_T;
            const int16_t offset = GET_UINT16_T;
            g_return_address[g_channel] = cursor;

            return cursor + offset;
        }
        else
        {
            cursor--;
            return cursor;
        }
    }
}

uint16_t parse_frame_commands(uint16_t cursor)
{
    uint8_t cmd = GET_UINT8_T;

    if(cmd == 0xBF)         /* %10111111 */
    {
        int16_t offset = GET_UINT16_T;
        parse_frame_data(cursor + offset);
    }
    else if(cmd == 0xFF)    /* %11111111 */
    {
        uint8_t val = GET_UINT8_T;
        int16_t offset1 = -256 + val;

        parse_frame_data(cursor + offset1);
    }
    else if(cmd >= 0xE0)    /* %111ttttt */
    {
        g_skip[g_channel] = 256 - cmd;
    }
    else if(cmd >= 0xC0)    /* %110ddddd */
    {
        set_slide(cmd + 0x30);
    }
    else cursor = parse_frame_data(cursor - 1);

    return cursor;
}

uint16_t parse_frame_data(uint16_t cursor)
{
    uint8_t data = GET_UINT8_T;

    if((data & 0xC0) != 0) set_key_off();

    if((data & 0x01) != 0)
    {
        const uint16_t frequency = GET_UINT16_T;
        set_frequency(frequency);
    }

    uint8_t regs = (data & 0x3E) >> 1;
    if(regs)
    {
        for(uint8_t i = 0; i < regs; i++)
        {
            uint8_t reg = GET_UINT8_T;
            uint8_t val = GET_UINT8_T;
            set_register(reg, val);
        }
    }

    if((data & 0x80) != 0) set_key_on();

    return cursor;
}

void set_slide(uint8_t slide)
{
    const uint16_t new_frequency = (g_frequency[g_channel] & 0xFF00) | ((g_frequency[g_channel] + slide) & 0xFF);
    set_frequency(new_frequency);
}

void set_frequency(uint16_t frequency)
{
    g_frequency[g_channel] = frequency;
    set_register(0xA4 + g_channel - g_channel_offset, frequency >> 8);
    set_register(0xA0 + g_channel - g_channel_offset, frequency & 0xFF);
}

void set_key_off(void)
{
    set_register(0x28, g_channel);
}

void set_key_on(void)
{
    set_register(0x28, 0xF0 | g_channel);
}