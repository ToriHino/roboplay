/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2025
 *
 * sa2.h
 *
 * SA2: Surprise! Adlib Tracker
 */

#include <string.h>

#include "player_interface.h"
#include "sa2.h"

bool load(char *const file_name)
{  
  g_roboplay_interface->open(file_name, false);

  g_roboplay_interface->read(&g_sat_header.id, ID_SIZE);
  if(strncmp(g_sat_header.id, "SAdT", ID_SIZE))
  {
    g_roboplay_interface->close();
    return false;
  }
  
  g_note_dis = 0;
  g_roboplay_interface->read(&g_sat_header.version, 1);
  switch(g_sat_header.version)
  {
    case 1:
      g_note_dis = +0x18;
      g_sat_type = HAS_UNKNOWN127 | HAS_OLDPATTERNS | HAS_OLDBPM;
      break;
    case 2:
      g_note_dis = +0x18;
      g_sat_type = HAS_OLDPATTERNS | HAS_OLDBPM;
      break;
    case 3:
      g_note_dis = +0x0c;
      g_sat_type = HAS_OLDPATTERNS | HAS_OLDBPM;
      break;
    case 4:
      g_note_dis = +0x0c;
      g_sat_type = HAS_OLDPATTERNS | HAS_OLDBPM;
      break;
    case 5:
      g_note_dis = +0x0c;
      g_sat_type = HAS_ARPEGGIO | HAS_ARPEGGIOLIST | HAS_OLDPATTERNS | HAS_OLDBPM;
      break;
    case 6:
      g_sat_type = HAS_ARPEGGIO | HAS_ARPEGGIOLIST | HAS_OLDPATTERNS | HAS_OLDBPM;
      break;
    case 7:
      g_sat_type = HAS_ARPEGGIO | HAS_ARPEGGIOLIST | HAS_V7PATTERNS;
      break;
    case 8:
      g_sat_type = HAS_ARPEGGIO | HAS_ARPEGGIOLIST | HAS_TRACKORDER;
      break;
    case 9:
      g_sat_type = HAS_ARPEGGIO | HAS_ARPEGGIOLIST | HAS_TRACKORDER | HAS_ACTIVECHANNELS;
      break;
  }

  for(uint8_t i = 0; i < NUMBER_OF_INSTRUMENTS; i++)
  {
    g_roboplay_interface->read(g_sat_header.instruments[i].data, INSTRUMENT_DATA_SIZE);
    if(g_sat_type & HAS_ARPEGGIO)
    {
      g_roboplay_interface->read(&g_sat_header.instruments[i].arpstart, 1);
      g_roboplay_interface->read(&g_sat_header.instruments[i].arpspeed, 1);
      g_roboplay_interface->read(&g_sat_header.instruments[i].arppos, 1);
      g_roboplay_interface->read(&g_sat_header.instruments[i].arpspdcnt, 1);
    }
  }
  g_roboplay_interface->seek(NUMBER_OF_INSTRUMENTS * INSTRUMENT_NAME_SIZE, ROBOPLAY_SEEK_CURRENT);

  g_roboplay_interface->read(&g_sat_header.pattern_order, NUMBER_OF_PATTERNS);

  if(g_sat_type & HAS_UNKNOWN127)
    g_roboplay_interface->seek(127, ROBOPLAY_SEEK_CURRENT);

  g_roboplay_interface->read(&g_sat_header.number_of_patterns_used, 2);
  g_roboplay_interface->read(&g_sat_header.song_length, 1);
  g_roboplay_interface->read(&g_sat_header.restart_position, 1);
  
  g_roboplay_interface->read(&g_sat_header.bpm, 2);

  if(g_sat_type & HAS_OLDBPM)
    g_sat_header.bpm = g_sat_header.bpm * 125 / 50;

  if(g_sat_type & HAS_ARPEGGIOLIST)
  {
    g_roboplay_interface->read(&g_sat_header.arpeggio_list, ARPEGGIO_LIST_SIZE);
    g_roboplay_interface->read(&g_sat_header.arpeggio_commands, ARPEGGIO_LIST_SIZE);
  }

  if(g_sat_type & HAS_TRACKORDER)
    g_roboplay_interface->read(&g_sat_header.track_order, NUMBER_OF_TRACKS * NUMBER_OF_CHANNELS);
  else
  {
    for(uint8_t i = 0; i < NUMBER_OF_TRACKS; i++)
    {
      for(uint8_t j = 0; j < NUMBER_OF_CHANNELS; j++)
      {
        g_sat_header.track_order[i][j] = i * NUMBER_OF_CHANNELS + j;
      }
    }
  }

  if(g_sat_type & HAS_ACTIVECHANNELS)
    g_roboplay_interface->read(&g_sat_header.active_channels, 2);

  /* Read tracks */
  uint16_t bytes_read = 0;
  uint16_t bytes_left = DATA_SEGMENT_SIZE;
  uint8_t* destination = (uint8_t *)DATA_SEGMENT_BASE;
  uint8_t* read_buffer = (uint8_t *)READ_BUFFER;
  uint8_t  total_tracks = 0;

  uint8_t segment_nr = START_SEGMENT_INDEX;

  if(g_sat_type & HAS_OLDPATTERNS)
  {
    bytes_read = g_roboplay_interface->read(read_buffer, NUMBER_OF_CHANNELS * 5);

    while(bytes_read == NUMBER_OF_CHANNELS * 5)
    {
      for(uint8_t k = 0; k < NUMBER_OF_CHANNELS; k++)
      {
        g_tracks[total_tracks + k].segment = segment_nr;
        g_tracks[total_tracks + k].data    = (SAT_TRACK_LINE *)destination;

        destination += TRACK_SIZE;
      }

      for(uint8_t i = 0; i < TRACK_LENGTH; i++)
      {
        for(uint8_t j = 0; j < NUMBER_OF_CHANNELS; j++)
        {
          g_tracks[total_tracks + j].data[i].note    = read_buffer[j * 5 + 0] ? (read_buffer[j * 5 + 0] + g_note_dis) : 0;
          g_tracks[total_tracks + j].data[i].inst    = read_buffer[j * 5 + 1];
          g_tracks[total_tracks + j].data[i].command = read_buffer[j * 5 + 2];
          g_tracks[total_tracks + j].data[i].param1  = read_buffer[j * 5 + 3];
          g_tracks[total_tracks + j].data[i].param2  = read_buffer[j * 5 + 4];
          g_tracks[total_tracks + j].data[i].param   = (g_tracks[total_tracks].data[i].param1 << 4) + g_tracks[total_tracks].data[i].param2;

        }

        bytes_read = g_roboplay_interface->read(read_buffer, NUMBER_OF_CHANNELS * 5); 
      }
      total_tracks += NUMBER_OF_CHANNELS;

      if(bytes_read == NUMBER_OF_CHANNELS * 5)
      {
        bytes_left -= NUMBER_OF_CHANNELS * TRACK_SIZE;
        if(bytes_left < NUMBER_OF_CHANNELS * TRACK_SIZE)
        {
          destination = (uint8_t *)DATA_SEGMENT_BASE;
          bytes_left = DATA_SEGMENT_SIZE;
          segment_nr = g_roboplay_interface->get_new_segment();
          g_roboplay_interface->set_segment(segment_nr);
        }
      }
    }
  }
  else if(g_sat_type & HAS_V7PATTERNS)
  {

  }
  else
  {
    bytes_read = g_roboplay_interface->read(read_buffer, TRACK_LENGTH * 3);
    while(bytes_read == TRACK_LENGTH * 3)
    {
      g_tracks[total_tracks].segment = segment_nr;
      g_tracks[total_tracks].data    = (SAT_TRACK_LINE *)destination;

      for(uint8_t i = 0; i < TRACK_LENGTH; i++)
      {
        g_tracks[total_tracks].data[i].note    = read_buffer[i * 3 + 0] >> 1;
        g_tracks[total_tracks].data[i].inst    = ((read_buffer[i * 3 + 0] & 1) << 4) + (read_buffer[i * 3 + 1] >> 4);
        g_tracks[total_tracks].data[i].command = read_buffer[i * 3 + 1] & 0x0f;
        g_tracks[total_tracks].data[i].param1  = read_buffer[i * 3 + 2] >> 4;
        g_tracks[total_tracks].data[i].param2  = read_buffer[i * 3 + 2] & 0x0f;
        g_tracks[total_tracks].data[i].param   = (g_tracks[total_tracks].data[i].param1 << 4) + g_tracks[total_tracks].data[i].param2;
      }

      bytes_read = g_roboplay_interface->read(read_buffer, TRACK_LENGTH * 3);

      if(bytes_read == TRACK_LENGTH * 3)
      {
        destination += TRACK_SIZE;
        total_tracks++;

        bytes_left -= TRACK_SIZE;
        if(bytes_left < TRACK_SIZE)
        {
          destination = (uint8_t *)DATA_SEGMENT_BASE;
          bytes_left = DATA_SEGMENT_SIZE;
          segment_nr = g_roboplay_interface->get_new_segment();
          g_roboplay_interface->set_segment(segment_nr);
        }
      }
    }
  }

  g_roboplay_interface->close();
  return true;
}

