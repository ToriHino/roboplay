/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * sbm.c
 *
 * SBM: SCC Blaffer NT
 */

#include <string.h>

#include "player_interface.h"
#include "sbm.h"

bool load(const char* file_name)
{
    g_roboplay_interface->open(file_name, false);

    char signature[16];
    g_roboplay_interface->read(&signature, sizeof(signature));
    if(strncmp(signature, "Blaf NT Song    ", sizeof(signature)))
    {
        g_roboplay_interface->close();
        return false;
    }

    g_sbm_header = (SBM_HEADER *)DATA_SEGMENT_BASE;
    g_roboplay_interface->read(g_sbm_header, sizeof(SBM_HEADER));

    strncpy(g_song_name, g_sbm_header->song_name, SBM_SONG_NAME_LENGTH);
    g_song_name[SBM_SONG_NAME_LENGTH] = '\0';

    uint8_t number_of_patterns_stored;
    g_roboplay_interface->read(&number_of_patterns_stored, sizeof(number_of_patterns_stored));

    for(uint8_t i = 0; i < number_of_patterns_stored; i++)
    {
        uint8_t *pattern_address;
        g_roboplay_interface->read(&pattern_address, sizeof(pattern_address));
        g_roboplay_interface->read(pattern_address, SBM_PATTERN_SIZE);
    }

    g_roboplay_interface->close();

    strcpy(g_instrument_kit_name, file_name);
    uint8_t found = strlen(g_instrument_kit_name) - 1;
    while(g_instrument_kit_name[found] != '\\' && g_instrument_kit_name[found] != ':' && found > 0) found--;
    if(found) found++;

    uint8_t i;
    for(i = 0; i < 8; i++)
    {
        if(g_sbm_header->instrument_kit_name[i] == ' ') break;
        g_instrument_kit_name[found + i] = g_sbm_header->instrument_kit_name[i];
    }
    g_instrument_kit_name[found + i++]='.';
    strncpy(&g_instrument_kit_name[found + i], g_sbm_header->instrument_kit_ext, 3);
    g_instrument_kit_name[found + i +3]='\0';

    g_roboplay_interface->open(g_instrument_kit_name, false);

    g_roboplay_interface->read(&signature, sizeof(signature));
    if(strncmp(signature, "Blaf NT Ins Kit ", sizeof(signature)))
    {
        g_roboplay_interface->close();
        return false;
    }

    g_instrument_segment = g_roboplay_interface->get_new_segment();
    g_roboplay_interface->set_segment(g_instrument_segment);

    uint16_t data_size = DATA_SEGMENT_SIZE;
    uint8_t* destination = (uint8_t*)DATA_SEGMENT_BASE;
    while(data_size > 0)
    {
        /* It's not possible to read directly to non-primary mapper memory segments,
           so use a buffer inbetween. */
        uint16_t bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, READ_BUFFER_SIZE);
        if(bytes_read == 0) break;

        memcpy(destination, (void*)READ_BUFFER, bytes_read);

        data_size -= bytes_read;
        destination += bytes_read;
    }

    g_roboplay_interface->close();

    g_roboplay_interface->set_segment(START_SEGMENT_INDEX);

    return true;
}

