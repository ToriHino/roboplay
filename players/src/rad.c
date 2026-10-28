/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * rad.c
 *
 * RAD: Reality ADlib Tracker v1 and v2 (file version 2.1) player
 */

#include <string.h>

#include "player_interface.h"
#include "rad.h"

/* V2 addresses the tune with segment/offset arithmetic based on 16KB segments */
typedef char segment_size_check[(DATA_SEGMENT_SIZE == (1 << SEGMENT_SHIFT)) ? 1 : -1];

bool load(const char* file_name)
{    
    g_roboplay_interface->open(file_name, false);  
    g_file_data = (void *)DATA_SEGMENT_BASE;
    
    g_segment_index = 0;
    g_segment_list[g_segment_index++] = START_SEGMENT_INDEX;

    uint16_t bytes_read = 0;
    for(;;)
    {
        bytes_read = g_roboplay_interface->read(g_file_data, DATA_SEGMENT_SIZE);
        if(bytes_read < DATA_SEGMENT_SIZE) break;

        /* File does not fit in MAX_SEGMENT_INDEX segments */
        if(g_segment_index >= MAX_SEGMENT_INDEX)
        {
            g_roboplay_interface->close();
            g_roboplay_interface->set_segment(g_segment_list[0]);
            return false;
        }

        g_segment_list[g_segment_index] = g_roboplay_interface->get_new_segment();
        g_roboplay_interface->set_segment(g_segment_list[g_segment_index]);

        g_segment_index++;
    }

    g_roboplay_interface->close();

    g_file_size = (uint16_t)(g_segment_index - 1) * DATA_SEGMENT_SIZE + bytes_read;

    /* Segments that were not used point to segment 0, so a corrupt tune can never map a bogus segment */
    for(uint8_t i = g_segment_index; i < MAX_SEGMENT_INDEX; i++)
        g_segment_list[i] = g_segment_list[0];

    g_roboplay_interface->set_segment(g_segment_list[0]);

    if(g_file_size < sizeof(RAD_HEADER)) return false;

    g_header = (RAD_HEADER *)g_file_data;
    if(strncmp(g_header->id, "RAD by REALiTY!!", 16)) return false;

    if(g_header->version == RAD_VERSION_1)
    {
        g_version = 1;
        return load_v1();
    }

    if(g_header->version == RAD_VERSION_2)
    {
        g_version = 2;
        return load_v2();
    }

    return false;
}

bool load_v1(void)
{
    uint8_t *data = g_file_data + sizeof(RAD_HEADER);

    /* Slow timer tune? */
    if(g_header->flags & 0x40) 
        g_hertz = 18.2;
    else
        g_hertz = 50.0;

    /* Skip any description */
    if(g_header->flags & 0x80)
    {
        while(*data) data++;
        data++;
    }

    /* Record pointers to the instruments */
    while(*data)
    {
        uint8_t instrument_number = *data++;

        g_instruments[instrument_number - 1] = (RAD_INSTRUMENT *)data;
        data += sizeof(RAD_INSTRUMENT);
    }
    data++;

    /* Get order list */
    g_order_list_size = *data++;
    g_order_list = data;
    data += g_order_list_size;

    /* Patterns */
    g_patterns = data;

    return true;
}

bool update(void)
{
    return (g_version == 2) ? update_v2() : update_v1();
}

bool update_v1(void)
{
    uint8_t i;

    if(!g_speed_count)
    {
        /* Reset any slide */
        for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
        {
            g_channel_data[i].port_slide = 0;
            g_channel_data[i].volume_slide = 0;
            g_channel_data[i].tone_slide = false;
        }
        
        g_next_line_jump = -1;

        if(g_current_line == g_next_line) 
        {
            for(i = 0; i < MAX_CHANNEL_NUMBER; i++)
            {
                if(g_note_buffer[i].new_data) 
                    play_note(i);
            }

            /* Was this the last line? */
            if(g_is_last_line) g_pattern_data = NULL;
        }

        g_speed_count = g_speed - 1;

        g_next_line++;
        if(g_next_line >= MAX_PATTERN_LINE || g_next_line_jump >= 0)
        {
            if(g_next_line_jump >= 0)
                g_next_line = g_next_line_jump;
            else
                g_next_line = 0;

            next_pattern();
        }

        g_current_line = *g_pattern_data & 0x7F;
        g_is_last_line = *g_pattern_data & 0x80;

        if(g_speed == 1 && g_pattern_data)
        {
            if(g_current_line == g_next_line)
                fill_note_buffer();
        }
    }
    else
    {
        g_speed_count--;

        /* Next update has a new line? */
        if(!g_speed_count && g_pattern_data)
        {
            if(g_current_line == g_next_line)
                fill_note_buffer();
        }
    }

    /* Now update notes */
    update_notes();

    return true;
}

void rewind_v1(void)
{
    /* Configure OPL */
    g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL3);

    g_roboplay_interface->opl4_write_fm_1(0x01,  0x20); /* Allow waveforms */
    g_roboplay_interface->opl4_write_fm_1(0x08,  0x00); /* No split point */
    g_roboplay_interface->opl4_write_fm_1(0xBD,  0x00); /* No drums, etc. */

    /* Initial values */
    g_speed = g_header->flags & 0x3F;

    g_order_position = 0;
    g_speed_count    = 1;
    
    g_pattern_data = g_patterns[g_order_list[g_order_position]] + DATA_SEGMENT_BASE;
    g_next_line    = 0;

    g_current_line = *g_pattern_data & 0x7F;
    g_is_last_line = *g_pattern_data & 0x80;

    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        g_channel_data[i].tone_slide = false;
        g_channel_data[i].tone_slide_speed = 0;
        g_channel_data[i].tone_slide_frequency= 0;
        g_channel_data[i].tone_slide_octave = 0;

        g_channel_data[i].port_slide = 0;
        g_channel_data[i].volume_slide = 0;

        g_channel_data[i].instrument_number = 0;

        g_channel_data[i].frequency = 0;
        g_channel_data[i].octave = 0;

        g_channel_data[i].old_43 = 0;
        g_channel_data[i].old_A0 = 0;
        g_channel_data[i].old_B0 = 0;
    }

    g_segment_index = 0;
}

void rewind(const uint8_t subsong)
{
    subsong;

    if(g_version == 2)
        rewind_v2();
    else
        rewind_v1();
}

