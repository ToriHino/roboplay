/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2025
 *
 * d00.c
 *
 * D00: EdLib packed module player
 */

#include <string.h>

#include "player_interface.h"
#include "d00.h"

static inline uint16_t LE_WORD(const uint16_t *val)
{
  const uint8_t *b = (const uint8_t *)val;
  return (b[1] << 8) + b[0];
}

bool load(const char *file_name)
{  
  g_song_data = (uint8_t *)DATA_SEGMENT_BASE;

  g_roboplay_interface->open(file_name, false);
  uint16_t bytes_read = g_roboplay_interface->read(g_song_data, DATA_SEGMENT_SIZE - 1);
  g_song_data[bytes_read] = 0x00;   
  g_roboplay_interface->close();

  g_header    = (D00_HEADER *)g_song_data;
  g_header_v1 = (D00_HEADER_V1 *)g_song_data;

  bool version_1 = false;

  /* Check for version 2-4 header */
  if(strncmp(g_header->id, "JCH\x26\x02\x66",6) || g_header->type || !g_header->subsongs || g_header->soundcard)
  {
    /* Check for version 0 or 1 header */
    if(g_header_v1->version > 1 || !g_header_v1->subsongs) return false;
    version_1 = true;
  }

  if(version_1)
  {
    /* Version 1 */
    g_version = g_header_v1->version;
    g_song_description = (uint8_t *)(g_song_data + LE_WORD(&g_header_v1->song_description));
    g_inst = (INSTRUMENT_DATA *)(g_song_data + LE_WORD(&g_header_v1->instrument_data));
    g_sequence_data = (uint16_t *)(g_song_data + LE_WORD(&g_header_v1->sequence_data));       
  }
  else
  {
    /* Version 2 and above */
    g_version = g_header->version;
    g_song_description = (uint8_t *)(g_song_data + LE_WORD(&g_header->song_description));
    g_inst = (INSTRUMENT_DATA *)(g_song_data + LE_WORD(&g_header->instrument_data));
    g_sequence_data = (uint16_t *)(g_song_data + LE_WORD(&g_header->sequence_data));

    for(int i = 31; i >= 0; i--)        
    {
        if(g_header->songname[i] == ' ') g_header->songname[i] = '\0';
        if(g_header->author[i] == ' ') g_header->author[i] = '\0';
    }
  }

  switch(g_version)
  {
    case 0:
      g_level_pulse = 0;
      g_spfx = 0;
      /* V0 files default to 70Hz */
      g_header_v1->speed = 70;
      break;
    case 1:
      g_level_pulse = (LEVEL_PULS_DATA *)(g_song_data + LE_WORD(&g_header_v1->lpulptr));
      g_spfx = 0;
      break;
    case 2:
      g_level_pulse = (LEVEL_PULS_DATA *)(g_song_data + LE_WORD(&g_header->spfx_data));
      g_spfx = 0;
      break;
    case 3:
      g_spfx = 0;
      g_level_pulse = 0;
      break;
    case 4:
      g_spfx = (SPFX_DATA *)(g_song_data + LE_WORD(&g_header->spfx_data));
      g_level_pulse = 0;
      break;
  }

  uint8_t *str;
  if((str = strstr(g_song_description, "\xff\xff")))
  {
    while((*str == 0xff || *str == ' ') && str >= g_song_description)
    {
      *str = '\0'; str--;
    }
  }

  return true;
}

