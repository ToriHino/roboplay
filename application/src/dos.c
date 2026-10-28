/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * dos.c
 */

#include "support/inc/asm.h"

#include "dos.h"

#define EXTBIO 0xFFCA

static Z80_registers regs;

FILE_INFO_BLOCK g_file_info_block;

uint8_t g_primary_mapper_slot;
uint8_t g_total_memory_segments;

MAPPER_ROUTINE* g_mapper_routines_table;
SEGMENTSTATUS   g_segment_status;

void dos_exit(uint8_t error)
{
  regs.Bytes.B = error;
  DosCall(_TERM, &regs, REGS_MAIN, REGS_NONE);
}

bool dos_get_environment(const char* env, char* value)
{
  regs.UWords.HL = (unsigned int)env;
  regs.UWords.DE = (unsigned int)value;
  regs.Bytes.B   = 0xff;
  DosCall(_GENV, &regs, REGS_MAIN, REGS_MAIN);

  return (regs.Bytes.A == 0x00);
}

int8_t dos_open(const char* name, uint8_t mode)
{
  regs.UWords.DE = (uint16_t)name;
  regs.Bytes.B   = mode;
  DosCall(_OPEN, &regs, REGS_MAIN, REGS_MAIN);

  if(regs.Bytes.A) dos_exit(regs.Bytes.A);

  return regs.Bytes.B;
}

int8_t dos_close(uint8_t handle)
{
  regs.Bytes.B = handle;
  DosCall(_CLOSE, &regs, REGS_MAIN, REGS_AF);

  return regs.Bytes.A;
}

int16_t dos_read(uint8_t handle, uint8_t* destination, uint16_t size)
{
  regs.Bytes.B   = handle;
  regs.UWords.DE = (uint16_t)destination;
  regs.UWords.HL = size;
  DosCall(_READ, &regs, REGS_MAIN, REGS_MAIN);

  return regs.UWords.HL;
}

uint32_t dos_lseek(int8_t handle, int32_t offset, uint8_t whence)
{
  regs.Bytes.B = handle;
  regs.UWords.DE = offset >> 16;
  regs.UWords.HL = offset & 0xffff;
  regs.Bytes.A = whence;
  DosCall(_SEEK, &regs, REGS_MAIN, REGS_MAIN);

  uint32_t result = regs.UWords.DE;
  result = result << 16;
  result = result + regs.UWords.HL;

  return result;
}

int8_t dos_find_first_entry(char *name)
{
  regs.UWords.DE = (uint16_t)name;
  regs.UWords.IX = (uint16_t)&g_file_info_block;
  regs.Bytes.B   = 0x00;
  DosCall(_FFIRST, &regs, REGS_ALL, REGS_AF);

  return regs.Bytes.A;
}

int8_t dos_find_next_entry(void)
{
  regs.UWords.IX = (uint16_t)&g_file_info_block;
  DosCall(_FNEXT, &regs, REGS_ALL, REGS_AF);

  return regs.Bytes.A;
}

uint8_t dos_init_ram_mapper_info(void)
{
  regs.Bytes.D = 0x04;
  regs.Bytes.A = 0x00;
  regs.Bytes.E = 0x02;
  AsmCall(EXTBIO, &regs, REGS_MAIN, REGS_MAIN);

  g_total_memory_segments = regs.Bytes.A;
  g_primary_mapper_slot   = regs.Bytes.B;
  g_mapper_routines_table = (MAPPER_ROUTINE*)regs.UWords.HL;

  return g_primary_mapper_slot;
}

SEGMENTSTATUS* dos_allocate_segment(uint8_t segment_type, uint8_t slot_address)
{
  regs.Bytes.A = segment_type;
  regs.Bytes.B = slot_address;
  AsmCall(g_mapper_routines_table[ALL_SEG].routine_address, &regs, REGS_MAIN, REGS_MAIN);

  g_segment_status.allocatedSegmentNumber = regs.Bytes.A;
  g_segment_status.slotAddressOfMapper    = regs.Bytes.B;
  g_segment_status.carryFlag              = (regs.Flags.C != 0);

  return &g_segment_status;
}

uint8_t dos_get_p2(void)
{
  AsmCall(g_mapper_routines_table[GET_P2].routine_address, &regs, REGS_NONE, REGS_AF);
  return regs.Bytes.A;
}

void dos_put_p2(uint8_t segment)
{
  regs.Bytes.A = segment;
  AsmCall(g_mapper_routines_table[PUT_P2].routine_address, &regs, REGS_AF, REGS_NONE);
}
