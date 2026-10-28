/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mbm_org.h
 *
 * MBM: MoonBlaster on original device(s)
 */

#pragma once 

#include <stdint.h>

#define NR_OF_SAMPLE_BLOCKS     14
#define SAMPLE_FILE_HEADER_SIZE 14*4

#define OPL_WAVE_MEMORY  0x200000
#define OPL_WAVE_ADDRESS OPL_WAVE_MEMORY + 384 * 12

#define NR_OF_AUDIO_INSTRUMENTS 16
#define NR_OF_MUSIC_INSTRUMENTS 16
#define NR_OF_ORIGINAL_INSTRUMENTS 6

#define MAX_POSITION 255
#define MAX_STEP     15

#define DRUM_CHANNEL    11
#define COMMAND_CHANNEL 12

#define STEP_BUFFER_SIZE 13

#define STEREO_SETTING_SIZE  10

#define NR_OF_VOICE_PATCHES 15
#define MUSIC_INSTRUMENT_LENGTH 8
#define PRESET_DATA_SIZE 11

#define AUDIO_INSTRUMENT_DATA_SIZE 9

#define CHANNEL_AUDIO  0x01
#define CHANNEL_MUSIC  0x02
#define CHANNEL_STEREO 0x03

#define PAN_SETTING_LEFT  0x10
#define PAN_SETTING_MID   0x30
#define PAN_SETTING_RIGHT 0x20

#define DEFAULT_TRANSPOSE 48

#define NR_OF_CHANNELS 9
#define NR_OF_DRUM_CHANNELS 3

#define FIRST_DRUM_CHANNEL  6

#define NOTE_ON             96      /* 001 - 096 */
#define NOTE_OFF            97      /* 097       */
#define INSTRUMENT_CHANGE   98      /* 098 - 113 */
#define VOLUME_CHANGE       114     /* 114 - 176 */     
#define STEREO_SET          177     /* 177 - 179 */
#define NOTE_LINK           180     /* 180 - 198 */
#define PITCH               199     /* 199 - 217 */
#define BRIGHTNESS_NEGATIVE 218     /* 218 - 223 */
#define REVERB              224     /* 224 - 230 */
#define BRIGHTNESS_POSITIVE 231     /* 231 - 236 */
#define SUSTAIN             237     /* 237       */
#define MODULATION          238     /* 238       */

#define COMMAND_TEMPO          23   /* 001 - 023 */
#define COMMAND_PATTERN_END    24   /* 024       */
#define COMMAND_DRUM_SET_MUSIC 25   /* 025 - 027 */
#define COMMAND_STATUS_BYTE    28   /* 028 - 039 */
#define COMMAND_TRANSPOSE      49   /* 049 - ... */

#define FREQUENCY_MODE_NORMAL     0
#define FREQUENCY_MODE_PITCH_BEND 1
#define FREQUENCY_MODE_MODULATION 2

typedef struct
{
    uint8_t instrument;
    uint8_t volume;
} INSTRUMENT_DATA_MUSIC;

typedef struct 
{
    uint8_t  song_length;
    uint16_t id;
    
    uint8_t  voice_data_audio[NR_OF_AUDIO_INSTRUMENTS][AUDIO_INSTRUMENT_DATA_SIZE];

    uint8_t               instrument_list_audio[NR_OF_AUDIO_INSTRUMENTS];
    INSTRUMENT_DATA_MUSIC instrument_list_music[NR_OF_MUSIC_INSTRUMENTS];

    uint8_t  channel_chip_set[STEREO_SETTING_SIZE];
    uint8_t  start_tempo;
    uint8_t  sustain;
    char     track_name[41];

    uint8_t  start_instruments_audio[NR_OF_CHANNELS];
    uint8_t  start_instruments_music[NR_OF_CHANNELS];
    uint8_t  original_instrument_data[NR_OF_ORIGINAL_INSTRUMENTS][MUSIC_INSTRUMENT_LENGTH];
    uint8_t  original_instrument_prog[NR_OF_ORIGINAL_INSTRUMENTS];

    char     sample_kit_name[8];

    uint8_t  drum_setup_music_psg[15];
    uint8_t  drum_volumes_music[3];
    uint16_t drum_frequencies_music[3][3];
    uint8_t  dummy[2];
    uint8_t  start_reverb[NR_OF_CHANNELS];
    uint8_t  loop_position;
} MBM_HEADER;

typedef struct
{
    uint8_t  frequency_register;
    uint8_t  volume_register;
} MBM_REGISTER_DATA;

