/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * ssy.h
 *
 * SSY: PETSCII Robots Adlib player
 */

#pragma once

#define MAX_NR_OF_CHANNELS 9
#define NR_OF_REGS         10

#define PAN_SETTING_LEFT  0x10
#define PAN_SETTING_MID   0x30
#define PAN_SETTING_RIGHT 0x20

typedef struct
{
  uint8_t  segment;
  uint8_t* ptr;
} SSY_PTR;

const uint8_t SSY_OPL2_OPERATOR_ORDER[9] = 
{
	0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12
};

const uint8_t SSY_OPL2_INSTRUMENT_REGS[MAX_NR_OF_CHANNELS][NR_OF_REGS] = 
{
	{0xC0, 0x20, 0x40, 0x60, 0x80, 0xE0, 0x63, 0x83, 0x23, 0xE3},
	{0xC1, 0x21, 0x41, 0x61, 0x81, 0xE1, 0x64, 0x84, 0x24, 0xE4},
	{0xC2, 0x22, 0x42, 0x62, 0x82, 0xE2, 0x65, 0x85, 0x25, 0xE5},
	{0xC3, 0x28, 0x48, 0x68, 0x88, 0xE8, 0x6B, 0x8B, 0x2B, 0xEB},
	{0xC4, 0x29, 0x49, 0x69, 0x89, 0xE9, 0x6C, 0x8C, 0x2C, 0xEC},
	{0xC5, 0x2A, 0x4A, 0x6A, 0x8A, 0xEA, 0x6D, 0x8D, 0x2D, 0xED},
	{0xC6, 0x30, 0x50, 0x70, 0x90, 0xF0, 0x73, 0x93, 0x33, 0xF3},
	{0xC7, 0x31, 0x51, 0x71, 0x91, 0xF1, 0x74, 0x94, 0x34, 0xF4},
	{0xC8, 0x32, 0x52, 0x72, 0x92, 0xF2, 0x75, 0x95, 0x35, 0xF5}
};

uint8_t  g_segment_list[256];

uint8_t  g_nr_of_channels;

uint16_t g_temp;

SSY_PTR  g_ptr[MAX_NR_OF_CHANNELS];                         // Current pointer
SSY_PTR  g_loop[MAX_NR_OF_CHANNELS];                        // Loop pointer
SSY_PTR  g_ret[MAX_NR_OF_CHANNELS];                         // Return pointer from a block reference
SSY_PTR  g_refprev[MAX_NR_OF_CHANNELS];	                    // Previous reference pointer

uint8_t  g_wait[MAX_NR_OF_CHANNELS];
uint8_t  g_volprev[MAX_NR_OF_CHANNELS];		                  // Previous volume
uint16_t g_pitch_1[MAX_NR_OF_CHANNELS];		                  // Previous pitch
uint16_t g_pitch_2[MAX_NR_OF_CHANNELS];		                  // Pitch
uint8_t  g_volopl[MAX_NR_OF_CHANNELS];                      // Volume
uint8_t  g_keyoff[MAX_NR_OF_CHANNELS];                      // Keyoff flag
uint8_t  g_opl_regs[MAX_NR_OF_CHANNELS][NR_OF_REGS];
uint8_t  g_opl_prev[MAX_NR_OF_CHANNELS][NR_OF_REGS];

void increment_ptr(uint8_t channel);
