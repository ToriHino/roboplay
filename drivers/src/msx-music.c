/*
 * RoboPlay for MSX
 * Copyright (C) 2026 by RoboSoft Inc.
 *
 * msx-music.c
 */

#include "support/inc/asm.h"

#include "msx-music.h"

#define MSX_MUSIC_ID_ADDRESS_INTERNAL 0x4018
#define MSX_MUSIC_ID_ADDRESS_EXTERNAL 0x401C

#define MSX_MUSIC_SET_IO_ADDRESS      0x7ff6

#define RDSLT  0x000C
#define WRTSLT 0x0014

/*
 * I/O ports used for MSX-MUSIC
*/
__sfr __at 0x7C msx_music_address;
__sfr __at 0x7D msx_music_data;

static Z80_registers regs;

static const char* g_msx_music_internal_txt = "APRLOPLL";
static const char* g_msx_music_external_txt = "OPLL";

static uint8_t g_msx_music_slot;

static bool g_msx_music_detected = false;
static bool g_msx_music_external = false;

bool msx_music_detect(void)
{
  g_msx_music_detected = false;

  uint8_t* exptbl = (uint8_t *)0xfcc1;
  for(uint8_t i = 0; i < 4 && !g_msx_music_detected; i++)
  {
    if(!(exptbl[i] & 0x80))
      g_msx_music_detected = msx_music_test_slot(i);
    else
      g_msx_music_detected = msx_music_test_sub(i);
  }

  if(g_msx_music_detected && g_msx_music_external)
  {
    /* Set to direct I/O access */
    regs.Bytes.A   = g_msx_music_slot;
    regs.UWords.HL = MSX_MUSIC_SET_IO_ADDRESS;
    AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);
    regs.Bytes.E   = regs.Bytes.A | 0x01;
    regs.Bytes.A   = g_msx_music_slot;
    regs.UWords.HL = MSX_MUSIC_SET_IO_ADDRESS;
    AsmCall(WRTSLT, &regs, REGS_MAIN, REGS_NONE);
  }

  return g_msx_music_detected;
}

uint8_t msx_music_get_slot(void)
{
  return g_msx_music_slot;
}

void msx_music_reset(void)
{
  if(!g_msx_music_detected) return;

  msx_music_write_register(0x0e, 0x00);
  msx_music_write_register(0x07, 0x0f);

  for(uint8_t i = 0; i < 9; i++)
  {
    msx_music_write_register(0x30 + i, 0x0f);
    msx_music_write_register(0x10 + i, 0x00);
    msx_music_write_register(0x20 + i, 0x00);
  }

  for(uint8_t i = 0x00; i < 0x39; i++)
  {
    msx_music_write_register(i, 0x00);
  }

  if(g_msx_music_external)
  {
    /* Reset direct I/O access */
    regs.Bytes.A   = g_msx_music_slot;
    regs.UWords.HL = MSX_MUSIC_SET_IO_ADDRESS;
    AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);
    regs.Bytes.E   = regs.Bytes.A & 0xfe;
    regs.Bytes.A   = g_msx_music_slot;
    regs.UWords.HL = MSX_MUSIC_SET_IO_ADDRESS;
    AsmCall(WRTSLT, &regs, REGS_MAIN, REGS_NONE);
  }
}

void msx_music_write_register(const uint8_t reg, const uint8_t value)
{
  msx_music_address = reg;
  __asm__("in  a, (_msx_music_address)");
  __asm__("in  a, (_msx_music_address)");
  msx_music_data = value;
  __asm__("in  a, (0x9a)");
  __asm__("in  a, (0x9a)");
}

bool msx_music_test_slot(uint8_t slot)
{
  bool result = true;
  g_msx_music_slot = slot;

  /* First check for internal MSX-MUSIC */
  for(uint8_t i = 0; i < sizeof(g_msx_music_internal_txt); i++)
  {
    regs.Bytes.A   = g_msx_music_slot;
    regs.UWords.HL = MSX_MUSIC_ID_ADDRESS_INTERNAL + i;
    AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);

    if(regs.Bytes.A != g_msx_music_internal_txt[i])
    {
      result = false;
      break;
    }

    g_msx_music_external = false;
  }

  /* If not found internal, check for external MSX-MUSIC */
  if(!result)
  {
    result = true;
    for(uint8_t i = 0; i < sizeof(g_msx_music_internal_txt); i++)
    {
      regs.Bytes.A   = g_msx_music_slot;
      regs.UWords.HL = MSX_MUSIC_ID_ADDRESS_EXTERNAL + i;
      AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);

      if(regs.Bytes.A != g_msx_music_external_txt[i])
      {
        result = false;
        break;
      }

      g_msx_music_external = true;
    }
  }

  return result;
}

bool msx_music_test_sub(uint8_t slot)
{
  bool result = false;

  for(uint8_t i = 0; i < 4 && !result; i++)
  {
    uint8_t sub_slot = 0x80 + (i << 2) + slot;
    result = msx_music_test_slot(sub_slot);
  }

  return result;
}
