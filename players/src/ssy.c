/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * ssy.c
 *
 * SSY: PETSCII Robots Adlib player
 */

#include <string.h>

#include "player_interface.h"
#include "ssy.h"

bool load(char *const file_name)
{  
  g_roboplay_interface->open(file_name, false);

  uint8_t segment_index = 0;
  g_segment_list[segment_index] = START_SEGMENT_INDEX;

  uint16_t page_left = DATA_SEGMENT_SIZE;
  uint8_t* destination = (uint8_t*)DATA_SEGMENT_BASE;

  uint16_t bytes_read = 0;
  do
  {
    /* It's not possible to read directly to non-primary mapper memory segments,
        so use a buffer inbetween. */
    bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, READ_BUFFER_SIZE);
    memcpy(destination, (void*)READ_BUFFER, bytes_read);

    destination += bytes_read;
    page_left -= bytes_read;
    if(page_left == 0)
    {
      segment_index++;
      g_segment_list[segment_index] = g_roboplay_interface->get_new_segment();
      g_roboplay_interface->set_segment(g_segment_list[segment_index]);

      page_left = DATA_SEGMENT_SIZE;
      destination = (uint8_t*)DATA_SEGMENT_BASE;
    }
  } while(bytes_read > 0);

  g_roboplay_interface->close();

  return true;
}

