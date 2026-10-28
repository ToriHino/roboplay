/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * vgm.c
 *
 * VGM: Video Game Music player
 */

#include <string.h>

#include "player_interface.h"
#include "vgm.h"

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

  g_device_type = VGM_DEVICE_ID_YM3526;
  g_clock = g_vgm_header.ym3526Clock;
  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_Y8950;
    g_clock = g_vgm_header.y8950Clock;
  }

  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_YM3812;
    g_clock = g_vgm_header.ym3812Clock;
    if(g_clock & VGM_DUAL_BIT) g_device_type = VGM_DEVICE_ID_YM3812_DUAL;
  }

  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_YMF262;
    g_clock = g_vgm_header.ymf262Clock;
  }

  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_YMF278B;
    g_clock = g_vgm_header.ymf278bClock;
  }

  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_YM2151;
    g_clock = g_vgm_header.ym2151Clock;
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
    if(g_clock && (g_vgm_header.ayType == 0x03)) g_device_type = VGM_DEVICE_ID_AY8930;
  }

  if(!g_clock)
  {
    g_device_type = VGM_DEVICE_ID_SAA1099;
    g_clock = g_vgm_header.saa1099Clock;
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
            case CMD_OPL:
            case CMD_OPL1:
            case CMD_OPL2:
            case CMD_OPL3_PORT0:
                reg = get_vgm_data();
                val = get_vgm_data();
                if(reg != 2 && reg != 3 && reg != 4)
                    g_roboplay_interface->opl4_write_fm_1(reg, val);
                break;
            case CMD_OPL2_2ND:
            case CMD_OPL3_PORT1:
                reg = get_vgm_data();
                val = get_vgm_data();
                if(reg != 2 && reg != 3 && reg != 4)
                    g_roboplay_interface->opl4_write_fm_2(reg, val);
                break;
            case CMD_OPL4:
                port = get_vgm_data();
                reg = get_vgm_data();
                val = get_vgm_data();
                if(reg != 2 && reg != 3 && reg != 4)
                {
                    (port == 0) ? g_roboplay_interface->opl4_write_fm_1(reg, val) : g_roboplay_interface->opl4_write_fm_2(reg, val);
                }
                break;
            case CMD_OPM:
                reg = get_vgm_data();
                val = get_vgm_data();
                if(reg != 0x14)
                    g_roboplay_interface->opm_write(reg, val);
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
            case CMD_SA1099:
                reg = get_vgm_data();
                val = get_vgm_data();
                g_roboplay_interface->soundstar_write(reg, val);
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

    /* Start with standard OPL2 mode */
    g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL2);

    if(g_device_type == VGM_DEVICE_ID_YM3812_DUAL || g_device_type == VGM_DEVICE_ID_YMF262)
        g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL3);
    else if(g_device_type == VGM_DEVICE_ID_YMF278B)
        g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL4);

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
    return "Video Game Music (VGM) player V1.0 by RoboSoft Inc.";
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
    "YM3526 OPL capture",
    "Y8950 MSX-AUDIO capture",
    "YM3812 OPL2 capture",
    "YM3812 OPL2 Dual capture",
    "YMF262 OPL3 capture",
    "YMF278B OPL4 capture",
    "YM2151 OPM capture",
    "K051649 SCC capture",
    "AY8910 PSG capture",
    "AY8930 EPSG capture",
    "SAA1099 capture"
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