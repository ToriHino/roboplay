/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mid_fm.c
 *
 * MID: Standard MIDI file player (FM)
 */

#include <string.h>

#include "player_interface.h"
#include "mid_fm.h"

bool load(const char* file_name)
{
    g_roboplay_interface->open(file_name, false);

    /* Load the MIDI file header */
    uint8_t *file_data = (uint8_t *)READ_BUFFER;
    g_roboplay_interface->read(file_data, MIDI_HEADER_SIZE);
    if(file_data[0] != 'M' || file_data[1] != 'T' && file_data[2] != 'h' && file_data[3] != 'd' ||
       file_data[4] != 0 || file_data[5] != 0 || file_data[6] != 0 || file_data[7] != 6 )
    {
        return false;
    }
    if(file_data[8] != 0) return false;
    g_header.file_format = file_data[9];
    if(g_header.file_format > 2) return false;

    g_header.number_of_tracks = file_data[10];
    g_header.number_of_tracks <<= 8;
    g_header.number_of_tracks |= file_data[11];

    g_header.ticks_per_qnote = file_data[12];
    g_header.ticks_per_qnote <<= 8;
    g_header.ticks_per_qnote |= file_data[13];

    if(g_header.ticks_per_qnote < 1) return false;

    /* Load MIDI track data */
    load_track_data();

    g_roboplay_interface->close();

    return true;
}

bool update(void)
{
    bool song_finished = true;

    g_MIDI_counter += g_ticks_per_update;
    for(g_track = 0; g_track < g_header.number_of_tracks; g_track++)
    {
        if(!g_track_data[g_track].track_finished)
        {
            g_roboplay_interface->set_segment(g_track_data[g_track].segment);
            while(g_MIDI_counter >= g_track_data[g_track].waiting_for)
            {
                handle_track_event();
                get_delta_time();
            }
        }
        if(!g_track_data[g_track].track_finished) song_finished = false;
    }

    return !song_finished;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    /* Reset tempo to 120BPM */
    g_qnote_duration = 500000;

    g_MIDI_counter = 0;

    enable_drums();

    for(g_track = 0; g_track < g_header.number_of_tracks; g_track++)
    {
        g_track_data[g_track].segment = g_track_data[g_track].start_segment;
        g_track_data[g_track].track_data = g_track_data[g_track].start_track_data;

        g_track_data[g_track].track_finished = false;
        g_track_data[g_track].waiting_for = 0;
        g_track_data[g_track].last_command = 0xFF;

        g_roboplay_interface->set_segment(g_track_data[g_track].segment);
        get_delta_time();
    }

    for(uint8_t channel = 0; channel < NR_OF_MIDI_CHANNELS; channel++)
    {
        g_midi_channel_data[channel].instrument = 0;
        g_midi_channel_data[channel].fm_panning = PAN_SETTING_MID;
        g_midi_channel_data[channel].panning    = 0;
    }

    for(uint8_t voice = 0; voice < NR_OF_FM_CHANNELS; voice++)
    {
        g_fm_channel_data[voice].in_use = false;
        g_fm_channel_data[voice].activated = 0;
        g_fm_channel_data[voice].midi_link = 0;
        g_fm_channel_data[voice].note_number = 0;
        g_fm_channel_data[voice].note_velocity = 0;
    }
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    /* Use a fixed replay rate of 60 Hz */
    float rate = 60.0;

    float tick_duration = g_qnote_duration / g_header.ticks_per_qnote;
    float update_duration  = 1000000.0 / rate;
    float ticks_per_update = 0.5 + (update_duration / tick_duration);

    g_ticks_per_update = ticks_per_update;

    return rate;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "Standard MIDI file player (FM) V1.0 by RoboSoft Inc.";
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
    switch(g_header.file_format)
    {
        case 0x00:
            return "SMF format 0";
            break;

        case 0x01:
            return "SMF format 1";
            break;

        case 0x02:
            return "SMF format 2";
            break;

        default:
            return "-";            
    }

}

