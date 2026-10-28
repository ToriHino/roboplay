/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * memory.h
 */

#pragma once

#include <stdint.h>

#define MAX_NR_SEGMENTS 256
#define BUFFER_SIZE     256

extern char *g_current_song_name;
extern char *g_current_player_name;
extern char *g_m3u_name;
extern char *work_buffer;

uint16_t pre_allocate_ram_segments(void);
void restore_ram_segment(void);

void free_used_ram_segments(void);
uint8_t get_free_ram_segment(void);
void set_active_ram_segment(uint8_t segment_index);