typedef struct
{
    uint8_t  last_note;
    uint8_t  last_instrument;
    uint8_t  last_frequency_low;
    uint8_t  last_frequency_high;
    uint8_t  next_frequency_low;
    uint8_t  next_frequency_high;
    uint8_t  brightness;
} MBM_NOTE_DATA;

typedef struct 
{
    uint8_t  frequency_mode;

    int8_t   pitch_bend_value;
    uint8_t  reverb_value;

    MBM_NOTE_DATA audio;
    MBM_NOTE_DATA music;
} MBM_STATUS_TABLE;

typedef struct
{
    uint8_t psg_count;
    uint8_t data_size;
    uint8_t data[3][2];
    uint8_t psg_volume;
}  MBM_PSG_DRUM_DATA;

typedef struct
{
    uint8_t low;
    uint8_t high;
} SAMPLE_PITCH;

static const MBM_REGISTER_DATA g_audio_register_data[NR_OF_CHANNELS] =
    { 
        {0xA0, 0x43}, {0xA1, 0x44}, {0xA2, 0x45}, {0xA3, 0x4B},
        {0xA4, 0x4C}, {0xA5, 0x4D}, {0xA6, 0x53}, {0xA7, 0x54},
        {0xA8, 0x55}
    };

static const uint8_t g_instrument_registers[NR_OF_CHANNELS][AUDIO_INSTRUMENT_DATA_SIZE] =
    {
        {0x20, 0x23, 0x40, 0x43, 0x60, 0x63, 0x80, 0x83, 0xC0},
        {0x21, 0x24, 0x41, 0x44, 0x61, 0x64, 0x81, 0x84, 0xC1},
        {0x22, 0x25, 0x42, 0x45, 0x62, 0x65, 0x82, 0x85, 0xC2},

        {0x28, 0x2B, 0x48, 0x4B, 0x68, 0x6B, 0x88, 0x8B, 0xC3},
        {0x29, 0x2C, 0x49, 0x4C, 0x69, 0x6C, 0x89, 0x8C, 0xC4},
        {0x2A, 0x2D, 0x4A, 0x4D, 0x6A, 0x6D, 0x8A, 0x8D, 0xC5},

        {0x30, 0x33, 0x50, 0x53, 0x70, 0x73, 0x90, 0x93, 0xC6},
        {0x31, 0x34, 0x51, 0x54, 0x71, 0x74, 0x91, 0x94, 0xC7},
        {0x32, 0x35, 0x52, 0x55, 0x72, 0x75, 0x92, 0x95, 0xC8}
    };

static const uint8_t g_preset_registers[NR_OF_CHANNELS][PRESET_DATA_SIZE] =
    {
        {0x20, 0x23, 0x40, 0x43, 0x60, 0x63, 0x80, 0x83, 0xC0, 0xE0, 0xE3},
        {0x21, 0x24, 0x41, 0x44, 0x61, 0x64, 0x81, 0x84, 0xC1, 0xE1, 0xE4},
        {0x22, 0x25, 0x42, 0x45, 0x62, 0x65, 0x82, 0x85, 0xC2, 0xE2, 0xE5},

        {0x28, 0x2B, 0x48, 0x4B, 0x68, 0x6B, 0x88, 0x8B, 0xC3, 0xE8, 0xEB},
        {0x29, 0x2C, 0x49, 0x4C, 0x69, 0x6C, 0x89, 0x8C, 0xC4, 0xE9, 0xEC},
        {0x2A, 0x2D, 0x4A, 0x4D, 0x6A, 0x6D, 0x8A, 0x8D, 0xC5, 0xEA, 0xED},

        {0x30, 0x33, 0x50, 0x53, 0x70, 0x73, 0x90, 0x93, 0xC6, 0xF0, 0xF3},
        {0x31, 0x34, 0x51, 0x54, 0x71, 0x74, 0x91, 0x94, 0xC7, 0xF1, 0xF4},
        {0x32, 0x35, 0x52, 0x55, 0x72, 0x75, 0x92, 0x95, 0xC8, 0xF2, 0xF5}
    };


