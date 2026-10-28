/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * d00.h
 *
 * D00: EdLib packed module player
 */

#pragma once

#include <stdint.h>

#define NR_OF_CHANNELS 9

#define NO_SPFX        0xffff
#define NO_LEVEL_PULSE 0xff

#define HIBYTE(val) (val >> 8)
#define LOBYTE(val) (val & 0xff)

typedef struct 
{
    char id[6];
    uint8_t type;

    uint8_t version;
    uint8_t speed;
    int8_t  subsongs;

    uint8_t soundcard;

    char    songname[32];
    char    author[32];
    char    dummy[32];

    uint16_t arrangement_data;
    uint16_t sequence_data;
    uint16_t instrument_data;
    uint16_t song_description;
    uint16_t spfx_data;
    uint16_t endmark;
} D00_HEADER;

typedef struct
{
    uint8_t version;
    uint8_t speed;
    int8_t  subsongs;

    uint16_t arrangement_data;
    uint16_t sequence_data;
    uint16_t instrument_data;
    uint16_t song_description;
    uint16_t lpulptr;
    uint16_t endmark;   
} D00_HEADER_V1;

typedef struct
{
    bool      key, sequence_end;
    uint16_t *order_list, order_pos, pattern_pos;
    uint16_t  del, speed, rhcount, irhcount;
    uint16_t  freq, inst;
    uint16_t  spfx, ispfx;
    int16_t   transpose, slide, slideval, vibspeed;
    uint8_t   vol, vibdepth, fxdel, modvol, cvol, levpuls, frameskip, nextnote, note, ilevpuls, trigger, fxflag;
} CHANNEL_DATA;

typedef struct 
{
    uint8_t data[11];
    uint8_t tunelev;
    uint8_t timer;
    uint8_t sr;
    uint8_t dummy[2];
} INSTRUMENT_DATA;

typedef struct
{
    uint16_t instnr;
    int8_t   halfnote;
    uint8_t  modlev;
    int8_t   modlevadd;
    uint8_t  duration;
    uint16_t ptr;
} SPFX_DATA;

typedef struct
{
    uint8_t level;
    int8_t  voladd;
    uint8_t duration;
    uint8_t ptr;
} LEVEL_PULS_DATA;

typedef struct
{
    uint16_t ptr[NR_OF_CHANNELS];
    uint8_t  volume[NR_OF_CHANNELS];
    uint8_t  dummy[5];  
} ARRANGEMENT;

static const unsigned char op_table[9] =
    {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};

static const uint16_t note_table[12] =
    {340,363,385,408,432,458,485,514,544,577,611,647};

CHANNEL_DATA     g_channel_info[NR_OF_CHANNELS];
INSTRUMENT_DATA *g_inst;
SPFX_DATA       *g_spfx;
LEVEL_PULS_DATA *g_level_pulse;

D00_HEADER    *g_header;
D00_HEADER_V1 *g_header_v1;

uint8_t *g_song_data;
uint8_t *g_song_description;

uint16_t* g_sequence_data;

uint8_t  g_version;
uint8_t  g_current_subsong;

bool g_song_end;

void update_fx(void);

void handle_note_event(const uint8_t channel, uint8_t note, uint8_t count);
bool handle_fx_event(const uint8_t channel, uint8_t fx, uint16_t fxop);

void set_volume(const uint8_t channel);
void set_modvolume(const uint8_t channel);
void set_frequency(const uint8_t channel);
void set_instrument(const uint8_t channel);
void play_note(const uint8_t channel);
void vibrato(const uint8_t channel);
