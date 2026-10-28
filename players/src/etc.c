/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * etc.c
 *
 * ETC: Sam Coupe E-Tracker compiled
 */

#include <string.h>

#include "etc.h"
#include "player_interface.h"


static uint8_t value;

static const uint8_t g_note_frequencies[] =
{
  0x05,  /* C  */
  0x21,  /* C# */
  0x3c,  /* D  */
  0x55,  /* D# */
  0x6d,  /* E  */
  0x84,  /* F  */
  0x99,  /* F# */
  0xad,  /* G  */
  0xc0,  /* G# */
  0xd2,  /* A  */
  0xe3,  /* A# */
  0xf3   /* B  */ 
};

static const uint8_t g_instrument_none[] =
{
  0xfe,  /* set loop */
  0x01,
  0x00,
  0x00,
  0xfc   /* get loop*/
};

static const uint8_t g_ornament_none[] = 
{
  0xfe,  /* set loop */
  0x00,
  0xff   /* get loop*/
};

static const uint8_t g_envelopes[] =
{
  ENVELOPE_LEFT_RIGHT_SAME    | ENVELOPE_BITS_4 | ENVELOPE_MODE_ZERO            | ENVELOPE_RESET,

  ENVELOPE_LEFT_RIGHT_SAME    | ENVELOPE_BITS_3 | ENVELOPE_MODE_REPEAT_DECAY    | ENVELOPE_ENABLED,
  ENVELOPE_LEFT_RIGHT_SAME    | ENVELOPE_BITS_3 | ENVELOPE_MODE_REPEAT_ATTACK   | ENVELOPE_ENABLED,
  ENVELOPE_LEFT_RIGHT_SAME    | ENVELOPE_BITS_3 | ENVELOPE_MODE_REPEAT_TRIANGLE | ENVELOPE_ENABLED,

  ENVELOPE_LEFT_RIGHT_SAME    | ENVELOPE_BITS_4 | ENVELOPE_MODE_REPEAT_DECAY    | ENVELOPE_ENABLED,
  ENVELOPE_LEFT_RIGHT_SAME    | ENVELOPE_BITS_4 | ENVELOPE_MODE_REPEAT_ATTACK   | ENVELOPE_ENABLED,
  ENVELOPE_LEFT_RIGHT_SAME    | ENVELOPE_BITS_4 | ENVELOPE_MODE_REPEAT_TRIANGLE | ENVELOPE_ENABLED,

  ENVELOPE_LEFT_RIGHT_INVERSE | ENVELOPE_BITS_3 | ENVELOPE_MODE_REPEAT_DECAY    | ENVELOPE_ENABLED,
  ENVELOPE_LEFT_RIGHT_INVERSE | ENVELOPE_BITS_3 | ENVELOPE_MODE_REPEAT_ATTACK   | ENVELOPE_ENABLED,
  ENVELOPE_LEFT_RIGHT_INVERSE | ENVELOPE_BITS_3 | ENVELOPE_MODE_REPEAT_TRIANGLE | ENVELOPE_ENABLED,

  ENVELOPE_LEFT_RIGHT_INVERSE | ENVELOPE_BITS_4 | ENVELOPE_MODE_REPEAT_DECAY    | ENVELOPE_ENABLED,
  ENVELOPE_LEFT_RIGHT_INVERSE | ENVELOPE_BITS_4 | ENVELOPE_MODE_REPEAT_ATTACK   | ENVELOPE_ENABLED,
  ENVELOPE_LEFT_RIGHT_INVERSE | ENVELOPE_BITS_4 | ENVELOPE_MODE_REPEAT_TRIANGLE | ENVELOPE_ENABLED,
};

static const uint8_t g_div12[256] = 
{
  0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,5,5, 6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,
  10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,
  13,13,13,13,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,
  16,16,16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18,18,18,18,18,
  19,19,19,19,19,19,19,19,19,19,19,19,20,20,20,20,20,20,20,20,20,20,20,20,21,21,21,21
};

static const uint8_t g_mod12[256] =
{
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
  0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3
};

bool load(char *const file_name) 
{
  g_roboplay_interface->open(file_name, false);

  g_file_data = (uint8_t *)DATA_SEGMENT_BASE;
  g_roboplay_interface->read(g_file_data, DATA_SEGMENT_SIZE);

  g_header = (ETC_HEADER *)g_file_data;
  if(strncmp("ETracker (C) BY ESI.", g_header->signature, 0x14)) 
  {
    /* Skip included player code */
    g_roboplay_interface->seek( 0x4b3, ROBOPLAY_SEEK_START);
    g_roboplay_interface->read(g_file_data, DATA_SEGMENT_SIZE);
    g_roboplay_interface->close();

    g_header = (ETC_HEADER *)g_file_data;
    if(strncmp("ETracker (C) BY ESI.", g_header->signature, 0x14)) 
      return false;
  }
  else g_roboplay_interface->close();

  g_patterns  = &g_file_data[g_header->patterns_offset];
  g_samples   = &g_file_data[g_header->samples_offset];
  g_ornaments = &g_file_data[g_header->ornaments_offset];

  g_default_volume_marker = g_file_data[g_header->samples_volume_decode_table];
  g_volume_table   = (ETC_VOLUME_TABLE_ENTRY *)(&g_file_data[g_header->samples_volume_decode_table + 1]);

  return true;
}