bool update(void)
{
  bool    pattern_break = false;
  uint8_t info1, info2, info;

  for(uint8_t i = 0; i < NUMBER_OF_CHANNELS; i++)
  {
    /* Special arpeggio */
    uint8_t arpcmd = g_sat_header.arpeggio_commands[g_channel_data[i].arppos];

    if(g_sat_header.instruments[g_channel_data[i].inst].arpstart)
    {
      if(g_channel_data[i].arpspdcnt)
        g_channel_data[i].arpspdcnt--;
      else if(arpcmd != 255)
      {
        switch(arpcmd)
        {
          case 252:
            g_channel_data[i].vol1 = g_sat_header.arpeggio_list[g_channel_data[i].arppos];
            if(g_channel_data[i].vol1 > 63) g_channel_data[i].vol1 = 63;
            g_channel_data[i].vol2 = g_channel_data[i].vol1;
            set_volume(i);
            break;
          case 253:
            g_channel_data[i].key = false;
            set_frequency(i);
            g_channel_data[i].arppos = g_sat_header.arpeggio_list[g_channel_data[i].arppos];
            break;
          case 254:
            if(arpcmd)
            {
              if(arpcmd / 10)
                g_roboplay_interface->opl4_write_fm_1(0xe3 + g_op_table[i], (arpcmd / 10) - 1);
              if(arpcmd % 10)
                g_roboplay_interface->opl4_write_fm_1(0xe0 + g_op_table[i], (arpcmd % 10) - 1);
              if(arpcmd < 10)
              g_roboplay_interface->opl4_write_fm_1(0xe0 + g_op_table[i], arpcmd - 1);
            }
            break;
          default:
            break;
        }

        if(arpcmd != 252)
        {
          if(g_sat_header.arpeggio_list[g_channel_data[i].arppos] <= 96)
            set_note(i, g_channel_data[i].note + g_sat_header.arpeggio_list[g_channel_data[i].arppos]);
          if(g_sat_header.arpeggio_list[g_channel_data[i].arppos] >= 100)
            set_note(i, g_sat_header.arpeggio_list[g_channel_data[i].arppos] - 100);
        }
        else
          set_note(i, g_channel_data[i].note);

        set_frequency(i);
        if(arpcmd != 255)
          g_channel_data[i].arppos++;
        g_channel_data[i].arpspdcnt = g_sat_header.instruments[g_channel_data[i].inst].arpspeed - 1;
      }
    }

    info1 = g_channel_data[i].info1;
    info2 = g_channel_data[i].info2;
    info  = g_channel_data[i].info;

    switch(g_channel_data[i].fx)
    {
      case FX_ARPEGGIO:
        if(info)
        {
          if(g_channel_data[i].trigger < 2)
            g_channel_data[i].trigger++;
          else
            g_channel_data[i].trigger = 0;

          switch(g_channel_data[i].trigger)
          {
            case 0: 
              set_note(i, g_channel_data[i].note);
              break;
            case 1:
              set_note(i, g_channel_data[i].note + info1);
              break;
            case 2:
              set_note(i, g_channel_data[i].note + info2);
              break;
          }
          set_frequency(i);
        }
        break;
      case FX_TONE_SLIDE_UP:
        slide_up(i, info);
        set_frequency(i);
        break;
      case FX_TONE_SLIDE_DOWN:
        slide_down(i, info);
        set_frequency(i);
        break;
      case FX_PORTAMENTO:
        tone_portamento(i, g_channel_data[i].portainfo);
        break;
      case FX_VIBRATO:
        vibrato(i, g_channel_data[i].vibinfo1, g_channel_data[i].vibinfo2);
        break;
      case FX_PORTAMENTO_VOL_SLIDE:
      case FX_VIBRATO_VOL_SLIDE:
        if(g_channel_data[i].fx == FX_PORTAMENTO_VOL_SLIDE)
          tone_portamento(i, g_channel_data[i].portainfo);
        else
          vibrato(i, g_channel_data[i].vibinfo1, g_channel_data[i].vibinfo2);
      case FX_VOLUME_SLIDE:
        if(g_delay % 4) break;
        
        if(info1)
          vol_up(i, info1);
        else
          vol_down(i, info2);
        set_volume(i);
        break;
    }
  }

  if(g_delay)
  {
    g_delay--;
    return !g_song_end;
  }

  uint8_t pattern = g_sat_header.pattern_order[g_pattern];

  for(uint8_t i = 0; i < NUMBER_OF_CHANNELS; i++)
  {   
    uint8_t track = g_sat_header.track_order[pattern][i];

    if(track > 0)
    {
      g_roboplay_interface->set_segment(g_tracks[track - 1].segment);
      SAT_TRACK_LINE* event_data = &g_tracks[track - 1].data[g_row];

      bool do_note = false;
      if(event_data->inst)
      {
        g_channel_data[i].inst = event_data->inst - 1;
        g_channel_data[i].vol1 = 63 - g_sat_header.instruments[g_channel_data[i].inst].data[10] & 63;
        g_channel_data[i].vol2 = 63 - g_sat_header.instruments[g_channel_data[i].inst].data[9] & 63;
        set_volume(i);
      }

      if(event_data->note && event_data->command != FX_PORTAMENTO)
      {
        g_channel_data[i].note = event_data->note;
        set_note(i, event_data->note);
        g_channel_data[i].next_freq = g_channel_data[i].freq;
        g_channel_data[i].next_oct  = g_channel_data[i].oct;
        g_channel_data[i].arppos    = g_sat_header.instruments[g_channel_data[i].inst].arpstart;
        g_channel_data[i].arpspdcnt = 0;
        if(event_data->note != 127) do_note = true;
      }

      g_channel_data[i].fx    = event_data->command;
      g_channel_data[i].info1 = event_data->param1;
      g_channel_data[i].info2 = event_data->param2;
      g_channel_data[i].info  = event_data->param;

      if(do_note) play_note(i);

      info1 = g_channel_data[i].info1;
      info2 = g_channel_data[i].info2;
      info  = g_channel_data[i].info;

      switch(g_channel_data[i].fx)
      {
        case FX_PORTAMENTO:
          if(event_data->note)
          {
            g_channel_data[i].next_freq = g_note_table[event_data->note - 1];
            g_channel_data[i].next_oct =  g_oct_table[event_data->note - 1];
            
            if(event_data->note == 127)
            {
              g_channel_data[i].next_freq = g_channel_data[i].freq;
              g_channel_data[i].next_oct  = g_channel_data[i].oct;
            }
          }

          if(info) g_channel_data[i].portainfo = info;
          break;
        case FX_VIBRATO:
          if(info)
          {
            g_channel_data[i].vibinfo1 = info1;
            g_channel_data[i].vibinfo2 = info2;
          }
          break;
        case FX_RELEASE_NOTE:
          g_channel_data[i].key = false;
          set_frequency(i);
          break;
        case FX_POSITION_JMP:
          pattern_break = true;
          g_row = 0;
          if(info < g_pattern) g_song_end = true;
          g_pattern = info;
          break;
        case FX_SET_VOLUME:
          g_channel_data[i].vol1 = info;
          g_channel_data[i].vol2 = info;
          if(g_channel_data[i].vol1 > 63) g_channel_data[i].vol1 = 63;
          if(g_channel_data[i].vol2 > 63) g_channel_data[i].vol2 = 63;
          set_volume(i);
          break;
        case FX_PATTERN_BREAK:
          if(!pattern_break)
          {
            pattern_break = true;
            g_row = info;
            g_pattern++;
          }
          break;
        case FX_SET_SPEED:
          if(info <= 0x1f) g_speed = info;
          if(info >= 0x32)
          {
            g_tempo = info;
            g_roboplay_interface->update_refresh();
          }
          if(!info) g_song_end = true;
          break;
      }
    }
  }

  g_delay = g_speed - 1;

  if(!pattern_break)
  {
    g_row++;
    if(g_row >= TRACK_LENGTH)
    {
      g_row = 0;
      g_pattern++;
    }
  }

  if(g_pattern >= g_sat_header.song_length)
    g_pattern = g_sat_header.restart_position;

  return true;
}

