/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * soundstar.c
 */

#include "support/inc/asm.h"

#include "players/inc/player_interface.h"

#include "soundstar.h"

__sfr __at 0x05 soundstar_register_port;
__sfr __at 0x04 soundstar_value_port;

#define SOUNDSTAR_ID_LENGTH 7

#define RDSLT  0x0c
#define RAMAD2 0xF343

static Z80_registers regs;

static const char* soundstar_device_id = "SAA1099";

static uint8_t g_soundstar_slot;
static uint8_t g_original_slot;

static uint8_t g_soundstar_major_version;
static uint8_t g_soundstar_minor_version;

bool soundstar_test_slot(uint8_t slot)
{
  bool result = true;
  g_soundstar_slot = slot;

  g_soundstar_major_version = 0;
  g_soundstar_minor_version = 0;

  for(uint8_t i = 0; i < SOUNDSTAR_ID_LENGTH; i++)
  {
    regs.Bytes.A = g_soundstar_slot;
    regs.UWords.HL = 0x4010 + i;
    AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);
    if(regs.Bytes.A != soundstar_device_id[i])
    {
      result = false;
      break;
    }
  }

  if(result)
  {
    regs.Bytes.A = g_soundstar_slot;
    regs.UWords.HL = 0x4018;
    AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);
    g_soundstar_major_version = regs.Bytes.A;

    regs.Bytes.A = g_soundstar_slot;
    regs.UWords.HL = 0x4019;
    AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);
    g_soundstar_minor_version = regs.Bytes.A;
  }

  return result;
}

bool soundstar_test_sub(uint8_t slot)
{
  bool result = false;

  for(uint8_t i = 0; i < 4 && !result; i++)
  {
    uint8_t sub_slot = 0x80 + (i << 2) + slot;
    result = soundstar_test_slot(sub_slot);
  }

  return result;
}

bool soundstar_find_slot(void)
{
  bool soundstar_detected = false;

  uint8_t* ramad2 = (uint8_t *)RAMAD2;
  g_original_slot = *ramad2;

  uint8_t* exptbl = (uint8_t *)0xfcc1;
  for(uint8_t i = 0; i < 4 && !soundstar_detected; i++)
  {
    if(!(exptbl[i] & 0x80))
      soundstar_detected = soundstar_test_slot(i);
    else
      soundstar_detected = soundstar_test_sub(i);
  }

  return soundstar_detected;
}

uint8_t soundstar_get_slot(void)
{
    return g_soundstar_slot;
}

uint8_t soundstar_get_major_version(void)
{
  return g_soundstar_major_version;
}

uint8_t soundstar_get_minor_version(void)
{
  return g_soundstar_minor_version;
}

void soundstar_reset(void)
{
  soundstar_write(0x1c, 0x02);

  for(uint8_t i = 0; i < 0x20; i++)
    soundstar_write(i, 0x00);

  soundstar_write(0x1c, 0x01);
}

void soundstar_write(const uint8_t reg, const uint8_t value)
{
  soundstar_register_port = reg;
  soundstar_value_port = value;
}
