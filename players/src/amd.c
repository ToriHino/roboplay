/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * amd.c
 *
 * AMD: AMUSiC Adlib Tracker player
 */

#include <string.h>

#include "player_interface.h"
#include "amd.h"

bool load(const char* file_name)
{
  g_roboplay_interface->open(file_name, false);

  g_roboplay_interface->read(&g_amd_header, sizeof(AMD_HEADER));

  if(strncmp(g_amd_header.id, "<o\xefQU\xeeRoR", ID_SIZE) &&
     strncmp(g_amd_header.id, "MaDoKaN96", ID_SIZE))
  {
    g_roboplay_interface->close();
    return false;
  }

  strncpy(g_song_name, g_amd_header.title, NAME_SIZE);
  g_song_name[NAME_SIZE] = '\0';

  strncpy(g_author, g_amd_header.author, NAME_SIZE);
  g_author[NAME_SIZE] = '\0';

  for(uint8_t i = 0; i < MAX_PATTERN_NUMBER + 1; i++)
  {
    g_segments[i].isAllocated = false;
  }

  uint8_t segment = 0;
  g_segments[segment].allocated_segment = START_SEGMENT_INDEX;
  g_segments[segment].isAllocated = true;

  if(g_amd_header.version == PACKED_MODULE)
  {
    g_roboplay_interface->read(g_track_order, (g_amd_header.max_pattern_number + 1) * NUMBER_OF_CHANNELS * sizeof(uint16_t));
    g_roboplay_interface->read(&g_number_of_tracks, sizeof(uint16_t));

    for(uint8_t i = 0; i < g_number_of_tracks; i++)
    {
      uint16_t track_position;
      g_roboplay_interface->read(&track_position, sizeof(uint16_t));

      uint8_t pattern_nr = track_position / NUMBER_OF_CHANNELS;
      uint8_t channel = track_position % NUMBER_OF_CHANNELS;
      
      segment = pattern_nr / PATTERNS_PER_SEGMENT;
      if(!g_segments[segment].isAllocated)
      {
        g_segments[segment].allocated_segment = g_roboplay_interface->get_new_segment();
        g_segments[segment].isAllocated = true;
      }
      g_roboplay_interface->set_segment(g_segments[segment].allocated_segment);

      AMD_PATTERN* pattern = (AMD_PATTERN *)(DATA_SEGMENT_BASE + (pattern_nr % PATTERNS_PER_SEGMENT) * sizeof(AMD_PATTERN));
      uint8_t track_line = 0;
      do
      {
        uint8_t buffer;
        g_roboplay_interface->read(&buffer, 1);
        if(buffer & 0x80)
        {
          for(uint8_t t = track_line; t < track_line + (buffer & 0x7f) && t < (MAX_PATTERN_LINE + 1); t++)
          {
            pattern->lines[t][channel].data_1 = 0x00;
            pattern->lines[t][channel].data_2 = 0x00;
            pattern->lines[t][channel].data_3 = 0x00;
          }
          track_line += buffer & 0x7f;
        }
        else
        {
          pattern->lines[track_line][channel].data_1 = buffer;
          g_roboplay_interface->read(&pattern->lines[track_line][channel].data_2, 1);
          g_roboplay_interface->read(&pattern->lines[track_line][channel].data_3, 1);

          track_line++;
        }
      } while (track_line <= MAX_PATTERN_LINE);
    }
  }
  else if(g_amd_header.version == NORMAL_MODULE)
  {
    for(uint16_t i = 0; i < 64 * NUMBER_OF_CHANNELS; i++)
      g_track_order[i / NUMBER_OF_CHANNELS][i % NUMBER_OF_CHANNELS] = i;

    uint16_t bytes_left = (g_amd_header.max_pattern_number + 1) * NUMBER_OF_CHANNELS * EVENT_SIZE * (MAX_PATTERN_LINE + 1);
    while(bytes_left)
    {
      uint16_t bytes_read = g_roboplay_interface->read((void *)DATA_SEGMENT_BASE, PATTERNS_PER_SEGMENT * NUMBER_OF_CHANNELS * EVENT_SIZE * (MAX_PATTERN_LINE + 1));

      if(bytes_left > bytes_read)
        bytes_left -= bytes_read;
      else
        bytes_left = 0;

      if(bytes_left)
      {
        segment++;
        g_segments[segment].allocated_segment = g_roboplay_interface->get_new_segment();
        g_segments[segment].isAllocated = true;
        g_roboplay_interface->set_segment(g_segments[segment].allocated_segment);
      }
    }
  }
  else
  {
    g_roboplay_interface->close();
    return false;
  }

  g_roboplay_interface->close();

  return true;
}

