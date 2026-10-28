/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * pt3.c
 *
 * PT3: ProTracker 3.x / Vortex Tracker II player
 */

#include <string.h>

#include "player_interface.h"
#include "pt3.h"

bool load(char *const file_name)
{
  g_pt3 = (PT3*)READ_BUFFER;
  g_pt3[MAIN_PT3].valid = false;
  g_pt3[SUB_PT3].valid = false;
 
  g_current_pt3 = 0;
  g_roboplay_interface->open(file_name, false);

  /* Check for 6 channels */
  uint8_t buffer[16];
  g_roboplay_interface->seek(-16, ROBOPLAY_SEEK_END);
  g_roboplay_interface->read(buffer, 16);
  g_roboplay_interface->seek(0, ROBOPLAY_SEEK_START);

  if(!memcmp("02TS", &buffer[12], 4))
  {
    /* Main PT3 song */
    if(!memcmp("PT3!", &buffer[0], 4))
    {
      g_pt3[MAIN_PT3].valid = true;
      g_pt3[MAIN_PT3].song_length = buffer[5];
      g_pt3[MAIN_PT3].song_length <<= 8;
      g_pt3[MAIN_PT3].song_length += buffer[4];

      g_pt3[MAIN_PT3].segment = START_SEGMENT_INDEX;
    }

    /* Sub PT3 song */
    if(!memcmp("PT3!", &buffer[6], 4))
    {
      g_pt3[SUB_PT3].valid = true;
      g_pt3[SUB_PT3].song_length = buffer[11];
      g_pt3[SUB_PT3].song_length <<= 8;
      g_pt3[SUB_PT3].song_length += buffer[10];

      g_pt3[SUB_PT3].segment = g_roboplay_interface->get_new_segment();
    }
  }
  else
  {
    g_pt3[MAIN_PT3].valid = true;
    g_pt3[MAIN_PT3].segment = START_SEGMENT_INDEX;
    g_pt3[MAIN_PT3].song_length = DATA_SEGMENT_SIZE;
  }

  bool result = false;
  
  if(g_pt3[MAIN_PT3].valid)
  {
    g_roboplay_interface->set_segment(g_pt3[MAIN_PT3].segment);
    g_current_pt3 = MAIN_PT3;
    result = load_pt3();
  }

  if(result && g_pt3[SUB_PT3].valid)
  {
    g_roboplay_interface->set_segment(g_pt3[SUB_PT3].segment);
    g_current_pt3 = SUB_PT3;
    result = load_pt3();
  }

  g_roboplay_interface->close();

  return result;
}

bool update(void)
{
  if(g_pt3[SUB_PT3].valid)
  {
    g_current_pt3 = SUB_PT3;
    g_roboplay_interface->set_segment(g_pt3[g_current_pt3].segment);
    update_pt3();
  }

  g_current_pt3 = MAIN_PT3;
  g_roboplay_interface->set_segment(g_pt3[g_current_pt3].segment);
  update_pt3();

  update_registers();

  return true;
}

void rewind(int8_t subsong)
{
  /* No subsongs in this format */
  subsong;

  if(g_pt3[SUB_PT3].valid)
  {
    g_current_pt3 = SUB_PT3;
    g_roboplay_interface->set_segment(g_pt3[g_current_pt3].segment);
    rewind_pt3();
  }

  g_current_pt3 = MAIN_PT3;
  g_roboplay_interface->set_segment(g_pt3[g_current_pt3].segment);
  rewind_pt3();
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
  return "ProTracker 3.x / Vortex Tracker II player V1.2 by RoboSoft Inc.";
}

char* get_title(void)
{
  return g_pt3[MAIN_PT3].header.name;
}

char* get_author(void)
{
  return g_pt3[MAIN_PT3].header.author;
}

char* get_description(void)
{
  if(g_pt3[1].valid)
    return "6 Channels";
  else
    return "3 Channels";
}

