/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * vgm_org.c
 *
 * VGM: Video Game Music player on oroginal device(s)
 */

#include <string.h>

#include "player_interface.h"
#include "vgm_org.h"

bool load(const char *file_name)
{  
  g_roboplay_interface->open(file_name, false);
  g_roboplay_interface->read(&g_vgm_header, sizeof(VGM_HEADER));

  if (strncmp(&g_vgm_header.vgmIdent, VGM_HEADER_ID, 4))
  {
    g_roboplay_interface->close();
    return false;
  }

  if(g_vgm_header.version < 0x151)
  {
    g_roboplay_interface->close();
    return false;
  }

  g_device_type = VGM_DEVICE_ID_YM2413;
  g_clock = g_vgm_header.ym2413Clock;
  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_Y8950;
    g_clock = g_vgm_header.y8950Clock;
  }


  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_K051649;
    g_clock = g_vgm_header.k051649Clock;
  }

  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_AY8910;
    g_clock = g_vgm_header.ay8910Clock;
  }

  if(!g_clock)
  {
    g_roboplay_interface->close();
    return false;
  }

  g_vgm_size = (g_vgm_header.gd3Offset > 0) ? g_vgm_header.gd3Offset - g_vgm_header.vgmDataOffset : g_vgm_header.eofOffset - g_vgm_header.vgmDataOffset;

  g_roboplay_interface->seek(g_vgm_header.vgmDataOffset - 0x34, ROBOPLAY_SEEK_START);

  g_segment_index = 0;
  g_segment_list[g_segment_index++] = START_SEGMENT_INDEX;

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
        g_segment_list[g_segment_index] = g_roboplay_interface->get_new_segment();
        g_roboplay_interface->set_segment(g_segment_list[g_segment_index++]);

        page_left = DATA_SEGMENT_SIZE;
        destination = (uint8_t*)DATA_SEGMENT_BASE;
      }
  } while(bytes_read);

  g_roboplay_interface->close();

  return true;
}

bool update(void)
{
    if(g_delay_counter >= VGM_STEP_VALUE)
    {
        g_delay_counter -= VGM_STEP_VALUE;
    }

    while(g_delay_counter < VGM_STEP_VALUE)
    {
        if(g_index_pointer >= g_vgm_size)
        {
            return false;
        }

        uint8_t cmd = get_vgm_data();
        uint8_t port;
        uint8_t reg;
        uint8_t val;
        switch(cmd)
        {
            case CMD_YM2413:
                reg = get_vgm_data();
                val = get_vgm_data();
                g_roboplay_interface->msx_music_write(reg, val);
                break;
            case CMD_Y8950:
                reg = get_vgm_data();
                val = get_vgm_data();
                g_roboplay_interface->msx_audio_write(reg, val);
                break;
            case CMD_SCC:
                port = get_vgm_data();
                reg = get_vgm_data();
                val = get_vgm_data();
                switch(port)
                {
                    case 0:
                        g_roboplay_interface->scc_write_register(SCC_WAVEFORM + reg, val);
                        break;
                    case 1:
                        g_roboplay_interface->scc_write_register(SCC_FREQUENCY + reg, val);
                        break;
                    case 2:
                        g_roboplay_interface->scc_write_register(SCC_VOLUME + reg, val);
                        break;
                    case 3:
                        g_roboplay_interface->scc_write_register(SCC_ON_OFF + reg, val);
                        break;
                }
                break;
            case CMD_PSG:
                reg = get_vgm_data();
                val = get_vgm_data();
                g_roboplay_interface->psg_write(reg, val);
                break;
            case CMD_WAIT:
                g_delay_counter = get_vgm_data();
                g_delay_counter |= get_vgm_data() << 8;
                break;
            case CMD_WAIT_735:
                g_delay_counter = 735;
                break;
            case CMD_WAIT_882:
                g_delay_counter = 882;
                break;
            case CMD_DATA_END:
                g_index_pointer = g_vgm_size;
                break;
            default:
                if(cmd >= CMD_WAIT_N && cmd <= CMD_WAIT_N + 0xF)
                {
                    g_delay_counter = (cmd & 0x0F) + 1;
                }
        }

        if(g_index_pointer >= g_vgm_size)
        {
          g_index_pointer = (g_vgm_header.loopOffset > 0) ? g_vgm_header.loopOffset : 0;

          g_song_data = (void*)DATA_SEGMENT_BASE;
          g_song_data += (g_index_pointer % DATA_SEGMENT_SIZE);
          g_segment_index = g_index_pointer / DATA_SEGMENT_SIZE;
          g_roboplay_interface->set_segment(g_segment_list[g_segment_index]);
        }
    }

    return true;
}

void rewind(const int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    g_segment_index = 0;
    g_roboplay_interface->set_segment(g_segment_list[g_segment_index]);
    g_song_data = (void*)DATA_SEGMENT_BASE;

    g_delay_counter = 0;
    g_index_pointer = 0;
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    return VGM_REPLAY_FREQ;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "Video Game Music (VGM) player on original device(s) V1.0 by RoboSoft Inc.";
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
  const char *deviceDescriptions[VGM_DEVICE_ID_CARD] =
  {
    "YM2413 OPLL capture",
    "Y8950 MSX-AUDIO capture",
    "K051649 SCC capture",
    "AY8910 PSG capture",
  };
 
  return (char *)deviceDescriptions[g_device_type];
}

uint8_t get_vgm_data(void)
{
    uint8_t data = *g_song_data++;
    if(g_song_data == (void*)(DATA_SEGMENT_BASE + DATA_SEGMENT_SIZE))
    {
       g_song_data = (void*)DATA_SEGMENT_BASE;
       g_roboplay_interface->set_segment(g_segment_list[++g_segment_index]);
    }

    g_index_pointer++;

    return data;
}