bool update(void)
{
  g_delay--;
  if(g_delay)
  {
    /* Just handle the FX */
    handle_fx();
  }
  else
  {
    for(uint8_t i = 0; i < NUMBER_OF_CHANNELS; i++)
    {
      uint16_t track_position = g_track_order[g_amd_header.patterns[g_position]][i];

      uint8_t pattern_nr = track_position / NUMBER_OF_CHANNELS;
      uint8_t track_channel = track_position % NUMBER_OF_CHANNELS;
      uint8_t segment = pattern_nr / PATTERNS_PER_SEGMENT;

      g_roboplay_interface->set_segment(g_segments[segment].allocated_segment);

      AMD_PATTERN* pattern = (AMD_PATTERN *)(DATA_SEGMENT_BASE + (pattern_nr % PATTERNS_PER_SEGMENT) * sizeof(AMD_PATTERN));

      channel_next_event(i);
      channel_update(i);
    }

    g_delay = g_speed;

    if(g_break_state)
    {
      g_break_state = false;
      g_position = g_break_pattern;
      g_row = g_break_line;
    }
    else
    {
      g_row++;
    }

    if(g_row > MAX_PATTERN_LINE)
    {
      g_position++;
      g_row = 0;
    }

    if(g_position >= g_amd_header.length)
    {
      g_row = 0;
      g_position = 0;
    }
  }

  return true;
}

void rewind(const uint8_t subsong)
{
  subsong;

  /* Configure OPL */
  g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL3);

  g_roboplay_interface->opl4_write_fm_1(0x01,  0x20); /* Allow waveforms */
  g_roboplay_interface->opl4_write_fm_1(0x08,  0x00); /* No split point */
  
  g_regbd = 0x00;
  g_roboplay_interface->opl4_write_fm_1(0xBD,  g_regbd); /* No drums, etc. */

  g_roboplay_interface->set_segment(START_SEGMENT_INDEX);

  g_position = 0;
  g_row      = 0;

  g_break_state = false;
  g_break_line = 0;
  g_break_pattern = 0;

  g_refresh  = DEFAULT_REFRESH;
  g_speed    = DEFAULT_SPEED;
  g_delay    = 1;

  for(uint8_t i = 0; i < NUMBER_OF_CHANNELS; i++)
  {
    g_last_events[i].instrument = 0;
    g_last_events[i].volume = 0;
    g_last_events[i].pitch = 0;
    g_last_events[i].octave = 0;

    g_last_events[i].frequency = 0;
    g_last_events[i].portamento_frequency = 0;

    g_last_events[i].arpeggio = 0;

    g_last_events[i].command = 0;
    g_last_events[i].parameters = 0;

    g_last_events[i].carrier_volume = 0;
    g_last_events[i].modulator_volume = 0;
  }
}

void command(const uint8_t id)
{
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
  return "AMUSiC Adlib Tracker player V1.0 by RoboSoft Inc.";
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
  if(g_amd_header.version == PACKED_MODULE)
    return "Packed module";
  else
    return "Unpacked module";
}

void channel_next_event(const uint8_t channel)
{
  uint16_t track_position = g_track_order[g_amd_header.patterns[g_position]][channel];

  uint8_t pattern_nr = track_position / NUMBER_OF_CHANNELS;
  uint8_t track_channel = track_position % NUMBER_OF_CHANNELS;

  uint8_t segment = pattern_nr / PATTERNS_PER_SEGMENT;
  g_roboplay_interface->set_segment(g_segments[segment].allocated_segment);

  AMD_PATTERN* pattern = (AMD_PATTERN *)(DATA_SEGMENT_BASE + (pattern_nr % PATTERNS_PER_SEGMENT) * sizeof(AMD_PATTERN));
  AMD_EVENT*   event   = &pattern->lines[g_row][track_channel];

  g_current_event.pitch = event->data_3 >> 4;
  g_current_event.octave = (event->data_3 & 0x0e) >> 1;
  g_current_event.instrument = (event->data_2 >> 4) + ((event->data_3 & 1) << 4);
  g_current_event.command = event->data_2 & 0x0f;
  g_current_event.parameters = event->data_1 & 0x7f;
}

