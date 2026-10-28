/*
 * RoboPlay for MSX
 * Copyright (C) 2026 by RoboSoft Inc.
 *
 * opl4.c
 */

#include "opl4.h"
#include "data/inc/yrw801.h"

#define OPL4_WAVE_CHANNELS 24
#define OPL4_MAX_RAM_BANK  64
#define OPL4_MAGIC_NUMBER  123
#define OPL4_MAGIC_NUMBER_INVERT 321

#define OPL4_WAIT    while(opl4_fm_base & 0x01);
#define OPL4_WAIT_LD while(opl4_fm_base & 0x02);

#define OPL4_TIMER1_COUNT  0x02
#define OPL4_TIMER2_COUNT  0x03
#define OPL4_TIMER_CONTROL 0x04

#define NR_OF_OPL_WAVE_DRUMS  5
#define OPL_WAVE_DRUM_VOLUME  8

#define OPL4_DEVICE_ID 0x20

extern bool g_wave_opl_drums;

const YRW801_REGION_DATA opl_drums[NR_OF_OPL_WAVE_DRUMS] =
{
  /* BASE DRUM */
  {0x23, 0x23, {0x011,4098,100, 0,0,0x00,0xea,0x00,0xf0,0x16,0x06,0x0}},    // YRW801: Accoustic Bass Drum

  /* SNARE DRUM*/
  {0x26, 0x26, {0x0d1,4298,100, 0,0,0x00,0xd6,0x00,0xf0,0x05,0x05,0x0}},    // YRW801: Accoustic Snare

  /* TOM_TOM */
  {0x2f, 0x2f, {0x0c7,5185,100, 2,0,0x00,0xe0,0x00,0xf6,0x17,0x07,0x0}},    // YRW801: LoW-Mid Tom

  /* TOP_CYMBAL */
    {0x34, 0x34, {0x07a,3009,100,-2,0,0x00,0xea,0x00,0xf2,0x15,0x05,0x0}},  // YRW801: Chinese Cymbal

  /* HIGH HAT */
  {0x2a, 0x2a, {0x079,6160,100, 2,0,0x00,0xe0,0x00,0xf5,0x19,0x09,0x0}},    // YRW801: Closed Hi Hat

};

/*
 * OUT ports used for OPL4
*/
__sfr __at 0xC4 opl4_fm_base;
__sfr __at 0x7E opl4_wave_base;
__sfr __at 0xC4 opl4_status;

__sfr __at 0xC4 opl4_fm_reg1;
__sfr __at 0xC5 opl4_fm_data1;
__sfr __at 0xC6 opl4_fm_reg2;
__sfr __at 0xC7 opl4_fm_data2;

__sfr __at 0x7E opl4_wave_reg;
__sfr __at 0x7F opl4_wave_data;

bool opl4_detect(void)
{
  /* Set to OPL4 mode */
  opl4_fm_reg2 = 0x05;
  opl4_fm_data2 = 0x03;

  opl4_wave_reg = 0x02;
  uint8_t device_id =  opl4_wave_data & 0xEF;

  /* Set back to OPL mode */
  opl4_fm_reg2 = 0x05;
  opl4_fm_data2 = 0x00;

  return (device_id == OPL4_DEVICE_ID) ? true : false;
}