bool update(void) 
{
  for(uint8_t i = 0; i < NR_OF_REGISTERS; i++)
  {
    g_roboplay_interface->soundstar_write(i, g_out_values[i]);
  }

  if(--g_delay == 0)
  {
    g_delay = g_tune_delay;

    for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
    {
      if(!g_channel_data[i].delay_next_note--)
        while(!get_note(i));
    }
  }

  for(uint8_t i = 0; i < NR_OF_CHANNELS; i++) 
  {
    update_channel(i);
  }

  for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
  {
    g_out_values[0x00 + i] = g_channel_data[i].amplitude;
    g_out_values[0x08 + i] = g_channel_data[i].tone_frequency;

    if((i % 2) == 0)
    {
      g_out_values[0x10 + (i >> 1)] = g_channel_data[i].octave;
    }
    else
    {
      g_out_values[0x10 + (i >> 1)] += (g_channel_data[i].octave << 4);
    }

    g_out_values[0x14] = g_out_values[0x14] << 1;
    if(g_channel_data[NR_OF_CHANNELS - 1 - i].frequency_enable)
      g_out_values[0x14] |= 1;
    else
      g_out_values[0x14] &= 0xfe;

    g_out_values[0x15] = g_out_values[0x15] << 1;
    if(g_channel_data[NR_OF_CHANNELS - 1 - i].noise_enable)
      g_out_values[0x15] |= 1;
    else
      g_out_values[0x15] &= 0xfe;

    g_out_values[0x16] = g_noise_0 | g_noise_1 | g_extended_noise_0 | g_extended_noise_1;

    g_out_values[0x18] = g_envelope_generator_0;
    g_out_values[0x19] = g_envelope_generator_1;
  }

  return true; 
}

void rewind(int8_t subsong) 
{
  /* No subsongs in this format */
  subsong;

  g_tune_delay = DEFAULT_TUNE_DELAY;
  g_delay = 1;

  g_envelope_generator_0 = g_envelopes[0];
  g_envelope_generator_1 = g_envelopes[0];

  g_noise_0 = 0;
  g_noise_1 = 0;

  g_extended_noise_0 = 0;
  g_extended_noise_1 = 0;

  g_song_position = &g_file_data[g_header->positions_offset];

  for(uint8_t i = 0; i < NR_OF_REGISTERS; i++)
    g_out_values[i] = 0x00;

  g_loop_position = &g_file_data[g_header->positions_offset];
  g_transposition = 0;
  update_song_position();

  
  for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
  {
    g_channel_data[i].instrument = (uint8_t *)g_instrument_none;
    g_channel_data[i].instrument_loop = (uint8_t *)g_instrument_none;
    g_channel_data[i].ornament = (uint8_t *)g_ornament_none;
    g_channel_data[i].ornament_loop = (uint8_t *)g_ornament_none;

    g_channel_data[i].note = 0;
    g_channel_data[i].ornament_note = 0;
    g_channel_data[i].tone_deviation = 0;

    g_channel_data[i].amplitude = 0;
    g_channel_data[i].tone_frequency = 0;
    g_channel_data[i].octave = 0;

    g_channel_data[i].frequency_enable = false;
    g_channel_data[i].noise_enable = false;

    g_channel_data[i].instrument_start = (uint8_t *)g_instrument_none; 
    g_channel_data[i].ornament_start = (uint8_t *)g_ornament_none;

    g_channel_data[i].delay_next_note = 0;
    g_channel_data[i].delay_next_ornament = 1;
    g_channel_data[i].delay_next_instrument = 1;
    g_channel_data[i].delay_next_volume = 1;

    g_channel_data[i].instrument_inversion = false;
    g_channel_data[i].volume_reduction = 0;
  }
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
  /* No subsongs in this format */
  return 0;
}

char *get_player_info(void) 
{
  return "Sam Coupe E-Tracker Compiled player V1.2 by RoboSoft Inc.";
}

char *get_title(void) 
{
  return "-";
}

char *get_author(void) 
{
  return "-";
}

char *get_description(void)
{
  return "-";
}

