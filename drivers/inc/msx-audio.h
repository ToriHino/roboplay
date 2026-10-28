/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * msx-audio.h
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool msx_audio_detect(void);
void msx_audio_reset(void);

uint8_t msx_audio_read_status_register(void);
void    msx_audio_write_register(const uint8_t reg, const uint8_t value);
void    msx_audio_write_adpcm_data(const uint8_t *const data, const uint16_t size);