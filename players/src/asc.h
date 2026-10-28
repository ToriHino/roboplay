/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * asc.h
 *
 * ASC: ASC Sound Master (ZX Spectrum, AY-3-8910 / YM2149) player
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_CHANNEL_NUMBER   3
#define MAX_SAMPLES          32
#define MAX_ORNAMENTS        32
#define MAX_SAMPLE_LINES     150
#define MAX_ORNAMENT_LINES   30
#define MAX_PATTERNS         32
#define MAX_POSITIONS        100
#define MAX_PATTERN_LINES    64

/* Layout of the optional "ASM COMPILATION OF <title> BY <author>" block that follows the position list */
#define ASC_ID_LENGTH        19
#define ASC_TITLE_LENGTH     20
#define ASC_BY_LENGTH        4
#define ASC_AUTHOR_LENGTH    20
#define ASC_ID_BLOCK_SIZE    (ASC_ID_LENGTH + ASC_TITLE_LENGTH + ASC_BY_LENGTH + ASC_AUTHOR_LENGTH)

#define ASC_SAMPLE_LINE_SIZE   3
#define ASC_ORNAMENT_LINE_SIZE 2
#define ASC_LIST_SIZE          64   /* 32 words */

#define MIN_TEMPO 0x03
#define MAX_TEMPO 0x32

/* Flags in the first byte of a sample / ornament line */
#define LINE_LOOP_BEGIN 0x80
#define LINE_LOOP_END   0x40
#define LINE_FINISHED   0x20

/* Pattern stream commands */
#define CMD_NOTE_LAST       0x55
#define CMD_STOP            0x56    /* 0x56 - 0x5D: empty cell */
#define CMD_STOP_LAST       0x5D
#define CMD_BREAK_SAMPLE    0x5E
#define CMD_REST            0x5F
#define CMD_SKIP_FIRST      0x60    /* 0x60 - 0x9F: lines to wait after this cell */
#define CMD_SKIP_LAST       0x9F
#define CMD_SAMPLE_FIRST    0xA0    /* 0xA0 - 0xBF */
#define CMD_SAMPLE_LAST     0xBF
#define CMD_ORNAMENT_FIRST  0xC0    /* 0xC0 - 0xDF */
#define CMD_ORNAMENT_LAST   0xDF
#define CMD_ENVELOPE_VOL    0xE0    /* 0xE0: full volume + envelope, 0xE1 - 0xEF: volume without envelope */
#define CMD_NOISE           0xF0
#define CMD_CONTINUE_LAST   0xF3    /* 0xF1 - 0xF3: continue sample (bit 0) and / or ornament (bit 1) */
#define CMD_TEMPO           0xF4
#define CMD_GLISSADE_DOWN   0xF5
#define CMD_GLISSADE_UP     0xF6
#define CMD_SLIDE_TONE      0xF7    /* Stepped slide that continues the sample */
#define CMD_SLIDE           0xF9
#define CMD_AMP_DELAY       0xFB
#define CMD_END_OF_PATTERN  0xFF    /* On channel A: end of the pattern */

/* Sample line commands */
#define SAMPLE_CMD_NONE     0
#define SAMPLE_CMD_ENVELOPE 1
#define SAMPLE_CMD_DEC_VOL  2
#define SAMPLE_CMD_INC_VOL  3

#define NO_TARGET_NOTE 0xFF

#define PSG_MIXER_PORT_BITS 0x80    /* On MSX: bit 7 = port B output, bit 6 = port A input. Never change these! */
#define PSG_VOLUME_ENVELOPE 0x10

/* ---- Data structures ---- */

typedef struct
{
    uint16_t offset;    /* Offset of the first line in the file, 0 = not present */
    uint8_t  size;      /* Number of lines, including the one flagged as finished */
    uint8_t  loop;
    uint8_t  limit;     /* Loop end */
} ASC_LINES;

/* State of the pattern stream parser of one channel */
typedef struct
{
    uint16_t offset;
    uint8_t  period;    /* Lines to wait after every cell */
    uint8_t  counter;
    bool     envelope;  /* Notes are followed by an envelope tone byte */
} ASC_PARSE;

/* One decoded pattern cell */
typedef struct
{
    bool has_note;
    bool enable;
    bool disable;
    bool has_sample;
    bool has_ornament;
    bool has_volume;
    bool has_noise;
    bool cont_sample;
    bool cont_ornament;
    bool break_sample;
    bool env_on;
    bool env_off;
    bool has_env_type;
    bool has_env_tone;
    bool has_gliss;
    bool has_slide;
    bool slide_to_note;
    bool slide_tone;
    bool has_vol_slide;

    uint8_t note;
    uint8_t sample;
    uint8_t ornament;
    uint8_t volume;
    uint8_t noise;
    uint8_t env_type;
    uint8_t env_tone;
    uint8_t slide_target;
    uint8_t vol_slide_period;

    int8_t  vol_slide_delta;
    int8_t  slide_steps;
    int16_t gliss;
} ASC_CELL;

/* The sample and ornament line that is current for a channel */
typedef struct
{
    int8_t  adding;
    int8_t  deviation;
    uint8_t level;
    uint8_t command;
    bool    tone_mask;
    bool    noise_mask;

    int8_t  note_offset;
    int8_t  noise_offset;
} ASC_TICK;

