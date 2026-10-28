/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * msx-music.h
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool msx_music_detect(void);
uint8_t msx_music_get_slot(void);

void msx_music_reset(void);
void msx_music_write_register(const uint8_t reg, const uint8_t value);

bool msx_music_test_slot(uint8_t slot);
bool msx_music_test_sub(uint8_t slot);
