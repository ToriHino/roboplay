/*
 * Frequency to key conversion routine used from VGMPlay by Grauw
 *
 */

#pragma once

#include <stdint.h>

extern uint8_t g_key_code;
extern uint8_t g_key_fraction;

void opm_frequency2key(uint8_t msb, uint8_t lsb);
