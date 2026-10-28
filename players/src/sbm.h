/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * sbm.h
 *
 * SBM: SCC Blaffer NT
 */

#pragma once

#include <stdint.h>

#define SBM_SONG_NAME_LENGTH 67

#define SBM_PATTERN_SIZE   192
#define SBM_NR_OF_PATTERNS 256
#define SBM_NR_OF_ROWS     16

#define SBM_NR_OF_SCC_CHANNELS 5
#define SBM_NR_OF_PSG_CHANNELS 3

#define SBM_MAX_VOLUME     15
#define SBM_MAX_TEMPO      26

#define SBM_PSG_MAX_VOLUME 15
#define SBM_PSG_MAX_TEMPO  15
#define SBM_PSG_LAST_ROW   16

#define SBM_PSG            10
#define SBM_COMMAND        11

#define SBK_SCC_INSTRUMENT_DATA_SIZE 32
#define SBK_PSG_INSTRUMENT_DATA_SIZE 176

#define SBM_PATTERN_DATA 0x81C0

#define SBK_SCC_INSTRUMENTS 0x8000
#define SBK_PSG_INSTRUMENTS 0xA060

#define SBM_WAVEFORM_SIZE 32

#define SBM_COMMAND_CHANGE_VOLUME       0
#define SBM_COMMAND_CHANGE_FREQUENCY   97
#define SBM_COMMAND_NOTE_OFF           97
#define SBM_COMMAND_CHANGE_INSTRUMENT  98
#define SBM_COMMAND_PITCH_1_UP         99
#define SBM_COMMAND_PITCH_1_DOWN      100
#define SBM_COMMAND_PITCH_2_UP        101
#define SBM_COMMAND_PITCH_2_DOWN      102
#define SBM_COMMAND_MODULATE_UP       103
#define SBM_COMMAND_MODULATE_DOWN     104
#define SBM_COMMAND_DETUNE_UP         105
#define SBM_COMMAND_DETUNE_DOWN       106
#define SBM_COMMAND_VOLUME_SLIDE_UP   107
#define SBM_COMMAND_VOLUME_SLIDE_DOWN 108
#define SBM_COMMAND_AUTO_SLIDE_UP     109
#define SBM_COMMAND_AUTO_SLIDE_DOWN   110

#define PSG_SET_REPEAT  16
#define PSG_DO_REPEAT   16
#define PSG_WAIT        26
#define PSG_POINT       26
#define PSG_GOTO_POINT  27
#define PSG_END_PATTERN 28
#define PSG_NEW_TEMPO   45

#define PSG_NO_REPEAT   255    

#define PSG_FREQUENCY_OFF  0x1000
#define PSG_FREQUENCY_DOWN 0x2000
#define PSG_FREQUENCY_UP   0x4000

#define PSG_NOISE_CHANGE 0x40
#define PSG_VOLUME_UP    0x10
#define PSG_VOLUME_DOWN  0x20

#define PSG_NOISE_FREQUENCY_DOWN 0x20
#define PSG_NOISE_FREQUENCY_UP   0x40

typedef struct 
{
    char song_name[SBM_SONG_NAME_LENGTH];
    char instrument_kit_name[8];
    char instrument_kit_ext[3];

    uint8_t last_position;
    uint8_t loop;
    uint8_t loop_position;
    uint8_t patterns[SBM_NR_OF_PATTERNS];
    uint8_t tempo;
    uint8_t volumes[SBM_NR_OF_SCC_CHANNELS];
    uint8_t instruments[SBM_NR_OF_SCC_CHANNELS - 1];
    uint8_t detune[SBM_NR_OF_SCC_CHANNELS];
    uint8_t auto_volume_slide[SBM_NR_OF_SCC_CHANNELS];
    uint8_t future[91];
} SBM_HEADER;

typedef struct
{
    uint8_t command;
    uint8_t value;
} SCC_DATA;

typedef struct
{
    SCC_DATA scc_data[SBM_NR_OF_SCC_CHANNELS];
    uint8_t  psg_data;
    uint8_t  command;
} SBM_ROW;

typedef struct
{
    SBM_ROW row[SBM_NR_OF_ROWS];
} SBM_PATTERN;

typedef struct
{
    char    name[8];
    uint8_t data[SBK_SCC_INSTRUMENT_DATA_SIZE];
} SBK_SCC_INSTRUMENT;

typedef struct
{
    uint8_t  command;
    uint16_t frequency[SBM_NR_OF_PSG_CHANNELS];
    uint8_t  volume[SBM_NR_OF_PSG_CHANNELS];
    uint8_t  noise_frequency;
} SBK_PSG_DATA;

typedef struct
{
    char         name[8];
    SBK_PSG_DATA data[SBM_PSG_LAST_ROW];
} SBK_PSG_INSTRUMENT;

typedef enum
{
    SBM_DIRECTION_DOWN = 0,
    SBM_DIRECTION_UP   = 1
} SBM_DIRECTION;