bool load_pt3(void)
{
  g_pt3[g_current_pt3].song.data = (uint8_t *)DATA_SEGMENT_BASE;
  g_roboplay_interface->read(g_pt3[g_current_pt3].song.data, g_pt3[g_current_pt3].song_length);

  memcpy(&g_pt3[g_current_pt3].header.magic, &g_pt3[g_current_pt3].song.data[0x00], 13);
  g_pt3[g_current_pt3].header.magic[13] = '\0';

  if(memcmp(g_pt3[g_current_pt3].header.magic, "ProTracker 3.", 13) && memcmp(g_pt3[g_current_pt3].header.magic, "Vortex Tracke", 13))
    return false;
  
  g_pt3[g_current_pt3].header.version = 6;
  if(g_pt3[g_current_pt3].song.data[0x0d] >= '0' && g_pt3[g_current_pt3].song.data[0x0d] <= '9')
    g_pt3[g_current_pt3].header.version = g_pt3[g_current_pt3].song.data[0x0d] - '0';

  memcpy(&g_pt3[g_current_pt3].header.name, &g_pt3[g_current_pt3].song.data[0x1e], 32);
  g_pt3[g_current_pt3].header.name[32] = '\0';

  memcpy(&g_pt3[g_current_pt3].header.author, &g_pt3[g_current_pt3].song.data[0x42], 32);
  g_pt3[g_current_pt3].header.author[32] = '\0';

  g_pt3[g_current_pt3].header.which_frequency_table = g_pt3[g_current_pt3].song.data[0x63];
  g_pt3[g_current_pt3].header.speed                 = g_pt3[g_current_pt3].song.data[0x64];
  g_pt3[g_current_pt3].header.num_patterns          = g_pt3[g_current_pt3].song.data[0x65] + 1;
  g_pt3[g_current_pt3].header.loop                  = g_pt3[g_current_pt3].song.data[0x66];
  g_pt3[g_current_pt3].header.pattern_loc           = (g_pt3[g_current_pt3].song.data[0x68] << 8) | g_pt3[g_current_pt3].song.data[0x67];

  /* Sample positions */
  for(uint8_t i = 0; i < NUMBER_OF_SAMPLE_PATTERNS; i++)
  {
    g_pt3[g_current_pt3].header.sample_patterns[i] =
      (g_pt3[g_current_pt3].song.data[0x6a + (i * 2)] << 8) | g_pt3[g_current_pt3].song.data[0x69 + (i * 2)];
  }

  /* Ornament positions */
  for(uint8_t i = 0; i < NUMBER_OF_ORNAMENT_PATTERNS; i++)
  {
    g_pt3[g_current_pt3].header.ornament_patterns[i] =
      (g_pt3[g_current_pt3].song.data[0xaa + (i * 2)] << 8) | g_pt3[g_current_pt3].song.data[0xa9 + (i * 2)];
  }

  /* Pattern order */
  g_pt3[g_current_pt3].header.pattern_order = (g_pt3[g_current_pt3].song.data[0xca] << 8) | g_pt3[g_current_pt3].song.data[0xc9];

  return true;
}

void update_pt3(void)
{
  g_pt3[g_current_pt3].delay--;

  if(!g_pt3[g_current_pt3].delay)
  {
    g_pt3[g_current_pt3].delay = g_pt3[g_current_pt3].header.speed;
    decode_line();

    if(g_pt3[g_current_pt3].song.a.all_done && g_pt3[g_current_pt3].song.b.all_done && g_pt3[g_current_pt3].song.c.all_done)
    {
      g_pt3[g_current_pt3].song.current_pos++;
      if(g_pt3[g_current_pt3].song.data[0xc9 + g_pt3[g_current_pt3].song.current_pos] == 0xff)
        g_pt3[g_current_pt3].song.current_pos = g_pt3[g_current_pt3].header.loop;

      set_pattern(g_pt3[g_current_pt3].song.current_pos);
      decode_line();
    }
  }

  create_frame();
}