static const uint8_t g_music_voice_patches[][MUSIC_INSTRUMENT_LENGTH] =
    {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},      /* User patch (irrelevant) */
        {0x71, 0x61, 0x1E, 0x17, 0xD0, 0x78, 0x00, 0x17},
        {0x13, 0x41, 0x1A, 0x0D, 0xD8, 0xF7, 0x23, 0x13},
        {0x13, 0x01, 0x99, 0x00, 0xF2, 0xC4, 0x11, 0x23},
        {0x31, 0x61, 0x0E, 0x07, 0xA8, 0x64, 0x70, 0x27},
        {0x32, 0x21, 0x1E, 0x06, 0xE0, 0x76, 0x00, 0x28},
        {0x31, 0x22, 0x16, 0x05, 0xE0, 0x71, 0x00, 0x18},
        {0x21, 0x61, 0x1D, 0x07, 0x82, 0x81, 0x10, 0x07},
        {0x23, 0x21, 0x2D, 0x14, 0xA2, 0x72, 0x00, 0x07},
        {0x61, 0x61, 0x1B, 0x06, 0x64, 0x65, 0x10, 0x17},
        {0x41, 0x61, 0x0B, 0x18, 0x85, 0xF7, 0x71, 0x07},
        {0x13, 0x01, 0x83, 0x11, 0xFA, 0xE4, 0x10, 0x04},
        {0x17, 0xC1, 0x24, 0x07, 0xF8, 0xF8, 0x22, 0x12},
        {0x61, 0x50, 0x0C, 0x05, 0xC2, 0xF5, 0x20, 0x42},
        {0x01, 0x01, 0x55, 0x03, 0xC9, 0x95, 0x03, 0x02},
        {0x61, 0x41, 0x89, 0x03, 0xF1, 0xE4, 0x40, 0x13}
    };

static const uint8_t g_music_volume_table[] =
    {
        63, 63-16, 63-18, 63-20, 63-22, 63-24, 63-26, 63-28, 
        63-33, 63-37, 63-40, 63-45, 63-50, 63-55, 63-60, 63-63
    };

const uint8_t g_music_drums[][AUDIO_INSTRUMENT_DATA_SIZE] =
    {
        { 0x01, 0x01, 0x0A, 0x02, 0xDF, 0xF8, 0x6A, 0x6D, PAN_SETTING_MID },     /* BD      */
        { 0x01, 0x01, 0x0A, 0x02, 0xC8, 0xD8, 0xA7, 0x48, PAN_SETTING_MID },     /* SD / HH */
        { 0x05, 0x01, 0x08, 0x08, 0xF8, 0xAA, 0x59, 0x55, PAN_SETTING_MID },     /* TM / TC */
    };

const MBM_PSG_DRUM_DATA g_psg_drums[] =
    {
        { 5, 3, { {0, 0xB3}, {1, 0x06}, {7, 0xBE} }, 15 },
        { 6, 2, { {6, 0x13}, {7, 0xB7}, {0, 0x00} }, 14 },
        { 6, 2, { {6, 0x09}, {7, 0xB7}, {0, 0x00} }, 14 },
        { 4, 3, { {0, 0xAD}, {1, 0x01}, {7, 0xBE} }, 14 },
        { 4, 3, { {0, 0xAC}, {1, 0x00}, {7, 0xBE} }, 14 },
        { 5, 2, { {6, 0x00}, {7, 0xB7}, {0, 0x00} }, 14 },
        { 5, 2, { {6, 0x01}, {7, 0xB7}, {0, 0x00} }, 13 }
    };

static const uint16_t g_frequency_table[] =
    {
        0x00AD, 0x00B7, 0x00C2, 0x00CD, 0x00D9, 0x00E6,
        0x00F4, 0x0103, 0x0112, 0x0122, 0x0134, 0x0146,
        0x02AD, 0x02B7, 0x02C2, 0x02CD, 0x02D9, 0x02E6,
        0x02F4, 0x0303, 0x0312, 0x0322, 0x0334, 0x0346,
        0x04AD, 0x04B7, 0x04C2, 0x04CD, 0x04D9, 0x04E6,
        0x04F4, 0x0503, 0x0512, 0x0522, 0x0534, 0x0546,
        0x06AD, 0x06B7, 0x06C2, 0x06CD, 0x06D9, 0x06E6,
        0x06F4, 0x0703, 0x0712, 0x0722, 0x0734, 0x0746,
        0x08AD, 0x08B7, 0x08C2, 0x08CD, 0x08D9, 0x08E6,
        0x08F4, 0x0903, 0x0912, 0x0922, 0x0934, 0x0946,
        0x0AAD, 0x0AB7, 0x0AC2, 0x0ACD, 0x0AD9, 0x0AE6,
        0x0AF4, 0x0B03, 0x0B12, 0x0B22, 0x0B34, 0x0B46,
        0x0CAD, 0x0CB7, 0x0CC2, 0x0CCD, 0x0CD9, 0x0CE6,
        0x0CF4, 0x0D03, 0x0D12, 0x0D22, 0x0D34, 0x0D46,
        0x0EAD, 0x0EB7, 0x0EC2, 0x0ECD, 0x0ED9, 0x0EE6,
        0x0EF4, 0x0F03, 0x0F12, 0x0F22, 0x0F34, 0x0F46
    };

