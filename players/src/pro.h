/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * pro.h
 *
 * PRO: Pro-Tracker player
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_SONG_LENGTH 99

#define SONG_NAME_LENGTH 16
#define AUTHOR_LENGTH 16

#define NR_OF_CHANNELS 9
#define NR_OF_FM_CHANNELS 6
#define NR_OF_DRUM_CHANNELS 3

#define NR_OF_LINES 64
#define LINE_SIZE 8

#define NR_OF_VOICE_PATCHES 15
#define NR_OF_DRUM_PATCHES  3
#define INSTRUMENT_LENGTH   8

#define PAN_SETTING_LEFT  0x10
#define PAN_SETTING_MID   0x30
#define PAN_SETTING_RIGHT 0x20

#define NOTE_ON             0x60    /* 0x01 - 0x60 */
#define INSTRUMENT_CHANGE   0x61    /* 0x61 - 0x7F */
#define NOTE_OFF            0x80    /* 0x80        */
#define SLIDE_UP            0x81    /* 0x81 - 0x9F */
#define RELEASE             0xA0    /* 0xA0        */
#define SLIDE_DOWN          0xA1    /* 0xA1 - 0xBF */
#define VOLUME_CHANGE       0xC0    /* 0xC0 - 0xCF */

#define FLIP                0xE0    /* 0xE0 - 0xE7, 0xE8 - 0xEF */
#define PITCH               0xF0    /* 0xF0 - 0xF7, 0xF8 - 0xFF */

typedef struct
{
    char signature[4];
    char song_name[SONG_NAME_LENGTH];
    char author[AUTHOR_LENGTH];
    uint8_t original_instruments[15][16];
    uint8_t instrument_volumes[32];
    uint8_t start_instruments[NR_OF_FM_CHANNELS];
    uint8_t speed;
    uint8_t drum_frequencies[6];
    uint8_t drum_volume;
    uint8_t song_length;
    uint8_t unknown[2];
} PRO_HEADER;

typedef struct
{
    bool update;
    uint8_t instrument;
    uint8_t volume;
} INSTRUMENT_DATA;

typedef struct 
{
    uint8_t low;
    uint8_t high;
} PRO_FREQUENCY;

typedef enum
{
    SLIDE_TYPE_OFF,
    SLIDE_TYPE_UP,
    SLIDE_TYPE_DOWN
} SLIDE_TYPE;

typedef struct
{
    SLIDE_TYPE type;
    uint16_t   value;
    uint8_t    remainder;
} SLIDE_DATA;

typedef struct 
{
    uint8_t counter;
    uint8_t value;
} FLIP_DATA;

typedef struct
{
    INSTRUMENT_DATA instrument_data;
    SLIDE_DATA      slide_data;
    FLIP_DATA       flip_data;

    PRO_FREQUENCY   frequency;
    uint8_t         note;
    uint8_t         pitch_value;

} CHANNEL_DATA;

static const uint8_t g_voice_patches[NR_OF_VOICE_PATCHES][INSTRUMENT_LENGTH] =
{
    {0x61, 0x61, 0x54, 0x27, 0xF5, 0x7C, 0x14, 0x17},
    {0x02, 0x41, 0x1C, 0x25, 0xB3, 0xC3, 0xAA, 0xAA},
    {0x01, 0x01, 0x4F, 0x24, 0xB3, 0xD3, 0x7C, 0x77},
    {0x31, 0x71, 0x20, 0x21, 0x51, 0x51, 0x16, 0x16},
    {0x22, 0x71, 0x56, 0x21, 0x78, 0x63, 0x06, 0x06},
    {0x31, 0x34, 0x18, 0x25, 0xF2, 0x62, 0x15, 0x15},
    {0x71, 0x31, 0x1E, 0x27, 0xA1, 0xD1, 0x16, 0x06},
    {0x23, 0x21, 0x9C, 0x23, 0xDC, 0x70, 0x28, 0x26},
    {0x61, 0x31, 0x18, 0x26, 0x65, 0xD2, 0x15, 0x05},
    {0x61, 0x61, 0x0E, 0x24, 0x75, 0x94, 0x36, 0x16},
    {0x13, 0x01, 0xC0, 0xA0, 0xB0, 0xB3, 0xBA, 0xCA},
    {0x49, 0xC1, 0x67, 0x23, 0x93, 0xD2, 0xBB, 0xDB},
    {0x40, 0x44, 0x4C, 0x65, 0xB3, 0x93, 0xA9, 0xA9},
    {0x11, 0x01, 0x18, 0x22, 0x62, 0x71, 0x91, 0xA6},
    {0x51, 0x01, 0x16, 0x26, 0xB1, 0xB2, 0x96, 0x96}
};

static const uint8_t g_drum_patches[NR_OF_DRUM_PATCHES][INSTRUMENT_LENGTH + 1] =
{
    {0x00, 0x00, 0xC2, 0x00, 0xA3, 0xA3, 0x07, 0x07, PAN_SETTING_MID},
    {0x01, 0x00, 0x00, 0x00, 0xF8, 0x97, 0x88, 0x86, PAN_SETTING_MID},
    {0x04, 0x81, 0xC0, 0xC7, 0xC7, 0xF6, 0xB7, 0x88, PAN_SETTING_MID}
 };

