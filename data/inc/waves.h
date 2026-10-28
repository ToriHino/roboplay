/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * waves.h
 *
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FRQ_TAB_PC    176
#define FRQ_TAB_AMIGA 177
#define FRQ_TAB_TURBO 178

typedef struct
{
    uint8_t   next_patch_note;
    uint8_t   tone_low;
    uint8_t   tone_high;
    uint16_t *frequency_table;
} PATCH_PART;

typedef struct
{
    bool        transpose;
    uint8_t    *header_bytes;
    PATCH_PART  patch_part;
} PATCH;

typedef struct 
{
    uint8_t   tone;
    uint16_t  frequency;
    uint8_t  *header_bytes;
} GM_DRUM_PATCH;

typedef struct
{
    uint8_t next_patch_note;
    uint8_t tone;
    uint8_t tone_note;
} OWN_PATCH_PART;

typedef struct
{
    bool transpose;
    OWN_PATCH_PART patch_part[8];
} OWN_PATCH;