void rewind_pt3(void)
{
	memset(&g_pt3[g_current_pt3].song.a, 0, sizeof(PT3_NOTE_TYPE));
	memset(&g_pt3[g_current_pt3].song.b, 0, sizeof(PT3_NOTE_TYPE));
	memset(&g_pt3[g_current_pt3].song.c, 0, sizeof(PT3_NOTE_TYPE));

	g_pt3[g_current_pt3].song.a.which='A';
	g_pt3[g_current_pt3].song.a.volume = 15;
	g_pt3[g_current_pt3].song.a.tone_sliding = 0;
	g_pt3[g_current_pt3].song.a.amplitude_sliding = 0;
	g_pt3[g_current_pt3].song.a.enabled = false;
	g_pt3[g_current_pt3].song.a.envelope_enabled = false;
	g_pt3[g_current_pt3].song.a.ornament = 0;
	g_pt3[g_current_pt3].song.a.ornament_position = 0;
	load_ornament('A');
	g_pt3[g_current_pt3].song.a.sample_position = 0;
	g_pt3[g_current_pt3].song.a.sample = 1;
	load_sample('A');

	g_pt3[g_current_pt3].song.b.which = 'B';
	g_pt3[g_current_pt3].song.b.volume = 15;
	g_pt3[g_current_pt3].song.b.tone_sliding = 0;
	g_pt3[g_current_pt3].song.b.amplitude_sliding = 0;
	g_pt3[g_current_pt3].song.b.enabled = false;
	g_pt3[g_current_pt3].song.b.envelope_enabled = false;
	g_pt3[g_current_pt3].song.b.ornament = 0;
	g_pt3[g_current_pt3].song.b.ornament_position = 0;
	load_ornament('B');
	g_pt3[g_current_pt3].song.b.sample_position = 0;
	g_pt3[g_current_pt3].song.b.sample = 1;
	load_sample('B');

	g_pt3[g_current_pt3].song.c.which = 'C';
	g_pt3[g_current_pt3].song.c.volume = 15;
	g_pt3[g_current_pt3].song.c.tone_sliding = 0;
	g_pt3[g_current_pt3].song.c.amplitude_sliding = 0;
	g_pt3[g_current_pt3].song.c.enabled = false;
	g_pt3[g_current_pt3].song.c.envelope_enabled = false;
	g_pt3[g_current_pt3].song.c.ornament = 0;
	g_pt3[g_current_pt3].song.c.ornament_position = 0;
	load_ornament('C');
	g_pt3[g_current_pt3].song.c.sample_position = 0;
	g_pt3[g_current_pt3].song.c.sample = 1;
	load_sample('C');

	memset(&g_pt3[g_current_pt3].song.a_old, 0, sizeof(PT3_NOTE_TYPE));
	memset(&g_pt3[g_current_pt3].song.b_old, 0, sizeof(PT3_NOTE_TYPE));
	memset(&g_pt3[g_current_pt3].song.c_old, 0, sizeof(PT3_NOTE_TYPE));
 
	g_pt3[g_current_pt3].song.noise_period = 0;
	g_pt3[g_current_pt3].song.noise_add = 0;
	g_pt3[g_current_pt3].song.envelope_period = 0;
	g_pt3[g_current_pt3].song.envelope_type = 0;
	g_pt3[g_current_pt3].song.envelope_type_old = 0xff;
	g_pt3[g_current_pt3].song.envelope_slide_add = 0;
	g_pt3[g_current_pt3].song.current_pattern = 0;

  setup_frequency_table();

  for(uint8_t i = 0; i < NR_OF_PSG_REGISTERS; i++)
    g_pt3[g_current_pt3].registers[i] = 0x00;

  g_pt3[g_current_pt3].song.current_pos = 0;
  set_pattern(g_pt3[g_current_pt3].song.current_pos);

  g_pt3[g_current_pt3].delay = 1;
}

void setup_frequency_table(void)
{
  if (g_pt3[g_current_pt3].header.which_frequency_table == 0)
  {
    if (g_pt3[g_current_pt3].header.version <= 3) 
    {
      /* create table #0 v3.3 "PT3NoteTable_PT_33_34r" */
      note_table_propagate(base0_v3);
    }
    else
    {
      /* create table #0 v3.4 "PT3NoteTable_PT_34_35" */
      note_table_propagate(base0_v4);
      note_table_adjust(table0_v4_adjust);
    }
  }
  else if (g_pt3[g_current_pt3].header.which_frequency_table == 1) 
  {
    /* create table #1 "PT3NoteTable_ST" */
    note_table_propagate(base1);
    g_pt3[g_current_pt3].frequency_table[23] += 13;
    g_pt3[g_current_pt3].frequency_table[46] -= 1;
  }
  else if (g_pt3[g_current_pt3].header.which_frequency_table == 2)
  {
    if (g_pt3[g_current_pt3].header.version <= 3)
    {
      /* Create Table #2, v3, "PT3NoteTable_ASM_34r" */
      note_table_propagate(base2_v3);
      note_table_adjust(table2_v3_adjust);
      g_pt3[g_current_pt3].frequency_table[86] += 1;
      g_pt3[g_current_pt3].frequency_table[87] += 1;
    }
    else 
    {
      /* Create Table #2, v4+, "PT3NoteTable_ASM_34_35" */
      note_table_propagate(base2_v4);
      note_table_adjust(table2_v4_adjust);
    }
  }
  else 
  {
    if (g_pt3[g_current_pt3].header.version <= 3) 
    {
      /* Create Table #3, v3, "PT3NoteTable_REAL_34r" */
      note_table_propagate(base3);
      note_table_adjust(table3_v4_adjust);
      g_pt3[g_current_pt3].frequency_table[43]++;
    }
    else 
    {
      /* Create Table #3, v4+, "PT3NoteTable_REAL_34_35" */
      note_table_propagate(base3);
      note_table_adjust(table3_v4_adjust);
    }
  }
}