void channel_update(const uint8_t channel)
{
  /* Instrument change? */
  if(g_current_event.instrument)
  {
    g_last_events[channel].instrument = g_current_event.instrument;
    g_last_events[channel].volume     = 0x3f;
  }

  /* Pitch change? */
  if(g_current_event.pitch && g_current_event.command != CMD_PORTAMENTO)
  {
    g_last_events[channel].pitch = g_current_event.pitch;  
    g_last_events[channel].octave = g_current_event.octave;

    g_last_events[channel].frequency = get_frequency(g_current_event.pitch, g_current_event.octave);
  }
  else channel_set_volume(channel);

  /* Check the FX */
  g_last_events[channel].command = g_current_event.command;
  g_last_events[channel].parameters = g_current_event.parameters;

  switch(g_current_event.command)
  {
    case CMD_ARPEGGIO:
      init_arpeggio(channel);
      break;
    case CMD_TONE_SLIDE_UP:
      slide_frequency_up(channel);
      break;
    case CMD_TONE_SLIDE_DOWN:
      slide_frequency_down(channel);
      break;
    case CMD_SET_VOL:
      set_instrument_volume(channel);
      break;
    case CMD_JUMP_TO_PATTERN:
      jump_pattern();
      break;
    case CMD_PATTERN_BREAK:
      pattern_break();
      break;
    case CMD_SET_SPEED:
      set_song_speed();
      break;
    case CMD_PORTAMENTO:
      init_tone_portamento(channel);
      break;
    case CMD_EXTENDED:
      extended_command(channel);
      break;
  }

  if(g_current_event.pitch && g_current_event.command != CMD_PORTAMENTO)
    channel_play_note(channel);
}

void channel_set_volume(const uint8_t channel)
{
  AMD_INSTRUMENT *instrument = &g_amd_header.instruments[g_last_events[channel].instrument - 1];

  uint8_t volume = (g_volume_table[g_last_events[channel].volume] ^ 0x3f) + (g_last_events[channel].carrier_volume & 0x3f);
  if (volume > 0x3f) volume = 0x3f;

  g_roboplay_interface->opl4_write_fm_1(g_channel_offsets[channel] + 0x23, volume | (instrument->data[6] & 0xC0));
  g_roboplay_interface->opl4_write_fm_2(g_channel_offsets[channel] + 0x23, volume | (instrument->data[6] & 0xC0));

  if(instrument->data[10] & 1)
  {
    volume = (g_volume_table[g_last_events[channel].volume] ^ 0x3f) + (g_last_events[channel].modulator_volume & 0x3f);
    if (volume > 0x3f) volume = 0x3f;

    g_roboplay_interface->opl4_write_fm_1(g_channel_offsets[channel] + 0x20, volume | (instrument->data[1] & 0xC0));
    g_roboplay_interface->opl4_write_fm_2(g_channel_offsets[channel] + 0x20, volume | (instrument->data[1] & 0xC0));
  }
  else
  {
    volume = g_last_events[channel].modulator_volume & 0x3f;

    g_roboplay_interface->opl4_write_fm_1(g_channel_offsets[channel] + 0x20, volume | (instrument->data[1] & 0xC0));
    g_roboplay_interface->opl4_write_fm_2(g_channel_offsets[channel] + 0x20, volume | (instrument->data[1] & 0xC0));
  }
}