typedef struct
{
    bool    enabled;
    bool    envelope;
    bool    break_sample;

    uint8_t volume;
    int8_t  volume_addon;
    uint8_t vol_slide_delay;
    int8_t  vol_slide_addon;
    uint8_t vol_slide_counter;

    int16_t base_noise;
    int16_t current_noise;

    uint8_t note;
    int8_t  note_addon;

    uint8_t sample_num;
    uint8_t current_sample;
    uint8_t pos_in_sample;

    uint8_t ornament_num;
    uint8_t current_ornament;
    uint8_t pos_in_ornament;

    int16_t tone_deviation;

    int8_t  sliding_steps;      /* < 0: endless */
    int32_t sliding;            /* 1/16 of a tone period unit, needs 32 bits for long slides */
    int32_t glissade;
    uint8_t sliding_target;
} ASC_CHANNEL;

/* ---- Constant tables ---- */

/* ASC Sound Master note table (96 entries, 0x56 are reachable) */
const uint16_t g_note_periods[96] =
{
    0xEDC, 0xE07, 0xD3E, 0xC80, 0xBCC, 0xB22, 0xA82, 0x9EC, 0x95C, 0x8D6, 0x858, 0x7E0,
    0x76E, 0x704, 0x69F, 0x640, 0x5E6, 0x591, 0x541, 0x4F6, 0x4AE, 0x46B, 0x42C, 0x3F0,
    0x3B7, 0x382, 0x34F, 0x320, 0x2F3, 0x2C8, 0x2A1, 0x27B, 0x257, 0x236, 0x216, 0x1F8,
    0x1DC, 0x1C1, 0x1A8, 0x190, 0x179, 0x164, 0x150, 0x13D, 0x12C, 0x11B, 0x10B, 0x0FC,
    0x0EE, 0x0E0, 0x0D4, 0x0C8, 0x0BD, 0x0B2, 0x0A8, 0x09F, 0x096, 0x08D, 0x085, 0x07E,
    0x077, 0x070, 0x06A, 0x064, 0x05E, 0x059, 0x054, 0x050, 0x04B, 0x047, 0x043, 0x03F,
    0x03C, 0x038, 0x035, 0x032, 0x02F, 0x02D, 0x02A, 0x028, 0x026, 0x024, 0x022, 0x020,
    0x01E, 0x01C, 0x01A, 0x019, 0x017, 0x016, 0x015, 0x014, 0x013, 0x012, 0x011, 0x010
};

const char g_asc_id[ASC_ID_LENGTH + 1] = "ASM COMPILATION OF ";

/* ---- Module data ---- */

uint8_t *g_data;
uint16_t g_file_size;

uint8_t  g_version;             /* 0: ASM 0.xx (no loop position byte), 1: later versions */
uint8_t  g_initial_tempo;
uint8_t  g_loop_position;
uint8_t  g_order_size;
uint16_t g_order_list;          /* File offset of the position list */
uint16_t g_patterns;            /* File offset of the pattern table */

ASC_LINES g_samples[MAX_SAMPLES];
ASC_LINES g_ornaments[MAX_ORNAMENTS];

char g_title[ASC_TITLE_LENGTH + 1];
char g_author[ASC_AUTHOR_LENGTH + 1];

/* ---- Player state ---- */

uint8_t g_tempo;
uint8_t g_tick;

uint8_t g_order_position;
uint8_t g_line;

ASC_PARSE   g_parse[MAX_CHANNEL_NUMBER];
ASC_CHANNEL g_channels[MAX_CHANNEL_NUMBER];

/* The envelope registers are shared by the three channels */
uint16_t g_env_tone;
bool     g_env_tone_changed;
uint8_t  g_env_type;
bool     g_env_type_changed;

/* PSG values calculated for the current tick */
uint16_t g_frame_tone[MAX_CHANNEL_NUMBER];
uint8_t  g_frame_volume[MAX_CHANNEL_NUMBER];
uint8_t  g_frame_mixer;
uint8_t  g_frame_noise;
bool     g_frame_noise_set;

/* ---- Functions ---- */

uint16_t read_word(uint16_t pos);

bool parse_header(uint8_t version);
void parse_lines(uint16_t table, uint8_t line_size, uint8_t max_lines, ASC_LINES *lines);
void parse_id_block(void);
uint8_t trim_copy(char *destination, const uint8_t *source, uint8_t length);

void start_pattern(void);
void next_position(void);
bool pattern_ended(void);

void play_line(void);
uint8_t next_data_byte(ASC_PARSE *parse);
void parse_cell(uint8_t channel, ASC_CELL *cell);
void apply_cell(uint8_t channel, const ASC_CELL *cell);

void synthesize_channel(uint8_t channel);
void load_lines(const ASC_CHANNEL *channel, ASC_TICK *tick);
void update_tone(uint8_t channel, ASC_CHANNEL *state, const ASC_TICK *tick);
void update_volume(uint8_t channel, ASC_CHANNEL *state, const ASC_TICK *tick);
void update_noise(uint8_t channel, ASC_CHANNEL *state, const ASC_TICK *tick);
void update_slide(ASC_CHANNEL *state);
void next_lines(ASC_CHANNEL *state);
void write_psg(void);
void silence_psg(void);

int8_t  sign_extend_5(uint8_t value);
int16_t div_16(int32_t value);
int16_t div_256(int32_t value);
int32_t div_steps(int32_t value, int8_t steps);