bool update(void)
{
  /* First handle FX updates */
  update_fx();

  /* Continue song */
  for(uint8_t c = 0; c < NR_OF_CHANNELS; c++)
  {
    if((g_version < 3 && g_channel_info[c].del) || (g_version >= 3 && g_channel_info[c].del <= 0x7f))
    {
      /* V4: Hard restart SR */
      if(g_version == 4)  
      {
        if(g_channel_info[c].del == g_inst[g_channel_info[c].inst].timer)
        {
          if(g_channel_info[c].nextnote)
              g_roboplay_interface->opl4_write_fm_1(0x83 + op_table[c], g_inst[g_channel_info[c].inst].sr);
        }
      }
      
      if(g_version < 3)
        g_channel_info[c].del--;
      else if(g_channel_info[c].speed)
        g_channel_info[c].del += g_channel_info[c].speed;
      else 
      {
        g_channel_info[c].sequence_end = true;
        continue;
      }
    }
    else 
    {
      if(g_channel_info[c].speed) 
      {
        if(g_version < 3)
          g_channel_info[c].del = g_channel_info[c].speed;
        else 
        {
          g_channel_info[c].del &= 0x7f;
          g_channel_info[c].del += g_channel_info[c].speed;
        }
      }
      else 
      {
        g_channel_info[c].sequence_end = true;
        continue;
      }
      if(g_channel_info[c].rhcount) 
      {
        /* process pending REST/HOLD events */
        g_channel_info[c].rhcount--;
        continue;
      }

      bool process_order = true;
      while(process_order)
      {
        uint16_t order = LE_WORD(&g_channel_info[c].order_list[g_channel_info[c].order_pos]);

        if(order == 0xfffe)
        {
          g_channel_info[c].sequence_end = true;
          process_order = false;
        }
        else if(order == 0xffff)
        {
          /* Jump to order */
          g_channel_info[c].order_pos = LE_WORD(&g_channel_info[c].order_list[g_channel_info[c].order_pos + 1]);
          g_channel_info[c].sequence_end = true;
        }
        else
        {
          uint8_t order_hi = order >> 8;
          if(order_hi >= 0x90)
          {
            /* Set speed */
            g_channel_info[c].speed = order & 0xff;
            order = LE_WORD(&g_channel_info[c].order_list[g_channel_info[c].order_pos - 1]);
            g_channel_info[c].order_pos++;
          }
          else if(order_hi >= 0x80)
          {
            /* Transpose track */
            g_channel_info[c].transpose = order & 0xff;
            if((order & 0x0100) != 0) g_channel_info[c].transpose = -g_channel_info[c].transpose;
            order = LE_WORD(&g_channel_info[c].order_list[++g_channel_info[c].order_pos]);
          }

          uint16_t *pattern = (uint16_t *)(g_song_data + LE_WORD(&g_sequence_data[order]));
          g_channel_info[c].fxflag = 0;

          bool process_pattern = true;
          while(process_pattern)
          {
            /* V0: Always initialize rhcount */
            if(!g_version) g_channel_info[c].rhcount = g_channel_info[c].irhcount;

            uint16_t pattern_pos = LE_WORD(&pattern[g_channel_info[c].pattern_pos]);
            if(pattern_pos == 0xffff)
            {
              /* Pattern ended */
              g_channel_info[c].pattern_pos = 0;
              g_channel_info[c].order_pos++;
              process_pattern = false;
              process_order = true;
            }
            else
            {
              uint8_t  count  = HIBYTE(pattern_pos);
              uint8_t  note = LOBYTE(pattern_pos);
              uint8_t  fx   = pattern_pos >> 12;
              uint16_t fxop = pattern_pos & 0x0fff;

              g_channel_info[c].pattern_pos++; 
              pattern_pos = LE_WORD(&pattern[g_channel_info[c].pattern_pos]);
              
              g_channel_info[c].nextnote = LOBYTE(pattern_pos) & 0x7f;

              if(g_version ? count < 0x40 : !fx)
              {
                handle_note_event(c, note, count);
                process_pattern = false;
                process_order = false;
              }
              else if(handle_fx_event(c, fx, fxop))
              {
                process_pattern = false;
                process_order = false;
              }
              else
              {
                /* Event is incomplete, note follows */
                process_pattern = true;
              }
            }
          }
        }
      }
    }
  }

  uint8_t trackend = 0;
  for(uint8_t channel = 0; channel < NR_OF_CHANNELS; channel++)
  {
    if(g_channel_info[channel].sequence_end)
      trackend++;
  }
  if(trackend == NR_OF_CHANNELS) g_song_end = true;

  return !g_song_end;
}

