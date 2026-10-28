/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * dro.h
 *
 * DRO: DOSBox Raw OPL player
 */

#pragma once

#include <stdint.h>

#define DRO1_HARDWARE_TYPE_OPL2       0
#define DRO1_HARDWARE_TYPE_OPL3       1
#define DRO1_HARDWARE_TYPE_DUAL_OPL2  2

#define DRO2_HARDWARE_TYPE_OPL2       0
#define DRO2_HARDWARE_TYPE_DUAL_OPL2  1
#define DRO2_HARDWARE_TYPE_OPL3       2

typedef struct 
{
    char signature[8];
    uint16_t version_major;
    uint16_t version_minor;
} DRO_HEADER;

typedef struct
{
    uint32_t length_ms;
    uint32_t length_bytes;
    uint8_t  hardware_type;
    uint8_t  hardware_extra[3];
} DRO1_HEADER;

typedef struct
{
    uint32_t length_pairs;
    uint32_t length_ms;
    uint8_t hardware_type;
    uint8_t format;
    uint8_t compression;
    uint8_t short_delay_code;
    uint8_t long_delay_code;
    uint8_t code_map_length;   
} DRO2_HEADER;

typedef enum
{
    DRO_TYPE_0_1,
    DRO_TYPE_2_0
} DRO_TYPE;

static DRO_HEADER   g_dro_header;
static DRO1_HEADER  g_dro1_header;
static DRO2_HEADER  g_dro2_header;

static DRO_TYPE g_dro_type;

static uint8_t g_code_map[256];

static uint8_t g_segment_list[256];
static uint8_t g_segment_index;

static uint16_t g_delay_counter;
static uint32_t g_index_pointer;

static uint8_t *g_song_data;

static bool g_high_opl_chip = false;

uint8_t get_next_data_byte(void);

void handle_dro1_update(void);
void handle_dro2_update(void);