void rewind(int8_t subsong)
{
  /* No subsongs in this format */
  subsong;

  /* Set to OPL3 mode */
  g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL3);

  g_delay = 0;
  g_song_end = false;
  g_tempo = g_sat_header.bpm;
  g_speed = 6;

  g_pattern = 0;
  g_row = 0;

  for(uint8_t i = 0; i < NUMBER_OF_CHANNELS; i++)
  {
    g_channel_data[i].freq = 0;
    g_channel_data[i].next_freq = 0;
    g_channel_data[i].oct = 0;
    g_channel_data[i].next_oct = 0;
    g_channel_data[i].note = 0;
    g_channel_data[i].vol1 = 0;
    g_channel_data[i].vol2 = 0;
    g_channel_data[i].inst = 0;
    g_channel_data[i].fx = 0;
    g_channel_data[i].info1 = 0;
    g_channel_data[i].info2 = 0;
    g_channel_data[i].info = 0;
    g_channel_data[i].key = false;
    g_channel_data[i].portainfo = 0;
    g_channel_data[i].vibinfo1 = 0;
    g_channel_data[i].vibinfo2 = 0;
    g_channel_data[i].arppos = 0;
    g_channel_data[i].arpspdcnt = 0;
    g_channel_data[i].trigger = 0;
  }
}