void command(const uint8_t id)
{
    id;
}

float get_refresh(void)
{
    return g_hertz;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "Reality ADlib Tracker player V2.0 by RoboSoft Inc.";
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
    return (g_version == 2) ? "RAD V2" : "RAD V1";
}

uint8_t next_byte(void)
{
    uint8_t byte = *g_pattern_data++;

    if((uint16_t)g_pattern_data == (DATA_SEGMENT_BASE + DATA_SEGMENT_SIZE))
    {
        g_segment_index++;
        g_roboplay_interface->set_segment(g_segment_list[g_segment_index]);
        g_pattern_data = (uint8_t *)DATA_SEGMENT_BASE;
    }

    return byte;
}

void fill_note_buffer(void)
{
    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
        g_note_buffer[i].new_data = false;

    /* Move to first channel */
    next_byte();

    bool last_channel = false;
    while(!last_channel)
    {
        uint8_t data_0 = next_byte();
        uint8_t data_1 = next_byte();
        uint8_t data_2 = next_byte();

        last_channel = (data_0 & 0x80) ? true : false;

        uint8_t channel = data_0 & 0x7F;

        g_note_buffer[channel].new_data = true;

        g_note_buffer[channel].octave  = (data_1 & 0x70) >> 4;
        g_note_buffer[channel].note    = data_1 & 0x0F;

        g_note_buffer[channel].instrument_number = ((data_2 & 0xF0) >> 4) + ((data_1 & 0x80) >> 4);

        g_note_buffer[channel].command = data_2 & 0x0F;

        if(g_note_buffer[channel].command)
            g_note_buffer[channel].data = next_byte();
    }
}

void next_pattern(void)
{
    g_roboplay_interface->set_segment(g_segment_list[0]);

    g_order_position++;
    if(g_order_position >= g_order_list_size) g_order_position = 0;

    uint8_t pattern_number = g_order_list[g_order_position];
    if(pattern_number >= 0x80)
    {
        g_order_position = pattern_number - 0x80;
        pattern_number = g_order_list[g_order_position];
    }

    g_pattern_data = g_patterns[pattern_number];

    g_segment_index = 0;
    while((uint16_t)g_pattern_data > DATA_SEGMENT_SIZE)
    {
        g_pattern_data -= DATA_SEGMENT_SIZE;
        g_segment_index++;
    }

    g_pattern_data += DATA_SEGMENT_BASE;
    g_roboplay_interface->set_segment(g_segment_list[g_segment_index]);
}

void update_notes(void)
{
    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        /* Process portamentos and tone slides */
        if(g_channel_data[i].port_slide || g_channel_data[i].tone_slide)
        {
            uint16_t frequency = g_channel_data[i].frequency;
            uint8_t  octave = g_channel_data[i].octave;

            if(g_channel_data[i].port_slide)
                frequency += g_channel_data[i].port_slide;

            if(g_channel_data[i].tone_slide)
                frequency += g_channel_data[i].tone_slide_speed;

            if(frequency < FREQ_START)
            {
                if(octave > OCTAVE_START)
                {
                    octave--;
                    frequency += FREQ_RANGE;
                }
                else frequency = FREQ_START;
            }
            else if(frequency > FREQ_END)
            {
                if(octave < OCTAVE_END)
                {
                    octave ++;
                    frequency -= FREQ_RANGE;
                }
                else frequency = FREQ_END;
            }

            if(g_channel_data[i].tone_slide)
            {
                 if(g_channel_data[i].tone_slide_speed >= 0)
                 {
                    if(octave > g_channel_data[i].tone_slide_octave || 
                      (octave == g_channel_data[i].tone_slide_octave && frequency >= g_channel_data[i].tone_slide_frequency))
                    {
                        frequency = g_channel_data[i].tone_slide_frequency;
                        octave = g_channel_data[i].tone_slide_octave;
                    }
                 }
                else
                {
                    if(octave < g_channel_data[i].tone_slide_octave || 
                      (octave == g_channel_data[i].tone_slide_octave && frequency <= g_channel_data[i].tone_slide_frequency))
                    {
                        frequency = g_channel_data[i].tone_slide_frequency;
                        octave = g_channel_data[i].tone_slide_octave;
                    }
                }
            }

            g_channel_data[i].frequency = frequency;
            g_channel_data[i].octave = octave;

            g_channel_data[i].old_A0 = (g_channel_data[i].frequency & 0xFF);
            g_channel_data[i].old_B0 = (g_channel_data[i].old_B0 & 0xE0) + (g_channel_data[i].octave << 2) + (g_channel_data[i].frequency >> 8);

            g_roboplay_interface->opl4_write_fm_1(0xA0 + i, g_channel_data[i].old_A0);
            g_roboplay_interface->opl4_write_fm_2(0xA0 + i, g_channel_data[i].old_A0 + 1);

            g_roboplay_interface->opl4_write_fm_1(0xB0 + i, g_channel_data[i].old_B0);
            g_roboplay_interface->opl4_write_fm_2(0xB0 + i, g_channel_data[i].old_B0);
        }

        /* Process volume slides */
        if(g_channel_data[i].volume_slide)
        {
            int8_t volume = (g_channel_data[i].old_43 & 0x3F) ^ 0x3F;
           
            volume += g_channel_data[i].volume_slide;
            if(volume < 0) volume = 0;

            set_volume(i, volume);
        }
    }

}

