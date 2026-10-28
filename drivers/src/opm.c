/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * opm.c
 */

#include "support/inc/asm.h"

#include "opm.h"

#define RDSLT  0x000C
#define WRTSLT 0x0014

#define OPM_REG_TEST        0x01
#define OPM_REG_KEYON       0x08
#define OPM_REG_NOISE       0x0F
#define OPM_REG_TIMER_CTRL  0x14
#define OPM_REG_LFO_FREQ    0x18
#define OPM_REG_PMD_AMD     0x19
#define OPM_REG_CT_LFOWAVE  0x1B
#define OPM_REG_RL_FB_CON   0x20   /* + channel 0-7 */
#define OPM_REG_TL          0x60   /* + slot 0-31 */
 
/* Full registers per operator slot, base 0x40, 0x80, 0xA0, 0xC0, 0xE0 */
#define OPM_REG_DT1_MUL     0x40
#define OPM_REG_KS_AR       0x80
#define OPM_REG_AMS_D1R     0xA0
#define OPM_REG_DT2_D2R     0xC0
#define OPM_REG_D1L_RR      0xE0

static Z80_registers regs;

static bool g_opm_detected = false;

bool opm_test_slot(uint8_t slot)
{
  bool result = true;
  g_opm_slot = slot;

  for(uint8_t i = 0; i < sizeof(g_opm_id_txt); i++)
  {
    regs.Bytes.A   = g_opm_slot;
    regs.UWords.HL = OPM_ID_ADDRESS + i;
    AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);

    if(regs.Bytes.A != g_opm_id_txt[i])
    {
      result = false;
      break;
    }
  }

  return result;
}

bool opm_test_sub(uint8_t slot)
{
  bool result = false;

  for(uint8_t i = 0; i < 4 && !result; i++)
  {
    uint8_t sub_slot = 0x80 + (i << 2) + slot;
    result = opm_test_slot(sub_slot);
  }

  return result;
}

bool opm_find_slot(void)
{
  g_opm_detected = false;

  uint8_t* exptbl = (uint8_t *)0xfcc1;
  for(uint8_t i = 0; i < 4 && !g_opm_detected; i++)
  {
    if(!(exptbl[i] & 0x80))
      g_opm_detected = opm_test_slot(i);
    else
      g_opm_detected = opm_test_sub(i);
  }

  return g_opm_detected;
}

uint8_t opm_get_slot(void)
{
  return g_opm_slot;
}

void opm_reset(void)
{
  if(!g_opm_detected) return;

  uint8_t ch, slot;
 
  /* 1. Key-off every channel first, before any TL/EG-affecting writes.
    *    Writing the channel index with no M1/M2/C1/C2 bits set forces
    *    release on all four operators of that channel without retrigger. */
  for (ch = 0; ch < 8; ch++)
      opm_write_register(OPM_REG_KEYON, ch);

  /* 2. Silence routing: RL=00 (no output), FB=0, CON=0 on every channel. */
  for (ch = 0; ch < 8; ch++)
    opm_write_register(OPM_REG_RL_FB_CON + ch, 0x00);

  /* 3. Max-attenuate every operator's Total Level (32 slots: 8ch x 4op). */
  for (slot = 0; slot < 32; slot++)
    opm_write_register(OPM_REG_TL + slot, 0x7F);

  /* 4. Reset LFO phase, then clear test register. */
  opm_write_register(OPM_REG_TEST, 0x02);   /* bit 1 = LFO phase reset */
  opm_write_register(OPM_REG_TEST, 0x00);

  opm_write_register(OPM_REG_LFO_FREQ, 0x00);
  opm_write_register(OPM_REG_PMD_AMD, 0x00);   /* bit7=0: clears AMD */
  opm_write_register(OPM_REG_PMD_AMD, 0x80);   /* bit7=1: clears PMD */
  opm_write_register(OPM_REG_CT_LFOWAVE, 0x00);

  /* 5. Noise off. */
  opm_write_register(OPM_REG_NOISE, 0x00);

  /* 6. Zero remaining per-operator envelope/pitch registers for a fully
    *    deterministic state (not strictly required for silence, but
    *    avoids stale AR/D1R/D2R/RR/KS/MUL/DT1/DT2 bleeding into the
    *    next voice load). */
  for (slot = 0; slot < 32; slot++) {
    opm_write_register(OPM_REG_DT1_MUL  + slot, 0x00);
    opm_write_register(OPM_REG_KS_AR    + slot, 0x00);
    opm_write_register(OPM_REG_AMS_D1R  + slot, 0x00);
    opm_write_register(OPM_REG_DT2_D2R  + slot, 0x00);
    opm_write_register(OPM_REG_D1L_RR   + slot, 0x00);
  }
}

void opm_write_register(const uint8_t reg, const uint8_t value) __naked
{
  reg;
  value;

  __asm
    ld    e, l                        ; E = value
    push  de
    ld    hl, #OPM_ADDRESS_REGISTER
    ld    e, a                        ; E = reg
    ld    a, (_g_opm_slot)
    call  WRTSLT

    pop   de                          ; E = value
    ld    hl, #OPM_DATA_REGISTER
    ld    a, (_g_opm_slot)
    jp    WRTSLT
  __endasm;
}

uint8_t opm_read_status_register(void)
{
  regs.Bytes.A   = g_opm_slot;
  regs.UWords.HL = OPM_STATUS_REGISTER;
  AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);

  return regs.Bytes.A;
}