void set_instrument(uint8_t channel, uint8_t *instrument)
{
  g_channel_data[channel].instrument = instrument;
  set_ornament(channel, g_channel_data[channel].ornament_start);
}

void set_ornament(uint8_t channel, uint8_t *ornament)
{
  g_channel_data[channel].ornament = ornament;

  g_channel_data[channel].delay_next_ornament = 1;
  g_channel_data[channel].delay_next_instrument = 1;
  g_channel_data[channel].delay_next_volume = 1;
}

void update_song_position(void)
{
  bool pattern_found = false;
  do
  {
    value = *g_song_position++;

    if(value == 0xff) g_song_position = g_loop_position;
    else if(value == 0xfe) g_loop_position = g_song_position;
    else if(value >= 0x60) g_transposition = value - 0x60;
    else
    {
      uint16_t *track = &g_patterns[(value / 3) * NR_OF_CHANNELS];
      for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
      {
        g_channel_data[i].track = (uint8_t *)(DATA_SEGMENT_BASE + *track++);
      }
      pattern_found = true;
    }
  } while (!pattern_found);
}

void update_channel(uint8_t channel)
{
  g_current_channel = channel;
  g_current_channel_data = &g_channel_data[channel];

  if(!--g_current_channel_data->delay_next_instrument)
    handle_instrument();

  if(!--g_current_channel_data->delay_next_ornament)
    handle_ornament();

  if(!--g_current_channel_data->delay_next_volume)
    handle_volume();

  uint16_t note = g_current_channel_data->note + g_current_channel_data->ornament_note;

  if(note != 0x5f)
  {
    note += g_transposition;
    if(note >= 0x100)
      note -= 0x60;
  }

  uint16_t instrument_pitch = g_div12[note] << 8;
  instrument_pitch += g_note_frequencies[g_mod12[note]];
  instrument_pitch += g_channel_data[channel].tone_deviation;
  instrument_pitch = instrument_pitch & 0x07ff;

  g_current_channel_data->octave = instrument_pitch >> 8;
  g_current_channel_data->tone_frequency = instrument_pitch & 0xff;
}

void handle_instrument(void)
{
  uint8_t delay_next_instrument = 1;
  bool loop_or_delay = false;

  do
  {
    value = *g_current_channel_data->instrument++;

    loop_or_delay = (value & 1) == 0;
    if(loop_or_delay)
    {
      if(value == 0xfe)
        g_current_channel_data->instrument_loop = g_current_channel_data->instrument;
      else if(value == 0xfc)
        g_current_channel_data->instrument = g_current_channel_data->instrument_loop;
      else
        delay_next_instrument = 2 + (value >> 1);
    }
  } while (loop_or_delay);

  g_current_channel_data->delay_next_instrument = delay_next_instrument;
  if(delay_next_instrument == 0) g_current_channel_data->delay_next_instrument++;

  g_current_channel_data->frequency_enable = ((value & 16) != 0);
  g_current_channel_data->noise_enable = ((value & 32) != 0);

  if(g_current_channel < 3)
    g_noise_0 = value >> 6;
  else
    g_noise_1 = (value >> 6) << 4;

  g_current_channel_data->tone_deviation = ((value & 0x0e) >> 1) << 8;
  g_current_channel_data->tone_deviation += *g_current_channel_data->instrument++;
}

void handle_ornament(void)
{
  bool loop_or_delay = false;
  uint8_t delay_next_ornament = 1;

  do 
  {
    value = *g_current_channel_data->ornament++;

    loop_or_delay = (value >= (8 * 12));
    if(loop_or_delay)
    {
      if(value == 0xff)
        g_current_channel_data->ornament = g_current_channel_data->ornament_loop;
      else if(value == 0xfe)
        g_current_channel_data->ornament_loop = g_current_channel_data->ornament;
      else
        delay_next_ornament = value - (8 * 12);
    }
  } while(loop_or_delay);

  g_current_channel_data->delay_next_ornament = delay_next_ornament;
  if(delay_next_ornament == 0) g_current_channel_data->delay_next_ornament++;
  
  g_current_channel_data->ornament_note = value;
}