void load_track_data(void)
{
    uint8_t  *destination     = (uint8_t *)DATA_SEGMENT_BASE;
    uint16_t  page_left       = DATA_SEGMENT_SIZE;
    uint8_t   current_segment = START_SEGMENT_INDEX;

    for(uint8_t i = 0; i < g_header.number_of_tracks; i++)
    {
        g_track_data[i].start_segment = current_segment;
        g_track_data[i].start_track_data = destination;

        uint8_t track_buffer[4];
        g_roboplay_interface->read(track_buffer, 4);  // MTrk
        g_roboplay_interface->read(track_buffer, 4);  // Header Length

        g_track_data[i].length = track_buffer[0];
        g_track_data[i].length <<= 8;
        g_track_data[i].length |= track_buffer[1];
        g_track_data[i].length <<= 8;
        g_track_data[i].length |= track_buffer[2];
        g_track_data[i].length <<= 8;
        g_track_data[i].length |= track_buffer[3];

        uint32_t data_size = g_track_data[i].length;
        while(data_size > 0)
        {
            uint16_t read_size = (data_size < READ_BUFFER_SIZE) ? data_size : READ_BUFFER_SIZE;
            read_size = (read_size < page_left) ? read_size : page_left;

            /* It's not possible to read directly to non-primary mapper memory segments,
               so use a buffer inbetween. */
            uint16_t bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, read_size);
            memcpy(destination, (void*)READ_BUFFER, bytes_read);

            data_size -= bytes_read;
            destination += bytes_read;
            page_left -= bytes_read;
            if(page_left == 0)
            {
                current_segment = g_roboplay_interface->get_new_segment();
                g_roboplay_interface->set_segment(current_segment);

                page_left = DATA_SEGMENT_SIZE;
                destination = (uint8_t *)DATA_SEGMENT_BASE;
            }
        }
    }
}

void enable_drums(void)
{
    /* Enable waveform select */
    g_roboplay_interface->opl4_write_fm_1(1, 0x20);

    /* Enable percussion mode, amplify AM & VIB */
    g_byte_bd = AM | VIB | 0x20;
    g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd);

    /* Set drum frequencies */
    g_roboplay_interface->opl4_write_fm_1(0xA6, 400 & 0xFF);
    g_roboplay_interface->opl4_write_fm_1(0xB6, (400 >> 8) + (2 << 2));
    g_roboplay_interface->opl4_write_fm_1(0xA7, 500 & 0xFF);
    g_roboplay_interface->opl4_write_fm_1(0xB7, (500 >> 8) + (2 << 2));
    g_roboplay_interface->opl4_write_fm_1(0xA8, 650 & 0xFF);
    g_roboplay_interface->opl4_write_fm_1(0xB8, (650 >> 8) + (2 << 2));

    for(uint8_t i = 0; i < NR_OF_DRUM_DATA; i++)
    {
        for(uint8_t j = 0; j < DRUM_DATA_SIZE; j++)
        {
            g_roboplay_interface->opl4_write_fm_1(g_drum_registers[i][j], g_drum_data[i][j]);
        }
    }
}

void change_speed(uint32_t speed)
{
    g_qnote_duration = speed;
    g_roboplay_interface->update_refresh();
}

uint8_t read_byte(void)
{
    uint8_t data = *(g_track_data[g_track].track_data++);

    if(g_track_data[g_track].track_data == (uint8_t *)(DATA_SEGMENT_BASE + DATA_SEGMENT_SIZE))
    {
        g_track_data[g_track].segment++;
        g_roboplay_interface->set_segment(g_track_data[g_track].segment);
        g_track_data[g_track].track_data = (uint8_t*)DATA_SEGMENT_BASE;
    }

    return data;
}

