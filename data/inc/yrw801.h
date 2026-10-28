/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * yrw801.h
 */

#pragma once

#include <stdint.h>

typedef struct
{
    uint16_t tone;
    int16_t  pitch_offset;
    uint8_t  key_scaling;
    int8_t   panpot;
    uint8_t  vibrato;
    uint8_t  tone_attenuate;
    uint8_t  volume_factor;
    uint8_t  reg_lfo_vibrato;
    uint8_t  reg_attack_decay1;
    uint8_t  reg_level_decay2;
    uint8_t  reg_release_correction;
    uint8_t  reg_tremolo;   
} YRW801_WAVE_DATA;

typedef struct
{
    uint8_t key_min;
    uint8_t key_max;

    YRW801_WAVE_DATA wave_data;
} YRW801_REGION_DATA;
