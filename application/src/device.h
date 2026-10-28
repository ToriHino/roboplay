/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * devices.h
 */

#pragma once

#include <stdint.h>

#include "drivers/inc/vdp.h"

#include "drivers/inc/opl4.h"
#include "drivers/inc/scc.h"
#include "drivers/inc/opm.h"
#include "drivers/inc/psg.h"
#include "drivers/inc/midi-pac.h"
#include "drivers/inc/msx-midi.h"
#include "drivers/inc/darky.h"
#include "drivers/inc/soundstar.h"
#include "drivers/inc/msx-audio.h"
#include "drivers/inc/msx-music.h"
#include "drivers/inc/sid.h"

#include "players/inc/player_interface.h"

extern float g_refresh;

void init_devices(void);
void reset_devices(void);
void restore_devices(void);

bool opl4_detected(void);

void set_opl4_mode(const roboplay_opl_mode mode);

void write_opl4_fm_1(const uint8_t reg, const uint8_t value);
void write_opl4_fm_2(const uint8_t reg, const uint8_t value);

void write_opm_fm(const uint8_t reg, const uint8_t value);
void write_msx_audio_fm(const uint8_t reg, const uint8_t value);
void write_msx_music_fm(const uint8_t reg, const uint8_t value);

void init_refresh(void);
void wait_for_refresh(void);
void set_refresh(void);