typedef struct
{
    uint8_t       type;
    bool          active;
    SBM_DIRECTION direction;
    uint8_t       value;
} SBM_PITCH_DATA;

typedef struct 
{
    bool          active;   
    SBM_DIRECTION direction; 
    uint8_t       value;
} SBM_MODULATE_DATA;

typedef struct
{
    bool          active;   
    SBM_DIRECTION direction; 
    uint8_t       value;
    uint8_t       counter;
} SBM_VOLUME_SLIDE_DATA;

typedef struct
{
    bool     last_row_played;
    uint8_t  transpose;
    uint16_t last_frequencies[SBM_NR_OF_SCC_CHANNELS];
    uint8_t  last_volumes[SBM_NR_OF_SCC_CHANNELS];
    uint8_t  last_instruments[SBM_NR_OF_SCC_CHANNELS - 1];

    SBM_PITCH_DATA    pitch[SBM_NR_OF_SCC_CHANNELS];
    SBM_MODULATE_DATA modulate[SBM_NR_OF_SCC_CHANNELS];

    uint8_t  modulate_counter;

    SBM_VOLUME_SLIDE_DATA volume_slide[SBM_NR_OF_SCC_CHANNELS];

    uint8_t auto_volume_slide[SBM_NR_OF_SCC_CHANNELS];
} SBM_DATABLOCK;

static const uint16_t g_frequency_table[] =
    {
        3421, 3228, 3047, 2876, 2715, 2562, 2419, 2283, 2155, 2034, 1920, 1812,      /* C1 - B1 */
        1711, 1614, 1524, 1438, 1358, 1281, 1210, 1142, 1078, 1017,  960,  906,      /* C2 - B2 */
         855,  807,  762,  719,  679,  641,  605,  569,  539,  509,  480,  453,      /* C3 - B3 */
         428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  227,      /* C4 - B4 */
         214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,      /* C5 - B5 */
         107,  101,   95,   90,   85,   80,   76,   71,   67,   64,   60,   57,      /* C6 - B6 */
          53,   50,   48,   45,   42,   40,   38,   36,   34,   32,   30,   28,      /* C7 - B7 */
          27,   25,   24,   22,   21,   20,   19,   18,   17,   16,   15,   14       /* C8 - B8 */
    };

static SBM_HEADER *g_sbm_header;
static char g_song_name[SBM_SONG_NAME_LENGTH + 1];

static char g_instrument_kit_name[256];

static SBM_PATTERN        *g_song_patterns;
static SBK_SCC_INSTRUMENT *g_scc_instruments;
static SBK_PSG_INSTRUMENT *g_psg_instruments;

static uint8_t g_instrument_segment;

static SBM_DATABLOCK g_data_block;

static bool g_last_position_done;

static uint8_t  g_song_tempo;
static uint8_t  g_tempo_counter;

static uint8_t  g_detune[SBM_NR_OF_SCC_CHANNELS];

static uint8_t  g_song_position;
static uint8_t  g_song_row;
static uint16_t g_song_frequencies[SBM_NR_OF_SCC_CHANNELS];
static uint8_t  g_song_volumes[SBM_NR_OF_SCC_CHANNELS];

static bool     g_psg_active;
static uint8_t  g_psg_instrument;
static uint8_t  g_psg_row;

static uint8_t  g_psg_tempo;
static uint8_t  g_psg_tempo_count;
static uint8_t  g_psg_wait;

static uint8_t  g_psg_number_of_repeats;
static uint8_t  g_psg_repeat_row;

static uint8_t  g_psg_point;

static uint8_t  g_psg_max_volume;
static uint8_t  g_psg_volume[SBM_NR_OF_PSG_CHANNELS];

bool new_position(void);
void end_of_pattern(void);
void do_command_channel(void);
void do_auto_slides(void);
void play_row(void);
void do_volume_slides(void);
void do_pitch_slides(void);
void do_modulate(void);
void do_psg(void);

void change_tempo(uint8_t tempo);
void change_transpose(uint8_t transpose);
void change_volume(uint8_t channel, uint8_t volume);
void change_frequency(uint8_t channel, uint8_t note, uint8_t value);
void note_off(uint8_t channel);
void change_instrument(uint8_t channel, uint8_t instrument);

void pitch_1_up(uint8_t channel, uint8_t value);
void pitch_1_down(uint8_t channel, uint8_t value);
void pitch_2_up(uint8_t channel, uint8_t value);
void pitch_2_down(uint8_t channel, uint8_t value);
void modulate_up(uint8_t channel, uint8_t value);
void modulate_down(uint8_t channel, uint8_t value);
void detune_up(uint8_t channel, uint8_t value);
void detune_down(uint8_t channel, uint8_t value);
void volume_slide_up(uint8_t channel, uint8_t value);
void volume_slide_down(uint8_t channel, uint8_t value);
void auto_slide_up(uint8_t channel, uint8_t value);
void auto_slide_down(uint8_t channel, uint8_t value);
