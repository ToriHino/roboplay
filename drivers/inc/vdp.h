/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * vdp.h
 */

#pragma once

#include <stdint.h>

#define VDP_REFRESH_60_HZ 0
#define VDP_REFRESH_50_HZ 1

void init_vdp_device(void);

uint8_t get_vdp_refresh(void);
uint8_t vdp_read_status_register(void);