bool update(void)
{
    if(g_tempo_counter == 0)
    {
        g_tempo_counter = g_song_tempo;

        if(g_data_block.last_row_played || ++g_song_row == SBM_NR_OF_ROWS)
        {
            if(!new_position()) return false;
        }

        /* Clear pitch bend 2 data */
        for(uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS; i++)
        {
            if(g_data_block.pitch[i].type == 2)
                g_data_block.pitch[i].active = false;
        }

        /* Check command channel */
        do_command_channel();

        /* Do all auto slides */
        do_auto_slides();

        /* Play new row */
        play_row();
    }
    else
    {
        g_tempo_counter--;
    }

    /* Out all volume slides */
    do_volume_slides();

    /* Pitch slide 1 and 2 */
    do_pitch_slides();

    /* Modulate */
    do_modulate();

    /* Out all volumes and frequencies to the SCC */
    for(uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS; i++)
    {
        if(g_song_frequencies[i] != g_data_block.last_frequencies[i])
        {
            g_data_block.last_frequencies[i] = g_song_frequencies[i];
            g_roboplay_interface->scc_set_frequency(i, g_song_frequencies[i]);
        }

        if(g_song_volumes[i] != g_data_block.last_volumes[i])
        {
            g_data_block.last_volumes[i] = g_song_volumes[i];
            g_roboplay_interface->scc_set_volume(i, g_song_volumes[i]);
        }
    }

    /* Handle PSG */
    do_psg();

    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    g_song_patterns   = (SBM_PATTERN *)SBM_PATTERN_DATA;
    g_scc_instruments = (SBK_SCC_INSTRUMENT *)SBK_SCC_INSTRUMENTS;
    g_psg_instruments = (SBK_PSG_INSTRUMENT *)SBK_PSG_INSTRUMENTS;

    g_song_position = 255;
    g_last_position_done = false;
    g_song_row = SBM_NR_OF_ROWS - 1;

    g_song_tempo = 26 - g_sbm_header->tempo;
    g_tempo_counter = 0;

    g_psg_active = false;
    g_psg_max_volume = SBM_PSG_MAX_VOLUME;

    memset(&g_data_block, 0, sizeof(SBM_DATABLOCK));
    memset(&g_song_frequencies, 0, sizeof(g_song_frequencies));

    for(uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS; i++)
        g_roboplay_interface->scc_set_frequency(i, g_song_frequencies[i]);

    g_data_block.modulate_counter = 3;

    memcpy(g_data_block.last_instruments, g_sbm_header->instruments, SBM_NR_OF_SCC_CHANNELS - 1);
    memcpy(g_song_volumes, g_sbm_header->volumes, SBM_NR_OF_SCC_CHANNELS);
    memcpy(g_data_block.auto_volume_slide, g_sbm_header->auto_volume_slide, SBM_NR_OF_SCC_CHANNELS);
    memcpy(g_detune, g_sbm_header->detune, SBM_NR_OF_SCC_CHANNELS);

    g_roboplay_interface->set_segment(g_instrument_segment);
    for(uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS - 1; i++)
    {
        g_roboplay_interface->scc_set_waveform(i, g_scc_instruments[g_data_block.last_instruments[i]].data);
    }
    g_roboplay_interface->set_segment(START_SEGMENT_INDEX);
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
    return "SCC Blaffer NT player V1.0 by RoboSoft Inc.";
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
    return "-";
}

bool new_position(void)
{
    g_data_block.last_row_played = false;
    g_song_row = 0;       

    if(g_last_position_done)
    {
        if(g_sbm_header->loop != 0) return false;
        g_last_position_done = false;
        g_song_position = g_sbm_header->loop_position - 1;
    }

    g_song_position++;
    if(g_song_position == g_sbm_header->last_position) g_last_position_done = true;

    return true;
}

void end_of_pattern(void)
{
    g_data_block.last_row_played = true;
}

void do_command_channel(void)
{
    uint8_t pattern = g_sbm_header->patterns[g_song_position];
    SBM_ROW *row = &g_song_patterns[pattern].row[g_song_row];

    if(row->command)
    {
        if(row->command < SBM_MAX_TEMPO) change_tempo(row->command);
        else if(row->command == 26) end_of_pattern();
        else if(row->command < 40) change_transpose(row->command);
        else g_psg_max_volume = row->command - 40;
    }

}

void do_auto_slides(void)
{
    for (uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS; i++)
    {
        if(g_data_block.auto_volume_slide[i] == 0x10)
            g_data_block.volume_slide[i].active = false;
        else
        {
            g_data_block.volume_slide[i].active = true;

            uint8_t value = g_data_block.auto_volume_slide[i] & 0x1F;

            if(value <= 0x10)
            {
                g_data_block.volume_slide[i].direction = SBM_DIRECTION_DOWN;
                value = 0x10 - value;
            }
            else
                g_data_block.volume_slide[i].direction = SBM_DIRECTION_UP;

            value &= 0x0F;
            g_data_block.volume_slide[i].value = 1 + (value / g_song_tempo + 1);
            g_data_block.volume_slide[i].counter = value;
        }
    }
}

