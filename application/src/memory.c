/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * memory.c
 */

#include <stdio.h>

#include "support/inc/asm.h"

#include "players/inc/player_interface.h"

#include "dos.h"
#include "memory.h"

static Z80_registers regs;

#define ENASLT  0x0024

#define SONG_NAME_BUFFER     0xD000
#define PLAYER_NAME_BUFFER   SONG_NAME_BUFFER + BUFFER_SIZE
#define M3U_NAME_BUFFER      PLAYER_NAME_BUFFER + BUFFER_SIZE
#define WORK_BUFFER          M3U_NAME_BUFFER + BUFFER_SIZE

char *g_current_song_name = SONG_NAME_BUFFER;
char *g_current_player_name = PLAYER_NAME_BUFFER;
char *g_m3u_name = M3U_NAME_BUFFER;
char *work_buffer = WORK_BUFFER;

typedef struct
{
  uint8_t allocated_segment_number;
  uint8_t slot_address_of_mapper;
} SEGMENT_INFO;

static uint16_t g_segments_found = MAX_NR_SEGMENTS;
static uint8_t  g_last_used_segment = 0;
static uint8_t  g_current_slot = 0;

static SEGMENT_INFO g_segment_list[MAX_NR_SEGMENTS];

uint16_t pre_allocate_ram_segments(void)
{
  g_segment_list[START_SEGMENT_INDEX].slot_address_of_mapper = dos_init_ram_mapper_info();
  g_current_slot = g_segment_list[START_SEGMENT_INDEX].slot_address_of_mapper;
  
  SEGMENTSTATUS*   pSegmentStatus = 0;

  g_segment_list[START_SEGMENT_INDEX].allocated_segment_number = dos_get_p2();
  g_last_used_segment = START_SEGMENT_INDEX;

  for (uint16_t i = START_SEGMENT_INDEX + 1; i < MAX_NR_SEGMENTS; i++)
  {
    pSegmentStatus = dos_allocate_segment(0, 0x20);

    if (pSegmentStatus->carryFlag)
    {
      g_segments_found = i;
      break;
    }

    g_segment_list[i].allocated_segment_number = pSegmentStatus->allocatedSegmentNumber;
    g_segment_list[i].slot_address_of_mapper = pSegmentStatus->slotAddressOfMapper;
  }

  return g_segments_found;
}

void restore_ram_segment(void)
{
  set_active_ram_segment(START_SEGMENT_INDEX);
}

void free_used_ram_segments(void)
{
  g_last_used_segment = START_SEGMENT_INDEX;
}

uint8_t get_free_ram_segment(void)
{
  g_last_used_segment++;
  if (g_last_used_segment >= g_segments_found)
  {
    restore_ram_segment();
    dos_exit(_NORAM);
  }

  return g_last_used_segment;
}

void set_active_ram_segment(uint8_t segment_number)
{
  dos_put_p2(g_segment_list[segment_number].allocated_segment_number);

  if (g_current_slot != g_segment_list[segment_number].slot_address_of_mapper)
  {
    g_current_slot = g_segment_list[segment_number].slot_address_of_mapper;

    regs.Bytes.A = g_current_slot;
    regs.UWords.HL = DATA_SEGMENT_BASE;
    AsmCall(ENASLT, &regs, REGS_MAIN, REGS_NONE);
  }
}