uint32_t get_variable_len(void)
{
    uint8_t data;
    uint32_t value = 0;

    do
    {
        data = read_byte();
        value = (value << 7) + (data & 0x7F);
    } while (data & 0x80);

    return value;
}

void get_delta_time(void)
{
    g_track_data[g_track].waiting_for += get_variable_len();
}

void handle_track_event(void)
{
    uint8_t data_byte_1, data_byte_2, data_byte_3;

    data_byte_1 =  read_byte();

    /* If MSB is not set, this is a running status */
    if((data_byte_1 & 128) != 0)
    {
        g_track_data[g_track].last_command = data_byte_1;
        data_byte_2 = read_byte();
    }
    else
    {
        data_byte_2 = data_byte_1;
        data_byte_1 = g_track_data[g_track].last_command;
    }

    if(data_byte_1 == 0xFF)
        handle_meta_event(data_byte_2);
    else if(data_byte_1 == 0xF0)
        handle_sys_ex_event();
    else
    {
        uint8_t midi_channel = data_byte_1 & 0x0F;
        uint8_t midi_command = data_byte_1 >> 4;

        switch(midi_command)
        {
            case 0x8:  // Note OFF
                data_byte_3 = read_byte();
                if(midi_channel == 9)
                    drum_off(data_byte_2 & 0x7F, data_byte_3);
                else
                    note_off(midi_channel, data_byte_2 & 0x7F, data_byte_3);
                break;
            case 0x9:  // Note ON
                data_byte_3 = read_byte();
                if(midi_channel == 9)
                    drum_on(data_byte_2 & 0x7F, data_byte_3);
                else
                    note_on(midi_channel, data_byte_2 & 0x7F, data_byte_3);
                break;
            case 0xA:  // Key after-touch
                data_byte_3 = read_byte();
                break;
            case 0xB:  // Control change
                data_byte_3 = read_byte();
                control_change(midi_channel, data_byte_2, data_byte_3);
                break;
            case 0xC:  // Program change
                program_change(midi_channel, data_byte_2 & 0x7F);
                break;
            case 0xD:  // Channel after-touch
                data_byte_2 = read_byte();               
                break;
            case 0xE:  // Pitch wheel
                data_byte_3 = read_byte();
                pitch_wheel(midi_channel, ((data_byte_3 <<= 7) | data_byte_2));
                break;
        }
    }
}

void handle_meta_event(uint8_t sub_type)
{
    uint32_t length = get_variable_len() ;
   
    switch(sub_type)
    {
        case 0x2f:
            g_track_data[g_track].track_finished = true;
            break;
        case 0x51:
            g_qnote_duration = read_byte();
            g_qnote_duration = (g_qnote_duration << 8) + read_byte();
            g_qnote_duration = (g_qnote_duration << 8) + read_byte();
            g_roboplay_interface->update_refresh();
            break;
        default:
            for(uint32_t i = 0; i < length; i++) read_byte();
    }
}

void handle_sys_ex_event(void)
{
    uint8_t data;

    /* Skip SysEx data */
    do{
        data = read_byte();
    } while(data != 0xF7);
}

void write_opl_control(uint8_t reg, uint8_t fm_channel, uint8_t value)
{
    if(fm_channel > 8)
        g_roboplay_interface->opl4_write_fm_2(reg + (fm_channel - 9), value);
    else
        g_roboplay_interface->opl4_write_fm_1(reg + fm_channel, value);
}

void write_opl_operator(uint8_t reg, uint8_t fm_channel, uint8_t value)
{
    if(fm_channel > 8)
        g_roboplay_interface->opl4_write_fm_2(reg + g_op_addr[fm_channel - 9], value);
    else
        g_roboplay_interface->opl4_write_fm_1(reg + g_op_addr[fm_channel], value);
}