void play_note(uint8_t channel)
{
  if(g_note_buffer[channel].note)
  {
    if(g_note_buffer[channel].command == CMD_TONE_SLIDE)
    {
      if(g_note_buffer[channel].note < KEY_OFF)  
      {
        g_channel_data[channel].tone_slide_frequency = g_note_frequencies[g_note_buffer[channel].note - 1];
        g_channel_data[channel].tone_slide_octave = g_note_buffer[channel].octave;
      }
    }
    else
    {
      /* Key off old note */
      g_channel_data[channel].old_B0 = g_channel_data[channel].old_B0 & 0xDF;
      g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, g_channel_data[channel].old_B0);
      g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, g_channel_data[channel].old_B0);

      /* Load instrument (if any) */
      if(g_note_buffer[channel].instrument_number)
      {
        if(g_note_buffer[channel].instrument_number != g_channel_data[channel].instrument_number)
        {
          g_channel_data[channel].instrument_number = g_note_buffer[channel].instrument_number;
          load_instrument(channel);
        }
        else
          load_instrument_volume(channel);
      }

      if(g_note_buffer[channel].note < KEY_OFF)
      {           
        g_channel_data[channel].frequency = g_note_frequencies[g_note_buffer[channel].note - 1];
        g_channel_data[channel].octave = g_note_buffer[channel].octave;

        g_channel_data[channel].old_A0 = (g_channel_data[channel].frequency & 0xFF);
        g_channel_data[channel].old_B0 = 0x20 + (g_channel_data[channel].octave << 2) + (g_channel_data[channel].frequency >> 8);

        g_roboplay_interface->opl4_write_fm_1(0xA0 + channel, g_channel_data[channel].old_A0);
        g_roboplay_interface->opl4_write_fm_2(0xA0 + channel, g_channel_data[channel].old_A0 + 1);

        g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, g_channel_data[channel].old_B0);
        g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, g_channel_data[channel].old_B0);
      }
    }
  }

  switch(g_note_buffer[channel].command)
  {
    case CMD_PORTAMENTO_UP:
      g_channel_data[channel].port_slide = g_note_buffer[channel].data;
      break;
    case CMD_PORTAMENTO_DOWN:
      g_channel_data[channel].port_slide = -g_note_buffer[channel].data;
      break;
    case CMD_TONE_SLIDE:
      tone_slide(channel);
      break;
    case CMD_TONE_VOL_SLIDE:
      tone_volume_slide(channel);
      break;
    case CMD_VOL_SLIDE:
      volume_slide(channel);
      break;
    case CMD_SET_VOL:
      set_volume(channel, g_note_buffer[channel].data);
      break;
    case CMD_JUMP_TO_LINE:
      if(g_note_buffer[channel].data < MAX_PATTERN_LINE)
        g_next_line_jump = g_note_buffer[channel].data;
      break;
    case CMD_SET_SPEED:
      g_speed = g_note_buffer[channel].data;
      break;
  }
}

void load_instrument(uint8_t channel)
{
    g_roboplay_interface->set_segment(g_segment_list[0]);

    RAD_INSTRUMENT *instrument = g_instruments[g_channel_data[channel].instrument_number - 1];
    g_channel_data[channel].old_43 = instrument->data[2];

    uint8_t reg = g_channel_offsets[channel];
    uint8_t offset = 0;
    for(uint8_t i = 0; i < 4; i++)
    {
        g_roboplay_interface->opl4_write_fm_1(reg, instrument->data[offset + 1]);
        g_roboplay_interface->opl4_write_fm_2(reg, instrument->data[offset + 1]);
        reg += 0x03;
        g_roboplay_interface->opl4_write_fm_1(reg, instrument->data[offset]);
        g_roboplay_interface->opl4_write_fm_2(reg, instrument->data[offset]);
        reg += 0x20 - 0x03;

        offset += 2;
    }

    reg += 0x40;
    g_roboplay_interface->opl4_write_fm_1(reg, instrument->data[offset + 2]);
    g_roboplay_interface->opl4_write_fm_2(reg, instrument->data[offset + 2]);
    reg += 0x03;
    g_roboplay_interface->opl4_write_fm_1(reg, instrument->data[offset + 1]);
    g_roboplay_interface->opl4_write_fm_2(reg, instrument->data[offset + 1]);

    g_roboplay_interface->opl4_write_fm_1(0xC0 + channel, instrument->data[offset] + PAN_SETTING_LEFT);
    g_roboplay_interface->opl4_write_fm_2(0xC0 + channel, instrument->data[offset] + PAN_SETTING_RIGHT);

    g_roboplay_interface->set_segment(g_segment_list[g_segment_index]);
}

void load_instrument_volume(uint8_t channel)
{
    g_roboplay_interface->set_segment(g_segment_list[0]);

    RAD_INSTRUMENT *instrument = g_instruments[g_channel_data[channel].instrument_number - 1];
    g_channel_data[channel].old_43 = instrument->data[2];

    g_roboplay_interface->opl4_write_fm_1(g_channel_offsets[channel] + 0x23, g_channel_data[channel].old_43);
    g_roboplay_interface->opl4_write_fm_2(g_channel_offsets[channel] + 0x23, g_channel_data[channel].old_43);   

    g_roboplay_interface->set_segment(g_segment_list[g_segment_index]);
}

void set_volume(uint8_t channel, uint8_t volume)
{
    if(volume > 0x3F) volume = 0x3F;
    g_channel_data[channel].old_43 = (g_channel_data[channel].old_43 & 0xC0) + (volume ^ 0x3F);

    g_roboplay_interface->opl4_write_fm_1(g_channel_offsets[channel] + 0x23, g_channel_data[channel].old_43);
    g_roboplay_interface->opl4_write_fm_2(g_channel_offsets[channel] + 0x23, g_channel_data[channel].old_43);
}

void tone_slide(uint8_t channel)
{
    if(g_note_buffer[channel].data) g_channel_data[channel].tone_slide_speed = g_note_buffer[channel].data;
    g_channel_data[channel].tone_slide = true;

}

void tone_volume_slide(uint8_t channel)
{
    volume_slide(channel);
    g_channel_data[channel].tone_slide = true;
}

void volume_slide(uint8_t channel)
{
    if(g_note_buffer[channel].data > 50)
        g_channel_data[channel].volume_slide = (g_note_buffer[channel].data - 50);
    else
        g_channel_data[channel].volume_slide = -g_note_buffer[channel].data;
}

/* ======================================================================== */
/*  RAD V2 (file version 2.1)                                               */
/*                                                                          */
/*  Port of Reality's player20.cpp. All tune data (instruments, tracks,     */
/*  riffs) is addressed by 16 bit offsets into the file, which is spread    */
/*  over several 16KB segments, so every access goes through peek_byte()    */
/*  which maps the right segment first.                                     */
/* ======================================================================== */

uint8_t peek_byte(uint16_t pos)
{
    uint8_t segment = (uint8_t)(pos >> SEGMENT_SHIFT);

    if(segment != g_v2_mapped_segment)
    {
        g_roboplay_interface->set_segment(g_segment_list[segment]);
        g_v2_mapped_segment = segment;
    }

    return *(uint8_t *)(DATA_SEGMENT_BASE + (pos & SEGMENT_OFFSET_MASK));
}

