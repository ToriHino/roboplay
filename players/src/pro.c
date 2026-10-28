/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * pro.c
 *
 * PRO: Pro-Tracker player
 */

#include <string.h>

#include "player_interface.h"
#include "pro.h"

bool load(const char* file_name)
{
    g_roboplay_interface->open(file_name, false);

    g_roboplay_interface->read(&g_pro_header, sizeof(PRO_HEADER));
    if(strncmp(g_pro_header.signature, "PT10", sizeof(g_pro_header.signature)))
    {
        g_roboplay_interface->close();
        return false;
    }

    g_roboplay_interface->read(g_pattern_list, g_pro_header.song_length);
    g_roboplay_interface->read((uint8_t *)DATA_SEGMENT_BASE, DATA_SEGMENT_SIZE);
    g_roboplay_interface->close();

    strncpy(g_song_name, g_pro_header.song_name, SONG_NAME_LENGTH);
    g_song_name[SONG_NAME_LENGTH] = '\0';

    strncpy(g_author, g_pro_header.author, AUTHOR_LENGTH);
    g_author[AUTHOR_LENGTH] = '\0';

    return true;
}

bool update(void)
{
    handle_slide();

    g_speed_count++;
    if(g_speed_count >= g_speed)
    {
        g_speed_count = 0;

        for(uint8_t i = 0; i < NR_OF_FM_CHANNELS; i++)
            g_channel_data[i].slide_data.type = SLIDE_TYPE_OFF;

        unpack_line();

        g_line_count++;
        if(g_line_count >= NR_OF_LINES)
        {
            if(!next_pattern()) return false;
        }

        for(uint8_t i = 0; i < NR_OF_FM_CHANNELS; i++)
        {
            if(g_line_buffer[i])
                play_data(i, g_line_buffer[i]);
        }

        uint8_t command    = g_line_buffer[6] & 0x0F;
        uint8_t drum_value = (g_line_buffer[6] >> 4) & 0x0F;

        uint8_t value = g_line_buffer[7];
        switch(command)
        {
            case 0x0F:
                set_speed(value);
                break;
            case 0x0E:  /* Timer byte */
                break;
            case 0x0D:
                set_drum_data(value);
                break;
            case 0x0C:  /* CAPS led */
                break;
            case 0x0B:
                g_pattern_index = value - 1;
                next_pattern();
                break;
            case 0x0A:
                next_pattern();
                break;
            case 0x09:
                if(value > 0x0F)
                    g_transpose = ~(value & 0x0F) + 1;
                else
                    g_transpose = value &0x0F;
                break;
            default:
                set_original_audio_instrument_register(command - 1, value);
        }

        if(drum_value) play_drums(drum_value);
    }

    update_registers();

    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    g_speed = g_pro_header.speed;
    g_speed_count = g_speed;

    g_transpose = 0;

    g_pattern_data = (uint8_t *)DATA_SEGMENT_BASE;
    g_pattern_index = 0xFF;

    /* Select first pattern */
    next_pattern();

    /* Set initial instruments */
    for(uint8_t i = 0; i < NR_OF_FM_CHANNELS; i++)
    {
        set_instrument_data(i, g_pro_header.start_instruments[i]);
        g_channel_data[i].slide_data.type = SLIDE_TYPE_OFF;

        g_channel_data[i].flip_data.counter = 0;
        g_channel_data[i].flip_data.value = 0;

        g_channel_data[i].pitch_value = 0;
    }

    /* Set initial register values */
    update_registers();

    /* Set drum settings */
    set_drum_data(g_pro_header.drum_volume);
    set_drum_frequencies();
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
    return "Pro-Tracker player V1.0 by RoboSoft Inc.";
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
    return "-";
}

bool next_pattern(void)
{
    g_pattern_index++;
    if(g_pattern_index >= g_pro_header.song_length) 
        return false;

    g_pattern_data = (uint8_t *)DATA_SEGMENT_BASE;
    for(uint8_t i = 0; i < g_pattern_list[g_pattern_index]; i++)
    {
        uint16_t offset = *g_pattern_data++ + (256 * *g_pattern_data++);

        g_pattern_data += offset;
    }

    /* Skip pattern size */
    g_pattern_data += 2;

    g_toggle = false;
    g_count = 0;
    g_line_count = 0;

    return true;
}