static const int8_t g_modulation_values[] =
    { 1, 2, 2, -2, -2, -1, -2, -2, 2, 2 };

static const uint8_t g_pitch_table[][2] =
    {
         {0xB3, 0xA2}, {0xB4, 0x46}, {0xB4, 0xFB}, {0xB5, 0xC3}, {0xB6, 0x9E}, {0xB7, 0x78},
         {0xC0, 0x32}, {0xC0, 0xB1}, {0xC1, 0x30}, {0xC1, 0xC2}, {0xC2, 0x5C}, {0xC2, 0xF6},
         {0xC3, 0x9B}, {0xC4, 0x47}, {0xC5, 0x05}, {0xC5, 0xCD}, {0xC6, 0x9E}, {0xC7, 0x78},
         {0xD0, 0x37}, {0xD0, 0xB1}, {0xD1, 0x35}, {0xD1, 0xC2}, {0xD2, 0x57}, {0xD2, 0xF2},
         {0xD3, 0x9B}, {0xD4, 0x47}, {0xD5, 0x06}, {0xD5, 0xCD}, {0xD6, 0x9A}, {0xD7, 0x79},
         {0xE0, 0x37}, {0xE0, 0xB3}, {0xE1, 0x33}, {0xE1, 0xC2}, {0xE2, 0x58}, {0xE2, 0xF4},
         {0xE3, 0x9D}, {0xE4, 0x49}, {0xE5, 0x06}, {0xE5, 0xCB}, {0xE6, 0x9A}, {0xE7, 0x7B},
         {0xF0, 0x36}, {0xF0, 0xB3}, {0xF1, 0x34}, {0xF1, 0xC2}, {0xF2, 0x59}, {0xF2, 0xF3},
         {0xF3, 0x9C}, {0xF4, 0x48}, {0xF5, 0x06}, {0xF5, 0xCB}, {0xF6, 0x9B}, {0xF7, 0x7B},
         {0x00, 0x36}, {0x00, 0xB3}, {0x01, 0x34}, {0x01, 0xC2}, {0x02, 0x59}, {0x02, 0xF4}
    };

static MBM_HEADER g_mbm_header;
static uint8_t   *g_position_table;
static uint8_t  **g_patterns;

static uint8_t g_step_buffer[STEP_BUFFER_SIZE];

static MBM_STATUS_TABLE g_mbm_status_table[NR_OF_CHANNELS] =
    {
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
        {false, 0, 0, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
    };

static uint8_t g_refresh;

static uint8_t g_speed;
static uint8_t g_speed_count;

static uint8_t g_position;
static uint8_t g_step;

static uint8_t g_transpose_value;

static uint8_t *g_pattern_data;

static uint8_t g_stereo_settings[STEREO_SETTING_SIZE];

static bool g_play_samples;

static uint8_t g_psg_count;
static uint8_t g_psg_volume;

void next_song_position(void);

bool is_audio_channel_active(uint8_t channel);
bool is_music_channel_active(uint8_t channel);

void tempo_command(void);
void pattern_end_command(void);
void drum_set_music_command(void);
void status_byte_command(void);
void transpose_command(void);

void handle_psg(void);
void handle_frequency_mode(void);
void handle_pitch_bend(const uint8_t channel);
void handle_modulation(const uint8_t channel);

void update_instrument_audio(uint8_t channel, uint8_t instrument);
void update_instrument_music(uint8_t channel, uint8_t instrument);
void update_drum_set_music(uint8_t drum_set);

void play_event(const uint8_t channel, MBM_NOTE_DATA *note_data);

void music_play_drum(void);

void note_on_event(const uint8_t channel);
void note_off_event(const uint8_t channel);
void instrument_change_event(const uint8_t channel);
void volume_change_event(const uint8_t channel);
void stereo_change_event(const uint8_t channel);
void note_link_event(const uint8_t channel);
void pitch_event(const uint8_t channel);
void brightness_negative_event(const uint8_t channel);
void reverb_change_event(const uint8_t channel);
void brightness_positive_event(const uint8_t channel);
void sustain_event(const uint8_t channel);
void modulation_event(const uint8_t channel);

void play_sample(uint8_t frequency, uint8_t volume, uint8_t sample_number);
void write_sample_headers(void);
bool load_sample_kit(const char* file_name);

void adpcm2pcm(void);