uint8_t read_byte(uint16_t *pos)
{
    return peek_byte((*pos)++);
}

uint16_t read_word(uint16_t *pos)
{
    uint8_t low  = read_byte(pos);
    uint8_t high = read_byte(pos);

    return low | ((uint16_t)high << 8);
}

bool have_bytes(uint16_t pos, uint16_t count)
{
    return pos <= g_file_size && count <= (uint16_t)(g_file_size - pos);
}

bool load_v2(void)
{
    uint16_t pos = 0x11;
    uint16_t size;

    g_v2_mapped_segment = NO_SEGMENT;

    memset(g_v2_instruments, 0, sizeof(g_v2_instruments));
    memset(g_v2_tracks, 0, sizeof(g_v2_tracks));
    memset(g_v2_riffs, 0, sizeof(g_v2_riffs));

    uint8_t flags = read_byte(&pos);
    g_v2_initial_speed = flags & 0x1F;

    /* Is BPM value present? */
    g_hertz = 50.0;
    if(flags & 0x20)
    {
        if(!have_bytes(pos, 2)) return false;

        uint16_t bpm = read_word(&pos);
        if(bpm < 46 || bpm > 300) return false;

        g_hertz = (float)((bpm * 2) / 5);
    }

    /* Slow timer tune? */
    if(flags & 0x40) g_hertz = 18.2;

    /* Skip description */
    do
    {
        if(!have_bytes(pos, 1)) return false;
    } while(read_byte(&pos));

    /* Record the offsets of the instruments */
    for(;;)
    {
        if(!have_bytes(pos, 1)) return false;
        uint8_t instrument_number = read_byte(&pos);
        if(!instrument_number) break;
        if(instrument_number > V2_MAX_INSTRUMENTS) return false;

        /* Skip the name */
        if(!have_bytes(pos, 1)) return false;
        uint8_t name_length = read_byte(&pos);
        if(!have_bytes(pos, (uint16_t)name_length + 1)) return false;
        pos += name_length;

        g_v2_instruments[instrument_number - 1] = pos;

        /* Instrument is the algorithm byte plus 23 bytes (OPL) or 6 bytes (MIDI, ignored) */
        uint8_t algorithm = peek_byte(pos);
        size = ((algorithm & 7) < 7) ? 24 : 7;
        if(!have_bytes(pos, size)) return false;
        pos += size;

        /* Instrument riff? */
        if(algorithm & 0x80)
        {
            if(!have_bytes(pos, 2)) return false;
            size = read_word(&pos);
            if(!have_bytes(pos, size)) return false;
            pos += size;
        }
    }

    /* Order list */
    if(!have_bytes(pos, 1)) return false;
    g_order_list_size = read_byte(&pos);
    if(g_order_list_size > 128) return false;
    if(!have_bytes(pos, g_order_list_size)) return false;

    g_v2_order_list = pos;
    pos += g_order_list_size;

    /* Tracks (patterns) */
    for(;;)
    {
        if(!have_bytes(pos, 1)) return false;
        uint8_t track_number = read_byte(&pos);
        if(track_number >= V2_MAX_TRACKS) break;

        if(!have_bytes(pos, 2)) return false;
        size = read_word(&pos);
        if(!have_bytes(pos, size)) return false;

        g_v2_tracks[track_number] = pos;
        pos += size;
    }

    /* Riffs */
    for(;;)
    {
        if(!have_bytes(pos, 1)) break;
        uint8_t riff_id = read_byte(&pos);
        uint8_t riff_number = riff_id >> 4;
        uint8_t channel = riff_id & 15;
        if(riff_number >= V2_MAX_RIFF_TRACKS || channel > MAX_CHANNEL_NUMBER) break;
        if(channel == 0) return false;

        if(!have_bytes(pos, 2)) return false;
        size = read_word(&pos);
        if(!have_bytes(pos, size)) return false;

        g_v2_riffs[riff_number][channel - 1] = pos;
        pos += size;
    }

    return true;
}

void rewind_v2(void)
{
    g_v2_mapped_segment = NO_SEGMENT;

    /* Configure OPL */
    g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL3);

    /* Clear all registers, making sure envelopes decay all the way */
    for(uint16_t reg = 0x20; reg < 0xF6; reg++)
    {
        uint8_t value = (reg >= 0x60 && reg < 0xA0) ? 0xFF : 0;

        v2_set_opl3(reg, value);
        v2_set_opl3(reg + 0x100, value);
    }

    v2_set_opl3(0x001, 0x20);   /* Allow waveforms */
    v2_set_opl3(0x008, 0x00);   /* No split point */
    v2_set_opl3(0x0BD, 0x00);   /* No drums, etc. */
    g_v2_four_op = 0;
    v2_set_opl3(0x104, 0x00);   /* Everything 2-op by default */
    v2_set_opl3(0x105, 0x01);   /* OPL3 mode on */

    /* Initial values */
    memset(g_v2_channels, 0, sizeof(g_v2_channels));

    g_speed          = g_v2_initial_speed;
    g_speed_count    = 1;
    g_order_position = 0;
    g_v2_line        = 0;
    g_v2_entrances   = 0;
    g_v2_master_volume = 64;
    g_v2_track       = v2_get_track();
}

bool update_v2(void)
{
    /* Something else may have changed the mapped segment since the last update */
    g_v2_mapped_segment = NO_SEGMENT;

    /* Run riffs */
    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        v2_tick_riff(i, &g_v2_channels[i].iriff, false);
        v2_tick_riff(i, &g_v2_channels[i].riff, true);
    }

    /* Run main track */
    v2_play_line();

    /* Run effects */
    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
    {
        v2_continue_fx(i, &g_v2_channels[i].iriff.fx);
        v2_continue_fx(i, &g_v2_channels[i].riff.fx);
        v2_continue_fx(i, &g_v2_channels[i].fx);
    }

    return true;
}

void v2_set_opl3(uint16_t reg, uint8_t value)
{
    if(reg & 0x100)
        g_roboplay_interface->opl4_write_fm_2((uint8_t)reg, value);
    else
        g_roboplay_interface->opl4_write_fm_1((uint8_t)reg, value);
}

