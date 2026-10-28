/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * midi-pac.c
 */

#include "midi-pac.h"

#define DIRECT_MIDI 1
#define NORMAL_MODE 0

#define NR_OF_MIDI_CHANNELS 16

#define MIDI_PAC_DATA_OUT_WAIT                    \
  uint8_t handshake = midi_pac_readback & 0x02;   \
  midi_pac_register_data = data;                  \
  while((midi_pac_readback & 0x02) == handshake);

bool g_midi_pac_detected;

/*
 * OUT ports used for MIDI-PAC
*/
__sfr __at 0x7C midi_pac_readback;
__sfr __at 0x7C midi_pac_register_address;
__sfr __at 0x7D midi_pac_register_data;

bool midi_pac_detect(void)
{
  g_midi_pac_detected = false;

  uint8_t data = (midi_pac_readback & 0x04) ^ 0x04;
  midi_pac_register_address = 0xf5;
  g_midi_pac_detected = ((midi_pac_readback & 0x04) == data);

  data = (midi_pac_readback & 0x08) ^ 0x08;
  midi_pac_register_data = 0xff;
  g_midi_pac_detected = g_midi_pac_detected && ((midi_pac_readback & 0x08) == data);

  if(g_midi_pac_detected)
  {
    midi_pac_set_address(0xed);
    data = DIRECT_MIDI;
    MIDI_PAC_DATA_OUT_WAIT
  }

  return g_midi_pac_detected;
}

void midi_pac_reset(void)
{
  if(g_midi_pac_detected)
  {
    for(uint8_t i = 0; i < NR_OF_MIDI_CHANNELS; i++)
    {
      midi_pac_send_data_3(0xB0 + i, 123, 0);      /* All notes off */
      midi_pac_send_data_3(0xB0 + i, 120, 0);      /* All sounds off */
      midi_pac_send_data_3(0xB0 + i, 121, 0);      /* All controllers off */
    }
  }
}

void midi_pac_restore(void)
{
  if(g_midi_pac_detected)
  {
    midi_pac_set_address(0xed);
    uint8_t data = NORMAL_MODE;
    MIDI_PAC_DATA_OUT_WAIT
  }
}

void midi_pac_send_data(uint8_t data)
{
  if(g_midi_pac_detected)
  {
    while(midi_pac_register_address & 0x01);

    midi_pac_set_address(0xee);
    MIDI_PAC_DATA_OUT_WAIT
  }
}

void midi_pac_send_data_2(uint8_t data_1, uint8_t data_2)
{
  midi_pac_send_data(data_1);
  midi_pac_send_data(data_2);
}

void midi_pac_send_data_3(uint8_t data_1, uint8_t data_2, uint8_t data_3)
{
  midi_pac_send_data(data_1);
  midi_pac_send_data(data_2);
  midi_pac_send_data(data_3);
}

void midi_pac_set_address(uint8_t address)
{
  midi_pac_register_address = address;

  /* Delay needed for Turbo-R */
  for(uint8_t i = 0; i < 16; i++);
}

