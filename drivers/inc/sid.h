/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * scc.h
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
  SID_TYPE_6581,
  SID_TYPE_8580
} SID_TYPE;

bool sid_detect(void);
SID_TYPE get_sid_type(void);

void sid_reset(void);
void sid_write(const uint8_t reg, const uint8_t data);
uint8_t sid_read(const uint8_t reg);