void play_row(void)
{
    uint8_t pattern = g_sbm_header->patterns[g_song_position];
    SBM_ROW *row = &g_song_patterns[pattern].row[g_song_row];

    /* Handle SCC */
    for(uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS; i++)
    {
        uint8_t command = row->scc_data[i].command;
        uint8_t value   = row->scc_data[i].value;

        if(command == SBM_COMMAND_CHANGE_VOLUME) change_volume(i, value);
        else if(command < SBM_COMMAND_CHANGE_FREQUENCY) change_frequency(i, command, value);
        else if(command == SBM_COMMAND_NOTE_OFF) note_off(i);
        else if(command == SBM_COMMAND_CHANGE_INSTRUMENT) change_instrument(i, value);
        else if(command == SBM_COMMAND_PITCH_1_UP) pitch_1_up(i, value);
        else if(command == SBM_COMMAND_PITCH_1_DOWN) pitch_1_down(i, value);
        else if(command == SBM_COMMAND_PITCH_2_UP) pitch_2_up(i, value);
        else if(command == SBM_COMMAND_PITCH_2_DOWN) pitch_2_down(i, value);
        else if(command == SBM_COMMAND_MODULATE_UP) modulate_up(i, value);
        else if(command == SBM_COMMAND_MODULATE_DOWN) modulate_down(i, value);
        else if(command == SBM_COMMAND_DETUNE_UP) detune_up(i, value);
        else if(command == SBM_COMMAND_DETUNE_DOWN) detune_down(i, value);
        else if(command == SBM_COMMAND_VOLUME_SLIDE_UP) volume_slide_up(i, value);
        else if(command == SBM_COMMAND_VOLUME_SLIDE_DOWN) volume_slide_down(i, value);
        else if(command == SBM_COMMAND_AUTO_SLIDE_UP) auto_slide_up(i, value);
        else if(command == SBM_COMMAND_AUTO_SLIDE_DOWN) auto_slide_down(i, value);
    }

    /* Handle PSG */
    if(row->psg_data)
    {
        g_psg_active = true;
        g_psg_instrument = row->psg_data - 1;
        g_psg_row = 0;

        g_psg_tempo = SBM_PSG_MAX_TEMPO;
        g_psg_tempo_count = 0;
        g_psg_wait = 0;

        g_psg_number_of_repeats = PSG_NO_REPEAT;        

        for(uint8_t i = 0; i < SBM_NR_OF_PSG_CHANNELS; i++) 
            g_psg_volume[i] = SBM_PSG_MAX_VOLUME;

        g_roboplay_interface->psg_write(7, 0xBF);
        
    }
}

void do_volume_slides(void)
{
    for(uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS; i++)
    {
        if(g_data_block.volume_slide[i].active && (g_data_block.volume_slide[i].counter != 0))
        {
            g_data_block.volume_slide[i].counter--;
            if(g_data_block.volume_slide[i].direction == SBM_DIRECTION_UP)
            {
                if(g_song_volumes[i] + g_data_block.volume_slide[i].value <= SBM_MAX_VOLUME)
                    g_song_volumes[i] += g_data_block.volume_slide[i].value; 
                else
                    g_song_volumes[i] = SBM_MAX_VOLUME;
            }
            else
            {
                if(g_song_volumes[i] > g_data_block.volume_slide[i].value) 
                    g_song_volumes[i] -= g_data_block.volume_slide[i].value;
                else
                    g_song_volumes[i] = 0;
            }
        }
    }
}

void do_pitch_slides(void)
{
    for(uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS; i++)
    {
        if(g_data_block.pitch[i].active)
        {
            if(g_data_block.pitch[i].direction == SBM_DIRECTION_UP)
                g_song_frequencies[i] -= g_data_block.pitch[i].value;
            else
                g_song_frequencies[i] += g_data_block.pitch[i].value;
        }
    }
}

