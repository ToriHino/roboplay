/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * sa2.h
 *
 * SA2: Surprise! Adlib Tracker
 */

#pragma once

#define VERSION_LENGTH        23

#define ID_SIZE               4

#define NUMBER_OF_CHANNELS    9
#define NUMBER_OF_PATTERNS    128

#define NUMBER_OF_TRACKS      64
#define TRACK_LENGTH          64
#define TRACK_SIZE            (TRACK_LENGTH * sizeof(SAT_TRACK_LINE))

#define ARPEGGIO_LIST_SIZE    256

#define NUMBER_OF_INSTRUMENTS    31
#define INSTRUMENT_NAME_SIZE     16
#define INSTRUMENT_DATA_SIZE     11
#define INSTRUMENT_ARPEGGIO_SIZE 4

#define PAN_SETTING_LEFT  0x10
#define PAN_SETTING_MID   0x30
#define PAN_SETTING_RIGHT 0x20

#define SAT_CHORUS 0x01

#define FX_ARPEGGIO              0x00
#define FX_TONE_SLIDE_UP         0x01
#define FX_TONE_SLIDE_DOWN       0x02
#define FX_PORTAMENTO            0x03
#define FX_VIBRATO               0x04
#define FX_PORTAMENTO_VOL_SLIDE  0x05
#define FX_VIBRATO_VOL_SLIDE     0x06
#define FX_RELEASE_NOTE          0x08
#define FX_VOLUME_SLIDE          0x0A
#define FX_POSITION_JMP          0x0B
#define FX_SET_VOLUME            0x0C
#define FX_PATTERN_BREAK         0x0D
#define FX_SET_SPEED             0x0F

typedef enum
{
  HAS_ARPEGGIOLIST   = (1 << 7),
  HAS_V7PATTERNS     = (1 << 6),
  HAS_ACTIVECHANNELS = (1 << 5),
  HAS_TRACKORDER     = (1 << 4),
  HAS_ARPEGGIO       = (1 << 3),
  HAS_OLDBPM         = (1 << 2),
  HAS_OLDPATTERNS    = (1 << 1),
  HAS_UNKNOWN127     = (1 << 0)
} SAT_TYPE;

typedef struct
{
  uint8_t data[INSTRUMENT_DATA_SIZE];
  uint8_t arpstart;
  uint8_t arpspeed;
  uint8_t arppos;
  uint8_t arpspdcnt;
} SAT_INSTRUMENT;

typedef struct
{
  uint8_t  id[ID_SIZE];
  uint8_t  version;

  SAT_INSTRUMENT instruments[NUMBER_OF_INSTRUMENTS];
  
  uint8_t  pattern_order[NUMBER_OF_PATTERNS];
  uint16_t number_of_patterns_used;

  uint8_t  song_length;
  uint8_t  restart_position;
  uint16_t bpm;

  uint8_t  arpeggio_list[ARPEGGIO_LIST_SIZE];
  uint8_t  arpeggio_commands[ARPEGGIO_LIST_SIZE];
  
  uint8_t  track_order[NUMBER_OF_TRACKS][NUMBER_OF_CHANNELS];

  uint16_t active_channels;
} SAT_HEADER;

typedef struct
{
  uint8_t note;
  uint8_t inst;
  uint8_t command;
  uint8_t param1;
  uint8_t param2;
  uint8_t param;
} SAT_TRACK_LINE;

typedef struct
{
  uint8_t         segment;
  SAT_TRACK_LINE *data;
} SAT_TRACK_PTR;

typedef struct
{
  uint16_t freq;
  uint16_t next_freq;
  uint8_t  oct;
  uint8_t  next_oct;

  uint8_t  note;

  int8_t   vol1;
  int8_t   vol2;

  uint8_t  inst;
  uint8_t  fx;
  
  uint8_t  info1;
  uint8_t  info2;
  uint8_t  info;

  bool     key;

  uint8_t  portainfo;
  uint8_t  vibinfo1;
  uint8_t  vibinfo2;

  uint8_t  arppos;
  uint8_t  arpspdcnt;

  uint8_t  trigger;
} SAT_CHANNEL_DATA;

const uint8_t g_op_table[NUMBER_OF_CHANNELS] =
{
  0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12
};

const uint16_t g_note_table[8 * 12] = 
{
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
  343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
};

const uint8_t g_oct_table[8 * 12] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
};

const uint8_t g_vibrato_table[32] =
  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

uint8_t g_sat_type;
uint8_t g_note_dis;

char version[VERSION_LENGTH];

uint8_t g_tempo;
uint8_t g_speed;
uint8_t g_delay;
bool    g_song_end;

uint8_t g_pattern;
uint8_t g_row;

SAT_HEADER       g_sat_header;
SAT_CHANNEL_DATA g_channel_data[NUMBER_OF_CHANNELS];
SAT_TRACK_PTR    g_tracks[180];

void set_volume(uint8_t channel);
void set_frequency(uint8_t channel);

void set_note(uint8_t channel, uint8_t note);
void play_note(uint8_t channel);

void slide_down(uint8_t channel, uint8_t amount);
void slide_up(uint8_t channel, uint8_t amount);
void tone_portamento(uint8_t channel, uint8_t info);
void vibrato(uint8_t channel, uint8_t speed, uint8_t depth);
void vol_up(uint8_t channel, uint8_t amount);
void vol_down(uint8_t channel, uint8_t amount);