void rewind(const int8_t subsong)
{
  /* Set to standard OPL2 mode */
  g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL2);

  g_song_end = false;
  g_current_subsong = subsong;

  memset(g_channel_info, 0, sizeof(g_channel_info));

  ARRANGEMENT *arrangement_data;
  if(g_version > 1)
    arrangement_data = (ARRANGEMENT *)(g_song_data + LE_WORD(&g_header->arrangement_data));
  else
    arrangement_data = (ARRANGEMENT *)(g_song_data + LE_WORD(&g_header_v1->arrangement_data));

  for(uint8_t i = 0; i< NR_OF_CHANNELS; i++) 
  {
    if(LE_WORD(&arrangement_data[g_current_subsong].ptr[i]))
    {
      /* Track enabled */
      g_channel_info[i].speed = LE_WORD((uint16_t *)(g_song_data + LE_WORD(&arrangement_data[g_current_subsong].ptr[i])));
      g_channel_info[i].order_list = (uint16_t *)(g_song_data + LE_WORD(&arrangement_data[g_current_subsong].ptr[i]) + 2);
    } 
    else 
    {                    
      /* Track disabled */
      g_channel_info[i].speed = 0;
      g_channel_info[i].order_list = 0;
    }

    g_channel_info[i].ispfx = NO_SPFX; 
    g_channel_info[i].spfx = NO_SPFX;
    
    g_channel_info[i].ilevpuls = NO_LEVEL_PULSE;
    g_channel_info[i].levpuls = NO_LEVEL_PULSE;
    
    g_channel_info[i].cvol = arrangement_data[g_current_subsong].volume[i] & 0x7F;
    g_channel_info[i].vol = g_channel_info[i].cvol;
  }   
}

void command(const uint8_t id)
{
  /* No additional commmands supported */
  id;
}

float get_refresh(void)
{
  if(g_version > 1)
    return g_header->speed;
  else
    return g_header_v1->speed;
}

uint8_t get_subsongs(void)
{
  if(g_version > 1)
    return g_header->subsongs;
  else
    return g_header_v1->subsongs;
}

char* get_player_info(void)
{
  return "EdLib packed module player V1.0 by RoboSoft Inc.";
}

char* get_title(void)
{
  if(g_version > 1)
    return g_header->songname;
  else
    return "-";
}

char* get_author(void)
{
  if(g_version > 1)
    return g_header->author;
  else
    return "-";
}

char* get_description(void)
{
  if(*g_song_description)
    return g_song_description;
  else
    return "-";
}