void note_table_propagate(const uint16_t *base_table)
{
  for(uint8_t i = 0; i < 12; i++)
    g_pt3[g_current_pt3].frequency_table[i] = base_table[i];

  for(uint8_t i = 0; i < 84; i++)
    g_pt3[g_current_pt3].frequency_table[i + 12] = g_pt3[g_current_pt3].frequency_table[i] >> 1;
}

void note_table_adjust(const uint8_t* adjust_table)
{
  for(uint8_t i = 0; i < 12; i++)
  {
    uint8_t offset = i;
    uint8_t temp = adjust_table[i];
    for(uint8_t j = 0; j < 8; j++)
    {
      uint8_t extra = temp & 1;
      temp >>= 1;

      g_pt3[g_current_pt3].frequency_table[offset] += extra;
      offset += 12;
    }
  }
}

void set_pattern(const uint8_t position)
{
  g_pt3[g_current_pt3].song.current_pattern = g_pt3[g_current_pt3].song.data[0xc9 + position] / 3;

  g_pt3[g_current_pt3].song.a.addr = 
           g_pt3[g_current_pt3].song.data[(g_pt3[g_current_pt3].song.current_pattern * 6) + 0 + g_pt3[g_current_pt3].header.pattern_loc] |
          (g_pt3[g_current_pt3].song.data[(g_pt3[g_current_pt3].song.current_pattern * 6) + 1 + g_pt3[g_current_pt3].header.pattern_loc] << 8);

  g_pt3[g_current_pt3].song.b.addr =
           g_pt3[g_current_pt3].song.data[(g_pt3[g_current_pt3].song.current_pattern * 6) + 2 + g_pt3[g_current_pt3].header.pattern_loc] |
          (g_pt3[g_current_pt3].song.data[(g_pt3[g_current_pt3].song.current_pattern * 6) + 3 + g_pt3[g_current_pt3].header.pattern_loc] << 8);

  g_pt3[g_current_pt3].song.c.addr =
            g_pt3[g_current_pt3].song.data[(g_pt3[g_current_pt3].song.current_pattern * 6) + 4 + g_pt3[g_current_pt3].header.pattern_loc] |
          (g_pt3[g_current_pt3].song.data[(g_pt3[g_current_pt3].song.current_pattern * 6) + 5 + g_pt3[g_current_pt3].header.pattern_loc] << 8);

  g_pt3[g_current_pt3].song.a.all_done = false;
  g_pt3[g_current_pt3].song.b.all_done = false;
  g_pt3[g_current_pt3].song.c.all_done = false;

  g_pt3[g_current_pt3].song.noise_period = 0;
}

void decode_line(void)
{
  decode_note(&g_pt3[g_current_pt3].song.a);
  decode_note(&g_pt3[g_current_pt3].song.b);
  decode_note(&g_pt3[g_current_pt3].song.c);
}

