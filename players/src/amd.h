/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * amd.h
 *
 * AMD: AMUSiC Adlib Tracker player
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define NAME_SIZE 24
#define INSTRUMENT_NAME_SIZE 23
#define INSTRUMENT_DATA_SIZE 11
#define ID_SIZE 9
#define EVENT_SIZE 3

#define NUMBER_OF_CHANNELS    9
#define NUMBER_OF_INSTRUMENTS 26
#define NUMBER_OF_PATTERNS    128

#define NORMAL_MODULE 0x10
#define PACKED_MODULE 0x11

#define PATTERNS_PER_SEGMENT  9

#define MAX_PATTERN_NUMBER    127
#define MAX_PATTERN_LINE      63

#define MAX_VOLUME            63

#define DEFAULT_REFRESH     50.0
#define DEFAULT_SPEED       6

#define FREQ_START 0x156
#define FREQ_END   0x2ae
#define FREQ_RANGE (FREQ_END - FREQ_START)

#define CMD_ARPEGGIO        0x00
#define CMD_TONE_SLIDE_UP   0x01
#define CMD_TONE_SLIDE_DOWN 0x02
#define CMD_INTENSITY       0x03
#define CMD_SET_VOL         0x04
#define CMD_JUMP_TO_PATTERN 0x05
#define CMD_PATTERN_BREAK   0x06
#define CMD_SET_SPEED       0x07
#define CMD_PORTAMENTO      0x08
#define CMD_EXTENDED        0x09

#define CMD_EXT_CELL_TREMOLO        0x00
#define CMD_EXT_CELL_VIBRATO        0x01
#define CMD_EXT_SLIDE_VOL_UP_FAST   0x02
#define CMD_EXT_SLIDE_VOL_DOWN_FAST 0x03
#define CMD_EXT_SLIDE_VOL_UP_FINE   0x04
#define CMD_EXT_SLIDE_VOL_DOWN_FINE 0x05

#define PAN_SETTING_LEFT  0x10
#define PAN_SETTING_MID   0x30
#define PAN_SETTING_RIGHT 0x20

typedef struct
{
  char    name[INSTRUMENT_NAME_SIZE];
  uint8_t data[INSTRUMENT_DATA_SIZE];
} AMD_INSTRUMENT;

typedef struct
{
  char title[NAME_SIZE];
  char author[NAME_SIZE];

  AMD_INSTRUMENT instruments[NUMBER_OF_INSTRUMENTS];
  
  uint8_t length;
  uint8_t max_pattern_number;
  uint8_t patterns[NUMBER_OF_PATTERNS];

  uint8_t id[ID_SIZE];
  uint8_t version;              // 0x10 = normal module, 0x11 = packed module
} AMD_HEADER;

typedef struct 
{
  uint8_t data_1;
  uint8_t data_2;
  uint8_t data_3;
} AMD_EVENT;

typedef struct
{
  AMD_EVENT lines[MAX_PATTERN_LINE + 1][NUMBER_OF_CHANNELS];
} AMD_PATTERN;

typedef struct
{
  bool    isAllocated;
  uint8_t allocated_segment;
} DATA_SEGMENT;

typedef struct
{
  uint8_t pitch;
  uint8_t octave;
  uint8_t instrument;
  uint8_t command;
  uint8_t parameters;
} AMD_CURRENT_EVENT;

typedef struct
{
  uint8_t instrument;
  uint8_t volume;
  uint8_t pitch;
  uint8_t octave;

  int16_t frequency;
  int16_t portamento_frequency;

  uint8_t arpeggio;

  uint8_t command;
  uint8_t parameters;

  uint8_t carrier_volume;
  uint8_t modulator_volume;
} AMD_LAST_EVENT;

const uint16_t g_note_frequencies[] = 
{
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647
};

const uint8_t g_channel_offsets[] =
{
  0x20, 0x21, 0x22, 0x28, 0x29, 0x2a, 0x30, 0x31, 0x32
};

const uint8_t g_volume_table[] =
{
  0/31,    95/31,   182/31,  262/31,  336/31,  405/31,  470/31,  530/31,  587/31,  641/31,
	693/31,  741/31,  788/31,  832/31,  875/31,  916/31,  955/31,  993/31,  1029/31, 1064/31, 
  1098/31, 1131/31, 1163/31, 1193/31, 1223/31, 1252/31, 1280/31, 1308/31, 1335/31, 1360/31, 
  1386/31, 1410/31, 1435/31, 1458/31, 1481/31, 1504/31, 1526/31, 1547/31, 1568/31, 1589/31, 
  1609/31, 1629/31, 1648/31, 1667/31, 1686/31, 1704/31, 1722/31, 1740/31, 1757/31, 1774/31, 
  1791/31, 1808/31, 1824/31, 1840/31,	1856/31, 1871/31, 1887/31, 1902/31, 1916/31, 1931/31, 
  1945/31, 1960/31, 1974/31, 1980/31
};

char g_song_name[NAME_SIZE + 1];
char g_author[NAME_SIZE + 1];

AMD_HEADER g_amd_header;
uint16_t   g_track_order[NUMBER_OF_PATTERNS][NUMBER_OF_CHANNELS];
uint16_t   g_number_of_tracks;

DATA_SEGMENT g_segments[MAX_PATTERN_NUMBER + 1];

float   g_refresh;
uint8_t g_speed;
uint8_t g_delay;

uint8_t g_position;
int8_t  g_row;

bool    g_break_state;
uint8_t g_break_line;
uint8_t g_break_pattern;

uint8_t g_regbd;

AMD_CURRENT_EVENT g_current_event;
AMD_LAST_EVENT    g_last_events[NUMBER_OF_CHANNELS];

void channel_next_event(const uint8_t channel);
void channel_update(const uint8_t channel);
void channel_set_volume(const uint8_t channel);
void channel_play_note(const uint8_t channel);

void handle_fx(void);
void arpeggio(const uint8_t channel);
void tone_portamento(const uint8_t channel);

void init_arpeggio(const uint8_t channel);
void slide_frequency_up(const uint8_t channel);
void slide_frequency_down(const uint8_t channel);
void set_instrument_volume(const uint8_t channel);
void jump_pattern(void);
void pattern_break(void);
void set_song_speed(void);
void init_tone_portamento(const uint8_t channel);
void set_carrier_modulator_volume(const uint8_t channel);
void extended_command(const uint8_t channel);
void slide_volume_up(const uint8_t channel);
void slide_volume_down(const uint8_t channel);

uint16_t get_frequency(const uint8_t pitch, const uint8_t octave);
void     set_frequency(const uint8_t channel, const int16_t frequency);