void unpack_line(void)
{
    uint8_t index = 0;
    do
    {
        if(!g_count)
        {
            g_toggle = (g_toggle) ? false : true;
            g_count = *g_pattern_data++;
        }

        if(g_count)
        {
            if(g_toggle)
            {
                /* Data bytes */
                do
                {
                    g_line_buffer[index++] = *g_pattern_data++;
                    g_count--;
                } while(g_count && (index < LINE_SIZE));
            }
            else
            {
                /* Zero bytes */
                do
                {
                    g_line_buffer[index++] = 0x00;
                    g_count--;
                } while(g_count && (index < LINE_SIZE));
            }
        }
    } while(index < LINE_SIZE);
}

void set_speed(uint8_t value)
{
    if(value)
    {
        uint8_t speed = (value /6);
        if(!speed) speed++;
        g_speed = (value - speed) + 1;
        g_speed_count = 0;
    }
}

void handle_slide(void)
{
    for(uint8_t i = 0; i < NR_OF_FM_CHANNELS; i++)
    {
        if(g_channel_data[i].slide_data.type != SLIDE_TYPE_OFF)
        {
            uint16_t frequency = g_channel_data[i].frequency.low + (256 * g_channel_data[i].frequency.high);

            if(g_channel_data[i].slide_data.type == SLIDE_TYPE_UP)
            {
                frequency += g_channel_data[i].slide_data.value;

                if(g_channel_data[i].slide_data.remainder)
                {
                    g_channel_data[i].slide_data.remainder--;
                    frequency ++;
                }
            }
            else if(g_channel_data[i].slide_data.type == SLIDE_TYPE_DOWN)
            {
                frequency -= g_channel_data[i].slide_data.value;

                if(g_channel_data[i].slide_data.remainder)
                {
                    g_channel_data[i].slide_data.remainder--;
                    frequency --;
                }
            }

            g_channel_data[i].frequency.low = frequency & 0xFF;
            g_channel_data[i].frequency.high = (frequency >> 8);

            frequency &= 0x01FF;
            if(frequency < 0xAD)
            {
                frequency = 0xAD - frequency;
                frequency = 0x015A - frequency;

                uint8_t high = ((g_channel_data[i].frequency.high >> 1) - 1) & 0x07;
                high = (high << 1) + (frequency >> 8);

                g_channel_data[i].frequency.low = frequency & 0xFF;
                g_channel_data[i].frequency.high = (g_channel_data[i].frequency.high & 0x10) + high;
            }
            else if(frequency > 0x015A)
            {
                frequency -= 0x15B;
                frequency += 0xAD;

                uint8_t high = ((g_channel_data[i].frequency.high >> 1) + 1) & 0x07;
                high = (high << 1) + (frequency >> 8);

                g_channel_data[i].frequency.low = frequency & 0xFF;
                g_channel_data[i].frequency.high = (g_channel_data[i].frequency.high & 0x10) + high;
            }
        }
    }
}

void update_registers(void)
{
    for(uint8_t i = 0; i < NR_OF_FM_CHANNELS; i++)
    {
        if(g_channel_data[i].instrument_data.update)
        {
            g_channel_data[i].instrument_data.update = false;
            activate_instrument(i);
        }
    }

    for(uint8_t i = 0; i < NR_OF_FM_CHANNELS; i++)
    {
        uint16_t frequency = g_channel_data[i].frequency.low + (256 * g_channel_data[i].frequency.high);

        if(g_channel_data[i].pitch_value > 0x07)
            frequency -= g_channel_data[i].pitch_value & 0x07;
        else
            frequency += g_channel_data[i].pitch_value & 0x07;

        g_channel_data[i].flip_data.counter++;
        if(g_channel_data[i].flip_data.counter & 0x04)
        {
            if(g_channel_data[i].flip_data.value > 0x07)
                frequency -= g_channel_data[i].flip_data.value & 0x07;
            else
                frequency += g_channel_data[i].flip_data.value & 0x07;           
        }

        frequency <<= 1;

        g_roboplay_interface->opl4_write_fm_1(0xA0 + i, frequency & 0xFF);
        g_roboplay_interface->opl4_write_fm_1(0xB0 + i, (frequency >> 8));

        frequency--;
        g_roboplay_interface->opl4_write_fm_2(0xA0 + i, frequency & 0xFF);
        g_roboplay_interface->opl4_write_fm_2(0xB0 + i, (frequency >> 8));
    }
}

