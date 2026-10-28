/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * hsc.h
 *
 * HSC: HSC AdLib Composer
 */

#pragma once

#define NUMBER_OF_CHANNELS    9

#define NUMBER_OF_INSTRUMENTS 128
#define INSTRUMENT_SIZE       12

#define TRACKLIST_SIZE        51
#define PATTERN_LENGTH        64
#define PATTERN_SIZE          (PATTERN_LENGTH * NUMBER_OF_CHANNELS * sizeof(HSC_NOTE))

#define PAN_SETTING_LEFT  0x10
#define PAN_SETTING_MID   0x30
#define PAN_SETTING_RIGHT 0x20

typedef struct
{
  uint8_t note;
  uint8_t effect;
} HSC_NOTE;

typedef struct
{
  uint8_t  instrument;
  int8_t   slide;
  uint16_t frequency;
} HSC_CHANNEL;

typedef struct
{
  uint8_t   segment;
  HSC_NOTE *data;
} HSC_PATTERN;


HSC_CHANNEL g_channels[NUMBER_OF_CHANNELS];
uint8_t     g_instruments[NUMBER_OF_INSTRUMENTS][INSTRUMENT_SIZE];
uint8_t     g_tracklist[TRACKLIST_SIZE];

const uint16_t g_note_table[12] =
{ 
  363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647, 686
};

const unsigned char g_op_table[NUMBER_OF_CHANNELS] =
{
  0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12
};

HSC_PATTERN g_patterns[TRACKLIST_SIZE];

uint8_t g_pattern_position;
uint8_t g_song_position;
bool    g_pattern_break;
bool    g_song_end;
bool    g_6_voice_mode;
uint8_t g_bd;
uint8_t g_fade_in;

uint8_t g_speed;
uint8_t g_delay;

uint8_t g_frequencies[NUMBER_OF_CHANNELS];

void handle_event(uint8_t channel, uint8_t note, uint8_t effect);
void play_note(uint8_t channel, uint8_t note);

void set_frequency(uint8_t channel, uint16_t frequency);
void set_volume(uint8_t channel, uint8_t volume_carrier, uint8_t volume_modulator);
void set_instrument(uint8_t channel, uint8_t instrument);