void decode_note(PT3_NOTE_TYPE* channel)
{
  channel->new_note = false;

  channel->spec_command = 0;
  channel->spec_delay = 0;
  channel->spec_lo = 0;

  /* Skip decode if note still running */
  if(channel->len_count > 1)
  {
    channel->len_count--;
    return;
  }

  uint8_t prev_note = channel->note;
  int16_t prev_sliding = channel->tone_sliding;

  bool done = false;
  while(true)
  {
    channel->len_count = channel->len;

    uint16_t current_val = g_pt3[g_current_pt3].song.data[channel->addr];

    switch((current_val >> 4) & 0x0f)
    {
      case 0x00:
        if(current_val == 0x00)
        {
          channel->len_count = 0;
          channel->all_done = true;
          done = true;
        }
        else
        {
          channel->spec_command = current_val & 0x0f;
        }
        break;
      case 0x01:
        if((current_val & 0x0f) == 0x00)
          channel->envelope_enabled = false;
        else
        {
          g_pt3[g_current_pt3].song.envelope_type_old = 0x78;
          g_pt3[g_current_pt3].song.envelope_type = (current_val & 0x0f);

          channel->addr++;
          current_val = g_pt3[g_current_pt3].song.data[channel->addr];
          g_pt3[g_current_pt3].song.envelope_period = (current_val << 8);

          channel->addr++;
          current_val = g_pt3[g_current_pt3].song.data[channel->addr];
          g_pt3[g_current_pt3].song.envelope_period |= (current_val & 0xff);

          channel->envelope_enabled = true;
          g_pt3[g_current_pt3].song.envelope_slide = 0;
          g_pt3[g_current_pt3].song.envelope_delay = 0;
        }
        
        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->sample = (current_val / 2);
        load_sample(channel->which);
        channel->ornament_position = 0;
        break;
      case 0x02:
        g_pt3[g_current_pt3].song.noise_period = (current_val & 0x0f);
        break;
      case 0x03:
        g_pt3[g_current_pt3].song.noise_period = (current_val & 0x0f) + 0x10;
        break;
      case 0x04:
        channel->ornament = (current_val & 0x0f);
        load_ornament(channel->which);
        channel->ornament_position = 0;
        break;
      case 0x05:
      case 0x06:
      case 0x07:
      case 0x08:
      case 0x09:
      case 0x0a:
        channel->new_note = true;
        channel->note = (current_val - 0x50);
        channel->original_note = (current_val - 0x50);
        channel->sample_position = 0;
        channel->amplitude_sliding = 0;
        channel->noise_sliding = 0;
        channel->envelope_sliding = 0;
        channel->ornament_position = 0;
        channel->tone_slide_count = 0;
        channel->tone_sliding = 0;
        channel->tone_accumulator = 0;
        channel->onoff = 0;
        channel->enabled = true;

        done = true;
        break;
      case 0x0b:
        /* Disable envelope */
        if(current_val == 0xb0)
        {
          channel->envelope_enabled = false;
          channel->ornament_position = 0;
        }
        /* Set len */
        else if(current_val == 0xb1)
        {
          channel->addr++;
          current_val = g_pt3[g_current_pt3].song.data[channel->addr];
          channel->len = current_val;
          channel->len_count = channel->len;         
        }
        else
        {
          channel->envelope_enabled = true;
          g_pt3[g_current_pt3].song.envelope_type_old = 0x78;
          g_pt3[g_current_pt3].song.envelope_type = (current_val & 0x0f) - 1;

          channel->addr++;
          current_val = g_pt3[g_current_pt3].song.data[channel->addr];
          g_pt3[g_current_pt3].song.envelope_period = (current_val << 8);

          channel->addr++;
          current_val = g_pt3[g_current_pt3].song.data[channel->addr];
          g_pt3[g_current_pt3].song.envelope_period |= (current_val & 0xff);

          channel->ornament_position = 0;
          g_pt3[g_current_pt3].song.envelope_slide = 0;
          g_pt3[g_current_pt3].song.envelope_delay = 0;
        }
        break;
      case 0x0c:
        /* Volume */
        if((current_val & 0x0f) == 0x00)
        {
          channel->sample_position = 0;
          channel->amplitude_sliding = 0;
          channel->noise_sliding = 0;
          channel->envelope_sliding = 0;
          channel->ornament_position = 0;
          channel->tone_slide_count = 0;
          channel->tone_sliding = 0;
          channel->tone_accumulator = 0;
          channel->onoff = 0;
          channel->enabled = false;
          
          done = true;
        }
        else
        {
          channel->volume = (current_val & 0x0f);
        }
        break;
      case 0x0d:
        if(current_val == 0xd0)
          done = true;
        else
        {
          channel->sample = (current_val & 0x0f);
          load_sample(channel->which);
        }
        break;
      case 0x0e:
        channel->sample = (current_val - 0xd0);
        load_sample(channel->which);
        break;
      case 0x0f:
        channel->envelope_enabled = false;
        channel->ornament_position = 0;
        
        channel->ornament = (current_val & 0x0f);
        load_ornament(channel->which);

        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->sample = current_val / 2;
        load_sample(channel->which);
        break;
    }

    channel->addr++;

    if(done)
    {
      if(channel->spec_command == 0x00) 
      {
      }
      /* Tone down */
      else if(channel->spec_command == 0x01)
      {
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->spec_delay = current_val;
        channel->tone_slide_delay = current_val;
        channel->tone_slide_count = channel->tone_slide_delay;

        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->spec_lo = current_val;

        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->spec_hi = current_val;

        channel->tone_slide_step = channel->spec_hi;
        channel->tone_slide_step <<= 8;
        channel->tone_slide_step += channel->spec_lo;
        
        channel->simplegliss = true;
        channel->onoff = 0;

        channel->addr++;
      }
      /* Portamento */
      else if(channel->spec_command == 0x02)
      {
        channel->simplegliss = false;
        channel->onoff = 0;

        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->spec_delay = current_val;

        channel->tone_slide_delay = current_val;
        channel->tone_slide_count = channel->tone_slide_delay;
        
        channel->addr++;
        channel->addr++;
        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->spec_lo = current_val;

        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->spec_hi = current_val;

        channel->tone_slide_step = channel->spec_hi;
        channel->tone_slide_step <<= 8;
        channel->tone_slide_step += channel->spec_lo;

        channel->addr++;

        if(channel->tone_slide_step < 0)
          channel->tone_slide_step = -channel->tone_slide_step;

        channel->tone_delta = g_pt3[g_current_pt3].frequency_table[channel->note] - g_pt3[g_current_pt3].frequency_table[prev_note];
        channel->slide_to_note = channel->note;
        channel->note = prev_note;

        if(g_pt3[g_current_pt3].header.version > 6)
          channel->tone_sliding = prev_sliding;

        if(channel->tone_delta - channel->tone_sliding < 0)
          channel->tone_slide_step = -channel->tone_slide_step;
      }
      /* Position in sample */
      else if(channel->spec_command == 0x03)
      {
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->sample_position = current_val;
        channel->addr++;
      }
      /* Position in ornament */
      else if(channel->spec_command == 0x04)
      {
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->ornament_position = current_val;
        channel->addr++;
      }
      /* Vibrato */
      else if(channel->spec_command == 0x05)
      {
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->onoff_delay = current_val;
        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->offon_delay = current_val;
        channel->addr++;

        channel->onoff = channel->onoff_delay;
        channel->tone_slide_count = 0;
        channel->tone_sliding = 0;
      }
      /* Envelope slide */
      else if(channel->spec_command == 0x08)
      {
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        g_pt3[g_current_pt3].song.envelope_delay = current_val;
        g_pt3[g_current_pt3].song.envelope_delay_orig = current_val;
        channel->spec_delay = current_val;
      
        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->spec_lo = (current_val & 0xff);

        channel->addr++;
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        channel->spec_hi = (current_val & 0xff);

        channel->addr++;

        g_pt3[g_current_pt3].song.envelope_slide_add = channel->spec_hi;
        g_pt3[g_current_pt3].song.envelope_slide_add <<= 8;
        g_pt3[g_current_pt3].song.envelope_slide_add += channel->spec_lo;
      }
      /* Set speed */
      else if(channel->spec_command == 0x09)
      {
        current_val = g_pt3[g_current_pt3].song.data[channel->addr];
        g_pt3[g_current_pt3].header.speed = current_val;
        channel->addr++;
      }
      break;
    }
  }
}

