/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * soundstar.h
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool soundstar_find_slot(void);
void soundstar_reset(void);

bool soundstar_detected(void);
uint8_t soundstar_get_slot(void);
uint8_t soundstar_get_major_version(void);
uint8_t soundstar_get_minor_version(void);

void soundstar_reset(void);

void soundstar_write(const uint8_t reg, const uint8_t value);