void activate_instrument(uint8_t channel)
{
    uint8_t instrument = g_channel_data[channel].instrument_data.instrument;
    uint8_t volume     = g_volume_table[g_channel_data[channel].instrument_data.volume];

    uint8_t value;
    if(instrument == 0)
    {
        for(uint8_t i = 0; i < INSTRUMENT_LENGTH; i++)
        {
            if(i == 3)
                value = (g_original_instrument_audio[i] & 0xC0) | volume;
            else
                value = g_original_instrument_audio[i];

            g_roboplay_interface->opl4_write_fm_1(g_voice_registers[channel][i], value);
            g_roboplay_interface->opl4_write_fm_2(g_voice_registers[channel][i], value);
        }

        value = g_original_instrument_audio[INSTRUMENT_LENGTH];
        g_roboplay_interface->opl4_write_fm_1(g_voice_registers[channel][INSTRUMENT_LENGTH], value | PAN_SETTING_LEFT);
        g_roboplay_interface->opl4_write_fm_2(g_voice_registers[channel][INSTRUMENT_LENGTH], value | PAN_SETTING_RIGHT);
    }
    else
    {
        instrument--;
        for(uint8_t i = 0; i < INSTRUMENT_LENGTH; i++)
        {
            if(i == 3)
                value = (g_voice_patches[instrument][i] & 0xC0) | volume;
            else
                value = g_voice_patches[instrument][i];

            g_roboplay_interface->opl4_write_fm_1(g_voice_registers[channel][i], value);
            g_roboplay_interface->opl4_write_fm_2(g_voice_registers[channel][i], value);
        }

        value = (g_voice_patches[instrument][3] & 0x07) << 1;
        g_roboplay_interface->opl4_write_fm_1(g_voice_registers[channel][INSTRUMENT_LENGTH], value | PAN_SETTING_LEFT);
        g_roboplay_interface->opl4_write_fm_2(g_voice_registers[channel][INSTRUMENT_LENGTH], value | PAN_SETTING_RIGHT);
    }
}

void set_instrument_data(uint8_t channel, uint8_t instrument)
{
    g_channel_data[channel].instrument_data.update = true;
    note_off(channel);

    uint8_t volume = g_pro_header.instrument_volumes[instrument - 1];
    g_channel_data[channel].instrument_data.volume = volume;

    if(instrument > NR_OF_VOICE_PATCHES)
    {
        g_channel_data[channel].instrument_data.instrument = 0;
        g_channel_data[channel].instrument_data.volume = volume;

        instrument -= (NR_OF_VOICE_PATCHES + 1);
        for(uint8_t i = 0; i < INSTRUMENT_LENGTH; i++)
            g_original_instrument[i] = g_pro_header.original_instruments[instrument][i];

        set_original_audio_instrument_data();
    }
    else
    {
        g_channel_data[channel].instrument_data.instrument = instrument;
    }
}

void set_original_audio_instrument_data(void)
{
    for(uint8_t i = 0; i < INSTRUMENT_LENGTH; i++)
        g_original_instrument_audio[i] = g_original_instrument[i];

    g_original_instrument_audio[INSTRUMENT_LENGTH] = (g_original_instrument[3] & 0x07) << 1;

    for(uint8_t i = 0; i < NR_OF_FM_CHANNELS; i++)
        if(g_channel_data[i].instrument_data.instrument == 0)
            g_channel_data[i].instrument_data.update = true;
}

void set_original_audio_instrument_register(uint8_t index, uint8_t value)
{
    g_original_instrument[index] = value;
    set_original_audio_instrument_data();
}

void set_drum_data(uint8_t volume)
{
    g_roboplay_interface->opl4_write_fm_1(0xBD, 0x00);
    for(uint8_t i = 0; i < NR_OF_DRUM_CHANNELS; i++)
    {
        for(uint8_t j = 0; j < INSTRUMENT_LENGTH + 1; j++)
        {
            uint8_t value = g_drum_patches[i][j];
            if((j == 2 && i != 0) || j == 3) value = (value & 0xC0) | g_volume_table[volume];
            g_roboplay_interface->opl4_write_fm_1(g_drum_registers[i][j], value);
        }
    }
}