/* Write 0xB0 (frequency high/octave/key on) of the 2nd (which = 0) or 1st (which = 1) OPL channel */
void v2_set_b0(uint8_t channel, uint8_t which, uint8_t value)
{
    g_v2_channels[channel].reg_b0[which] = value;
    v2_set_opl3(0xB0 + (which ? g_v2_chan_offsets[channel] : g_v2_chan2_offsets[channel]), value);
}

/* Write 0xC0 (panning/feedback/connection), same indexing as v2_set_b0() */
void v2_set_c0(uint8_t channel, uint8_t which, uint8_t value)
{
    g_v2_channels[channel].reg_c0[which] = value;
    v2_set_opl3(0xC0 + (which ? g_v2_chan_offsets[channel] : g_v2_chan2_offsets[channel]), value);
}

/* Unpacks a single note, the results are left in g_v2_note, g_v2_octave, g_v2_inst, g_v2_effect and g_v2_param.
 * Returns true if this was the last channel of the line. */
bool v2_unpack_note(uint16_t *pos, uint8_t *last_instrument)
{
    uint8_t channel_id = read_byte(pos);

    g_v2_inst   = 0;
    g_v2_effect = 0;
    g_v2_param  = 0;

    uint8_t note = 0;
    if(channel_id & 0x40)
    {
        uint8_t n = read_byte(pos);
        note = n & 0x7F;

        /* Retrigger last instrument? */
        if(n & 0x80) g_v2_inst = *last_instrument;
    }

    if(channel_id & 0x20)
    {
        g_v2_inst = read_byte(pos);
        *last_instrument = g_v2_inst;
    }

    if(channel_id & 0x10)
    {
        g_v2_effect = read_byte(pos);
        g_v2_param  = read_byte(pos);
    }

    g_v2_note   = note & 15;
    g_v2_octave = note >> 4;

    return (channel_id & 0x80) != 0;
}

/* Get the current track as indicated by the order list */
uint16_t v2_get_track(void)
{
    if(!g_order_list_size) return 0;

    /* If at end of tune start again from the beginning */
    if(g_order_position >= g_order_list_size) g_order_position = 0;

    uint8_t track_number = peek_byte(g_v2_order_list + g_order_position);

    /* Jump marker? Multiple jump markers are not recognised as that could loop forever */
    if(track_number & 0x80)
    {
        g_order_position = track_number & 0x7F;
        if(g_order_position >= g_order_list_size) g_order_position = 0;

        track_number = peek_byte(g_v2_order_list + g_order_position) & 0x7F;
    }

    return (track_number < V2_MAX_TRACKS) ? g_v2_tracks[track_number] : 0;
}

/* Skip through the track till the given line, or the next higher one. Returns 0 if none. */
uint16_t v2_skip_to_line(uint16_t track, uint8_t line_number, bool channel_riff)
{
    for(;;)
    {
        uint8_t line_id = peek_byte(track);
        if((line_id & 0x7F) >= line_number) return track;
        if(line_id & 0x80) break;
        track++;

        /* Skip channel notes */
        uint8_t channel_id;
        do
        {
            channel_id = read_byte(&track);
            track += g_v2_note_size[(channel_id >> 4) & 7];
        } while(!(channel_id & 0x80) && !channel_riff);
    }

    return 0;
}

/* Plays one line of the current track and advances the pointers */
void v2_play_line(void)
{
    g_speed_count--;
    if(g_speed_count > 0) return;
    g_speed_count = g_speed;

    /* Reset channel effects */
    for(uint8_t i = 0; i < MAX_CHANNEL_NUMBER; i++)
        v2_reset_fx(&g_v2_channels[i].fx);

    g_v2_line_jump = -1;

    /* At the right line? */
    uint16_t track = g_v2_track;
    if(track && (peek_byte(track) & 0x7F) <= g_v2_line)
    {
        uint8_t line_id = read_byte(&track);
        uint8_t dummy_instrument = 0;

        /* Run through channels */
        bool last;
        do
        {
            uint8_t channel = peek_byte(track) & 15;

            if(channel < MAX_CHANNEL_NUMBER)
            {
                last = v2_unpack_note(&track, &g_v2_channels[channel].last_instrument);
                v2_play_note(channel, g_v2_note, g_v2_octave, g_v2_inst, g_v2_effect, g_v2_param, V2_SRC_NONE, 0);
            }
            else
            {
                /* Corrupt channel number, just skip the note */
                last = v2_unpack_note(&track, &dummy_instrument);
            }
        } while(!last);

        /* Was this the last line? */
        if(line_id & 0x80) track = 0;

        g_v2_track = track;
    }

    /* Move to next line */
    g_v2_line++;
    if(g_v2_line >= MAX_PATTERN_LINE || g_v2_line_jump >= 0)
    {
        if(g_v2_line_jump >= 0)
            g_v2_line = g_v2_line_jump;
        else
            g_v2_line = 0;

        /* Move to next track in order list */
        g_order_position++;
        g_v2_track = v2_get_track();
    }
}

