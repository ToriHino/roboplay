/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * vgm_org.h
 *
 * VGM: Video Game Music player on original device(s)
 */

#pragma once

#include <stdint.h>

#define VGM_GZIP_MIN    8           /* minimum size for GZip header */
#define VGM_HEADER_MIN  84          /* minimum size for header (till YM3812 clock field) */
#define VGM_HEADER_ID   "Vgm "      /* header ident */
#define OFFSET_EOF      0x04        /* End Of File offset */
#define OFFSET_GD3      0x14        /* GD3 tag offset */
#define OFFSET_LOOP     0x1C        /* Loop offset */
#define OFFSET_DATA     0x34        /* VGM data offset */
#define OFFSET_LOOPBASE 0x7E        /* Loop base offset */
#define OFFSET_LOOPMOD  0x7F        /* Loop modifier offset */
#define VGM_DUAL_BIT    0x40000000  /* VGM flag for dual-chip feature */
#define GD3_HEADER_ID   "Gd3 "      /* GD3 tag header ident */

#define CMD_YM2413      0x51        /* YM2413, write value <dd> to register <aa> */
#define CMD_Y8950       0x5C        /* Y8950, write value <dd> to register <aa> */
#define CMD_WAIT        0x61        /* Wait <n> samples, <n> can range from 0 to 65535 (approx 1.49 seconds) */
#define CMD_WAIT_735    0x62        /* wait 735 samples (60th of a second) */
#define CMD_WAIT_882    0x63        /* wait 882 samples (50th of a second) */
#define CMD_DATA_END    0x66        /* end of sound data */
#define CMD_WAIT_N      0x70        /* wait <n+1> samples, <n> can range from 0 to 15 */
#define CMD_PSG         0xA0        /* write value dd to register aa */

#define CMD_SCC         0xD2        /* SCC port pp, write value dd to register aa */

#define VGM_FREQUENCY   44100.0     /* VGM base sample frequency */
#define VGM_REPLAY_FREQ 735         /* VGM replay frequency */
#define VGM_STEP_VALUE  60          /* VGM_FREQUENCY / VGM_REPLAY_FREQ */

#define SCC_WAVEFORM    0x00
#define SCC_FREQUENCY   0x80
#define SCC_VOLUME      0x8A
#define SCC_ON_OFF      0x8F

typedef enum
{
    VGM_DEVICE_ID_YM2413,
    VGM_DEVICE_ID_Y8950,
    VGM_DEVICE_ID_K051649,
    VGM_DEVICE_ID_AY8910,

    VGM_DEVICE_ID_CARD
} VGM_DEVICE_ID;

typedef struct 
{
    uint32_t vgmIdent;
    uint32_t eofOffset;
    uint32_t version;
    uint32_t sn76589Clock;

    uint32_t ym2413Clock;
    uint32_t gd3Offset;
    uint32_t totalNrSamples;
    uint32_t loopOffset;

    uint32_t loopNrSamples;
    uint32_t rate;
    uint8_t  snFeedback;
    uint16_t snw;
    uint8_t  sf;
    uint32_t ym2612Clock;

    uint32_t ym2151Clock;
    uint32_t vgmDataOffset;
    uint32_t segaPCMClock;
    uint32_t spcmInterface;

    uint32_t rf5c68Clock;
    uint32_t ym2203Clock;
    uint32_t ym2608Clock;
    uint32_t ym2610bClock;

    uint32_t ym3812Clock;
    uint32_t ym3526Clock;
    uint32_t y8950Clock;
    uint32_t ymf262Clock;

    uint32_t ymf278bClock;
    uint32_t ymf271Clock;
    uint32_t ymz280bClock;
    uint32_t rf5c164Clock;

    uint32_t pwmClock;
    uint32_t ay8910Clock;
    uint8_t  ayType;
    uint8_t  ayFlags;
    uint8_t  ym2203AyFlags;
    uint8_t  ym2608AyFlags;

    uint8_t  volumeModifier;
    uint8_t  reserved;
    uint8_t  loopBase;

    uint8_t  loopModifier;

    uint32_t gbDMGClock;
    uint32_t nesAPUClock;
    uint32_t multiPCMClock;
    uint32_t uPD7759Clock;
    uint32_t okiM6258Clock;
    uint8_t  okiM6258Flags;
    uint8_t  k054539Flags;
    uint8_t  c140Type;
    uint8_t  reserved_2;
    uint32_t okiM6295Clock;
    uint32_t k051649Clock;
    uint32_t k054539Clock;
    uint32_t huC6280Clock;
    uint32_t c140Clock;
    uint32_t k053260Clock;
    uint32_t pokeyClock;
    uint32_t qSoundClock;

    uint32_t SCSP;

    uint32_t extraOffset;

    uint32_t wSwanClock;
    uint32_t vsuClock;
    uint32_t saa1099Clock;
    uint32_t es5503Clock;
    uint32_t es5506Clock;
    uint8_t  es5503Channels;
    uint8_t  es5506Channels;
    uint8_t  c352ClockDivider;
    uint8_t  esReserved;
    uint32_t x1_010Clock;
    uint32_t C352Clock;
    uint32_t GA20Clock;
    uint8_t  reserved_3;
} VGM_HEADER;

uint8_t g_segment_list[256];
uint8_t g_segment_index;

uint32_t g_index_pointer;
uint32_t g_vgm_size;

uint16_t g_delay_counter;

VGM_HEADER g_vgm_header;

VGM_DEVICE_ID g_device_type;

uint32_t g_clock;

uint8_t* g_song_data;

uint8_t get_vgm_data(void);
