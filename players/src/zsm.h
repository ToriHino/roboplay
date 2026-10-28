/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * zsm.h
 *
 * ZSM: Commander X16 zsound player
 */

#pragma once

/* Needs player_interface.h (uint8_t, uint16_t, bool, ...) to be included first. */

#define NR_OF_PSG_CHANNELS 16

/* The song data is kept in mapper segments; the list holds at most this many. */
#define ZSM_MAX_SEGMENTS   256

#define ZSM_END_OF_DATA    0x80

/* How many extra times the loop section is played before update() reports the
   end of the song. 0 ignores the loop point (old behaviour: end of data = end
   of song, the framework restarts from the beginning). */
#ifndef ZSM_LOOP_COUNT
#define ZSM_LOOP_COUNT     1
#endif

/* Experimental: let the noise channel follow the VERA noise frequency instead
   of using one fixed pitch. Needs tuning by ear. */
#ifndef ZSM_NOISE_TRACK
#define ZSM_NOISE_TRACK    0
#endif

#define WAVEFORM_PULSE     0x00
#define WAVEFORM_SAWTOOTH  0x40
#define WAVEFORM_TRIANGLE  0x80
#define WAVEFORM_NOISE     0xC0
#define WAVEFORM_NONE      0xFF   /* OPL channel not set up yet */

typedef struct
{
  uint8_t  magic_header[2];
  uint8_t  version;
  uint8_t  loop_point[3];
  uint8_t  pcm_offset[3];
  uint8_t  fm_channel_mask;
  uint16_t psg_channel_mask;
  uint16_t tick_rate;
  uint16_t reserved;
} ZSM_HEADER;

/* Shadow of the VERA voice registers */
typedef struct
{
  uint8_t freq_low;
  uint8_t freq_high;
  uint8_t pan_volume;
  uint8_t wave_pulse_width;
} ZSM_PSG_REGISTERS;

typedef enum
{
  FREQUENCY_LOW,
  FREQUENCY_HIGH,
  PAN_VOLUME,
  WAVE_PULSE_WIDTH
} ZSM_PSG_REGISTER_TYPE;

/* State of the OPL4 FM channel that emulates a VERA voice. The reg_* / *_tl
   members cache what was last written, so unchanged values are never sent. */
typedef struct
{
  uint8_t wave;         /* WAVEFORM_xxx loaded in the channel, or WAVEFORM_NONE */
  uint8_t pulse_width;  /* VERA pulse width, 0..63 */
  uint8_t volume;       /* VERA volume, 0..63 */
  uint8_t pan;          /* OPL4 CHA/CHB bits for register 0xC0 (0x10 = left, 0x20 = right) */
  uint8_t fb_conn;      /* feedback / connection bits for register 0xC0 */
  uint8_t fnum_low;     /* wanted register 0xA0 value */
  uint8_t blk_hi;       /* wanted register 0xB0 value without the key-on bit */
  uint8_t reg_a0;       /* last written 0xA0 */
  uint8_t reg_b0;       /* last written 0xB0 (includes key-on) */
  uint8_t mod_tl;       /* last written modulator TL (0xFF = unknown) */
  uint8_t car_tl;       /* last written carrier TL   (0xFF = unknown) */
} OPL_REGISTERS;

static ZSM_HEADER g_header;
static float      g_refresh;

/* Song data: header sits in front of segment 0 so file offsets map 1:1 onto
   (segment, offset) pairs. */
static uint8_t  g_segment_list[ZSM_MAX_SEGMENTS];
static uint8_t  g_segment_index;
static uint8_t  g_last_segment;
static uint8_t *g_data_end;       /* first unused byte in the last segment */

static uint8_t *g_song_data;
static uint8_t *g_song_limit;     /* end of valid data in the current segment */
static uint16_t g_delay_counter;

static bool     g_has_loop;
static uint8_t  g_loop_segment;
static uint16_t g_loop_offset;
static uint8_t  g_loops_done;

/* One bit per VERA voice */
static uint16_t g_freq_dirty;     /* pitch must be recomputed and written */
static uint16_t g_key_dirty;      /* only the key-on state may have changed */

static ZSM_PSG_REGISTERS g_psg_channels[NR_OF_PSG_CHANNELS];
static OPL_REGISTERS     g_opl_channels[NR_OF_PSG_CHANNELS];

uint8_t  get_next_data_byte(void);
void     seek_to(uint8_t segment_index, uint16_t offset);
void     psg_write(uint8_t data);
void     fm_write(uint8_t data);
void     ext_cmd(void);

void     opl_out(uint8_t channel, uint8_t reg, uint8_t value);
void     write_c0(uint8_t channel);
void     select_wave(uint8_t channel, uint8_t waveform);
void     update_level(uint8_t channel);
uint16_t vera_to_opl_freq(uint16_t fv);
void     update_frequency(uint8_t channel, bool recompute);
void     flush_channels(void);