void do_modulate(void)
{
    g_data_block.modulate_counter--;
    if(!g_data_block.modulate_counter)
    {
        g_data_block.modulate_counter = 3;

        for(uint8_t i = 0; i < SBM_NR_OF_SCC_CHANNELS; i++)
        {
            if(g_data_block.modulate[i].active)
            {
                if(g_data_block.modulate[i].direction == SBM_DIRECTION_UP)
                {
                    g_data_block.modulate[i].direction = SBM_DIRECTION_DOWN;
                    g_song_frequencies[i] -= g_data_block.modulate[i].value;
                }
                else
                {
                    g_data_block.modulate[i].direction = SBM_DIRECTION_UP;
                    g_song_frequencies[i] += g_data_block.modulate[i].value;
                }
            }
        }
    }
}

void do_psg(void)
{
    bool end_of_pattern = false;

    if(!g_psg_active) return;

    if(g_psg_tempo_count)
    {
        g_psg_tempo_count--;
        return;
    }

    if(g_psg_wait)
    {
        g_psg_wait--;
        return;
    }

    if(g_psg_row == SBM_PSG_LAST_ROW)
    {
        g_psg_active = false;
        return;
    }

    g_psg_tempo_count = SBM_PSG_MAX_TEMPO - g_psg_tempo;

    g_roboplay_interface->set_segment(g_instrument_segment);

    uint8_t command = g_psg_instruments[g_psg_instrument].data[g_psg_row].command;

    while(command)
    { 
        if(command < PSG_SET_REPEAT)
        {
            if(g_psg_number_of_repeats == PSG_NO_REPEAT)
            {
                g_psg_number_of_repeats = command;
                g_psg_repeat_row = g_psg_row;
            }
            break;
        }
        else if(command == PSG_DO_REPEAT)
        {
            if( g_psg_number_of_repeats != PSG_NO_REPEAT)
            {
                g_psg_number_of_repeats--;
                if(g_psg_number_of_repeats != PSG_NO_REPEAT)
                {
                    g_psg_row = g_psg_repeat_row;
                    command = g_psg_instruments[g_psg_instrument].data[g_psg_row].command;
                }
                else break;
            }
        }
        else if(command < PSG_WAIT)
        {
            g_psg_wait = command - PSG_DO_REPEAT;
            break;
        }
        else if(command == PSG_POINT)
        {
            g_psg_point = g_psg_row;
            break;
        }
        else if(command == PSG_GOTO_POINT)
        {
            g_psg_row = g_psg_point;
            command = g_psg_instruments[g_psg_instrument].data[g_psg_row].command;
        }
        else if(command == PSG_END_PATTERN)
        {
            end_of_pattern = true;
            break;
        }
        else if(command < PSG_NEW_TEMPO)
        {
            g_psg_tempo = command - 29;
            g_psg_tempo_count = SBM_PSG_MAX_TEMPO - g_psg_tempo;
            break;
        }
        else break;
    }

    for(uint8_t i = 0; i < SBM_NR_OF_PSG_CHANNELS; i++)
    {
        /* Frequency */
        uint16_t frequency = g_psg_instruments[g_psg_instrument].data[g_psg_row].frequency[i];

        if(frequency & PSG_FREQUENCY_DOWN)
        {
            uint16_t value = g_roboplay_interface->psg_read(i * 2) + 256 * g_roboplay_interface->psg_read((i * 2)+ 1);
            value = value - (frequency & 0xFF);
            g_roboplay_interface->psg_write((i * 2), value & 0xFF);
            g_roboplay_interface->psg_write((i * 2) + 1, value >> 8);
        }
        else if(frequency & PSG_FREQUENCY_UP)
        {
            uint16_t value = g_roboplay_interface->psg_read(i * 2) + 256 * g_roboplay_interface->psg_read((i * 2)+ 1);
            value = value + (frequency & 0xFF);
            g_roboplay_interface->psg_write((i * 2), value & 0xFF);
            g_roboplay_interface->psg_write((i * 2) + 1, value >> 8);
        }
        else
        {
            uint8_t value = g_roboplay_interface->psg_read(7);

            if(frequency & PSG_FREQUENCY_OFF)
            {
                value = value | (0x01 << i);
            }
            else
            {
                value = value & ~(0x01 << i);
            }

            g_roboplay_interface->psg_write((i * 2), frequency & 0xFF);
            g_roboplay_interface->psg_write((i * 2) + 1, frequency >> 8);
            g_roboplay_interface->psg_write(7, value);
        }
    }

    for(uint8_t i = 0; i < SBM_NR_OF_PSG_CHANNELS; i++)
    {
        uint8_t volume = g_psg_instruments[g_psg_instrument].data[g_psg_row].volume[i];
        if(volume & PSG_NOISE_CHANGE)
        {
            uint8_t value = g_roboplay_interface->psg_read(7);
            value = value & ~(0x08 << i);
            g_roboplay_interface->psg_write(7, value);
        }
        
        volume &= 0x3F;
        if(volume & PSG_VOLUME_DOWN)
        {
            volume &= 0x0F;
            if(g_psg_volume[i] > volume)
                g_psg_volume[i] -= volume;
            else
                g_psg_volume[i] = 0;
        }
        else if(volume & PSG_VOLUME_UP)
        {
            volume &= 0x0F;
            g_psg_volume[i] += volume;
            if(g_psg_volume[i] > SBM_PSG_MAX_VOLUME) g_psg_volume[i] = SBM_PSG_MAX_VOLUME;
        }
        else
        {
            if(volume) g_psg_volume[i] = volume;
        }

        if(g_psg_volume[i] > g_psg_max_volume) g_psg_volume[i] = g_psg_max_volume;
        g_roboplay_interface->psg_write(8 + i, g_psg_volume[i]);
    }

    uint8_t noise_frequency = g_psg_instruments[g_psg_instrument].data[g_psg_row].noise_frequency;
    if(noise_frequency & PSG_NOISE_FREQUENCY_DOWN)
    {
        noise_frequency &= 0x01F;
        uint8_t value = g_roboplay_interface->psg_read(6);
        value -= noise_frequency;
        g_roboplay_interface->psg_write(6, value);
    } else if(noise_frequency & PSG_NOISE_FREQUENCY_UP)
    {
        noise_frequency &= 0x01F;
        uint8_t value = g_roboplay_interface->psg_read(6);
        value += noise_frequency;
        g_roboplay_interface->psg_write(6, value);
    }
    else
    {
        if(noise_frequency) g_roboplay_interface->psg_write(6, noise_frequency);
    }

    g_roboplay_interface->set_segment(START_SEGMENT_INDEX);

    if(end_of_pattern)
        g_psg_row = SBM_PSG_LAST_ROW;
    else
        g_psg_row++;
}

