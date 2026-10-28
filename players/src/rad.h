/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * rad.h
 *
 * RAD: Reality ADlib Tracker v1 and v2 (file version 2.1) player
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_SEGMENT_INDEX 4

#define MAX_CHANNEL_NUMBER    9
#define MAX_INSTRUMENT_NUMBER 31
#define MAX_PATTERN_NUMBER    32
#define MAX_PATTERN_LINE      64

#define KEY_OFF 15

/* Effects shared by V1 and V2 */
#define CMD_PORTAMENTO_UP   0x1
#define CMD_PORTAMENTO_DOWN 0x2
#define CMD_TONE_SLIDE      0x3
#define CMD_TONE_VOL_SLIDE  0x5
#define CMD_VOL_SLIDE       0xA
#define CMD_SET_VOL         0xC
#define CMD_JUMP_TO_LINE    0xD
#define CMD_SET_SPEED       0xF

/* Effects that only exist in V2 ('I' - 55 etc.) */
#define CMD_IGNORE          0x12
#define CMD_OP_MULTIPLIER   0x16
#define CMD_RIFF            0x1B
#define CMD_TRANSPOSE       0x1D
#define CMD_FEEDBACK        0x1E
#define CMD_OP_VOLUME       0x1F

#define FREQ_START  0x0156      /* Low end of frequency in each octave */
#define FREQ_END    0x02AE      /* High end of frequency in each octave */
#define FREQ_RANGE  (FREQ_END - FREQ_START)

#define OCTAVE_START 0x00
#define OCTAVE_END   0x07

#define PAN_SETTING_LEFT  0x10
#define PAN_SETTING_MID   0x30
#define PAN_SETTING_RIGHT 0x20

#define RAD_VERSION_1 0x10
#define RAD_VERSION_2 0x21

/* ---- V2 specific constants ---- */

/* V2 tune data is addressed by 16 bit offsets into the whole file (which spans
 * several 16KB segments), so a V2 tune must be smaller than 64KB */
#define SEGMENT_SHIFT       14
#define SEGMENT_OFFSET_MASK 0x3FFF
#define NO_SEGMENT          0xFF

#define V2_MAX_INSTRUMENTS  127
#define V2_MAX_TRACKS       100
#define V2_MAX_RIFF_TRACKS  10
#define V2_MAX_ENTRANCES    8       /* Riffs can trigger riffs, limit the recursion */

#define V2_SRC_NONE   0
#define V2_SRC_RIFF   1
#define V2_SRC_IRIFF  2

#define V2_KEY_ON     (1 << 0)
#define V2_KEY_OFF    (1 << 1)
#define V2_KEYED_ON   (1 << 2)

typedef struct
{
    char    id[16];
    uint8_t version;
    uint8_t flags;
} RAD_HEADER;

typedef struct 
{
    uint8_t data[11];
} RAD_INSTRUMENT;

typedef struct
{
    bool     tone_slide;
    int8_t   tone_slide_speed;
    uint16_t tone_slide_frequency;
    uint8_t  tone_slide_octave;

    int8_t port_slide;
    int8_t volume_slide;

    uint8_t instrument_number;

    uint16_t frequency;
    uint8_t  octave;

    uint8_t old_43;
    uint8_t old_A0;
    uint8_t old_B0;
} RAD_CHANNEL_DATA;

typedef struct
{
    bool    new_data;

    uint8_t note;
    uint8_t octave;

    uint8_t instrument_number;

    uint8_t command;
    uint8_t data;
} RAD_NOTE_DATA;

/* ---- V2 data structures ---- */

typedef struct
{
    int8_t   port_slide;
    int8_t   vol_slide;
    uint16_t tone_slide_freq;
    uint8_t  tone_slide_oct;
    uint8_t  tone_slide_speed;
    int8_t   tone_slide_dir;
} RAD2_EFFECTS;