void command(const uint8_t id)
{
  /* No additional commmands supported */
  id;
}

float get_refresh(void)
{
  return g_tempo / 2.5;
}

uint8_t get_subsongs(void)
{
  return 0;
}

char* get_player_info(void)
{
  return "Surprise! AdLib Tracker player V1.0 by RoboSoft Inc.";
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
  strcpy(version, "Used file version is 1");
  version[VERSION_LENGTH - 2] = g_sat_header.version + '0';

  return version;
}

void set_volume(uint8_t channel)
{
  g_roboplay_interface->opl4_write_fm_1(0x40 + g_op_table[channel], 63 - g_channel_data[channel].vol2 + (g_sat_header.instruments[g_channel_data[channel].inst].data[9] & 192));
  g_roboplay_interface->opl4_write_fm_2(0x40 + g_op_table[channel], 63 - g_channel_data[channel].vol2 + (g_sat_header.instruments[g_channel_data[channel].inst].data[9] & 192));

  g_roboplay_interface->opl4_write_fm_1(0x43 + g_op_table[channel], 63 - g_channel_data[channel].vol1 + (g_sat_header.instruments[g_channel_data[channel].inst].data[10] & 192));
  g_roboplay_interface->opl4_write_fm_2(0x43 + g_op_table[channel], 63 - g_channel_data[channel].vol1 + (g_sat_header.instruments[g_channel_data[channel].inst].data[10] & 192));
}

