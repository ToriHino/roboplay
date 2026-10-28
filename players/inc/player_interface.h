/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * player_interface.h
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define INTERFACE_MAJOR_VERSION 2
#define INTERFACE_MINOR_VERSION 0

#define ROBO_PLAYER_BASE 0x4000

#define DATA_SEGMENT_BASE 0x8000
#define DATA_SEGMENT_SIZE 0x4000

#define START_SEGMENT_INDEX 0

#define READ_BUFFER       0xC000
#define READ_BUFFER_SIZE  0x1000

//#define __DEBUG

typedef enum
{
  ROBOPLAY_OPL_MODE_OPL2,
  ROBOPLAY_OPL_MODE_OPL3,
  ROBOPLAY_OPL_MODE_OPL4
} roboplay_opl_mode;

typedef enum
{
  ROBOPLAY_SEEK_START,
  ROBOPLAY_SEEK_CURRENT,
  ROBOPLAY_SEEK_END
} roboplay_seek;

typedef struct
{
  char signature[8];
  uint8_t major_version;
  uint8_t minor_version;

  /* To be implemented by the player */
  bool(*load)(const char *file_name);
  bool(*update)(void);
  void(*rewind)(const uint8_t subsong);

  void(*command)(const uint8_t id);

  float(*get_refresh)(void);
  uint8_t(*get_subsongs)(void);

  char*(*get_player_info)(void);
  char*(*get_title)(void);
  char*(*get_author)(void);
  char*(*get_description)(void);

  /* Callbacks to RoboPlay environment */
  void(*open)(const char *file_name, const bool local);
  uint16_t(*read)(const void *destination, const uint16_t length);
  void(*close)(void);
  bool(*exists)(char *const file_name);
  void(*seek)(int32_t offset, roboplay_seek whence);

  uint8_t(*get_new_segment)(void);
  void(*set_segment)(const uint8_t segment);
  void(*set_default_segment)(void);

  void(*update_refresh)(void);

  /* OPL4 support */
  void(*opl4_set_mode)(const roboplay_opl_mode mode);

  void(*opl4_write_fm_1)(const uint8_t reg, const uint8_t value);
  void(*opl4_write_fm_2)(const uint8_t reg, const uint8_t value);
  void(*opl4_write_wave)(const uint8_t reg, const uint8_t value);
  void(*opl4_write_wave_data)(const uint8_t *const data, const uint16_t size);

  uint8_t(*opl4_read_status)(void);
  uint8_t(*opl4_read_fm_1)(const uint8_t reg);
  uint8_t(*opl4_read_fm_2)(const uint8_t reg);
  uint8_t(*opl4_read_wave_register)(const uint8_t reg);

  void(*opl4_wait_for_load)(void);

  /* PSG support */
  void(*psg_write)(const uint8_t reg, const uint8_t value);
  uint8_t(*psg_read)(const uint8_t reg);
  void(*psg2_write)(const uint8_t reg, const uint8_t value);
  uint8_t(*psg2_read)(const uint8_t reg);

  /* SCC support */
  void (*scc_set_waveform)(uint8_t channel, uint8_t *waveform);
  void (*scc_set_frequency)(uint8_t channel, uint16_t frequency);
  void (*scc_set_volume)(uint8_t channel, uint8_t volume);
  void (*scc_write_register)(uint8_t reg, uint8_t value);

  /* OPM support */
  void   (*opm_write)(const uint8_t reg, const uint8_t value);
  uint8_t(*opm_read_status)(void);

  /* MIDI support */
  void (*midi_send_data_1)(const uint8_t data_1);
  void (*midi_send_data_2)(const uint8_t data_1, const uint8_t data_2);
  void (*midi_send_data_3)(const uint8_t data_1, const uint8_t data_2, const uint8_t data_3);
  
  /* SoundStar support */
  void (*soundstar_write)(const uint8_t reg, const uint8_t value);

  /* MSX-AUDIO support */
  void (*msx_audio_write)(const uint8_t reg, const uint8_t value);
  uint8_t(*msx_audio_read_status)(void);
  void(*msx_audio_write_adpcm_data)(const uint8_t *const data, const uint16_t size);

  /* MSX-MUSIC support */
  void (*msx_music_write)(const uint8_t reg, const uint8_t value);

  /* SID support */
  void (*sid_write_register)(const uint8_t reg, const uint8_t value);
  uint8_t (*sid_read_register)(const uint8_t reg);
  
#ifdef __DEBUG
  void(*debug_string)(const char *str);
  void(*debug_hex)(const uint16_t num);
#endif

} ROBO_PLAYER_INTERFACE;

static ROBO_PLAYER_INTERFACE *const g_roboplay_interface = (ROBO_PLAYER_INTERFACE*)ROBO_PLAYER_BASE;
