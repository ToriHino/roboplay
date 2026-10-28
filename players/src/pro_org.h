/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * pro_prg.h
 *
 * PRO: Pro-Tracker player on original device
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

const uint8_t g_music_freq_registers[] =
{
  0x10, 0x20, 0x11, 0x21, 0x12, 0x22,
  0x13, 0x23, 0x14, 0x24, 0x15, 0x25,
  0x16, 0x26, 0x17, 0x27, 0x18, 0x28,
};

const PRO_FREQUENCY g_frequency_table[] =
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

const uint8_t g_drums_table[] =
{
    0b00110000,0b00101000,0b00100100,0b00100010,0b00100001,
    0b00111000,0b00110100,0b00110010,0b00110001,
    0b00101100,0b00101010,0b00101001,
    0b00100110,0b00100101,0b00111100
};

PRO_HEADER g_pro_header;

char g_song_name[SONG_NAME_LENGTH + 1];
char g_author[AUTHOR_LENGTH + 1];

uint8_t g_pattern_list[MAX_SONG_LENGTH];

uint8_t g_speed;
uint8_t g_speed_count;

uint8_t *g_pattern_data;
uint8_t g_pattern_index;
uint8_t g_line_count;

uint8_t g_transpose;

bool    g_toggle;
uint8_t g_count;
uint8_t g_line_buffer[LINE_SIZE];

uint8_t g_original_instrument[INSTRUMENT_LENGTH];

CHANNEL_DATA g_channel_data[NR_OF_FM_CHANNELS];

bool next_pattern(void);
void unpack_line(void);

void set_speed(uint8_t value);

void handle_slide(void);

void update_registers(void);
void activate_instrument(uint8_t channel);

void set_instrument_data(uint8_t channel, uint8_t instrument_number);

void set_drum_data(uint8_t volume);
void set_drum_frequencies(void);

void play_data(uint8_t channel, uint8_t data);
void play_drums(uint8_t data);

void note_on(uint8_t channel, uint8_t note);
void note_off(uint8_t channel);
void release(uint8_t channel);
void change_instrument(uint8_t channel, uint8_t value);
void change_volume(uint8_t channel, uint8_t value);
void slide_up(uint8_t channel, uint8_t value);
void slide_down(uint8_t channel, uint8_t value);
void flip(uint8_t channel, uint8_t value);
void pitch(uint8_t channel, uint8_t value);
