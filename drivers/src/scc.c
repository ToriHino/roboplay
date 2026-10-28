/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * scc.c
 */

#include <string.h>

#include "support/inc/asm.h"

#include "scc.h"

#define ENASLT 0x0024
#define RAMAD2 0xF343

static Z80_registers regs;

#define SELECT_SCC_SLOT                         \
  regs.Bytes.A = g_scc_slot;                    \
  regs.Bytes.H = SCC_PAGE;                      \
  AsmCall(ENASLT, &regs, REGS_MAIN, REGS_NONE);

#define DESELECT_SCC_SLOT                       \
  regs.Bytes.A = g_original_slot;               \
  regs.Bytes.H = SCC_PAGE;                      \
  AsmCall(ENASLT, &regs, REGS_MAIN, REGS_NONE);

bool scc_test_slot(uint8_t slot)
{
  bool result = false;
  g_scc_slot = slot;

  uint8_t* scc_bank_select = (uint8_t *)SCC_BANK_SELECT;
  uint8_t  org_bank_select = *scc_bank_select;

  uint8_t* scc_register_base = (uint8_t *)SCC_REGISTER_BASE;
  uint8_t  org_register_base = *scc_bank_select;

  SELECT_SCC_SLOT

  *scc_bank_select = 0x3f;
  
  *scc_register_base = 0x00;
  result = (*scc_register_base == 0x00);
  
  *scc_register_base = 0xff;
  result = result && (*scc_register_base == 0xff);

  result = result && (*scc_bank_select != 0x3f);

  DESELECT_SCC_SLOT

  *scc_bank_select   = org_bank_select;
  *scc_register_base = org_register_base;

  return result;
}

bool scc_test_sub(uint8_t slot)
{
  bool result = false;

  for(uint8_t i = 0; i < 4 && !result; i++)
  {
    uint8_t sub_slot = 0x80 + (i << 2) + slot;
    result = scc_test_slot(sub_slot);
  }

  return result;
}

bool scc_find_slot(void)
{
  bool scc_detected = false;

  uint8_t* ramad2 = (uint8_t *)RAMAD2;
  g_original_slot = *ramad2;

  uint8_t* exptbl = (uint8_t *)0xfcc1;
  for(uint8_t i = 0; i < 4 && !scc_detected; i++)
  {
    if(!(exptbl[i] & 0x80))
      scc_detected = scc_test_slot(i);
    else
      scc_detected = scc_test_sub(i);
  }

  return scc_detected;
}

uint8_t scc_get_slot(void)
{
    return g_scc_slot;
}

void scc_reset(void)
{
    SELECT_SCC_SLOT

    for(uint8_t i = 0; i < SCC_NR_OF_CHANNELS - 1; i++)
    {
        for(uint8_t j = 0; j < SCC_WAVEFORM_SIZE; j++)
            g_scc_registers.waveform[i][j] = 0;
    }

    for(uint8_t i = 0; i < SCC_NR_OF_CHANNELS; i++)
    {
        *g_scc_registers.frequency[i] = 0;
        *g_scc_registers.volume[i] = 0;
    }

    *g_scc_registers.voice_on = 0x1F;

    DESELECT_SCC_SLOT
}

void scc_set_waveform(uint8_t channel, uint8_t *waveform)
{
    memcpy(g_waveform_buffer, waveform, SCC_WAVEFORM_SIZE);

    SELECT_SCC_SLOT

    memcpy(g_scc_registers.waveform[channel], g_waveform_buffer, SCC_WAVEFORM_SIZE);

    DESELECT_SCC_SLOT
}

void scc_set_frequency(uint8_t channel, uint16_t frequency)
{
    SELECT_SCC_SLOT

    *g_scc_registers.frequency[channel] = frequency;

    DESELECT_SCC_SLOT
}

void scc_set_volume(uint8_t channel, uint8_t volume)
{
    SELECT_SCC_SLOT

    *g_scc_registers.volume[channel] = volume;

    DESELECT_SCC_SLOT
}

void scc_write_register(uint8_t reg, uint8_t value)
{
    SELECT_SCC_SLOT

    uint8_t* address = (uint8_t *)(SCC_REGISTER_BASE + reg);
    *address = value;

    DESELECT_SCC_SLOT
}