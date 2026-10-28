/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * psg.h
 */

#pragma once

#include <stdint.h>

void psg_reset(void);

void psg_write(const uint8_t reg, const uint8_t value);
uint8_t psg_read(const uint8_t reg);

void psg2_write(const uint8_t reg, const uint8_t value);
uint8_t psg2_read(const uint8_t reg);