void v2_play_note(uint8_t channel, int8_t note, int8_t octave, uint8_t instrument, uint8_t command, uint8_t param, uint8_t source, uint8_t op)
{
    RAD2_CHANNEL *chan = &g_v2_channels[channel];

    /* Recursion detector, needed as riffs can trigger other riffs and could end up in a loop */
    if(g_v2_entrances >= V2_MAX_ENTRANCES) return;
    g_v2_entrances++;

    /* Select which effects source we're using */
    RAD2_EFFECTS *fx = &chan->fx;
    if(source == V2_SRC_RIFF)
        fx = &chan->riff.fx;
    else if(source == V2_SRC_IRIFF)
        fx = &chan->iriff.fx;

    bool transposing = false;

    /* For tone slides the note is the target */
    if(command == CMD_TONE_SLIDE)
    {
        if(note > 0 && note <= 12)
        {
            fx->tone_slide_oct  = octave;
            fx->tone_slide_freq = g_note_frequencies[note - 1];
        }

        v2_start_tone_slide(channel, fx, param);

        g_v2_entrances--;
        return;
    }

    /* Playing a new instrument? */
    if(instrument > 0 && instrument <= V2_MAX_INSTRUMENTS && g_v2_instruments[instrument - 1])
    {
        uint16_t instrument_offset = g_v2_instruments[instrument - 1];
        uint8_t  old_instrument = chan->instrument;
        uint8_t  algorithm_byte = peek_byte(instrument_offset);

        chan->instrument = instrument;
        chan->algorithm  = algorithm_byte & 7;

        /* Ignore MIDI instruments */
        if(chan->algorithm == 7)
        {
            g_v2_entrances--;
            return;
        }

        v2_load_instrument(channel, instrument_offset);

        /* Bounce the channel */
        chan->key_flags |= V2_KEY_OFF | V2_KEY_ON;

        v2_reset_fx(&chan->iriff.fx);

        if(source != V2_SRC_IRIFF || instrument != old_instrument)
        {
            /* Instrument riff? The riff speed is the low nibble of the 3rd instrument byte, the
             * riff itself follows the 24 byte instrument and its 2 byte size */
            uint8_t riff_speed = peek_byte(instrument_offset + 2) & 15;

            if((algorithm_byte & 0x80) && riff_speed > 0)
            {
                chan->iriff.track = chan->iriff.track_start = instrument_offset + 24 + 2;
                chan->iriff.line = 0;
                chan->iriff.speed = riff_speed;
                chan->iriff.last_instrument = 0;

                /* Note given with riff command is used to transpose the riff */
                if(note >= 1 && note <= 12)
                {
                    chan->iriff.transpose_octave = octave;
                    chan->iriff.transpose_note = note;
                    transposing = true;
                }
                else
                {
                    chan->iriff.transpose_octave = 3;
                    chan->iriff.transpose_note = 12;
                }

                /* Do first tick of riff */
                chan->iriff.speed_count = 1;
                v2_tick_riff(channel, &chan->iriff, false);
            }
            else
                chan->iriff.speed_count = 0;
        }
    }

    /* Starting a channel riff? */
    if(command == CMD_RIFF || command == CMD_TRANSPOSE)
    {
        v2_reset_fx(&chan->riff.fx);

        uint8_t p0 = param / 10;
        uint8_t p1 = param % 10;

        chan->riff.track = (p1 > 0 && p0 < V2_MAX_RIFF_TRACKS) ? g_v2_riffs[p0][p1 - 1] : 0;
        if(chan->riff.track)
        {
            chan->riff.track_start = chan->riff.track;
            chan->riff.line = 0;
            chan->riff.speed = g_speed;
            chan->riff.last_instrument = 0;

            /* Note given with riff command is used to transpose the riff */
            if(command == CMD_TRANSPOSE && note >= 1 && note <= 12)
            {
                chan->riff.transpose_octave = octave;
                chan->riff.transpose_note = note;
                transposing = true;
            }
            else
            {
                chan->riff.transpose_octave = 3;
                chan->riff.transpose_note = 12;
            }

            /* Do first tick of riff */
            chan->riff.speed_count = 1;
            v2_tick_riff(channel, &chan->riff, true);
        }
        else
            chan->riff.speed_count = 0;
    }

    /* Play the note */
    if(!transposing && note > 0)
    {
        /* Key-off? */
        if(note == KEY_OFF) chan->key_flags |= V2_KEY_OFF;

        if(!chan->instrument || chan->algorithm < 7)
            v2_play_note_opl3(channel, octave, note);
    }

    /* Process effect */
    switch(command)
    {
        case CMD_SET_VOL:
            v2_set_volume(channel, param);
            break;

        case CMD_SET_SPEED:
            if(source == V2_SRC_NONE)
            {
                g_speed = param;
                g_speed_count = param;
            }
            else if(source == V2_SRC_RIFF)
            {
                chan->riff.speed = param;
                chan->riff.speed_count = param;
            }
            else
            {
                chan->iriff.speed = param;
                chan->iriff.speed_count = param;
            }
            break;

        case CMD_PORTAMENTO_UP:
            fx->port_slide = param;
            break;

        case CMD_PORTAMENTO_DOWN:
            fx->port_slide = -(int8_t)param;
            break;

        case CMD_TONE_VOL_SLIDE:
        case CMD_VOL_SLIDE:
        {
            int8_t value = (int8_t)param;
            if(value >= 50) value = -(value - 50);
            fx->vol_slide = value;

            if(command == CMD_TONE_VOL_SLIDE)
                v2_start_tone_slide(channel, fx, param);
            break;
        }

        case CMD_JUMP_TO_LINE:
            if(param >= MAX_PATTERN_LINE) break;

            /* Jump commands in riffs are checked for within v2_tick_riff() */
            if(source == V2_SRC_NONE) g_v2_line_jump = param;
            break;

        case CMD_OP_MULTIPLIER:
            if(source == V2_SRC_IRIFF) v2_load_op_multiplier(channel, op, param);
            break;

        case CMD_OP_VOLUME:
            if(source == V2_SRC_IRIFF) v2_load_op_volume(channel, op, param);
            break;

        case CMD_FEEDBACK:
            if(source == V2_SRC_IRIFF) v2_load_feedback(channel, param / 10, param % 10);
            break;
    }

    g_v2_entrances--;
}