void channel_play_note(const uint8_t channel)
{
    AMD_INSTRUMENT *instrument = &g_amd_header.instruments[g_current_event.instrument - 1];

    g_last_events[channel].modulator_volume = instrument->data[1] & 0x3f;
    g_last_events[channel].carrier_volume   = instrument->data[6] & 0x3f;

    if(g_current_event.command == CMD_INTENSITY)
      set_carrier_modulator_volume(channel);

    uint8_t reg = g_channel_offsets[channel];
    uint8_t offset = 0;
    for(uint8_t i = 0; i < 4; i++)
    {
        g_roboplay_interface->opl4_write_fm_1(reg, instrument->data[offset]);
        g_roboplay_interface->opl4_write_fm_2(reg, instrument->data[offset]);
        reg += 0x03;
        g_roboplay_interface->opl4_write_fm_1(reg, instrument->data[offset + 5]);
        g_roboplay_interface->opl4_write_fm_2(reg, instrument->data[offset + 5]);
        reg += 0x20 - 0x03;
        offset++;
    }

    reg += 0x40;
    g_roboplay_interface->opl4_write_fm_1(reg, instrument->data[offset]);
    g_roboplay_interface->opl4_write_fm_2(reg, instrument->data[offset]);
    reg += 0x03;
    g_roboplay_interface->opl4_write_fm_1(reg, instrument->data[offset + 5]);
    g_roboplay_interface->opl4_write_fm_2(reg, instrument->data[offset + 5]);
    offset++;

    channel_set_volume(channel);

    g_roboplay_interface->opl4_write_fm_1(0xC0 + channel, instrument->data[offset + 5] | PAN_SETTING_LEFT);
    g_roboplay_interface->opl4_write_fm_2(0xC0 + channel, instrument->data[offset + 5] | PAN_SETTING_RIGHT);

    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, 0x00);
    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, 0x00);
    set_frequency(channel, g_last_events[channel].frequency);
}

void handle_fx(void)
{
  for(uint8_t i = 0; i < NUMBER_OF_CHANNELS; i++)
  {
    switch(g_last_events[i].command)
    {
      case CMD_ARPEGGIO:
        arpeggio(i);
        break;
      case CMD_TONE_SLIDE_UP:
        slide_frequency_up(i);
        break;
      case CMD_TONE_SLIDE_DOWN:
        slide_frequency_down(i);
        break;
      case CMD_PORTAMENTO:
        tone_portamento(i);
      case CMD_EXTENDED:
        switch(g_last_events[i].parameters / 10)
        {
          case CMD_EXT_SLIDE_VOL_UP_FAST:
            slide_volume_up(i);
            break;
          case CMD_EXT_SLIDE_VOL_DOWN_FAST:
            slide_volume_down(i);
            break;
        }
        break;
    }
  }
}

void arpeggio(const uint8_t channel)
{
  if(g_last_events[channel].parameters)
  {
    if(++g_last_events[channel].arpeggio > 2)
      g_last_events[channel].arpeggio = 0;

    uint8_t pitch = g_last_events[channel].pitch;
    switch(g_last_events[channel].arpeggio)
    {
      case 1:
        if(g_delay > 1)
          pitch += g_last_events[channel].parameters / 10;
        break;
      case 2:
        if(g_delay > 2)
          pitch += g_last_events[channel].parameters % 10;
    }

    uint8_t octave = g_last_events[channel].octave;
    if(pitch > 12)
    {
      pitch -= 12;
      octave++;

      if(octave > 7)
      {
        octave = 7;
        pitch = 12;
      }
    }

    int16_t frequency = get_frequency(pitch, octave);
    set_frequency(channel, frequency);
  }
}

void tone_portamento(const uint8_t channel)
{
  if(g_last_events[channel].frequency > g_last_events[channel].portamento_frequency)
  {
      g_last_events[channel].frequency -= g_last_events[channel].parameters;
      if(g_last_events[channel].frequency < g_last_events[channel].portamento_frequency)
        g_last_events[channel].frequency = g_last_events[channel].portamento_frequency;
  }
  else
  {
      g_last_events[channel].frequency += g_last_events[channel].parameters;
      if(g_last_events[channel].frequency > g_last_events[channel].portamento_frequency)
        g_last_events[channel].frequency = g_last_events[channel].portamento_frequency;
  }
  set_frequency(channel, g_last_events[channel].frequency);
}

void init_arpeggio(const uint8_t channel)
{
  g_last_events[channel].arpeggio = 0;
}

void slide_frequency_up(const uint8_t channel)
{
  g_last_events[channel].frequency += g_last_events[channel].parameters;
  
  if(g_last_events[channel].frequency > (FREQ_RANGE * 7) + 305)
  {
    g_last_events[channel].frequency = (FREQ_RANGE * 7) + 305;
  }

  set_frequency(channel, g_last_events[channel].frequency);
}

void slide_frequency_down(const uint8_t channel)
{
  g_last_events[channel].frequency -= g_last_events[channel].parameters;
  
  if(g_last_events[channel].frequency < 0)
  {
    g_last_events[channel].frequency = 0;
  }

  set_frequency(channel, g_last_events[channel].frequency);
}