bool update(void)
{
  for(uint8_t channel = 1; channel < g_nr_of_channels; channel++)
  {
    if(g_wait[channel] != 0)
    {
        g_wait[channel]--;
        if(g_wait[channel] != 0) continue;
    }

    if(g_ptr[channel].ptr != 0)
    {
       g_roboplay_interface->set_segment(g_segment_list[g_ptr[channel].segment]);

       while(true)
       {
         uint8_t data = *(g_ptr[channel].ptr);
        increment_ptr(channel);

        if(data < 0xC0)
        {
          g_wait[channel] = data;
          break;
        }
        else if(data < 0xD0)
        {
          /* SPKR volume */
        }
        else if(data == 0xD0)
        {
          g_pitch_1[channel] &= 0xFF00;
          g_pitch_1[channel] |= *(g_ptr[channel].ptr);
          increment_ptr(channel);
          break;
        }
        else if(data == 0xD1)
        {
          g_pitch_1[channel] &= 0x00FF;
          g_temp = *(g_ptr[channel].ptr);
          g_temp <<= 8;
          g_pitch_1[channel] |= g_temp;
          increment_ptr(channel);
          break;
        }
        else if(data == 0xD2)
        {
          g_pitch_1[channel] = *(g_ptr[channel].ptr);
          increment_ptr(channel);
          g_temp = *(g_ptr[channel].ptr);
          g_temp <<= 8;
          g_pitch_1[channel] += g_temp;
          increment_ptr(channel);
          break;
        }
        else if(data == 0xD3)
        {
          data = *(g_ptr[channel].ptr);
          increment_ptr(channel);
          g_ret[channel] = g_ptr[channel];
          g_ptr[channel].ptr -= (2 + data);
          if(g_ptr[channel].ptr < (uint8_t *)DATA_SEGMENT_BASE)
          {
            g_ptr[channel].ptr += DATA_SEGMENT_SIZE;
            g_ptr[channel].segment--;
            g_roboplay_interface->set_segment(g_segment_list[g_ptr[channel].segment]); 
          }
          g_refprev[channel] = g_ptr[channel];
        }
        else if(data == 0xD4)
        {
          g_pitch_2[channel] &= 0xFF00;
          g_pitch_2[channel] |= *(g_ptr[channel].ptr);
          g_pitch_1[channel] = ~g_pitch_2[channel];
          increment_ptr(channel);
          break;
        }
        else if(data == 0xD5)
        {
          g_pitch_2[channel] &= 0x00FF;
          g_temp = *(g_ptr[channel].ptr);
          g_temp <<= 8;
          g_pitch_2[channel] |= g_temp;
          g_pitch_1[channel] = ~g_pitch_2[channel];
          increment_ptr(channel);
          break;
        }
        else if(data == 0xD6)
        {
          g_pitch_2[channel] = *(g_ptr[channel].ptr);
          increment_ptr(channel);
          g_temp = *(g_ptr[channel].ptr);
          g_temp <<= 8;
          g_pitch_2[channel] += g_temp;
          g_pitch_1[channel] = ~g_pitch_2[channel];
          increment_ptr(channel);
          break;
        }
        else if(data == 0xD7)
        {
          g_ret[channel] = g_ptr[channel];
          g_ptr[channel] = g_refprev[channel];
          g_roboplay_interface->set_segment(g_segment_list[g_ptr[channel].segment]);
        }
        else if(data == 0xD8)
        {
          /* SSY_TANDY_MODE */
          increment_ptr(channel);
        }
        else if(data < 0xE3)
        {
          g_opl_regs[channel][data - 0xD9] = *(g_ptr[channel].ptr);
          increment_ptr(channel);
        }
        else if(data == 0xE3)
        {
          g_volopl[channel] = *(g_ptr[channel].ptr);
          increment_ptr(channel);
        }
        else if(data == 0xE4)
        {
          g_keyoff[channel] = data;
        }
        else if(data == 0xFC)
        {
          if(g_ret[channel].ptr != 0)
          {
            g_ptr[channel] = g_ret[channel];
            g_roboplay_interface->set_segment(g_segment_list[g_ptr[channel].segment]);
            g_ret[channel].ptr = 0;
          }
        }
        else if(data == 0xFD)
        {
            uint8_t temp = *(g_ptr[channel].ptr);
            increment_ptr(channel);
            g_temp = *(g_ptr[channel].ptr);
            g_temp <<= 8;
            g_temp += temp;

            increment_ptr(channel);
            g_ret[channel] = g_ptr[channel];

            g_ptr[channel].segment = 0;
            while(g_temp >= DATA_SEGMENT_SIZE)
            {
              g_temp -= DATA_SEGMENT_SIZE;
              g_ptr[channel].segment++;
            }
            g_roboplay_interface->set_segment(g_segment_list[g_ptr[channel].segment]);

            g_ptr[channel].ptr = (uint8_t *)(DATA_SEGMENT_BASE + g_temp);
            g_refprev[channel] = g_ptr[channel];
        }
        else if(data == 0xFE)
        {
          g_loop[channel] = g_ptr[channel];
        }
        else if(data == 0xFF)
        {
          g_ptr[channel] = g_loop[channel];
          g_roboplay_interface->set_segment(g_segment_list[g_ptr[channel].segment]);
        }
      }
    }
  }

  for(uint8_t channel = 1; channel < MAX_NR_OF_CHANNELS; channel++)
  {
    if(g_keyoff[channel] != 0)
    {
      g_keyoff[channel] = 0;
      g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, 0);
      g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, 0);
    }

    if(g_volopl[channel] != g_volprev[channel])
    {
      g_volprev[channel] = g_volopl[channel];
      g_roboplay_interface->opl4_write_fm_1(0x43 + SSY_OPL2_OPERATOR_ORDER[channel], g_volopl[channel]);
      g_roboplay_interface->opl4_write_fm_2(0x43 + SSY_OPL2_OPERATOR_ORDER[channel], g_volopl[channel]);
    }

    for(uint8_t i = 0; i < NR_OF_REGS; i++)
    {
      if(g_opl_regs[channel][i] != g_opl_prev[channel][i])
      {
        g_opl_prev[channel][i] = g_opl_regs[channel][i];
        uint8_t reg = SSY_OPL2_INSTRUMENT_REGS[channel][i];
        uint8_t val = (reg >= 0xC0 && reg <= 0xC8) ? g_opl_regs[channel][i] | PAN_SETTING_LEFT : g_opl_regs[channel][i];
        g_roboplay_interface->opl4_write_fm_1(reg, val);
        val = (reg >= 0xC0 && reg <= 0xC8) ? g_opl_regs[channel][i] | PAN_SETTING_RIGHT : g_opl_regs[channel][i];
        g_roboplay_interface->opl4_write_fm_2(reg, val);
      }
    }

    if(g_pitch_2[channel] != g_pitch_1[channel])
    {
      g_pitch_1[channel] = g_pitch_2[channel];
      g_roboplay_interface->opl4_write_fm_1(0xA0 + channel, g_pitch_2[channel]);
      g_roboplay_interface->opl4_write_fm_1(0xB0 + channel, g_pitch_2[channel] >> 8);
      g_roboplay_interface->opl4_write_fm_2(0xA0 + channel, g_pitch_2[channel] + 1);
      g_roboplay_interface->opl4_write_fm_2(0xB0 + channel, g_pitch_2[channel] >> 8);
    }
  }

  return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    for(uint8_t channel = 0; channel < MAX_NR_OF_CHANNELS; channel++)
    {
      g_wait[channel] = 0;
      g_ret[channel].ptr = 0;
      g_ptr[channel].ptr = 0;
      g_loop[channel].ptr = 0;
      g_pitch_1[channel] = 0;
      g_pitch_2[channel] = 0;
      g_volprev[channel] = 0;
      g_refprev[channel].ptr = 0;
      g_keyoff[channel] = 0;
      g_volopl[channel] = 0;
      for(uint8_t i = 0; i < NR_OF_REGS; i++)
      {
        g_opl_regs[channel][i] = 0;
        g_opl_prev[channel][i] = 255;
      }
    }

    g_roboplay_interface->set_segment(g_segment_list[0]);
    uint8_t* song_data = (uint8_t *)(DATA_SEGMENT_BASE);
    g_nr_of_channels = *(song_data + 1) + 1;

    for(uint8_t channel = 1; channel < MAX_NR_OF_CHANNELS; channel++)
    {
      if(channel < g_nr_of_channels)
      {
        uint16_t ptr = (( *(song_data + (channel << 1) + 1) << 8 ) + *(song_data + (channel << 1)) );

        g_ptr[channel].ptr     = song_data + (ptr % DATA_SEGMENT_SIZE);       
        g_ptr[channel].segment = (ptr / DATA_SEGMENT_SIZE);

        g_loop[channel] = g_ptr[channel];
        g_volprev[channel] = 0xff;
      }
      else g_ptr[channel].ptr = NULL;
    }
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    /* Fixed replay rate of 72.8Hz */
    return 72.8;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "PETSCII Robots Adlib player V1.0 by RoboSoft Inc.";
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

void increment_ptr(uint8_t channel)
{
  g_ptr[channel].ptr++;
  if(g_ptr[channel].ptr >= (uint8_t *)(DATA_SEGMENT_BASE + DATA_SEGMENT_SIZE))
  {
    g_ptr[channel].ptr -= DATA_SEGMENT_SIZE;
    g_ptr[channel].segment++;
    g_roboplay_interface->set_segment(g_segment_list[g_ptr[channel].segment]);
  }
}
