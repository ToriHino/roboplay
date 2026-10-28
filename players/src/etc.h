/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * etc.h
 *
 * ETC: Sam CoupeE-Tracker compiled
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define NR_OF_CHANNELS     6
#define DEFAULT_TUNE_DELAY 6

#define TONES_PER_OCTAVE   12

#define NR_OF_REGISTERS    0x1A

#define DELAY_NEXT_NOTE       0xd2
#define SET_NOTE              0x72
#define SET_INSTRUMENT        0x52
#define END_OF_TRACK          0x51
#define STOP_SOUND            0x50
#define SET_ORNAMENT          0x30
#define INSTRUMENT_INVERSION  0x2e
#define ENVELOPE              0x21
#define VOLUME_REDUCTION      0x11
#define EXTENDED_NOISE        0x0f
#define TUNE_DELAY            0x00

#define ENVELOPE_LEFT_RIGHT_SAME        0b00000000  /* LEFT and RIGHT the same */
#define ENVELOPE_LEFT_RIGHT_INVERSE     0b00000001  /* LEFT and RIGHT inverse */

#define ENVELOPE_MODE_ZERO              0b00000000  /* zero amplitude               */
#define ENVELOPE_MODE_MAXIMUM           0b00000010  /* maximum amplitude            */
#define ENVELOPE_MODE_DECAY             0b00000100  /* single decay          \      */
#define ENVELOPE_MODE_REPEAT_DECAY      0b00000110  /* repetitve decay       \\\\   */
#define ENVELOPE_MODE_TRIANGLE          0b00001000  /* single triangular     /\     */
#define ENVELOPE_MODE_REPEAT_TRIANGLE   0b00001010  /* repetitve triangular  /\/\   */
#define ENVELOPE_MODE_ATTACK            0b00001100  /* single attack         /      */
#define ENVELOPE_MODE_REPEAT_ATTACK     0b00001110  /* repetitive attack     ////   */

#define ENVELOPE_BITS_4                 0b00000000  /* 4 BITS for envelope control (max 977 Hz)   */
#define ENVELOPE_BITS_3                 0b00010000  /* 3 BITS for envelope control (max 1_95 kHz) */

#define ENVELOPE_INTERNAL_CLOCK         0b00000000  /* internal envelope clock (frequency generator 1 or 4) */
#define ENVELOPE_EXTERNAL_CLOCK         0b00100000  /* external envelope clock (address write pulse)        */

#define ENVELOPE_RESET                  0b00000000  /* reset   */
#define ENVELOPE_ENABLED                0b10000000  /* enabled */

#define NOISE_0_VARIABLE                0b00000011  /* 61 Hz - 15.6 kHz (frequency generator 0) */
#define NOISE_1_VARIABLE                0b00110000  /* 61 Hz - 15.6 kHz (frequency generator 1) */

typedef struct
{
  uint16_t positions_offset;
  uint16_t patterns_offset;
  uint16_t samples_offset;
  uint16_t ornaments_offset;
  uint16_t samples_volume_decode_table;
  uint8_t  signature[0x14];
} ETC_HEADER;

typedef struct
{
  uint8_t *track;
  uint8_t *instrument;
  uint8_t *instrument_loop;
  uint8_t *ornament;
  uint8_t *ornament_loop;
  
  uint8_t  note;
  uint8_t  ornament_note;
  uint16_t tone_deviation;
  
  uint8_t amplitude;
  uint8_t tone_frequency;
  uint8_t octave;

  bool    frequency_enable;
  bool    noise_enable;

  uint8_t *instrument_start; 
  uint8_t *ornament_start;

  uint8_t delay_next_note;
  uint8_t delay_next_ornament;
  uint8_t delay_next_instrument;
  uint8_t delay_next_volume;

  bool     instrument_inversion;
  uint8_t  volume_reduction;
} ETC_CHANNEL_DATA;

typedef struct
{
  uint8_t code;
  uint8_t delay;
} ETC_VOLUME_TABLE_ENTRY;


uint8_t    *g_file_data;
ETC_HEADER *g_header;

uint16_t *g_patterns;
uint16_t *g_samples;
uint16_t *g_ornaments;

uint8_t  g_default_volume_marker;
ETC_VOLUME_TABLE_ENTRY *g_volume_table;

uint8_t *g_song_position;
uint8_t *g_loop_position;
uint8_t g_transposition;

uint8_t g_delay;
uint8_t g_tune_delay;

uint8_t g_envelope_generator_0;
uint8_t g_envelope_generator_1;

uint8_t g_noise_0;
uint8_t g_noise_1;

uint8_t g_extended_noise_0;
uint8_t g_extended_noise_1;

ETC_CHANNEL_DATA g_channel_data[NR_OF_CHANNELS];
ETC_CHANNEL_DATA* g_current_channel_data;
uint8_t           g_current_channel;

uint8_t g_out_values[NR_OF_REGISTERS];

void set_instrument(uint8_t channel, uint8_t *instrument);
void set_ornament(uint8_t channel, uint8_t *ornament);

void update_song_position(void);

void update_channel(uint8_t channel);
void handle_instrument(void);
void handle_ornament(void);
void handle_volume(void);

bool get_note(uint8_t channel);

void cmd_set_delay_next_note(uint8_t channel, uint8_t delay);
void cmd_set_note(uint8_t channel, uint8_t note);
void cmd_set_instrument(uint8_t channel, uint8_t instrument_index);
void cmd_end_of_track(void);
void cmd_stop_sound(uint8_t channel);
void cmd_set_ornament(uint8_t channel, uint8_t ornament_index);
void cmd_instrument_inversion(uint8_t channel, uint8_t inversion);
void cmd_envelope(uint8_t channel, uint8_t envelope);
void cmd_volume_reduction(uint8_t channel, uint8_t reduction);
void cmd_extended_noise(uint8_t channel, uint8_t extended_noise);
void cmd_tune_delay(uint8_t delay);