void load_ornament(char which)
{
  PT3_NOTE_TYPE* channel;

  if(which == 'A') channel = &(g_pt3[g_current_pt3].song.a);
  else if(which == 'B') channel = &(g_pt3[g_current_pt3].song.b);
  else if(which == 'C') channel = &(g_pt3[g_current_pt3].song.c);
  else return;

  channel->ornament_pointer = g_pt3[g_current_pt3].header.ornament_patterns[channel->ornament];
  channel->ornament_loop = g_pt3[g_current_pt3].song.data[channel->ornament_pointer];
  channel->ornament_pointer++;
  channel->ornament_length = g_pt3[g_current_pt3].song.data[channel->ornament_pointer];
  channel->ornament_pointer++;
}

void load_sample(char which)
{
  PT3_NOTE_TYPE* channel;

  if(which == 'A') channel = &(g_pt3[g_current_pt3].song.a);
  else if(which == 'B') channel = &(g_pt3[g_current_pt3].song.b);
  else if(which == 'C') channel = &(g_pt3[g_current_pt3].song.c);
  else return;

  channel->sample_pointer = g_pt3[g_current_pt3].header.sample_patterns[channel->sample];
  channel->sample_loop = g_pt3[g_current_pt3].song.data[channel->sample_pointer];
  channel->sample_pointer++;
  channel->sample_length = g_pt3[g_current_pt3].song.data[channel->sample_pointer];
  channel->sample_pointer++;
}

