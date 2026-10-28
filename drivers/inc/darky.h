/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * darky.h
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool darky_detect(void);
uint8_t darky_get_version(void);

void darky_reset(void);
void darky_restore(void);

void epsg1_write(const uint8_t reg, const uint8_t value);
uint8_t epsg1_read(const uint8_t reg);

void epsg2_write(const uint8_t reg, const uint8_t value);
uint8_t epsg2_read(const uint8_t reg);

