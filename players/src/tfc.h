/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * tfc.h
 *
 * TFC: TurboFM Compiled
 */

#pragma once

#include <stdint.h>

#define NR_OF_CHANNELS 6
#define CHANNEL_SPLIT  3

#define NR_OF_VOICES   8


typedef struct
{
    uint8_t  signature[6];
    uint8_t  version[3];
    uint8_t  frequency;
    uint8_t* offsets[6];
    uint8_t  reserved[12];
} TFC_HEADER;

static TFC_HEADER *g_tfc_header;
static uint8_t    *g_data;

char *g_title;
char *g_author;
char *g_comment;

uint8_t g_channel;
uint16_t g_cursor[NR_OF_CHANNELS];

uint8_t  g_repeat_frames[NR_OF_CHANNELS];
uint16_t g_return_address[NR_OF_CHANNELS];

uint16_t g_loop[NR_OF_CHANNELS];

const uint8_t g_lfo_lut[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0xC1, 0xC7, 0xC9, 0xCB, 0xCD, 0xD4, 0xF9, 0xFF
};

const uint16_t g_log2_table[] =
{
        0,   736,  1466,  2190,  2909,  3623,  4331,  5034,
     5732,  6425,  7112,  7795,  8473,  9146,  9814, 10477,
    11136, 11791, 12440, 13086, 13727, 14363, 14996, 15624,
    16248, 16868, 17484, 18096, 18704, 19308, 19909, 20505,
    21098, 21687, 22272, 22854, 23433, 24007, 24579, 25146,
    25711, 26272, 26830, 27384, 27936, 28484, 29029, 29571,
    30109, 30645, 31178, 31707, 32234, 32758, 33279, 33797,
    34312, 34825, 35334, 35841, 36346, 36847, 37346, 37842,
    38336, 38827, 39316, 39802, 40286, 40767, 41246, 41722,
    42196, 42667, 43137, 43603, 44068, 44530, 44990, 45448,
    45904, 46357, 46809, 47258, 47705, 48150, 48593, 49034,
    49472, 49909, 50344, 50776, 51207, 51636, 52063, 52488,
    52911, 53332, 53751, 54169, 54584, 54998, 55410, 55820,
    56229, 56635, 57040, 57443, 57845, 58245, 58643, 59039,
    59434, 59827, 60219, 60609, 60997, 61384, 61769, 62152,
    62534, 62915, 63294, 63671, 64047, 64421, 64794, 65166
};

uint8_t g_frequency_msb[NR_OF_VOICES];
uint8_t g_rl_fb_connect[NR_OF_VOICES];

uint16_t g_frequency[NR_OF_CHANNELS];
uint8_t  g_skip[NR_OF_CHANNELS];

uint8_t g_channel_offset;

void set_register(uint8_t reg, uint8_t val);
void frequency_to_key(uint8_t msb, uint8_t lsb);

void write_slots(uint8_t reg, uint8_t val);
void write_channels(uint8_t reg, uint8_t val);

uint16_t parse_frame_control(uint16_t cursor);
uint16_t parse_frame_commands(uint16_t cursor);
uint16_t parse_frame_data(uint16_t cursor);

void set_slide(uint8_t slide);
void set_frequency(uint16_t frequency);
void set_key_off(void);   
void set_key_on(void);
