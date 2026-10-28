/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * opl4.h
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint8_t tremolo;
  uint8_t vibrato;
  uint8_t sustain;
  uint8_t ksr;
  uint8_t freq_multiplier;
  uint8_t key_scale_level;
  uint8_t output_level;
  uint8_t attack_rate;
  uint8_t decay_rate;
  uint8_t sustain_level;
  uint8_t release_rate;
  uint8_t wave_select;
} opl4_cell_patch;


bool opl4_detect(void);
void opl4_reset(void);

uint8_t opl4_sample_ram_banks(void);

void opl4_set_refresh(float refresh);

void opl4_write_fm_register_array_1(const uint8_t reg, const uint8_t value);
void opl4_write_fm_register_array_2(const uint8_t reg, const uint8_t value);

void opl4_write_wave_register(const uint8_t reg, const uint8_t value);
void opl4_write_wave_data(const uint8_t *const data, const uint16_t size);

uint8_t opl4_read_status_register(void);
uint8_t opl4_read_fm_register_array_1(const uint8_t reg);
uint8_t opl4_read_fm_register_array_2(const uint8_t reg);
uint8_t opl4_read_wave_register(const uint8_t reg);

void opl4_wait_for_ld(void);

void opl4_play_drum_on_wave(const uint8_t drums);