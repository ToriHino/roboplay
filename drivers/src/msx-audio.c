/*
 * RoboPlay for MSX
 * Copyright (C) 2026 by RoboSoft Inc.
 *
 * msx-audio.c
 */

#include "msx-audio.h"

#define MSX_AUDIO_FLAG_CONTROL  0x04
#define MSX_AUDIO_ADPCM_CONTROL 0x07
#define MSX_AUDIO_ADPCM_DATA    0x0F

/*
 * I/O ports used for MSX-AUDIO
*/
__sfr __at 0xC0 msx_audio_status;
__sfr __at 0xC0 msx_audio_address;
__sfr __at 0xC1 msx_audio_data;

static bool g_msx_audio_detected = false;

bool msx_audio_detect(void)
{
  if(msx_audio_status & 0xF9)
  {
    g_msx_audio_detected = false;
  }
  else
  {
    msx_audio_write_register(MSX_AUDIO_ADPCM_CONTROL, 0x80);
    uint8_t status_1 = msx_audio_status & 0xF9;
    msx_audio_write_register(MSX_AUDIO_ADPCM_CONTROL, 0x00);
    uint8_t status_2 = msx_audio_status & 0xF9;

    g_msx_audio_detected = (status_1 == 0x01) && (status_2 == 0x00);
  }

  return g_msx_audio_detected;
}

void msx_audio_reset(void)
{
  if(!g_msx_audio_detected) return;

  msx_audio_write_register(MSX_AUDIO_ADPCM_CONTROL, 0x01); 
  msx_audio_write_register(0xbd, 0x00);
  
  for(uint8_t i = 0; i < 0x16; i++)
  {
    msx_audio_write_register(0x80 + i, 0x0f);
    msx_audio_write_register(0x40 + i, 0x3f);
  }

  for(uint8_t i = 0x07; i < 0xc9; i++)
  {
    msx_audio_write_register(i, 0x00);
  }
}

uint8_t msx_audio_read_status_register(void)
{
  return msx_audio_status;
}

void msx_audio_write_register(const uint8_t reg, const uint8_t value)
{
  msx_audio_address = reg;
  __asm__("in  a, (_msx_audio_status)");
  __asm__("ld  a, (hl)");
  msx_audio_data = value;
  __asm__("in  a, (0x9a)");
  __asm__("in  a, (0x9a)");
}

void msx_audio_write_adpcm_data(const uint8_t *const data, const uint16_t size)
{
  if(!g_msx_audio_detected) return;

  __asm__("di");
  msx_audio_write_register(MSX_AUDIO_FLAG_CONTROL, 0x70);
  for(uint16_t i = 0; i < size; i++)
  {
    msx_audio_write_register(MSX_AUDIO_ADPCM_DATA, data[i]);
    while(!(msx_audio_status & 0x08));
    msx_audio_write_register(MSX_AUDIO_FLAG_CONTROL, 0x80);
  }
  msx_audio_write_register(MSX_AUDIO_FLAG_CONTROL, 0x78);
  __asm__("ei");
}