/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * hsc.c
 *
 * HSC: HSC AdLib Composer
 */

#include <string.h>

#include "player_interface.h"
#include "hsc.h"

bool load(char *const file_name)
{  
  g_roboplay_interface->open(file_name, false);

  /* Read instruments */
  g_roboplay_interface->read(g_instruments, NUMBER_OF_INSTRUMENTS * INSTRUMENT_SIZE);
  for(uint8_t i = 0; i < NUMBER_OF_INSTRUMENTS; i++)
  {
    g_instruments[i][2] ^= (g_instruments[i][2] & 0x40) << 1;
    g_instruments[i][3] ^= (g_instruments[i][3] & 0x40) << 1;
    g_instruments[i][11] >>= 4;
  }

  /* Read track list */
  g_roboplay_interface->read(g_tracklist, TRACKLIST_SIZE);

  /* Read patterns */
  uint16_t  bytes_read = 0;
  uint16_t  bytes_left = DATA_SEGMENT_SIZE;
  uint8_t  *destination = (uint8_t *)DATA_SEGMENT_BASE;
  uint8_t   total_patterns = 0;

  uint8_t segment_nr = START_SEGMENT_INDEX;
  do
  {
    /* It's not possible to read directly to non-primary mapper memory segments, 
       so use a buffer inbetween. */
    bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, PATTERN_SIZE);

    if(bytes_read == PATTERN_SIZE)
    {
      memcpy(destination, (void*)READ_BUFFER, PATTERN_SIZE);

      g_patterns[total_patterns].segment = segment_nr;
      g_patterns[total_patterns].data    = (HSC_NOTE *)destination;

      destination += PATTERN_SIZE;
      total_patterns++;

      bytes_left -= bytes_read;
      if(bytes_left < PATTERN_SIZE)
      {
        destination = (uint8_t *)DATA_SEGMENT_BASE;
        bytes_left = DATA_SEGMENT_SIZE;
        segment_nr = g_roboplay_interface->get_new_segment();
        g_roboplay_interface->set_segment(segment_nr);
      }
    }
  } while (bytes_read == PATTERN_SIZE);
  
  for(uint8_t i = 0; i < TRACKLIST_SIZE; i++)
  {
    if((g_tracklist[i] & 0x7f) > 0x31 || (g_tracklist[i] & 0x7f) >= total_patterns)
      g_tracklist[i] = 0xff;
  }

  g_roboplay_interface->close();

  return true;
}

bool update(void)
{
  g_delay--;
  if(g_delay)
    return !g_song_end;

  if(g_fade_in)
    g_fade_in--;

  uint8_t pattern_nr = g_tracklist[g_song_position];
  if(pattern_nr >= 0xb2)
  {
    g_song_end = true;
    g_song_position = 0;
    pattern_nr = g_tracklist[g_song_position];
  }
  else if((pattern_nr & 0x80) && (pattern_nr <= 0xb1)) /* Goto pattern 'nr' */
  {
    g_song_position = g_tracklist[g_song_position] & 0x7f;
    g_pattern_position = 0;
    pattern_nr = g_tracklist[g_song_position];
    g_song_end = true;
  }

  g_roboplay_interface->set_segment(g_patterns[pattern_nr].segment);
  HSC_NOTE* pattern = g_patterns[pattern_nr].data;
  uint16_t  pattern_offset = g_pattern_position * NUMBER_OF_CHANNELS;

  for(uint8_t i =0; i < NUMBER_OF_CHANNELS; i++)
  {
    uint8_t note = pattern[pattern_offset].note;
    uint8_t effect = pattern[pattern_offset].effect;
    pattern_offset++;

    if(note & 0x80)
    {
      set_instrument(i, effect);
      continue;
    }

    handle_event(i, note, effect);

    if(g_fade_in)
      set_volume(i, g_fade_in * 2, g_fade_in * 2);

    if(!note)
      continue;

    note--;
    if((note == 0x7f - 1) || ((note / 12) & ~7))
    {
      g_frequencies[i] &= ~32;
      g_roboplay_interface->opl4_write_fm_1(0xb0 + i, g_frequencies[i]);
      continue;
    }

    play_note(i, note);
  }

  g_delay = g_speed;
  if(g_pattern_break)
  {
    g_pattern_position = 0;
    g_pattern_break = false;
    g_song_position++;
    g_song_position %= (TRACKLIST_SIZE - 1);
    if(!g_song_position)
      g_song_end = true;
  }
  else
  {
    g_pattern_position++;
    g_pattern_position &= (PATTERN_LENGTH - 1);
    if(!g_pattern_position)
    {
      g_song_position++;
      g_song_position %= (TRACKLIST_SIZE - 1);
      if(!g_song_position)
        g_song_end = true;
    }
  }

  return !g_song_end;
}

