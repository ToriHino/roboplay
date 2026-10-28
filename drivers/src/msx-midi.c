/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * msx-midi.c
 */

#include "msx-midi.h"

#define NR_OF_MIDI_CHANNELS 16

bool g_msx_midi_detected;
bool g_msx_midi_external;

/*
 * OUT ports used for MSX-MIDI
*/
__sfr __at 0xe8 msx_midi_data;
__sfr __at 0xe9 msx_midi_readback;
__sfr __at 0xe9 msx_midi_cmd;

__sfr __at 0xe0 msx_midi_external_data;
__sfr __at 0xe1 msx_midi_external_readback;
__sfr __at 0xe1 msx_midi_external_cmd;
__sfr __at 0xe2 msx_midi_external_ctrl;

bool msx_midi_detect(void)
{
  g_msx_midi_detected = false;
  g_msx_midi_external = false;

  msx_midi_external_ctrl = 0x01;
  msx_midi_external_cmd  = 0x01;
  if(msx_midi_external_readback == 0x05)
  {
    g_msx_midi_detected = true;
    g_msx_midi_external = true;
  }
  else
  {
    msx_midi_cmd = 0x01;

    if(msx_midi_readback == 0x05)
        g_msx_midi_detected = true;
  }

  return g_msx_midi_detected;
}

void msx_midi_reset(void)
{
  if(g_msx_midi_detected)
  {
    for(uint8_t i = 0; i < NR_OF_MIDI_CHANNELS; i++)
    {
      msx_midi_send_data_3(0xB0 + i, 123, 0);      /* All notes off */
      msx_midi_send_data_3(0xB0 + i, 120, 0);      /* All sounds off */
      msx_midi_send_data_3(0xB0 + i, 121, 0);      /* All controllers off */
    }
  }
}

void msx_midi_restore(void)
{
  if(g_msx_midi_detected)
  {
    if(g_msx_midi_external)
    {
      msx_midi_external_cmd = 0x00;
      msx_midi_external_ctrl = 0x81;
    }
    else
      msx_midi_cmd = 0x00;
  }
}

void msx_midi_send_data(uint8_t data)
{
  if(g_msx_midi_detected)
  {
    if(g_msx_midi_external)
    {
      while(!(msx_midi_external_readback & 0x01));
      msx_midi_external_data = data;
    }
    else
    {
      while(!(msx_midi_readback & 0x01));
      msx_midi_data = data;
    }
  }
}

void msx_midi_send_data_2(uint8_t data_1, uint8_t data_2)
{
  msx_midi_send_data(data_1);
  msx_midi_send_data(data_2);
}

void msx_midi_send_data_3(uint8_t data_1, uint8_t data_2, uint8_t data_3)
{
  msx_midi_send_data(data_1);
  msx_midi_send_data(data_2);
  msx_midi_send_data(data_3);
}