void enable_note(uint8_t voice, uint8_t note)
{
    uint8_t fm_channel = real_voice[voice];
    uint8_t block = note >> 4;

    /* Store data to disable note when necessary */
    g_note_off_data[fm_channel][0] = (g_fnumber[note] & 0xFF);
    g_note_off_data[fm_channel][1] = (g_fnumber[note] >> 8) + (block << 2);

    write_opl_control(0xA0, fm_channel, g_note_off_data[fm_channel][0]);
    write_opl_control(0xB0, fm_channel, g_note_off_data[fm_channel][1] | 0x20);
}

void disable_note(uint8_t voice)
{
    uint8_t fm_channel = real_voice[voice];

    write_opl_control(0xA0, fm_channel, g_note_off_data[fm_channel][0]);
    write_opl_control(0xB0, fm_channel, g_note_off_data[fm_channel][1]);
}

void cut_note(uint8_t voice)
{
    uint8_t fm_channel = real_voice[voice];

    /* Set decay rate to fast */
    write_opl_operator(0x80, fm_channel, 0x0F);
    write_opl_operator(0x83, fm_channel, 0x0F);

    disable_note(voice);
}

void set_instrument(uint8_t voice, uint8_t instrument, uint8_t volume, uint8_t panning)
{
    uint8_t fm_channel = real_voice[voice];

    /* Correction for volume */
    uint16_t value = 63 - (M4[instrument] & 63);
    value = (value * volume) / 127;
    if(value > 63) value = 0; else value = 63 - value;
    value = (M4[instrument] & 0xC0) | value;

    /* Set up voice modulator */
    write_opl_operator(0x20, fm_channel, M2[instrument]);
    write_opl_operator(0x40, fm_channel, value);
    write_opl_operator(0x60, fm_channel, M6[instrument]);
    write_opl_operator(0x80, fm_channel, M8[instrument]);
    write_opl_operator(0xE0, fm_channel, ME[instrument]);

    write_opl_control(0xC0, fm_channel, MC[instrument] | panning);

    /* Correction for volume */
    value = 63 - (C4[instrument] & 63);
    value = (value * volume) / 127;
    if(value > 63) value = 0; else value = 63 - value;
    value = (C4[instrument] & 0xC0) | (value >> 1);

    /* Set up voice carrier */
    write_opl_operator(0x23, fm_channel, C2[instrument]);
    write_opl_operator(0x43, fm_channel, value);
    write_opl_operator(0x63, fm_channel, C6[instrument]);
    write_opl_operator(0x83, fm_channel, C8[instrument]);
    write_opl_operator(0xE3, fm_channel, CE[instrument]);
}

void note_off(uint8_t midi_channel, uint8_t note, uint8_t velocity)
{
    /* Velocity parameter not used */
    velocity;

    for(uint8_t voice = 0; voice < NR_OF_FM_CHANNELS; voice++)
    {
        if(g_fm_channel_data[voice].in_use)
        {
            if(g_fm_channel_data[voice].midi_link == midi_channel && g_fm_channel_data[voice].note_number == note)
            {
                disable_note(voice);
                g_fm_channel_data[voice].in_use = false;
                break;
            }
        }
    }
}

void note_on(uint8_t midi_channel, uint8_t note, uint8_t velocity)
{
    /* Velocity of zero means note off */
    if(!velocity)
    {
        note_off(midi_channel, note, velocity);
        return;
    }

    /* Scan for a free voice */
    uint8_t free_voice = NR_OF_FM_CHANNELS;
    for(uint8_t voice = 0; voice < NR_OF_FM_CHANNELS; voice++)
    {
        if(!g_fm_channel_data[voice].in_use)
        {
            free_voice = voice;
            break;
        }
    }

    /* If no free voice found, deactivate the 'oldest' */
    if(free_voice == NR_OF_FM_CHANNELS)
    {
        uint8_t free_voice = 0;
        for(uint8_t voice = 1; voice < NR_OF_FM_CHANNELS; voice++)
        {
            if(g_fm_channel_data[voice].activated < g_fm_channel_data[free_voice].activated)
                free_voice = voice;
        }

        /* Disable the note currently playing */
        cut_note(free_voice);
    }

    /* Change instrument settings for the FM channel chosen */
    set_instrument(free_voice, g_midi_channel_data[midi_channel].instrument, velocity, g_midi_channel_data[midi_channel].fm_panning);

    /* Start playing the note */
    enable_note(free_voice, note);

    /* Store voice information */
    g_fm_channel_data[free_voice].in_use = true;
    g_fm_channel_data[free_voice].activated = g_MIDI_counter;
    g_fm_channel_data[free_voice].midi_link = midi_channel;
    g_fm_channel_data[free_voice].note_number = note;
    g_fm_channel_data[free_voice].note_velocity = velocity;
}