/* Sets the OPL3 registers for the given instrument and caches what is needed later */
void v2_load_instrument(uint8_t channel, uint16_t instrument_offset)
{
    RAD2_CHANNEL *chan = &g_v2_channels[channel];
    uint16_t pos = instrument_offset;

    uint8_t b = read_byte(&pos);
    uint8_t algorithm = b & 7;
    uint8_t panning_0 = (b >> 3) & 3;
    uint8_t panning_1 = (b >> 5) & 3;

    b = read_byte(&pos);
    uint8_t feedback_0 = b & 15;
    uint8_t feedback_1 = b >> 4;

    b = read_byte(&pos);
    uint8_t detune = b >> 4;

    uint8_t volume = read_byte(&pos);

    chan->volume   = volume;
    chan->detune_a = (detune + 1) >> 1;
    chan->detune_b = detune >> 1;

    /* Turn on 4-op mode for algorithms 2 and 3 (algorithms 4 to 6 are simulated with 2-op mode) */
    if(channel < 6)
    {
        uint8_t mask = 1 << channel;
        g_v2_four_op = (g_v2_four_op & ~mask) | ((algorithm == 2 || algorithm == 3) ? mask : 0);
        v2_set_opl3(0x104, g_v2_four_op);
    }

    /* Left/right/feedback/algorithm */
    v2_set_c0(channel, 1, ((panning_1 ^ 3) << 4) | (feedback_1 << 1) | ((algorithm == 3 || algorithm == 5 || algorithm == 6) ? 1 : 0));
    v2_set_c0(channel, 0, ((panning_0 ^ 3) << 4) | (feedback_0 << 1) | ((algorithm == 1 || algorithm == 6) ? 1 : 0));

    /* Load the operators */
    for(uint8_t i = 0; i < 4; i++)
    {
        uint8_t op[5];
        for(uint8_t j = 0; j < 5; j++)
            op[j] = read_byte(&pos);

        /* 2-op algorithms don't use the last two operators */
        if(algorithm < 2 && i >= 2)
        {
            op[0] = 0;
            op[1] = 0x3F;
            op[2] = 0;
            op[3] = 0xF0;
            op[4] = 0;
        }

        chan->op_volume[i] = op[1];
        chan->reg_20[i] = op[0];

        uint16_t reg = g_v2_op_offsets[channel][i];
        uint16_t vol = ~op[1] & 0x3F;

        /* Do volume scaling for carriers */
        if(g_v2_alg_carriers[algorithm][i])
        {
            vol = (vol * volume) >> 6;
            vol = (vol * g_v2_master_volume) >> 6;
        }

        op[1] = (op[1] & 0xC0) | ((vol ^ 0x3F) & 0x3F);

        for(uint8_t j = 0; j < 5; j++)
            v2_set_opl3(reg + g_v2_op_regs[j], op[j]);
    }
}

/* Play the note on the OPL3 */
void v2_play_note_opl3(uint8_t channel, int8_t octave, int8_t note)
{
    RAD2_CHANNEL *chan = &g_v2_channels[channel];

    uint16_t o1 = g_v2_chan_offsets[channel];
    uint16_t o2 = g_v2_chan2_offsets[channel];

    /* Key off the channel */
    if(chan->key_flags & V2_KEY_OFF)
    {
        chan->key_flags &= (uint8_t)~(V2_KEY_OFF | V2_KEYED_ON);

        v2_set_b0(channel, 1, chan->reg_b0[1] & ~0x20);
        v2_set_b0(channel, 0, chan->reg_b0[0] & ~0x20);
    }

    if(note == KEY_OFF || note > 12) return;

    bool op4 = (chan->instrument && chan->algorithm >= 2);

    uint16_t freq = g_note_frequencies[note - 1];
    uint16_t frq2 = freq;

    chan->curr_freq   = freq;
    chan->curr_octave = octave;

    /* Detune. Both channels are detuned in opposite directions so the note retains its tuning */
    freq += chan->detune_a;
    frq2 -= chan->detune_b;

    /* Frequency low byte */
    if(op4) v2_set_opl3(0xA0 + o1, frq2 & 0xFF);
    v2_set_opl3(0xA0 + o2, freq & 0xFF);

    /* Frequency high bits + octave + key on */
    if(chan->key_flags & V2_KEY_ON)
        chan->key_flags = (chan->key_flags & (uint8_t)~V2_KEY_ON) | V2_KEYED_ON;

    uint8_t key_on = (chan->key_flags & V2_KEYED_ON) ? 0x20 : 0;

    if(op4)
        v2_set_b0(channel, 1, (frq2 >> 8) | (octave << 2) | key_on);
    else
        v2_set_b0(channel, 1, 0);

    v2_set_b0(channel, 0, (freq >> 8) | (octave << 2) | key_on);
}

/* Prepare effects for a new line */
void v2_reset_fx(RAD2_EFFECTS *fx)
{
    fx->port_slide    = 0;
    fx->vol_slide     = 0;
    fx->tone_slide_dir = 0;
}

/* Tick the channel riff or instrument riff */
void v2_tick_riff(uint8_t channel, RAD2_RIFF *riff, bool channel_riff)
{
    uint8_t line_id;

    if(riff->speed_count == 0)
    {
        v2_reset_fx(&riff->fx);
        return;
    }

    riff->speed_count--;
    if(riff->speed_count > 0) return;
    riff->speed_count = riff->speed;

    uint8_t line = riff->line++;
    if(riff->line >= MAX_PATTERN_LINE) riff->speed_count = 0;

    v2_reset_fx(&riff->fx);

    /* Is this the current line in the track? */
    uint16_t track = riff->track;
    if(track && (peek_byte(track) & 0x7F) == line)
    {
        line_id = read_byte(&track);

        if(channel_riff)
        {
            /* Channel riff: play current note */
            v2_unpack_note(&track, &riff->last_instrument);
            v2_transpose(riff->transpose_note, riff->transpose_octave);
            v2_play_note(channel, g_v2_note, g_v2_octave, g_v2_inst, g_v2_effect, g_v2_param, V2_SRC_RIFF, 0);
        }
        else
        {
            /* Instrument riff: here each track channel is an extra effect that can run, but is not
             * actually a different physical channel */
            bool last;
            do
            {
                uint8_t column = peek_byte(track) & 15;

                last = v2_unpack_note(&track, &riff->last_instrument);
                if(g_v2_effect != CMD_IGNORE)
                    v2_transpose(riff->transpose_note, riff->transpose_octave);

                v2_play_note(channel, g_v2_note, g_v2_octave, g_v2_inst, g_v2_effect, g_v2_param,
                             V2_SRC_IRIFF, column > 0 ? (column - 1) & 3 : 0);
            } while(!last);
        }

        /* Last line? */
        if(line_id & 0x80) track = 0;

        riff->track = track;
    }

    /* Special case; if next line has a jump command, run it now */
    if(!track) return;

    uint16_t next = track;
    if((read_byte(&next) & 0x7F) != riff->line) return;

    v2_unpack_note(&next, &line_id);    /* line_id is just a dummy here */
    if(g_v2_effect == CMD_JUMP_TO_LINE && g_v2_param < MAX_PATTERN_LINE)
    {
        riff->line = g_v2_param;
        riff->track = v2_skip_to_line(riff->track_start, g_v2_param, channel_riff);
    }
}

/* Continue any effects that operate continuously (e.g. slides) */
void v2_continue_fx(uint8_t channel, RAD2_EFFECTS *fx)
{
    if(fx->port_slide)
        v2_portamento(channel, fx, fx->port_slide, false);

    if(fx->vol_slide)
    {
        int8_t volume = g_v2_channels[channel].volume;
        volume -= fx->vol_slide;
        if(volume < 0) volume = 0;

        v2_set_volume(channel, volume);
    }

    if(fx->tone_slide_dir)
        v2_portamento(channel, fx, fx->tone_slide_dir, true);
}