void update_fx(void)
{  
  /* Effect handling (timer dependent) */
  for(uint8_t channel = 0; channel < NR_OF_CHANNELS; channel++) 
  {
    g_channel_info[channel].slideval += g_channel_info[channel].slide; 
    set_frequency(channel);
    vibrato(channel);

    if(g_channel_info[channel].spfx != 0xffff) 
    {
      if(g_channel_info[channel].fxdel)
        g_channel_info[channel].fxdel--;
      else 
      {
        g_channel_info[channel].spfx = LE_WORD(&g_spfx[g_channel_info[channel].spfx].ptr);
        g_channel_info[channel].fxdel = g_spfx[g_channel_info[channel].spfx].duration;
        g_channel_info[channel].inst = LE_WORD(&g_spfx[g_channel_info[channel].spfx].instnr) & 0xfff;
        
        if(g_spfx[g_channel_info[channel].spfx].modlev != 0xff)
          g_channel_info[channel].modvol = g_spfx[g_channel_info[channel].spfx].modlev;
        
        set_instrument(channel);
        
        uint8_t note;
        uint16_t instnr = LE_WORD(&g_spfx[g_channel_info[channel].spfx].instnr);
        if(instnr > 0x7fffU)
          note = g_spfx[g_channel_info[channel].spfx].halfnote;
        else
          note = g_spfx[g_channel_info[channel].spfx].halfnote + g_channel_info[channel].note;
        g_channel_info[channel].freq = note_table[note%12] + ((note/12) << 10);

        set_frequency(channel);
      }
      g_channel_info[channel].modvol += g_spfx[g_channel_info[channel].spfx].modlevadd; 
      g_channel_info[channel].modvol &= 63;
      set_modvolume(channel);
    }

    /* Level pulse */
    if(g_channel_info[channel].levpuls != 0xff) 
    {
      if(g_channel_info[channel].frameskip)
        g_channel_info[channel].frameskip--;
      else 
      {
        g_channel_info[channel].frameskip = g_inst[g_channel_info[channel].inst].timer;
        if(g_channel_info[channel].fxdel)
          g_channel_info[channel].fxdel--;
        else 
        {
          g_channel_info[channel].levpuls = g_level_pulse[g_channel_info[channel].levpuls].ptr - 1;
          g_channel_info[channel].fxdel = g_level_pulse[g_channel_info[channel].levpuls].duration;
          if(g_level_pulse[g_channel_info[channel].levpuls].level != 0xff)
            g_channel_info[channel].modvol = g_level_pulse[g_channel_info[channel].levpuls].level;
        }
        g_channel_info[channel].modvol += g_level_pulse[g_channel_info[channel].levpuls].voladd; g_channel_info[channel].modvol &= 63;
        set_modvolume(channel);
      }
    }
  }
}

void handle_note_event(const uint8_t channel, uint8_t note, uint8_t count)
{
  switch(note) 
  {
    case 0:
      /* REST event */
    
    case 0x80:
      if(!note || g_version) 
      {
        g_channel_info[channel].key = false;
        set_frequency(channel);
      }
    
    case 0x7e:
      /* HOLD event */
      if(g_version) g_channel_info[channel].rhcount = count;
      g_channel_info[channel].nextnote = 0;
      break;

    default:
      /* Restart FX */
      if(!(g_channel_info[channel].fxflag & 1)) g_channel_info[channel].vibdepth = 0;
      if(!(g_channel_info[channel].fxflag & 2)) g_channel_info[channel].slideval = g_channel_info[channel].slide = 0;

      if(g_version)
      {
        /* Note handling for v1 and above */
        if(note > 0x80)
        {
          /* Locked note (no channel transpose) */
          note -= 0x80;
        }
        else
        {
          /* Unlocked note */
          note += g_channel_info[channel].transpose;
        }

        /* Remember note for SpFX */
        g_channel_info[channel].note = note;

        if(g_channel_info[channel].ispfx != NO_SPFX && count < 0x20) 
        {  
          /* Reset SpFX */
          g_channel_info[channel].spfx = g_channel_info[channel].ispfx;
          uint16_t instnr = LE_WORD(&g_spfx[g_channel_info[channel].spfx].instnr);
          if(instnr > 0x7fffU)
          {
            /* Locked frequency */
            note = g_spfx[g_channel_info[channel].spfx].halfnote;
          }
          else
          {
            /* Unlocked frequency */
            note += g_spfx[g_channel_info[channel].spfx].halfnote;
          }
          
          g_channel_info[channel].inst = LE_WORD(&g_spfx[g_channel_info[channel].spfx].instnr) & 0xfff;
          g_channel_info[channel].fxdel = g_spfx[g_channel_info[channel].spfx].duration;
          
          if(g_spfx[g_channel_info[channel].spfx].modlev != 0xff)
            g_channel_info[channel].modvol = g_spfx[g_channel_info[channel].spfx].modlev;
          else
            g_channel_info[channel].modvol = g_inst[g_channel_info[channel].inst].data[7] & 63;
        }

        if(g_channel_info[channel].ilevpuls != NO_LEVEL_PULSE && count < 0x20) 
        { 
          /* Reset LevelPuls */
          g_channel_info[channel].levpuls = g_channel_info[channel].ilevpuls;
          g_channel_info[channel].fxdel = g_level_pulse[g_channel_info[channel].levpuls].duration;
          g_channel_info[channel].frameskip = g_inst[g_channel_info[channel].inst].timer;
          
          if(g_level_pulse[g_channel_info[channel].levpuls].level != 0xff)
            g_channel_info[channel].modvol = g_level_pulse[g_channel_info[channel].levpuls].level;
          else
            g_channel_info[channel].modvol = g_inst[g_channel_info[channel].inst].data[7] & 63;
        }

        g_channel_info[channel].freq = note_table[note % 12] + ((note / 12) << 10);
        if(count < 0x20)
          play_note(channel);
        else 
        {
          set_frequency(channel);
          count -= 0x20;
        }
        g_channel_info[channel].rhcount = count;
    } 
      else 
      {  
        /* Note handling for v0 */
        if(count < 2) note += g_channel_info[channel].transpose;
        g_channel_info[channel].note = note;

        g_channel_info[channel].freq = note_table[note%12] + ((note/12) << 10);
        if(count == 1)
          set_frequency(channel);
        else
          play_note(channel);
      }
      break;
  }
}