static const uint8_t g_voice_registers[NR_OF_FM_CHANNELS][INSTRUMENT_LENGTH + 1] =
{
    {0x20, 0x23, 0x40, 0x43, 0x60, 0x63, 0x80, 0x83, 0xC0},
    {0x21, 0x24, 0x41, 0x44, 0x61, 0x64, 0x81, 0x84, 0xC1},
    {0x22, 0x25, 0x42, 0x45, 0x62, 0x65, 0x82, 0x85, 0xC2},
    {0x28, 0x2B, 0x48, 0x4B, 0x68, 0x6B, 0x88, 0x8B, 0xC3},
    {0x29, 0x2C, 0x49, 0x4C, 0x69, 0x6C, 0x89, 0x8C, 0xC4},
    {0x2A, 0x2D, 0x4A, 0x4D, 0x6A, 0x6D, 0x8A, 0x8D, 0xC5}
};

static const uint8_t g_drum_registers[NR_OF_DRUM_CHANNELS][INSTRUMENT_LENGTH + 1] =
{
   {0x30, 0x33, 0x50, 0x53, 0x70, 0x73, 0x90, 0x93, 0xC6},
   {0x31, 0x34, 0x51, 0x54, 0x71, 0x74, 0x91, 0x94, 0xC7},
   {0x32, 0x35, 0x52, 0x55, 0x72, 0x75, 0x92, 0x95, 0xC8}
};

static const uint8_t g_volume_table[] =
{
    63,63-16,63-18,63-20,63-22,63-24,63-26,63-28,
    63-33,63-37,63-40,63-45,63-50,63-55,63-60,63-63
};

static const PRO_FREQUENCY g_frequency_table[] =
{
    {0xAD,0},  {0xB7,0},  {0xC2,0},  {0xCD,0},  {0xD9,0},  {0xE6,0},
    {0xF4,0},  {0x03,1},  {0x12,1},  {0x22,1},  {0x34,1},  {0x46,1},
    {0xAD,2},  {0xB7,2},  {0xC2,2},  {0xCD,2},  {0xD9,2},  {0xE6,2},
    {0xF4,2},  {0x03,3},  {0x12,3},  {0x22,3},  {0x34,3},  {0x46,3},
    {0xAD,4},  {0xB7,4},  {0xC2,4},  {0xCD,4},  {0xD9,4},  {0xE6,4},
    {0xF4,4},  {0x03,5},  {0x12,5},  {0x22,5},  {0x34,5},  {0x46,5},
    {0xAD,6},  {0xB7,6},  {0xC2,6},  {0xCD,6},  {0xD9,6},  {0xE6,6},
    {0xF4,6},  {0x03,7},  {0x12,7},  {0x22,7},  {0x34,7},  {0x46,7},
    {0xAD,8},  {0xB7,8},  {0xC2,8},  {0xCD,8},  {0xD9,8},  {0xE6,8},
    {0xF4,8},  {0x03,9},  {0x12,9},  {0x22,9},  {0x34,9},  {0x46,9},
    {0xAD,10}, {0xB7,10}, {0xC2,10}, {0xCD,10}, {0xD9,10}, {0xE6,10},
    {0xF4,10}, {0x03,11}, {0x12,11}, {0x22,11}, {0x34,11}, {0x46,11},
    {0xAD,12}, {0xB7,12}, {0xC2,12}, {0xCD,12}, {0xD9,12}, {0xE6,12},
    {0xF4,12}, {0x03,13}, {0x12,13}, {0x22,13}, {0x34,13}, {0x46,13},
    {0xAD,14}, {0xB7,14}, {0xC2,14}, {0xCD,14}, {0xD9,14}, {0xE6,14},
    {0xF4,14}, {0x03,15}, {0x12,15}, {0x22,15}, {0x34,15}, {0x46,15}
};

static const uint8_t g_drums_table[] =
{
    0b00110000,0b00101000,0b00100100,0b00100010,0b00100001,
    0b00111000,0b00110100,0b00110010,0b00110001,
    0b00101100,0b00101010,0b00101001,
    0b00100110,0b00100101,0b00111100
};

static PRO_HEADER g_pro_header;

static char g_song_name[SONG_NAME_LENGTH + 1];
static char g_author[AUTHOR_LENGTH + 1];

static uint8_t g_pattern_list[MAX_SONG_LENGTH];

static uint8_t g_speed;
static uint8_t g_speed_count;

static uint8_t *g_pattern_data;
static uint8_t g_pattern_index;
static uint8_t g_line_count;

static uint8_t g_transpose;

static bool    g_toggle;
static uint8_t g_count;
static uint8_t g_line_buffer[LINE_SIZE];

static uint8_t g_original_instrument[INSTRUMENT_LENGTH];
static uint8_t g_original_instrument_audio[INSTRUMENT_LENGTH + 1];

static CHANNEL_DATA g_channel_data[NR_OF_FM_CHANNELS];

bool next_pattern(void);
void unpack_line(void);

void set_speed(uint8_t value);

void handle_slide(void);

void update_registers(void);
void activate_instrument(uint8_t channel);

void set_instrument_data(uint8_t channel, uint8_t instrument_number);
void set_original_audio_instrument_data(void);
void set_original_audio_instrument_register(uint8_t index, uint8_t value);

void set_drum_data(uint8_t volume);
void set_drum_frequencies(void);

void play_data(uint8_t channel, uint8_t data);
void play_drums(uint8_t data);

void note_on(uint8_t channel, uint8_t note);
void note_off(uint8_t channel);
void change_instrument(uint8_t channel, uint8_t value);
void change_volume(uint8_t channel, uint8_t value);
void slide_up(uint8_t channel, uint8_t value);
void slide_down(uint8_t channel, uint8_t value);
void flip(uint8_t channel, uint8_t value);
void pitch(uint8_t channel, uint8_t value);