void set_frequency(uint8_t channel)
{
  g_roboplay_interface->opl4_write_fm_1(0xA0 + channel, g_channel_data[channel].freq & 0xff);
  g_roboplay_interface->opl4_write_fm_2(0xA0 + channel, (g_channel_data[channel].freq + SAT_CHORUS) & 0xff);
  if(g_channel_data[channel].key)
  {
    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, ((g_channel_data[channel].freq & 0x300) >> 8) + (g_channel_data[channel].oct << 2) + 0x20);
    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, (((g_channel_data[channel].freq + SAT_CHORUS) & 0x300) >> 8) + (g_channel_data[channel].oct << 2) + 0x20);
  }
  else
  {
    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, ((g_channel_data[channel].freq & 0x300) >> 8) + (g_channel_data[channel].oct << 2));
    g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, (((g_channel_data[channel].freq + SAT_CHORUS) & 0x300) >> 8) + (g_channel_data[channel].oct << 2));
  }
}

void set_note(uint8_t channel, uint8_t note)
{
  if(note > 96)
  {
    if(note == 127)
    {
      g_channel_data[channel].key = false;
      set_frequency(channel);
      return;
    }
    else note = 96;
  }

  g_channel_data[channel].freq = g_note_table[note - 1];
  g_channel_data[channel].oct =  g_oct_table[note - 1];
}

