/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mid.c
 *
 * MID: Standard MIDI file player on external device
 */

#include <string.h>

#include "player_interface.h"
#include "mid_ext.h"

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
    g_volume_boost = 16;

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
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    /* Use a fixed replay rate of 60 Hz */
    static const float rate = 60.0;

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
    return "Standard MIDI file player (External device) V1.1 by RoboSoft Inc.";
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

void change_speed(uint32_t speed)
{
  g_qnote_duration = speed;
  g_roboplay_interface->update_refresh();
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
    uint8_t midi_command = data_byte_1 >> 4;

    switch(midi_command)
    {
      case 0x8:  // Note OFF
        data_byte_3 = read_byte();
        g_roboplay_interface->midi_send_data_3(data_byte_1, data_byte_2, data_byte_3);
        break;
      case 0x9:  // Note ON
        data_byte_3 = read_byte();
        g_roboplay_interface->midi_send_data_3(data_byte_1, data_byte_2, data_byte_3);
        break;
      case 0xA:  // Key after-touch
        data_byte_3 = read_byte();
        g_roboplay_interface->midi_send_data_3(data_byte_1, data_byte_2, data_byte_3);
        break;
      case 0xB:  // Control change
        data_byte_3 = read_byte();
        g_roboplay_interface->midi_send_data_3(data_byte_1, data_byte_2, data_byte_3);
        break;
      case 0xC:  // Program change
        g_roboplay_interface->midi_send_data_2(data_byte_1, data_byte_2);
        break;
      case 0xD:  // Channel after-touch
        data_byte_2 = read_byte();               
        g_roboplay_interface->midi_send_data_2(data_byte_1, data_byte_2);
        break;
      case 0xE:  // Pitch wheel
        data_byte_3 = read_byte();
        g_roboplay_interface->midi_send_data_3(data_byte_1, data_byte_2, data_byte_3);
        break;
      }
  }
}

void handle_meta_event(uint8_t sub_type)
{
  uint32_t length = get_variable_len();  
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
  uint32_t length = get_variable_len();
  
  g_roboplay_interface->midi_send_data_1(0xF0);

  uint8_t data;
  do
  {
    data = read_byte();
    g_roboplay_interface->midi_send_data_1(data);
  } while(data != 0xF7);
}