void handle_volume(void)
{
  value = *g_current_channel_data->instrument++;

  if(value == g_default_volume_marker)
  {
    g_current_channel_data->delay_next_volume = *g_current_channel_data->instrument++;
    g_current_channel_data->amplitude = *g_current_channel_data->instrument++;
  }
  else
  {
    uint8_t index = 0;
    while(g_volume_table[index].code)
    {
      if(g_volume_table[index].code == value)
      {
        g_current_channel_data->delay_next_volume = g_volume_table[index].delay;
        g_current_channel_data->amplitude = *g_current_channel_data->instrument++;
        break;
      }
      index++;
    }
    if(!value == g_volume_table[index].code)
    {
        g_current_channel_data->delay_next_volume = 1;
        g_current_channel_data->amplitude = value;
    }
  }

  uint8_t left = g_current_channel_data->amplitude & 0x0f;
  uint8_t right = (g_current_channel_data->amplitude & 0xf0) >> 4;

  if(left > g_current_channel_data->volume_reduction)
    left -= g_current_channel_data->volume_reduction;
  else
    left = 0;

  if(right > g_current_channel_data->volume_reduction)
    right -= g_current_channel_data->volume_reduction;
  else
    right = 0;

  if(g_current_channel_data->instrument_inversion)
    g_current_channel_data->amplitude = right + (left << 4);
  else
    g_current_channel_data->amplitude = (right << 4) + left;
}

bool get_note(uint8_t channel)
{
  bool result = true;

  bool wait_for_next_note = false;
  do
  {
    value = *g_channel_data[channel].track++;

    if(value >= DELAY_NEXT_NOTE)
    {
      cmd_set_delay_next_note(channel, value - DELAY_NEXT_NOTE);
      wait_for_next_note = true;
    }
    else if(value >= SET_NOTE)
    {
      cmd_set_note(channel, value - SET_NOTE);
    }
    else if(value >= SET_INSTRUMENT)
    {
      cmd_set_instrument(channel, value - SET_INSTRUMENT);
    }
    else if(value >= END_OF_TRACK)
    {
      cmd_end_of_track();
      result = false;
      wait_for_next_note = true;
    }
    else if(value >= STOP_SOUND)
    {
      cmd_stop_sound(channel);
    }
    else if(value >= SET_ORNAMENT)
    {
      cmd_set_ornament(channel, value - SET_ORNAMENT);
    }
    else if(value >= INSTRUMENT_INVERSION)
    {
      cmd_instrument_inversion(channel, value - INSTRUMENT_INVERSION);
    }
    else if(value >= ENVELOPE)
    {
      cmd_envelope(channel, value - ENVELOPE);
    }
    else if(value >= VOLUME_REDUCTION)
    {
      cmd_volume_reduction(channel, value - VOLUME_REDUCTION);
    }
    else if(value >= EXTENDED_NOISE)
    {
      cmd_extended_noise(channel, value - EXTENDED_NOISE);
    }
    else
    {
      cmd_tune_delay(value);
    }
  } while (!wait_for_next_note); 

  return result;
}

void cmd_set_delay_next_note(uint8_t channel, uint8_t delay)
{
  g_channel_data[channel].delay_next_note = delay;
}

void cmd_set_note(uint8_t channel, uint8_t note)
{
  g_channel_data[channel].note = note;
  set_instrument(channel, g_channel_data[channel].instrument_start);
}

void cmd_set_instrument(uint8_t channel, uint8_t instrument_index)
{
  uint16_t *instrument = &g_samples[instrument_index];
  g_channel_data[channel].instrument_start = (uint8_t *)(DATA_SEGMENT_BASE + *instrument);
  g_channel_data[channel].instrument_loop  = (uint8_t *)g_instrument_none;

  set_instrument(channel, g_channel_data[channel].instrument_start);
}

void cmd_end_of_track(void)
{
  update_song_position();
}

void cmd_stop_sound(uint8_t channel)
{
  set_instrument(channel, g_instrument_none);
}

void cmd_set_ornament(uint8_t channel, uint8_t ornament_index)
{
  uint16_t *ornament = &g_ornaments[ornament_index];

  g_channel_data[channel].ornament_start = (uint8_t *)(DATA_SEGMENT_BASE + *ornament);
  g_channel_data[channel].ornament_loop  = (uint8_t *)g_ornament_none;

  set_ornament(channel, g_channel_data[channel].ornament_start);
}

void cmd_instrument_inversion(uint8_t channel, uint8_t inversion)
{
  g_channel_data[channel].instrument_inversion = (inversion != 0);
}

void cmd_envelope(uint8_t channel, uint8_t envelope)
{
  if(channel < 3)
     g_envelope_generator_0 = g_envelopes[envelope];
  else
     g_envelope_generator_1 = g_envelopes[envelope];
}

void cmd_volume_reduction(uint8_t channel, uint8_t reduction)
{
  g_channel_data[channel].volume_reduction = reduction;
}

void cmd_extended_noise(uint8_t channel, uint8_t extended_noise)
{
  if(channel < 3)
  {
    g_extended_noise_0 = (extended_noise) ? NOISE_0_VARIABLE : 0; 
  }
  else
  {
    g_extended_noise_1 = (extended_noise) ? NOISE_1_VARIABLE : 0; 
  }
}

void cmd_tune_delay(uint8_t delay)
{
  g_tune_delay = delay + 1;
}