void drum_on(uint8_t number, uint8_t velocity)
{
    if(!velocity) return;

    /* Correction for volume */
    velocity = (uint16_t)(velocity << 3) / 10;
    velocity = (63 - (velocity >> 1)) / 2;

    /* Bass drum */
    if (number == 35 || number == 36 || number == 41 || number == 43) 
    {  
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd);
        g_roboplay_interface->opl4_write_fm_1(0x53, velocity);
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd | 16);
    }

    /* HiHat */
    else if (number == 37 || number == 39 || number == 42 || number == 44 ||
             number == 46 || number == 56 || number == 62 || number == 69 ||
             number == 70 || number == 71 || number == 72 || number == 78) 
    {       
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd);
        g_roboplay_interface->opl4_write_fm_1(0x51, velocity);
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd | 1);
    }

    /* Snare drum */
    else if (number == 38 || number == 40) 
    {       
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd);
        g_roboplay_interface->opl4_write_fm_1(0x54, velocity);
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd | 8);
    }

    /* TomTom */
    else if (number == 45 || number == 47 || number == 48 || number == 50 ||
             number == 60 || number == 61 || number == 63 || number == 64 ||
             number == 65 || number == 66 || number == 67 || number == 68 ||
             number == 73 || number == 74 || number == 75 || number == 76 ||
             number == 77) 
    {       
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd);
        g_roboplay_interface->opl4_write_fm_1(0x52, velocity);
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd | 4);
    }

    /* Cymbal */
    else if (number == 49 || number == 51 || number == 52 || number == 53 ||
             number == 54 || number == 55 || number == 57 || number == 58 ||
             number == 59 || number == 79 || number == 80 || number == 81) 
    {
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd);
        g_roboplay_interface->opl4_write_fm_1(0xBD, g_byte_bd | 2);
        g_roboplay_interface->opl4_write_fm_1(0x53, velocity);
    }
}

void drum_off(uint8_t note, uint8_t velocity)
{
    note;
    velocity;
}

void pitch_wheel(uint8_t midi_channel, uint8_t value)
{
    midi_channel;
    value;
}
void control_change(uint8_t midi_channel, uint8_t id, uint8_t value)
{
    switch(id)
    {
        case 10:
            /* Change panning */
            g_midi_channel_data[midi_channel].panning = value;
            g_midi_channel_data[midi_channel].fm_panning = PAN_SETTING_MID;
            if(value  < 64+32) g_midi_channel_data[midi_channel].fm_panning |= PAN_SETTING_LEFT;
            if(value >= 64-32) g_midi_channel_data[midi_channel].fm_panning |= PAN_SETTING_RIGHT;
            break;
    }
}

void channel_pressure(uint8_t midi_channel, uint8_t pressure)
{
    midi_channel;
    pressure;
}

void key_pressure(uint8_t midi_channel, uint8_t note, uint8_t pressure)
{
    midi_channel;
    note;
    pressure;
}

void program_change(uint8_t midi_channel, uint8_t program)
{
    g_midi_channel_data[midi_channel].instrument = program;
}