/* Sets the volume of the given channel */
void v2_set_volume(uint8_t channel, uint8_t volume)
{
    RAD2_CHANNEL *chan = &g_v2_channels[channel];

    /* Ensure volume is within range */
    if(volume > 64) volume = 64;

    chan->volume = volume;

    /* Scale volume to master volume */
    volume = ((uint16_t)volume * g_v2_master_volume) >> 6;

    if(!chan->instrument || chan->algorithm >= 7) return;

    /* Set volume of all carriers */
    for(uint8_t i = 0; i < 4; i++)
    {
        if(!g_v2_alg_carriers[chan->algorithm][i]) continue;

        uint8_t  op_volume = ((uint16_t)((chan->op_volume[i] & 63) ^ 63) * volume) >> 6;

        v2_set_opl3(0x40 + g_v2_op_offsets[channel][i], (chan->op_volume[i] & 0xC0) | (op_volume ^ 0x3F));
    }
}

/* Starts a tone slide */
void v2_start_tone_slide(uint8_t channel, RAD2_EFFECTS *fx, uint8_t speed)
{
    if(speed) fx->tone_slide_speed = speed;

    v2_get_slide_dir(channel, fx);
}

void v2_get_slide_dir(uint8_t channel, RAD2_EFFECTS *fx)
{
    RAD2_CHANNEL *chan = &g_v2_channels[channel];

    int8_t speed = (int8_t)fx->tone_slide_speed;
    if(speed > 0)
    {
        uint8_t  octave = fx->tone_slide_oct;
        uint16_t frequency = fx->tone_slide_freq;

        uint16_t old_frequency = chan->curr_freq;
        uint8_t  old_octave = chan->curr_octave;

        if(old_octave > octave)
            speed = -speed;
        else if(old_octave == octave)
        {
            if(old_frequency > frequency)
                speed = -speed;
            else if(old_frequency == frequency)
                speed = 0;
        }
    }

    fx->tone_slide_dir = speed;
}

/* Load multiplier value into operator */
void v2_load_op_multiplier(uint8_t channel, uint8_t op, uint8_t multiplier)
{
    RAD2_CHANNEL *chan = &g_v2_channels[channel];

    chan->reg_20[op] = (chan->reg_20[op] & 0xF0) | (multiplier & 15);
    v2_set_opl3(0x20 + g_v2_op_offsets[channel][op], chan->reg_20[op]);
}

/* Load volume value into operator */
void v2_load_op_volume(uint8_t channel, uint8_t op, uint8_t volume)
{
    v2_set_opl3(0x40 + g_v2_op_offsets[channel][op], (g_v2_channels[channel].op_volume[op] & 0xC0) | ((volume & 0x3F) ^ 0x3F));
}

/* Load feedback value into instrument (which = 0: 2nd OPL channel, 1: 1st OPL channel) */
void v2_load_feedback(uint8_t channel, uint8_t which, uint8_t feedback)
{
    if(which < 2)
        v2_set_c0(channel, which, (g_v2_channels[channel].reg_c0[which] & 0x31) | ((feedback & 7) << 1));
}

/* Adjusts the pitch of the channel's note. There may also be a limiting value (for tone slides). */
void v2_portamento(uint8_t channel, RAD2_EFFECTS *fx, int8_t amount, bool tone_slide)
{
    RAD2_CHANNEL *chan = &g_v2_channels[channel];

    uint16_t frequency = chan->curr_freq;
    uint8_t  octave = chan->curr_octave;

    frequency += amount;

    if(frequency < FREQ_START)
    {
        if(octave > OCTAVE_START)
        {
            octave--;
            frequency += FREQ_RANGE;
        }
        else frequency = FREQ_START;
    }
    else if(frequency > FREQ_END)
    {
        if(octave < OCTAVE_END)
        {
            octave++;
            frequency -= FREQ_RANGE;
        }
        else frequency = FREQ_END;
    }

    if(tone_slide)
    {
        if(amount >= 0)
        {
            if(octave > fx->tone_slide_oct || (octave == fx->tone_slide_oct && frequency >= fx->tone_slide_freq))
            {
                frequency = fx->tone_slide_freq;
                octave = fx->tone_slide_oct;
            }
        }
        else
        {
            if(octave < fx->tone_slide_oct || (octave == fx->tone_slide_oct && frequency <= fx->tone_slide_freq))
            {
                frequency = fx->tone_slide_freq;
                octave = fx->tone_slide_oct;
            }
        }
    }

    chan->curr_freq = frequency;
    chan->curr_octave = octave;

    /* Apply detunes */
    uint16_t frq2 = frequency - chan->detune_b;
    frequency += chan->detune_a;

    /* Write value back to OPL3 */
    v2_set_opl3(0xA0 + g_v2_chan2_offsets[channel], frequency & 0xFF);
    v2_set_b0(channel, 0, ((frequency >> 8) & 3) | (octave << 2) | (chan->reg_b0[0] & 0xE0));

    v2_set_opl3(0xA0 + g_v2_chan_offsets[channel], frq2 & 0xFF);
    v2_set_b0(channel, 1, ((frq2 >> 8) & 3) | (octave << 2) | (chan->reg_b0[1] & 0xE0));
}

/* Transpose the note returned by v2_unpack_note().
 * Note: due to RAD's legacy middle C is octave 3 note number 12. */
void v2_transpose(int8_t note, int8_t octave)
{
    if(g_v2_note >= 1 && g_v2_note <= 12)
    {
        int8_t octave_shift = octave - 3;
        if(octave_shift != 0)
        {
            g_v2_octave += octave_shift;
            if(g_v2_octave < 0)
                g_v2_octave = 0;
            else if(g_v2_octave > 7)
                g_v2_octave = 7;
        }

        int8_t note_shift = note - 12;
        if(note_shift != 0)
        {
            g_v2_note += note_shift;
            if(g_v2_note < 1)
            {
                g_v2_note += 12;
                if(g_v2_octave > 0)
                    g_v2_octave--;
                else
                    g_v2_note = 1;
            }
        }
    }
}