void set_drum_frequencies(void)
{
    uint8_t index = 0;
    for(uint8_t i = 0; i < NR_OF_DRUM_CHANNELS; i++)
    {
        uint16_t frequency = g_pro_header.drum_frequencies[index++] + (256 * g_pro_header.drum_frequencies[index++]);
        if(i != 0) frequency <<= 1;

        g_roboplay_interface->opl4_write_fm_1(0xA6 + i, frequency & 0xFF);
        g_roboplay_interface->opl4_write_fm_1(0xB6 + i, (frequency >> 8));
    }
}

void play_data(uint8_t channel, uint8_t data)
{
    if(data <= NOTE_ON) 
        note_on(channel, data);
    else if(data < NOTE_OFF) 
        change_instrument(channel, data - (INSTRUMENT_CHANGE - 1));
    else if(data == NOTE_OFF) 
        note_off(channel);
    else if(data < RELEASE) 
        slide_up(channel, data & 0x0F);
    else if(data == RELEASE) 
        note_off(channel);
    else if(data < VOLUME_CHANGE)
        slide_down(channel, data & 0x0F);
    else if(data < FLIP) 
        change_volume(channel, data & 0x0F);
    else if(data < PITCH)
        flip(channel, data);
    else 
        pitch(channel, data);
}

void play_drums(uint8_t data)
{
    g_roboplay_interface->opl4_write_fm_1(0xBD, 0x00);
    g_roboplay_interface->opl4_write_fm_1(0xBD, g_drums_table[data - 1]);
}

void stop_note(uint8_t channel)
{
    note_off(channel);
    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, g_channel_data[channel].frequency.high);
    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, g_channel_data[channel].frequency.high);
}

void note_on(uint8_t channel, uint8_t note)
{
    stop_note(channel);

    note--;
    note += g_transpose;

    while(note > NOTE_ON) note -= NOTE_ON;

    uint16_t frequency = g_frequency_table[note].low + (256 * g_frequency_table[note].high);

    g_channel_data[channel].note = note;
    g_channel_data[channel].frequency.low = frequency & 0xFF;
    g_channel_data[channel].frequency.high = (frequency >> 8) | 0x10;
}

void note_off(uint8_t channel)
{
    g_channel_data[channel].frequency.high &= 0xEF;
}

void change_volume(uint8_t channel, uint8_t value)
{
    g_channel_data[channel].instrument_data.update = true;
    g_channel_data[channel].instrument_data.volume = value;
}

void change_instrument(uint8_t channel, uint8_t value)
{
    stop_note(channel);
    set_instrument_data(channel, value);
}

void slide_up(uint8_t channel, uint8_t value)
{
    g_channel_data[channel].slide_data.type = SLIDE_TYPE_UP;

    uint8_t note     = g_channel_data[channel].note;
    uint8_t new_note = (note + value < NOTE_ON) ? note + value : NOTE_ON - 1;

    uint16_t frequency     = g_frequency_table[note].low + (256 * g_frequency_table[note].high);
    uint16_t new_frequency = g_frequency_table[new_note].low + (256 * g_frequency_table[new_note].high);
    uint16_t delta          = new_frequency - frequency;

    uint8_t octave_offset = (g_frequency_table[new_note].high >> 1) - (g_frequency_table[note].high >> 1);

    delta -= (octave_offset * 0x0153);

    g_channel_data[channel].slide_data.value = delta / g_speed;
    g_channel_data[channel].slide_data.remainder = delta % g_speed;

    g_channel_data[channel].note = new_note;
}

void slide_down(uint8_t channel, uint8_t value)
{
    g_channel_data[channel].slide_data.type = SLIDE_TYPE_DOWN;

    uint8_t note     = g_channel_data[channel].note;
    uint8_t new_note = (note > value) ? note - value : 0;

    uint16_t frequency     = g_frequency_table[note].low + (256 * g_frequency_table[note].high);
    uint16_t new_frequency = g_frequency_table[new_note].low + (256 * g_frequency_table[new_note].high);
    uint16_t delta          = frequency - new_frequency;

    uint8_t octave_offset = (g_frequency_table[note].high >> 1) - (g_frequency_table[new_note].high >> 1);

    delta -= (octave_offset * 0x0153);

    g_channel_data[channel].slide_data.value = delta / g_speed;
    g_channel_data[channel].slide_data.remainder = delta % g_speed;

    g_channel_data[channel].note = new_note;
}

void flip(uint8_t channel, uint8_t value)
{
    g_channel_data[channel].flip_data.value = value & 0x0F;
}

void pitch(uint8_t channel, uint8_t value)
{
    g_channel_data[channel].pitch_value = value & 0x0F;
}
