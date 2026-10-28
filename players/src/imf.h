/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * imf.h
 *
 * IMF: id Software Music Format player
 */

#pragma once

#include <stdint.h>

typedef enum
{
    IMF_TYPE_0,
    IMF_TYPE_1
} IMF_TYPE;

float g_refresh;

static IMF_TYPE g_imf_type;

static uint16_t g_imf_data_size;

static uint8_t g_segment_list[256];
static uint8_t g_segment_index;

static uint16_t g_delay_counter;
static uint32_t g_index_pointer;

static uint8_t *g_song_data;

uint8_t get_next_data_byte(void);

void handle_imf_update(void);