void calculate_note(PT3_NOTE_TYPE* channel)
{
  if(channel->enabled)
  {
    uint8_t b0 = g_pt3[g_current_pt3].song.data[channel->sample_pointer + (uint16_t)(channel->sample_position) * 4];
    uint8_t b1 = g_pt3[g_current_pt3].song.data[channel->sample_pointer + (uint16_t)(channel->sample_position) * 4 + 1];

		channel->tone = g_pt3[g_current_pt3].song.data[channel->sample_pointer + (channel->sample_position) * 4 + 2];
		channel->tone += (g_pt3[g_current_pt3].song.data[channel->sample_pointer + (channel->sample_position)* 4 + 3]) << 8;
		channel->tone += channel->tone_accumulator;

    if((b1 & 0x40) != 0)
      channel->tone_accumulator = channel->tone;

    /* Get the Note and add in the ornament value */
    int32_t t = g_pt3[g_current_pt3].song.data[channel->ornament_pointer + channel->ornament_position];
    t <<= 24;
    t >>= 24;
    int32_t j = channel->note + t;

    if(j < 0)
      j = 0;
    else if(j > 95)
      j = 95;

    /* Look up note in the frequency table */
    uint16_t w = g_pt3[g_current_pt3].frequency_table[j];

    channel->tone = (channel->tone + channel->tone_sliding + w) & 0xfff;

    /* If we are sliding, handle the sliding part */
    if(channel->tone_slide_count > 0)
    {
      channel->tone_slide_count--;
      if(channel->tone_slide_count == 0)
      {
        channel->tone_sliding += channel->tone_slide_step;
        channel->tone_slide_count = channel->tone_slide_delay;
        if(!channel->simplegliss)
        {
					if ( ((channel->tone_slide_step < 0) &&
					      (channel->tone_sliding <= channel->tone_delta)) ||
					     ((channel->tone_slide_step >= 0) &&
				 	      (channel->tone_sliding >= channel->tone_delta)) ) 
          {
            channel->note = channel->slide_to_note;
            channel->tone_slide_count = 0;
            channel->tone_sliding = 0;
          }
        }
      }
    }

    /* Calculate the amplitude */
    channel->amplitude = (b1 & 0x0f);

    /* Top bit of b0 indicates sliding */
    if((b0 & 0x80) != 0)
    {
      /* Next bit high means slide up */
      if((b0 & 0x40) != 0)
      {
        if(channel->amplitude_sliding < 15)
          channel->amplitude_sliding++;
      }
      else
      /* Next bit low means slide down */
      {
        if(channel->amplitude_sliding > -15)
          channel->amplitude_sliding--;
      }
    }
    channel->amplitude += channel->amplitude_sliding;
    if(channel->amplitude < 0)
      channel->amplitude = 0;
    else if(channel->amplitude > 15)
      channel->amplitude = 15;

    if(g_pt3[g_current_pt3].header.version < 4)
      channel->amplitude = PT3VolumeTable_33334[channel->volume][(uint8_t)channel->amplitude];
    else
      channel->amplitude = PT3VolumeTable_35[channel->volume][(uint8_t)channel->amplitude];

    /* Bottom bit of b0 indicates if sample has envelope */
    if(((b0 & 0x01) == 0) && channel->envelope_enabled)
      channel->amplitude |= 16;

    /* Envelope slide if b1 top bits are 10 or 11 */
    if((b1 & 0x80) != 0)
    {
      if((b0 & 0x20) != 0)
      {
        j = ((b0 >> 1) | 0xf0);
        j = (j << 24) >> 24;
        j += channel->envelope_sliding;
      }
      else
        j = ((b0 >> 1) & 0x0f) + channel->envelope_sliding;

      if((b1 & 0x20) != 0)
        channel->envelope_sliding = j;

      g_pt3[g_current_pt3].song.envelope_add += j;
    }
    /* Noise slide */
    else
    {
      g_pt3[g_current_pt3].song.noise_add = (b0 >> 1) + channel->noise_sliding;
      if((b1 & 0x20) != 0)
        channel->noise_sliding = g_pt3[g_current_pt3].song.noise_add;
    }

    g_pt3[g_current_pt3].song.mixer_value = ((b1 >> 1) & 0x48) | g_pt3[g_current_pt3].song.mixer_value;

    channel->sample_position++;
    if(channel->sample_position >= channel->sample_length)
      channel->sample_position = channel->sample_loop;

    channel->ornament_position++;
    if(channel->ornament_position >= channel->ornament_length)
      channel->ornament_position = channel->ornament_loop;
  }
  else
    channel->amplitude = 0x00;

  g_pt3[g_current_pt3].song.mixer_value = g_pt3[g_current_pt3].song.mixer_value >> 1;

  if(channel->onoff > 0)
  {
    channel->onoff--;
    if(channel->onoff == 0)
    {
      channel->enabled = !channel->enabled;
      if(channel->enabled) 
        channel->onoff = channel->onoff_delay;
      else
        channel->onoff = channel->offon_delay;
    }
  }
}

