/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2024
 *
 * vdp.c
 */

#include "support/inc/asm.h"

#include "vdp.h"

#define RDSLT 0x000C

static Z80_registers regs;

static uint8_t vdp_refresh;
static uint8_t vdp_read_port;
static uint8_t vdp_write_port;

void init_vdp_device(void)
{
  regs.Bytes.A   = 0x00;
  regs.UWords.HL = 0x002b;
  AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);

  if(regs.Bytes.A & 0x80)
    vdp_refresh = VDP_REFRESH_50_HZ;
  else
    vdp_refresh = VDP_REFRESH_60_HZ;

  regs.Bytes.A   = 0x00;
  regs.UWords.HL = 0x0006;
  AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);
  vdp_read_port = regs.Bytes.A;

  regs.Bytes.A   = 0x00;
  regs.UWords.HL = 0x0007;
  AsmCall(RDSLT, &regs, REGS_MAIN, REGS_AF);
  vdp_write_port = regs.Bytes.A;
}

uint8_t get_vdp_refresh(void)
{
  return vdp_refresh;
}

uint8_t vdp_read_status_register(void) __naked
{
  __asm
  ld  a, (_vdp_read_port)
  ld  c, a
  in	a, (c)
  ret
  __endasm;
}