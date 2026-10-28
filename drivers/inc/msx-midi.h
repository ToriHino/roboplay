/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * msx_midi.h
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool msx_midi_detect(void);
void msx_midi_reset(void);
void msx_midi_restore(void);

void msx_midi_send_data(uint8_t data);
void msx_midi_send_data_2(uint8_t data_1, uint8_t data_2);
void msx_midi_send_data_3(uint8_t data_1, uint8_t data_2, uint8_t data_3);