typedef struct
{
    RAD2_EFFECTS fx;
    uint16_t     track;             /* File offset of next line to play, 0 = none */
    uint16_t     track_start;
    uint8_t      line;
    uint8_t      speed;
    uint8_t      speed_count;
    int8_t       transpose_octave;
    int8_t       transpose_note;
    uint8_t      last_instrument;
} RAD2_RIFF;

typedef struct
{
    uint8_t      last_instrument;
    uint8_t      instrument;        /* 1..127, 0 = none yet */
    uint8_t      algorithm;         /* Cached from instrument, 7 = MIDI (silent) */
    uint8_t      op_volume[4];      /* Cached operator volume register values */
    uint8_t      volume;
    uint8_t      detune_a;
    uint8_t      detune_b;
    uint8_t      key_flags;
    uint8_t      reg_b0[2];         /* Last value written to 0xB0 (index 0 = 2nd OPL channel, 1 = 1st) */
    uint8_t      reg_c0[2];         /* Last value written to 0xC0 (same indexing) */
    uint8_t      reg_20[4];         /* Last value written to the operator multiplier registers */
    uint16_t     curr_freq;
    int8_t       curr_octave;
    RAD2_EFFECTS fx;
    RAD2_RIFF    riff;
    RAD2_RIFF    iriff;
} RAD2_CHANNEL;

/* ---- Constant tables ---- */

const uint16_t g_note_frequencies[] = 
{
    0x16B, 0x181, 0x198, 0x1B0, 0x1CA, 0x1E5,
    0x202, 0x220, 0x241, 0x263, 0x287, 0x2AE
};

const uint8_t g_channel_offsets[] =
{
    0x20, 0x21, 0x22, 0x28, 0x29, 0x2a, 0x30, 0x31, 0x32
};

/* V2: size in bytes of the note data that follows a channel byte, indexed by (chanid >> 4) & 7 */
const int8_t g_v2_note_size[8] = { 0, 2, 1, 3, 1, 3, 2, 4 };

/* V2: OPL3 first/second channel register offsets (bit 8 = second register set) */
const uint16_t g_v2_chan_offsets[9]  = { 0, 1, 2, 0x100, 0x101, 0x102, 6, 7, 8 };
const uint16_t g_v2_chan2_offsets[9] = { 3, 4, 5, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108 };

/* V2: operator registers (relative to the operator offset) in instrument data order */
const uint8_t g_v2_op_regs[5] = { 0x20, 0x40, 0x60, 0x80, 0xE0 };

const uint16_t g_v2_op_offsets[9][4] =
{
    { 0x00B, 0x008, 0x003, 0x000 },
    { 0x00C, 0x009, 0x004, 0x001 },
    { 0x00D, 0x00A, 0x005, 0x002 },
    { 0x10B, 0x108, 0x103, 0x100 },
    { 0x10C, 0x109, 0x104, 0x101 },
    { 0x10D, 0x10A, 0x105, 0x102 },
    { 0x113, 0x110, 0x013, 0x010 },
    { 0x114, 0x111, 0x014, 0x011 },
    { 0x115, 0x112, 0x015, 0x012 }
};

/* V2: which operators are carriers for each algorithm */
const bool g_v2_alg_carriers[7][4] =
{
    { true, false, false, false },
    { true, true,  false, false },
    { true, false, false, false },
    { true, false, false, true  },
    { true, false, true,  false },
    { true, false, true,  true  },
    { true, true,  true,  true  }
};

/* ---- Common state ---- */

RAD_HEADER *g_header;

uint8_t  *g_file_data;
uint8_t **g_patterns;

uint8_t *g_pattern_data;

uint8_t g_segment_list[MAX_SEGMENT_INDEX];
uint8_t g_segment_index;

uint8_t  g_version;     /* 1 or 2 */
uint16_t g_file_size;

float g_hertz;

uint8_t g_speed;
uint8_t g_speed_count;

uint8_t g_order_list_size;
uint8_t g_order_position;

/* ---- V1 state ---- */

RAD_INSTRUMENT *g_instruments[MAX_INSTRUMENT_NUMBER];

RAD_NOTE_DATA g_note_buffer[MAX_CHANNEL_NUMBER];

