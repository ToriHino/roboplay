/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * midi-pac.h
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool midi_pac_detect(void);
void midi_pac_reset(void);
void midi_pac_restore(void);

void midi_pac_send_data(uint8_t data);
void midi_pac_send_data_2(uint8_t data_1, uint8_t data_2);
void midi_pac_send_data_3(uint8_t data_1, uint8_t data_2, uint8_t data_3);

void midi_pac_set_address(uint8_t address);
void midi_pac_data_out_wait(uint8_t data);