void rewind(int8_t subsong)
{
  /* No subsongs in this format */
  subsong;

  /* Start with standard OPL2 mode */
  g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL2);

  g_pattern_position = 0;
  g_song_position    = 0;
  g_pattern_break    = false;
  g_speed            = 2;
  g_delay            = 1;
  g_song_end         = false;
  g_6_voice_mode     = false;
  g_bd               = 0x00;
  g_fade_in          = false;

  g_roboplay_interface->opl4_write_fm_1(0x01, 0x20);
  g_roboplay_interface->opl4_write_fm_1(0x08, 0x80);

  for(uint8_t i = 0; i < NUMBER_OF_CHANNELS; i++)
    set_instrument(i, i);
}

void command(const uint8_t id)
{
  /* No additional commmands supported */
  id;
}

float get_refresh(void)
{
  /* Fixed replay rate of 18.2 Hz */
  return 18.2;
}

uint8_t get_subsongs(void)
{
  return 0;
}

char* get_player_info(void)
{
  return "HSC AdLib Composer player V1.0 by RoboSoft Inc.";
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
  return "-";
}

void handle_event(uint8_t channel, uint8_t note, uint8_t effect)
{
  uint8_t value = 0;

  uint8_t effect_operator = effect & 0x0f;
  uint8_t instrument = g_channels[channel].instrument;
  if(note)
    g_channels[channel].slide = 0;

  switch(effect & 0xf0)
  {
    case 0:
      switch(effect_operator)
      {
        case 1:
          g_pattern_break = true;
          break;
        case 3:
          g_fade_in = 31;
          break;
        case 5:
          g_6_voice_mode = true;
          break;
        case 6:
          g_6_voice_mode = false;
      }
      break;
    case 0x20:
    case 0x10:
      if(effect & 0x10)
      {
        g_channels[channel].frequency += effect_operator;
        g_channels[channel].slide += effect_operator;
      }
      else
      {
        g_channels[channel].frequency -= effect_operator;
        g_channels[channel].slide -= effect_operator;
      }
      if(!note)
        set_frequency(channel, g_channels[channel].frequency);
      break;
    case 0x50:
      break;
    case 0x60:
      value = (g_instruments[instrument][8] & 1) + (effect_operator << 1);
      g_roboplay_interface->opl4_write_fm_1(0xc0 + channel, value | PAN_SETTING_LEFT);
      break;
    case 0xa0:
      value = effect_operator << 2; 
      g_roboplay_interface->opl4_write_fm_1(0x43 + g_op_table[channel], value | (g_instruments[instrument][2] & ~63));
      break;
    case 0xb0:
      value = effect_operator << 2; 
      g_roboplay_interface->opl4_write_fm_1(0x40 + g_op_table[channel], value | (g_instruments[instrument][3] & ~63));
      break;
    case 0xc0:
      value = effect_operator << 2;
      g_roboplay_interface->opl4_write_fm_1(0x43 + g_op_table[channel], value | (g_instruments[instrument][2] & ~63));
      if(g_instruments[instrument][8] & 1)
        g_roboplay_interface->opl4_write_fm_1(0x40 + g_op_table[channel], value | (g_instruments[instrument][3] & ~63));
      break;
    case 0xd0:
      g_pattern_break = true;
      g_song_position = effect_operator;
      g_song_end = true;
      break;
    case 0xf0:
      g_speed = effect_operator;
      g_delay = ++g_speed;
      break;
  }
}