uint8_t *g_order_list;

uint8_t g_next_line;
uint8_t g_current_line;
uint8_t g_is_last_line;

int8_t  g_next_line_jump;

RAD_CHANNEL_DATA g_channel_data[MAX_CHANNEL_NUMBER];

/* ---- V2 state ---- */

/* All V2 "pointers" are 16 bit offsets into the whole file, 0 = not present */
uint16_t g_v2_instruments[V2_MAX_INSTRUMENTS];
uint16_t g_v2_tracks[V2_MAX_TRACKS];
uint16_t g_v2_riffs[V2_MAX_RIFF_TRACKS][MAX_CHANNEL_NUMBER];
uint16_t g_v2_order_list;

uint8_t  g_v2_initial_speed;

uint16_t g_v2_track;
uint8_t  g_v2_line;
int8_t   g_v2_line_jump;
int8_t   g_v2_entrances;
uint8_t  g_v2_master_volume;

uint8_t  g_v2_mapped_segment;
uint8_t  g_v2_four_op;              /* Shadow of OPL3 register 0x104 */

RAD2_CHANNEL g_v2_channels[MAX_CHANNEL_NUMBER];

/* Values exported by v2_unpack_note() */
int8_t  g_v2_note;
int8_t  g_v2_octave;
uint8_t g_v2_inst;
uint8_t g_v2_effect;
uint8_t g_v2_param;

/* ---- V1 functions ---- */

bool load_v1(void);
bool update_v1(void);
void rewind_v1(void);

uint8_t next_byte(void);

void fill_note_buffer(void);

void next_pattern(void);
void update_notes(void);

void play_note(uint8_t channel);

void load_instrument(uint8_t channel);
void load_instrument_volume(uint8_t channel);

void set_volume(uint8_t channel, uint8_t volume);

void tone_slide(uint8_t channel);
void tone_volume_slide(uint8_t channel);
void volume_slide(uint8_t channel);

/* ---- V2 functions ---- */

bool load_v2(void);
bool update_v2(void);
void rewind_v2(void);

uint8_t  peek_byte(uint16_t pos);
uint8_t  read_byte(uint16_t *pos);
uint16_t read_word(uint16_t *pos);
bool     have_bytes(uint16_t pos, uint16_t count);

void v2_set_opl3(uint16_t reg, uint8_t value);
void v2_set_b0(uint8_t channel, uint8_t which, uint8_t value);
void v2_set_c0(uint8_t channel, uint8_t which, uint8_t value);

bool     v2_unpack_note(uint16_t *pos, uint8_t *last_instrument);
uint16_t v2_get_track(void);
uint16_t v2_skip_to_line(uint16_t track, uint8_t line_number, bool channel_riff);

void v2_play_line(void);
void v2_play_note(uint8_t channel, int8_t note, int8_t octave, uint8_t instrument, uint8_t command, uint8_t param, uint8_t source, uint8_t op);
void v2_load_instrument(uint8_t channel, uint16_t instrument_offset);
void v2_play_note_opl3(uint8_t channel, int8_t octave, int8_t note);
void v2_reset_fx(RAD2_EFFECTS *fx);
void v2_tick_riff(uint8_t channel, RAD2_RIFF *riff, bool channel_riff);
void v2_continue_fx(uint8_t channel, RAD2_EFFECTS *fx);
void v2_set_volume(uint8_t channel, uint8_t volume);
void v2_start_tone_slide(uint8_t channel, RAD2_EFFECTS *fx, uint8_t speed);
void v2_get_slide_dir(uint8_t channel, RAD2_EFFECTS *fx);
void v2_load_op_multiplier(uint8_t channel, uint8_t op, uint8_t multiplier);
void v2_load_op_volume(uint8_t channel, uint8_t op, uint8_t volume);
void v2_load_feedback(uint8_t channel, uint8_t which, uint8_t feedback);
void v2_portamento(uint8_t channel, RAD2_EFFECTS *fx, int8_t amount, bool tone_slide);
void v2_transpose(int8_t note, int8_t octave);