void change_tempo(uint8_t tempo)
{
    g_song_tempo = SBM_MAX_TEMPO - tempo;
    g_tempo_counter =  g_song_tempo;
}

void change_transpose(uint8_t transpose)
{
    g_data_block.transpose = 33 - transpose;
}

void change_volume(uint8_t channel, uint8_t volume)
{
    volume = volume & 0x0F;
    if(volume) g_song_volumes[channel] = volume;
}

void change_frequency(uint8_t channel, uint8_t note, uint8_t volume)
{
    if(!(volume & 0xF0))
    {
        g_data_block.pitch[channel].active = false;
        g_data_block.modulate[channel].active = false;
    }

    note = note + g_data_block.transpose;
    if(note > 96) note = 96;

    uint16_t frequency = g_frequency_table[note - 1];

    frequency -= (g_detune[channel] - 128);

    g_song_frequencies[channel] = frequency;
    g_song_volumes[channel] = (volume & 0x0F);
}

void note_off(uint8_t channel)
{
    g_song_frequencies[channel] = 0;
    g_data_block.pitch[channel].active = false;
    g_data_block.modulate[channel].active = false;
}

void change_instrument(uint8_t channel, uint8_t instrument)
{
    if(channel == 4) return;
    if(g_data_block.last_instruments[channel] != instrument)
    {
        g_data_block.last_instruments[channel] = instrument;
           
        g_roboplay_interface->set_segment(g_instrument_segment);
        g_roboplay_interface->scc_set_waveform(channel, g_scc_instruments[instrument].data);
        g_roboplay_interface->set_segment(START_SEGMENT_INDEX);
    }
}

