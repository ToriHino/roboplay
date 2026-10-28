/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * opm.h
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define OPM_ID_ADDRESS       0x0080
#define OPM_STATUS_REGISTER  0x3FF1
#define OPM_ADDRESS_REGISTER 0x3FF0
#define OPM_DATA_REGISTER    0x3FF1

static const char* g_opm_id_txt = "MCHFM0";
static uint8_t     g_opm_slot;

bool opm_find_slot(void);
uint8_t opm_get_slot(void);

void opm_reset(void);

void    opm_write_register(const uint8_t reg, const uint8_t value);
uint8_t opm_read_status_register(void);