void play_note(uint8_t channel, uint8_t note)
{
  uint8_t  octave = ((note / 12) & 7) << 2;
  uint16_t frequency = g_note_table[note % 12] + g_instruments[g_channels[channel].instrument][11] + g_channels[channel].slide;

  g_channels[channel].frequency = frequency;
  
  if(!g_6_voice_mode || channel < 6)
    g_frequencies[channel] = octave | 32;
  else
    g_frequencies[channel] = octave;

  g_roboplay_interface->opl4_write_fm_1(0xb0 + channel, 0x00);
  set_frequency(channel, frequency);

  if(g_6_voice_mode)
  {
    switch(channel)
    {
      case 6:
        g_roboplay_interface->opl4_write_fm_1(0xbd, g_bd & ~16);
        g_bd |= 48;
        break;
      case 7:
        g_roboplay_interface->opl4_write_fm_1(0xbd, g_bd & ~1);
        g_bd |= 33;
        break;
      case 8:
        g_roboplay_interface->opl4_write_fm_1(0xbd, g_bd & ~2);
        g_bd |= 34;
        break;
    }
    g_roboplay_interface->opl4_write_fm_1(0xbd, g_bd);
  }
}

void set_frequency(uint8_t channel, uint16_t frequency)
{
  g_frequencies[channel] = (g_frequencies[channel] & ~3) | (frequency >> 8);

  g_roboplay_interface->opl4_write_fm_1(0xa0 + channel, frequency & 0xff);
  g_roboplay_interface->opl4_write_fm_1(0xb0 + channel, g_frequencies[channel]);
}

void set_volume(uint8_t channel, uint8_t volume_carrier, uint8_t volume_modulator)
{
  uint8_t *instrument = g_instruments[g_channels[channel].instrument];
  uint8_t  op         = g_op_table[channel];

  g_roboplay_interface->opl4_write_fm_1(0x43 + op, volume_carrier | (instrument[2] & ~0x3f));
  if(instrument[8] & 1)
    g_roboplay_interface->opl4_write_fm_1(0x40 + op, volume_modulator | (instrument[3] & ~0x3f));
  else
    g_roboplay_interface->opl4_write_fm_1(0x40 + op, instrument[3]);
}

void set_instrument(uint8_t channel, uint8_t instrument_number)
{
  uint8_t *instrument = g_instruments[instrument_number];
  uint8_t  op         = g_op_table[channel];

  g_channels[channel].instrument = instrument_number;
  g_roboplay_interface->opl4_write_fm_1(0xb0 + op, 0x00);

  g_roboplay_interface->opl4_write_fm_1(0xc0 + channel, instrument[8] | PAN_SETTING_LEFT);
  g_roboplay_interface->opl4_write_fm_1(0x23 + op, instrument[0]);   /* Carrier */
  g_roboplay_interface->opl4_write_fm_1(0x20 + op, instrument[1]);   /* Modulator */
  g_roboplay_interface->opl4_write_fm_1(0x63 + op, instrument[4]);   /* Bits 0..3 = decay, 4..7 = attack */
  g_roboplay_interface->opl4_write_fm_1(0x60 + op, instrument[5]);
  g_roboplay_interface->opl4_write_fm_1(0x83 + op, instrument[6]);   /* Bits 0..3 = release, 4..7 = sustain */
  g_roboplay_interface->opl4_write_fm_1(0x80 + op, instrument[7]);
  g_roboplay_interface->opl4_write_fm_1(0xe3 + op, instrument[9]);   /* Bits 0..1 = waveform select */
  g_roboplay_interface->opl4_write_fm_1(0xe0 + op, instrument[10]);

  set_volume( channel, instrument[2] & 0x3f, instrument[3] & 0x3f);
}