void opl4_reset(void)
{
  /* Set to OPL4 mode */
  opl4_write_fm_register_array_2(0x05, 0x03);

  /* Reset FM registers */
  opl4_write_fm_register_array_1(0x01, 0x00);
  opl4_write_fm_register_array_1(0x02, 0x00);
  opl4_write_fm_register_array_1(0x03, 0x00);
  opl4_write_fm_register_array_1(0x04, 0x00);
  opl4_write_fm_register_array_1(0x08, 0x00);

  opl4_write_fm_register_array_2(0x01, 0x00);
  opl4_write_fm_register_array_2(0x02, 0x00);
  opl4_write_fm_register_array_2(0x03, 0x00);
  opl4_write_fm_register_array_2(0x04, 0x00);
  opl4_write_fm_register_array_2(0x08, 0x00);

  for (uint8_t i = 0x14; i < 0xF6; i++)
  {
    uint8_t value = (i >= 0x60 && i < 0xA0) ? 0xFF : 0;
    opl4_write_fm_register_array_1(i, value);
    opl4_write_fm_register_array_2(i, value);
  }

  /* Set mix control */
  opl4_write_wave_register(0xF8, 0x1B);
  opl4_write_wave_register(0xF9, 0x00);

  /* Reset WAVE registers */
  for (uint8_t i = 0; i < OPL4_WAVE_CHANNELS; i++)
  {
    opl4_write_wave_register(0x68 + i, 0x40);
  }

  /* Reset timer flags */
  opl4_write_fm_register_array_1(0x04, 0x80);

  /* Set drum waves for OPL drums */
  for(uint8_t channel = 0; channel < NR_OF_OPL_WAVE_DRUMS; channel++)
  {
    opl4_write_wave_register(0x20 + channel, opl_drums[channel].wave_data.tone >> 8);
    opl4_write_wave_register(0x08 + channel, opl_drums[channel].wave_data.tone & 0xff);

    OPL4_WAIT_LD;

    opl4_write_wave_register(0x50 + channel, (OPL_WAVE_DRUM_VOLUME << 1) | 0x01);

    opl4_write_wave_register(0x98 + channel, opl_drums[channel].wave_data.reg_attack_decay1);
    opl4_write_wave_register(0xB0 + channel, opl_drums[channel].wave_data.reg_level_decay2);
    opl4_write_wave_register(0xC8 + channel, opl_drums[channel].wave_data.reg_release_correction);
    opl4_write_wave_register(0xE0 + channel, opl_drums[channel].wave_data.reg_tremolo);
  }
}

uint8_t opl4_sample_ram_banks(void)
{
  uint8_t result = 0;

  /* Set custom sample headers to 16Mb area and READ/WRITE mode */
  opl4_write_wave_register(0x02, 0x11);

  /* Detect the number of 64K banks of RAM available */
  for (uint8_t i = 0; !result && i <= OPL4_MAX_RAM_BANK; i++)
  {
    /* Write block number to corresponding OPL4 RAM address */
    opl4_write_wave_register(0x03, i + 0x20);               /* Register 3: memory addres bits 16-21 */
    opl4_write_wave_register(0x04, 0x00);                   /* Register 4: memory addres bits 08-15 */
    opl4_write_wave_register(0x05, 0x00);                   /* Register 5: memory addres bits 00-07 */
    opl4_write_wave_register(0x06, i + OPL4_MAGIC_NUMBER);

    opl4_write_wave_register(0x03, i + 0x20);               /* Register 3: memory addres bits 16-21 */
    opl4_write_wave_register(0x04, 0x00);                   /* Register 4: memory addres bits 08-15 */
    opl4_write_wave_register(0x05, 0x01);                   /* Register 5: memory addres bits 00-07 */
    opl4_write_wave_register(0x06, i + OPL4_MAGIC_NUMBER_INVERT);

    /* Read back the number from the same location and check */
    opl4_write_wave_register(0x03, i + 0x20);               /* Register 3: memory addres bits 16-21 */
    opl4_write_wave_register(0x04, 0x00);                   /* Register 4: memory addres bits 08-15 */
    opl4_write_wave_register(0x05, 0x00);                   /* Register 5: memory addres bits 00-07 */

    if (opl4_read_wave_register(0x06) != (i + OPL4_MAGIC_NUMBER))
    {
      result = i;
      break;
    }


    /* Check if one of the earlier found banks has been overwritten */
    for (int8_t j = i; j >= 0; j--)
    {
      /* Read back the number from the same location and check */
      opl4_write_wave_register(0x03, j + 0x20);           /* Register 3: memory addres bits 16-21 */
      opl4_write_wave_register(0x04, 0x00);               /* Register 4: memory addres bits 08-15 */
      opl4_write_wave_register(0x05, 0x00);               /* Register 5: memory addres bits 00-07 */

      if (opl4_read_wave_register(0x06) != (j + OPL4_MAGIC_NUMBER))
      {
        result = i;
        break;
      }
    }
  }

  /* Set custom sample headers to 16Mb area and PLAY mode*/
  opl4_write_wave_register(0x02, 0x10);

  return result;
}