void play_note(uint8_t channel)
{
  /* Stop old note */
  g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, 0x00);
  g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, 0x00);

  SAT_INSTRUMENT *instrument = &g_sat_header.instruments[g_channel_data[channel].inst];
  uint8_t         op         = g_op_table[channel];

  g_roboplay_interface->opl4_write_fm_1(0x20 + op, instrument->data[1]);   /* Modulator */
  g_roboplay_interface->opl4_write_fm_1(0x23 + op, instrument->data[2]);   /* Carrier */
  g_roboplay_interface->opl4_write_fm_1(0x60 + op, instrument->data[3]);   /* Bits 0..3 = decay, 4..7 = attack */
  g_roboplay_interface->opl4_write_fm_1(0x63 + op, instrument->data[4]);
  g_roboplay_interface->opl4_write_fm_1(0x80 + op, instrument->data[5]);   /* Bits 0..3 = release, 4..7 = sustain */
  g_roboplay_interface->opl4_write_fm_1(0x83 + op, instrument->data[6]);
  g_roboplay_interface->opl4_write_fm_1(0xe0 + op, instrument->data[7]);   /* Bits 0..1 = waveform select */
  g_roboplay_interface->opl4_write_fm_1(0xe3 + op, instrument->data[8]);
  g_roboplay_interface->opl4_write_fm_1(0xc0 + channel, instrument->data[0] | PAN_SETTING_LEFT);

  g_roboplay_interface->opl4_write_fm_2(0x20 + op, instrument->data[1]);   /* Modulator */
  g_roboplay_interface->opl4_write_fm_2(0x23 + op, instrument->data[2]);   /* Carrier */
  g_roboplay_interface->opl4_write_fm_2(0x60 + op, instrument->data[3]);   /* Bits 0..3 = decay, 4..7 = attack */
  g_roboplay_interface->opl4_write_fm_2(0x63 + op, instrument->data[4]);
  g_roboplay_interface->opl4_write_fm_2(0x80 + op, instrument->data[5]);   /* Bits 0..3 = release, 4..7 = sustain */
  g_roboplay_interface->opl4_write_fm_2(0x83 + op, instrument->data[6]);
  g_roboplay_interface->opl4_write_fm_2(0xe0 + op, instrument->data[7]);   /* Bits 0..1 = waveform select */
  g_roboplay_interface->opl4_write_fm_2(0xe3 + op, instrument->data[8]);
  g_roboplay_interface->opl4_write_fm_2(0xc0 + channel, instrument->data[0] | PAN_SETTING_RIGHT);

  g_channel_data[channel].key = true;
  set_frequency(channel);
  set_volume(channel);
}