bool handle_fx_event(const uint8_t channel, uint8_t fx, uint16_t fxop)
{
  uint16_t buf;

  switch(fx) 
  {
    case 6:
      /* Cut/Stop Voice */
      buf = g_channel_info[channel].inst;
      g_channel_info[channel].inst = 0;
      play_note(channel);
      g_channel_info[channel].inst = buf;
      g_channel_info[channel].rhcount = fxop;

        /* No note follows this event */
      return true;

    case 7:
      /* Vibrato */
      g_channel_info[channel].vibspeed = fxop & 0xff;
      g_channel_info[channel].vibdepth = fxop >> 8;
      g_channel_info[channel].trigger = fxop >> 9;
      g_channel_info[channel].fxflag |= 1;
      break;

    case 8:
      /* V0: Duration */
      if(!g_version)
          g_channel_info[channel].irhcount = fxop;
      break;

    case 9:
      /* New volume level */
      g_channel_info[channel].vol = fxop & 63;

      /* Apply channel volume */
      if(g_channel_info[channel].vol + g_channel_info[channel].cvol < 63)
          g_channel_info[channel].vol += g_channel_info[channel].cvol;
      else
          g_channel_info[channel].vol = 63;
      set_volume(channel);
      break;

    case 0xB:
      /* V4: Set SpFX */
      if(g_version == 4)
          g_channel_info[channel].ispfx = fxop;
      break;

    case 0xC:
      /* Set Instrument */
      g_channel_info[channel].ispfx = NO_SPFX;
      g_channel_info[channel].spfx = NO_SPFX;
      g_channel_info[channel].inst = fxop;
      g_channel_info[channel].modvol = g_inst[fxop].data[7] & 63;

      /* Set LevelPuls */
      if(g_version < 3 && g_version && g_inst[fxop].tunelev)
          g_channel_info[channel].ilevpuls = g_inst[fxop].tunelev - 1;
      else 
      {
          g_channel_info[channel].ilevpuls = NO_LEVEL_PULSE;
          g_channel_info[channel].levpuls = NO_LEVEL_PULSE;
      }
      break;

    case 0xD:
      /* Slide up */
      g_channel_info[channel].slide = fxop;
      g_channel_info[channel].fxflag |= 2;
      break;

    case 0xE:
      /* Slide down */
      g_channel_info[channel].slide = -fxop;
      g_channel_info[channel].fxflag |= 2;
      break;
  }

  return false;
}

void set_modvolume(const uint8_t channel)
{
  uint8_t  op = op_table[channel];
  uint16_t inst_nr = g_channel_info[channel].inst;

  if(g_inst[inst_nr].data[10] & 1)
    g_roboplay_interface->opl4_write_fm_1(0x40 + op, (uint8_t)(63-((63-g_channel_info[channel].modvol)/63.0)*(63-g_channel_info[channel].vol)) + (g_inst[inst_nr].data[7] & 192));
  else
    g_roboplay_interface->opl4_write_fm_1(0x40 + op, g_channel_info[channel].modvol + (g_inst[inst_nr].data[7] & 192));
}