void create_frame(void)
{
  g_pt3[g_current_pt3].song.mixer_value = 0;
  g_pt3[g_current_pt3].song.envelope_add = 0;

  calculate_note(&g_pt3[g_current_pt3].song.a);
  calculate_note(&g_pt3[g_current_pt3].song.b);
  calculate_note(&g_pt3[g_current_pt3].song.c);

  /* Set Period for the 3 channels */
  g_pt3[g_current_pt3].registers[0] = g_pt3[g_current_pt3].song.a.tone & 0xff;
  g_pt3[g_current_pt3].registers[1] = (g_pt3[g_current_pt3].song.a.tone >> 8) & 0xff;
  g_pt3[g_current_pt3].registers[2] = g_pt3[g_current_pt3].song.b.tone & 0xff;
  g_pt3[g_current_pt3].registers[3] = (g_pt3[g_current_pt3].song.b.tone >> 8) & 0xff;
  g_pt3[g_current_pt3].registers[4] = g_pt3[g_current_pt3].song.c.tone & 0xff;
  g_pt3[g_current_pt3].registers[5] = (g_pt3[g_current_pt3].song.c.tone >> 8) & 0xff;

  /* Noise */
  g_pt3[g_current_pt3].registers[6] = (g_pt3[g_current_pt3].song.noise_period + g_pt3[g_current_pt3].song.noise_add) & 0x1f;

  /* Mixer value */
  g_pt3[g_current_pt3].registers[7] = g_pt3[g_current_pt3].song.mixer_value;

  /* Amplitude / Volume */
  g_pt3[g_current_pt3].registers[8] = g_pt3[g_current_pt3].song.a.amplitude;
  g_pt3[g_current_pt3].registers[9] = g_pt3[g_current_pt3].song.b.amplitude;
  g_pt3[g_current_pt3].registers[10] = g_pt3[g_current_pt3].song.c.amplitude;

  /* Envelope period */
  uint16_t temp_envelope = g_pt3[g_current_pt3].song.envelope_period + 
                           g_pt3[g_current_pt3].song.envelope_add + 
                           g_pt3[g_current_pt3].song.envelope_slide;

  g_pt3[g_current_pt3].registers[11] = (temp_envelope & 0xff);
  g_pt3[g_current_pt3].registers[12] = (temp_envelope >> 8);

  /* Envelope shape */
  if(g_pt3[g_current_pt3].song.envelope_type == g_pt3[g_current_pt3].song.envelope_type_old)
    g_pt3[g_current_pt3].registers[13] = 0xff;
  else
    g_pt3[g_current_pt3].registers[13] = g_pt3[g_current_pt3].song.envelope_type;
  g_pt3[g_current_pt3].song.envelope_type = g_pt3[g_current_pt3].song.envelope_type_old;

  /* Update envelope delay */
  if(g_pt3[g_current_pt3].song.envelope_delay > 0)
  {
    g_pt3[g_current_pt3].song.envelope_delay--;
    if(g_pt3[g_current_pt3].song.envelope_delay == 0)
    {
      g_pt3[g_current_pt3].song.envelope_delay = g_pt3[g_current_pt3].song.envelope_delay_orig;
      g_pt3[g_current_pt3].song.envelope_slide += g_pt3[g_current_pt3].song.envelope_slide_add;
    }
  }
}

void update_registers(void)
{
  if(!g_pt3[SUB_PT3].valid)
  {
    for(uint8_t i = 0; i < 7; i++)
    {
      g_roboplay_interface->psg_write(i, g_pt3[MAIN_PT3].registers[i]);
      g_roboplay_interface->psg2_write(i, g_pt3[MAIN_PT3].registers[i]);
    }

    g_roboplay_interface->psg_write(7, g_pt3[MAIN_PT3].registers[7]);
    g_roboplay_interface->psg2_write(7, g_pt3[MAIN_PT3].registers[7] & 0xf0);
    g_roboplay_interface->psg_write(8, g_pt3[MAIN_PT3].registers[8]);
    g_roboplay_interface->psg2_write(8, g_pt3[MAIN_PT3].registers[8]);
    g_roboplay_interface->psg_write(9, g_pt3[MAIN_PT3].registers[9] & 0xf0);
    g_roboplay_interface->psg2_write(9, g_pt3[MAIN_PT3].registers[9]);

    for(uint8_t i = 10; i < 13; i++)
    {
      g_roboplay_interface->psg_write(i, g_pt3[MAIN_PT3].registers[i]);
      g_roboplay_interface->psg2_write(i, g_pt3[MAIN_PT3].registers[i]);
    }

    if(g_pt3[MAIN_PT3].registers[13] != 0xff)
    {
      g_roboplay_interface->psg_write(13, g_pt3[MAIN_PT3].registers[13]);
      g_roboplay_interface->psg2_write(13, g_pt3[MAIN_PT3].registers[13]);
    }
  }
  else
  {
    for(uint8_t i = 0; i < NR_OF_PSG_REGISTERS - 1; i++)
    {
      g_roboplay_interface->psg_write(i, g_pt3[MAIN_PT3].registers[i]);
      g_roboplay_interface->psg2_write(i, g_pt3[SUB_PT3].registers[i]);
    }

    if(g_pt3[MAIN_PT3].registers[13] != 0xff)
    {
      g_roboplay_interface->psg_write(13, g_pt3[MAIN_PT3].registers[13]);
    }

    if(g_pt3[SUB_PT3].registers[13] != 0xff)
    {
      g_roboplay_interface->psg2_write(13, g_pt3[SUB_PT3].registers[13]);
    }
  }
}