void slide_down(uint8_t channel, uint8_t amount)
{
  g_channel_data[channel].freq -= amount;
  if(g_channel_data[channel].freq <= 342)
  {
    if(g_channel_data[channel].oct > 0)
    {
      g_channel_data[channel].oct--;
      g_channel_data[channel].freq <<= 1;
    }
    else g_channel_data[channel].freq = 342;
  }
}

void slide_up(uint8_t channel, uint8_t amount)
{
  g_channel_data[channel].freq += amount;
  if(g_channel_data[channel].freq >= 686)
  {
    if(g_channel_data[channel].oct < 7)
    {
      g_channel_data[channel].oct++;
      g_channel_data[channel].freq >>= 1;
    }
    else g_channel_data[channel].freq = 686;
  }
}

void tone_portamento(uint8_t channel, uint8_t info)
{
  if(g_channel_data[channel].freq + (g_channel_data[channel].oct << 10) < g_channel_data[channel].next_freq + (g_channel_data[channel].next_oct << 10))
  {
    slide_up(channel, info);
    if(g_channel_data[channel].freq + (g_channel_data[channel].oct << 10) > g_channel_data[channel].next_freq + (g_channel_data[channel].next_oct << 10))
    {
      g_channel_data[channel].freq = g_channel_data[channel].next_freq;
      g_channel_data[channel].oct  = g_channel_data[channel].next_oct;
    }
  }

  if(g_channel_data[channel].freq + (g_channel_data[channel].oct << 10) > g_channel_data[channel].next_freq + (g_channel_data[channel].next_oct << 10))
  {
    slide_down(channel, info);
    if(g_channel_data[channel].freq + (g_channel_data[channel].oct << 10) < g_channel_data[channel].next_freq + (g_channel_data[channel].next_oct << 10))
    {
      g_channel_data[channel].freq = g_channel_data[channel].next_freq;
      g_channel_data[channel].oct  = g_channel_data[channel].next_oct;
    }
  }
  set_frequency(channel);
}

void vibrato(uint8_t channel, uint8_t speed, uint8_t depth)
{
  if(!speed || !depth)
    return;

  if(depth > 14)
    depth = 14;

  for(uint8_t i = 0; i < speed; i++)
  {
    g_channel_data[channel].trigger++;
    while(g_channel_data[channel].trigger >= 64)
      g_channel_data[channel].trigger -= 64;

    if(g_channel_data[channel].trigger >= 16 && g_channel_data[channel].trigger < 48)
      slide_down(channel, g_vibrato_table[g_channel_data[channel].trigger - 16] / (16 - depth));
    if(g_channel_data[channel].trigger < 16)
      slide_up(channel, g_vibrato_table[g_channel_data[channel].trigger + 16] / (16 - depth));
    if(g_channel_data[channel].trigger >= 48)
      slide_up(channel, g_vibrato_table[g_channel_data[channel].trigger - 48] / (16 - depth));
  }

  set_frequency(channel);
}

void vol_up(uint8_t channel, uint8_t amount)
{
  if(g_channel_data[channel].vol1 + amount < 63)
    g_channel_data[channel].vol1 += amount;
  else
    g_channel_data[channel].vol1 = 63;

  if(g_channel_data[channel].vol2 + amount < 63)
    g_channel_data[channel].vol2 += amount;
  else
    g_channel_data[channel].vol2 = 63;
}

void vol_down(uint8_t channel, uint8_t amount)
{
  if(g_channel_data[channel].vol1 - amount > 0)
    g_channel_data[channel].vol1 -= amount;
  else
    g_channel_data[channel].vol1 = 0;

  if(g_channel_data[channel].vol2 - amount > 0)
    g_channel_data[channel].vol2 -= amount;
  else
    g_channel_data[channel].vol2 = 0;
}