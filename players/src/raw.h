/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * raw.h
 *
 * RAW: RAW OPL capture player
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define RAW_DELAY     0x00
#define RAW_CONTROL   0x02

#define RAW_CONTROL_TYPE_CLOCK        0x00
#define RAW_CONTROL_TYPE_FIRST_OPL    0x01
#define RAW_CONTROL_TYPE_SECOND_OPL   0x02

typedef struct
{
    char signature[8];
    uint16_t clock;
} RAW_HEADER;

typedef struct 
{
    uint8_t parameter;
    uint8_t command;
} RAW_DATA;

RAW_HEADER g_raw_header;

uint8_t g_segment_list[256];
uint8_t g_segment_index;

uint16_t g_delay_counter;
uint16_t g_speed;

RAW_DATA* g_song_data;
uint16_t g_position;

bool g_second_opl;

void next_position(void);