void set_instrument_volume(const uint8_t channel)
{
  g_last_events[channel].volume = g_current_event.parameters;
  channel_set_volume(channel);
}

void jump_pattern(void)
{
  g_break_line = g_row + 2;
  g_break_pattern = g_current_event.parameters;
  g_break_state = true;
}

void pattern_break(void)
{
  g_break_line = g_current_event.parameters & 0x3f;
  g_break_pattern = g_position + 1;
  g_break_state = true;
}

void set_song_speed(void)
{
  if(g_current_event.parameters <= 31 && g_current_event.parameters > 0)
    g_speed = g_current_event.parameters;
  else if(g_current_event.parameters > 31 )
  {
    g_refresh = g_current_event.parameters;
    g_roboplay_interface->update_refresh();
  }
  else
  {
    g_refresh = DEFAULT_REFRESH;
    g_roboplay_interface->update_refresh();
  }
}

void init_tone_portamento(const uint8_t channel)
{
  if(g_current_event.pitch)
  {
    g_last_events[channel].portamento_frequency = get_frequency(g_current_event.pitch, g_current_event.octave);
  }
  tone_portamento(channel);
}

void set_carrier_modulator_volume(const uint8_t channel)
{
  uint8_t modulator_volume = g_current_event.parameters % 10;
  uint8_t carrier_volume   = g_current_event.parameters / 10;

  if(modulator_volume)
    g_last_events[channel].modulator_volume = (modulator_volume * 7) ^ 0x3f;

  if(carrier_volume)
    g_last_events[channel].carrier_volume = (carrier_volume * 7) ^ 0x3f;
}

void extended_command(const uint8_t channel)
{
  uint8_t command = g_current_event.parameters / 10;
  uint8_t parameters = g_current_event.parameters % 10;

  switch(command)
  {
    case CMD_EXT_CELL_TREMOLO:
      if(g_current_event.parameters % 10)
        g_regbd |= 0x80;
      else
        g_regbd &= 0x7f;
      
      g_roboplay_interface->opl4_write_fm_1(0xBD, g_regbd);
      break;
    case CMD_EXT_CELL_VIBRATO:
      if(g_current_event.parameters % 10)
        g_regbd |= 0x40;
      else
        g_regbd &= 0xbf;
      
      g_roboplay_interface->opl4_write_fm_1(0xBD, g_regbd);
      break;
    case CMD_EXT_SLIDE_VOL_UP_FAST:
      slide_volume_up(channel);
      break;
    case CMD_EXT_SLIDE_VOL_DOWN_FAST:
      slide_volume_down(channel);
      break;
    case CMD_EXT_SLIDE_VOL_UP_FINE:
      slide_volume_up(channel);
      break;
    case CMD_EXT_SLIDE_VOL_DOWN_FINE:
      slide_volume_down(channel);
      break;
  }
}

void slide_volume_up(const uint8_t channel)
{
  uint8_t value = (g_last_events[channel].parameters % 10);
  g_last_events[channel].volume += value;

  if(g_last_events[channel].volume > 0x3f)
    g_last_events[channel].volume = 0x3f;

  channel_set_volume(channel);
}

void slide_volume_down(const uint8_t channel)
{
  uint8_t value = (g_last_events[channel].parameters % 10);
  if(g_last_events[channel].volume > value)
    g_last_events[channel].volume -= value;
  else
    g_last_events[channel].volume = 0;

  channel_set_volume(channel);
}

uint16_t get_frequency(const uint8_t pitch, const uint8_t octave)
{
  return  (g_note_frequencies[pitch - 1] - FREQ_START) + (octave * FREQ_RANGE);
}

void set_frequency(const uint8_t channel, const int16_t frequency)
{
  uint8_t octave = frequency / FREQ_RANGE;
  uint16_t pitch = (frequency % FREQ_RANGE) + FREQ_START;

  g_roboplay_interface->opl4_write_fm_1(0xA0 + channel, pitch & 0xff);
  g_roboplay_interface->opl4_write_fm_2(0xA0 + channel, (pitch & 0xff) + 1);

  g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, ((octave << 2) + (pitch >> 8)) | 0x20);
  g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, ((octave << 2) + (pitch >> 8)) | 0x20);
}