void pitch_1_up(uint8_t channel, uint8_t value)
{
    g_data_block.modulate[channel].active = false;
    if(value)
    {
        g_data_block.pitch[channel].active = true;
        g_data_block.pitch[channel].type = 1;
        g_data_block.pitch[channel].direction = SBM_DIRECTION_UP;
        g_data_block.pitch[channel].value = value;
    }
    else
        g_data_block.pitch[channel].active = false;
}

void pitch_1_down(uint8_t channel, uint8_t value)
{
    g_data_block.modulate[channel].active = false;
    if(value)
    {
        g_data_block.pitch[channel].active = true;
        g_data_block.pitch[channel].type = 1;
        g_data_block.pitch[channel].direction = SBM_DIRECTION_DOWN;
        g_data_block.pitch[channel].value = value;
    }
    else
        g_data_block.pitch[channel].active = false;
}

void pitch_2_up(uint8_t channel, uint8_t value)
{
    g_data_block.modulate[channel].active = false;
    if(value)
    {
        g_data_block.pitch[channel].active = true;
        g_data_block.pitch[channel].type = 2;
        g_data_block.pitch[channel].direction = SBM_DIRECTION_UP;
        g_data_block.pitch[channel].value = value / (g_song_tempo + 1);
    }
    else
        g_data_block.pitch[channel].active = false;
}

void pitch_2_down(uint8_t channel, uint8_t value)
{
    g_data_block.modulate[channel].active = false;
    if(value)
    {
        g_data_block.pitch[channel].active = true;
        g_data_block.pitch[channel].type = 2;
        g_data_block.pitch[channel].direction = SBM_DIRECTION_DOWN;
        g_data_block.pitch[channel].value = value / (g_song_tempo + 1);
    }
    else
        g_data_block.pitch[channel].active = false;
}

void modulate_up(uint8_t channel, uint8_t value)
{
    g_data_block.pitch[channel].active = false;
    g_data_block.modulate[channel].active = true;
    g_data_block.modulate[channel].direction = SBM_DIRECTION_UP;
    g_data_block.modulate[channel].value = value - 1;
}

void modulate_down(uint8_t channel, uint8_t value)
{
    g_data_block.pitch[channel].active = false;
    g_data_block.modulate[channel].active = true;
    g_data_block.modulate[channel].direction = SBM_DIRECTION_DOWN;
    g_data_block.modulate[channel].value = value - 1;
}

void detune_up(uint8_t channel, uint8_t value)
{
    g_detune[channel] = value | 0x80;
}

void detune_down(uint8_t channel, uint8_t value)
{
    g_detune[channel] = 0x80 - (value & 0x7F);
}

void volume_slide_up(uint8_t channel, uint8_t value)
{
    g_data_block.volume_slide[channel].active = true;
    g_data_block.volume_slide[channel].direction = SBM_DIRECTION_UP;
    g_data_block.volume_slide[channel].value = 1 + (value / g_song_tempo + 1);
    g_data_block.volume_slide[channel].counter = value;
}

void volume_slide_down(uint8_t channel, uint8_t value)
{
    g_data_block.volume_slide[channel].active = true;
    g_data_block.volume_slide[channel].direction = SBM_DIRECTION_DOWN;
    g_data_block.volume_slide[channel].value = 1 + (value / g_song_tempo + 1);
    g_data_block.volume_slide[channel].counter = value;
}

void auto_slide_up(uint8_t channel, uint8_t value)
{
    g_data_block.auto_volume_slide[channel] = (value & 0x0F) | 0x10;
}

void auto_slide_down(uint8_t channel, uint8_t value)
{
    g_data_block.auto_volume_slide[channel] = 0x10 - (value & 0x0F);
}