void opl4_set_refresh(float refresh)
{
if (refresh > 50.0)
{
  opl4_write_fm_register_array_1(OPL4_TIMER1_COUNT, 255 - (uint8_t)((1000.0 / refresh) / 0.08));
  opl4_write_fm_register_array_1(OPL4_TIMER_CONTROL, 0x21);
  opl4_write_fm_register_array_1(OPL4_TIMER_CONTROL, 0x80);
}
else
{
  opl4_write_fm_register_array_1(OPL4_TIMER2_COUNT, 256 - (uint8_t)((1000.0 / refresh) / 0.320));
  opl4_write_fm_register_array_1(OPL4_TIMER_CONTROL, 0x42);
  opl4_write_fm_register_array_1(OPL4_TIMER_CONTROL, 0x80);
}
}

void opl4_write_fm_register_array_1(const uint8_t reg, const uint8_t value)
{
  // Check if OPL rythm goes to wave
  if(g_wave_opl_drums && reg == 0xBD && (reg & 0x20))
  {
    opl4_play_drum_on_wave(value & 0x1f);
  }
  else
  {
    opl4_fm_reg1 = reg;

    OPL4_WAIT;
    opl4_fm_data1 = value;
  }
}

void opl4_write_fm_register_array_2(const uint8_t reg, const uint8_t value)
{
  opl4_fm_reg2 = reg;

  OPL4_WAIT;
  opl4_fm_data2 = value;
}

void opl4_write_wave_register(const uint8_t reg, const uint8_t value)
{
  opl4_wave_reg = reg;

  OPL4_WAIT;
  opl4_wave_data = value;
}

void opl4_write_wave_data(const uint8_t *const data, const uint16_t size)
{
  opl4_wave_reg = 6;

  OPL4_WAIT;
  for (uint16_t i = 0; i < size; i++)
  {
    opl4_wave_data = data[i];
  }
}

uint8_t opl4_read_status_register(void)
{
  return (uint8_t)opl4_fm_base;
}

uint8_t opl4_read_fm_register_array_1(const uint8_t reg)
{
  opl4_fm_reg1 = reg;

  OPL4_WAIT
  return opl4_fm_data1;
}

uint8_t opl4_read_fm_register_array_2(const uint8_t reg)
{
  opl4_fm_reg2 = reg;

  OPL4_WAIT
  return opl4_fm_data2;
}

uint8_t opl4_read_wave_register(const uint8_t reg)
{
  opl4_wave_reg = reg;
  OPL4_WAIT;
  return (uint8_t)opl4_wave_data;
}

void opl4_wait_for_ld(void)
{
  OPL4_WAIT_LD;
}

void opl4_play_drum_on_wave(const uint8_t drums)
{
  const uint8_t opl_drum_bits[] = { 0x10, 0x08, 0x04, 0x02, 0x01 };

  for(uint8_t channel = 0; channel < NR_OF_OPL_WAVE_DRUMS; channel++)
  {
    if(drums & opl_drum_bits[channel])
    {
      const uint8_t  octave = 0;
      const uint16_t fnumber = 0;
      const uint8_t  pan = 0;

      opl4_write_wave_register(0x68 + channel, 0);

      opl4_write_wave_register(0x38 + channel, (octave << 4) |((fnumber >> 7) & 0x07));
      opl4_write_wave_register(0x20 + channel, (opl_drums[channel].wave_data.tone >> 8) | ((fnumber << 1) & 0xFE));

      opl4_write_wave_register(0x68 + channel, 0x80 | (pan & 0x0F));
    }
  }
}