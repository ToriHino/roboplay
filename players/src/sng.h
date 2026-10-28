/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * sng.h
 *
 * SNG: SCC Musixx
 */

#pragma once

#define SNG_PATTERN_SIZE 0x600
#define SNG_MAX_PATTERNS 20

#define SNG_MAX_NR_OF_POSITIONS 100

#define SNG_DEFAULT_SPEED 8

#define SNG_NR_OF_CHANNELS 5
#define SNG_NR_OF_ROWS     64

#define SNG_NUMBER_OF_INSTRUMENTS 48
#define SNG_WAVE_SIZE 32
#define SNG_INSTRUMENT_NAME_LENGTH 8

#define SNG_MAX_VOLUME 15

#define SNG_MODULATE_COUNT 3

#define SNG_COMMAND_CHANGE_SPEED   0x0F
#define SNG_COMMAND_BYTE           0x0E
#define SNG_COMMAND_CAPS           0x0C
#define SNG_COMMAND_GOTO           0x0B
#define SNG_COMMAND_SKIP_ROWS      0x0A
#define SNG_COMMAND_REGISTER       0x09
#define SNG_COMMAND_TRANSPOSE_DOWN 0x07
#define SNG_COMMAND_TRANSPOSE_UP   0x06
#define SNG_COMMAND_VOLUME_SLIDE   0x05
#define SNG_COMMAND_MODULATE_DOWN  0x04
#define SNG_COMMAND_MODULATE_UP    0x03
#define SNG_COMMAND_SLIDE_DOWN     0x02
#define SNG_COMMAND_SLIDE_UP       0x01

typedef struct
{
    uint8_t wave[SNG_WAVE_SIZE];
    char    name[SNG_INSTRUMENT_NAME_LENGTH];
} SNG_INSTRUMENT;

typedef struct
{
    SNG_INSTRUMENT   instruments[SNG_NUMBER_OF_INSTRUMENTS];
    uint8_t          song_length;
    uint8_t          patterns[SNG_MAX_NR_OF_POSITIONS];
} SNG_HEADER;

typedef struct
{
    uint16_t frequency;
    uint8_t  instrument;
    uint8_t  volume;        /* Bit 7..4 = volume, Bit 3..0 = command */
    uint8_t  value;
} SNG_CHANNEL;

typedef struct
{
    uint16_t frequency;
    uint8_t  volume;        /* Bit 7..4 = volume, Bit 3..0 = command */
    uint8_t  value;   
} SNG_CHANNEL_5;            /* Channel 5 has no instrument setting */

typedef struct
{
    SNG_CHANNEL   channels[SNG_NR_OF_CHANNELS - 1];
    SNG_CHANNEL_5 channel_5;
} SNG_ROW;

typedef struct
{
    uint8_t  segment;
    SNG_ROW* data;
} SNG_PATTERN_DATA;

typedef enum
{
    SNG_DIRECTION_DOWN = 0,
    SNG_DIRECTION_UP   = 1
} SNG_DIRECTION;

typedef struct
{
    SNG_DIRECTION direction;
    uint8_t       value;    
} SNG_TRANSPOSE;

typedef struct 
{
    SNG_DIRECTION direction;
    uint8_t       value;
} SNG_MODULATE;

typedef struct
{
    SNG_DIRECTION direction; 
    uint8_t       value;
    uint8_t       rest;
} SNG_SLIDE;

typedef struct
{
    uint8_t  instrument;
    uint8_t  volume;

    uint16_t frequency_work;
    uint16_t frequency_original;

    SNG_MODULATE modulate_work;
    SNG_MODULATE modulate_original;

    SNG_SLIDE volume_slide;
    SNG_SLIDE frequency_slide;

    SNG_TRANSPOSE transpose;

    uint8_t command;
    uint8_t value;
} SNG_CHANNEL_DATA;

SNG_HEADER       *g_header;
SNG_PATTERN_DATA  g_pattern_data[SNG_MAX_PATTERNS];

uint8_t g_speed;
uint8_t g_speed_count;

uint8_t g_pattern_line;
uint8_t g_position;
uint8_t g_pattern_number;

uint8_t g_modulate_count;

SNG_CHANNEL_DATA g_channel_data[SNG_NR_OF_CHANNELS];

void do_slide(uint8_t channel);
void do_volume_slide(uint8_t channel);

void next_pattern(void);
void play_note(void);

bool handle_command(void);
void handle_transpose(uint8_t channel, SNG_DIRECTION direction, uint8_t value);
void handle_volume_slide(uint8_t channel, uint8_t value);
void handle_modulate(uint8_t channel, SNG_DIRECTION direction, uint8_t value);
void handle_slide(uint8_t channel, SNG_DIRECTION direction, uint8_t value);
