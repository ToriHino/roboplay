/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * scc.h
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SCC_NR_OF_CHANNELS 5
#define SCC_WAVEFORM_SIZE  32

#define SCC_PAGE 0x80

#define SCC_BANK_SELECT   0x9000
#define SCC_REGISTER_BASE 0x9800

typedef struct
{
    uint8_t  *waveform[SCC_NR_OF_CHANNELS - 1];    /* Channel 4 and 5 are shared */
    uint16_t *frequency[SCC_NR_OF_CHANNELS];
    uint8_t  *volume[SCC_NR_OF_CHANNELS];
    uint8_t  *voice_on;
} SCC_REGISTERS;

static const SCC_REGISTERS g_scc_registers =
{
    { 0x9800, 0x9820, 0x9840, 0x9860 },
    { 0x9880, 0x9882, 0x9884, 0x9886, 0x9888 },
    { 0x988A, 0x988B, 0x988C, 0x988D, 0x988E },
      0x988F
};

static uint8_t g_scc_slot;
static uint8_t g_original_slot;

static uint8_t g_waveform_buffer[SCC_WAVEFORM_SIZE];

bool scc_find_slot(void);
void scc_reset(void);

bool scc_detected(void);
uint8_t scc_get_slot(void);

void scc_set_waveform(uint8_t channel, uint8_t *waveform);
void scc_set_frequency(uint8_t channel, uint16_t frequency);
void scc_set_volume(uint8_t channel, uint8_t volume);

void scc_write_register(uint8_t reg, uint8_t value);