void set_volume(const uint8_t channel)
{
  uint8_t  op = op_table[channel];
  uint16_t inst_nr = g_channel_info[channel].inst;

  g_roboplay_interface->opl4_write_fm_1(0x43 + op, (uint8_t)(63-((63-(g_inst[inst_nr].data[2] & 63))/63.0)*(63-g_channel_info[channel].vol)) + (g_inst[inst_nr].data[2] & 192));

  if(g_inst[inst_nr].data[10] & 1)
    g_roboplay_interface->opl4_write_fm_1(0x40 + op, (uint8_t)(63-((63-g_channel_info[channel].modvol)/63.0)*(63-g_channel_info[channel].vol)) + (g_inst[inst_nr].data[7] & 192));
  else
    g_roboplay_interface->opl4_write_fm_1(0x40 + op, g_channel_info[channel].modvol + (g_inst[inst_nr].data[7] & 192));
}

void set_frequency(const uint8_t channel)
{
  uint16_t freq = g_channel_info[channel].freq;

  /* V4: Apply instrument finetune */
  if(g_version == 4)  
    freq += g_inst[g_channel_info[channel].inst].tunelev;

  freq += g_channel_info[channel].slideval;
  g_roboplay_interface->opl4_write_fm_1(0xA0 + channel, freq & 0xFF);

  if(g_channel_info[channel].key)
    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, ((freq >> 8) & 0x1F) | 0x20);
  else
    g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, (freq >> 8) & 0x1F);
}

void set_instrument(const uint8_t channel)
{
  uint8_t  op = op_table[channel];
  uint16_t inst_nr = g_channel_info[channel].inst;

  /* Set instrument data */
  g_roboplay_interface->opl4_write_fm_1(0x63 + op, g_inst[inst_nr].data[0]);
  g_roboplay_interface->opl4_write_fm_1(0x83 + op, g_inst[inst_nr].data[1]);
  g_roboplay_interface->opl4_write_fm_1(0x23 + op, g_inst[inst_nr].data[3]);
  g_roboplay_interface->opl4_write_fm_1(0xE3 + op, g_inst[inst_nr].data[4]);
  g_roboplay_interface->opl4_write_fm_1(0x60 + op, g_inst[inst_nr].data[5]);
  g_roboplay_interface->opl4_write_fm_1(0x80 + op, g_inst[inst_nr].data[6]);
  g_roboplay_interface->opl4_write_fm_1(0x20 + op, g_inst[inst_nr].data[8]);
  g_roboplay_interface->opl4_write_fm_1(0xE0 + op, g_inst[inst_nr].data[9]);
  
  if(g_version)
    g_roboplay_interface->opl4_write_fm_1(0xC0 + channel, g_inst[inst_nr].data[10]);
  else
    g_roboplay_interface->opl4_write_fm_1(0xC0 + channel, (g_inst[inst_nr].data[10] << 1) + (g_inst[inst_nr].tunelev & 1));
}

void play_note(const uint8_t channel)
{
  /* Stop old note */
  g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, 0);

  set_instrument(channel);
  g_channel_info[channel].key = true;
  set_frequency(channel);
  set_volume(channel);
}

void vibrato(const uint8_t channel)
{
  if(!g_channel_info[channel].vibdepth)
    return;

  if(g_channel_info[channel].trigger)
    g_channel_info[channel].trigger--;
  else 
  {
    g_channel_info[channel].trigger = g_channel_info[channel].vibdepth;
    g_channel_info[channel].vibspeed = -g_channel_info[channel].vibspeed;
  }
  
  g_channel_info[channel].freq += g_channel_info[channel].vibspeed;
  set_frequency(channel);
}
