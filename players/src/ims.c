/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * ims.c
 *
 * IMS: Iyagi Music Sound
 */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "player_interface.h"

/* =========================================================
 * Constants
 * ========================================================= */

#define IMS_REFRESH_HZ           60.0f

#define IMS_MAX_SONG_INSTRUMENTS 32

#define IMS_MAX_CHANNELS         11
#define IMS_MELODIC_CHANNELS     9
#define IMS_MAX_INSTRUMENTS      64
#define IMS_MAX_BNK_NAMES        672
#define MAX_SEGMENTS             8

/* Header sizes */
#define IMS_TUNE_NAME_SIZE       30
#define IMS_BNK_NAME_SIZE        9
#define IMS_BNK_SIG_SIZE         6
#define IMS_BNK_RECORD_SIZE      30

/* MIDI command bytes */
#define MIDI_NOTE_OFF            0x80
#define MIDI_NOTE_ON             0x90
#define MIDI_AFTERTOUCH          0xA0
#define MIDI_CTRL_CHANGE         0xB0
#define MIDI_PROG_CHANGE         0xC0
#define MIDI_CHAN_PRESSURE       0xD0
#define MIDI_PITCH_BEND          0xE0
#define MIDI_SYSEX               0xF0
#define MIDI_EOX                 0xF7
#define MIDI_OVERFLOW            0xF8
#define MIDI_STOP                0xFC
#define MIDI_ADLIB_CTRL          0x7F
#define MIDI_TEMPO_CTRL          0x00

/* OPL2 registers */
#define OPL2_WAVE_CTRL           0x01
#define OPL2_AMMULTI             0x20
#define OPL2_KSLTL               0x40
#define OPL2_ARDR                0x60
#define OPL2_SLRR                0x80
#define OPL2_FREQLO              0xA0
#define OPL2_KEYONFNUM           0xB0
#define OPL2_AMVIB_RHYTHM        0xBD
#define OPL2_FEEDCON             0xC0
#define OPL2_WAVEFORM            0xE0

/* Masks */
#define OPL2_KEYON_MASK          0x20
#define OPL2_RHYTHM_MASK         0x20
#define OPL2_ENABLE_WAVEFORM     0x20
#define OPL2_KSL_MASK            0xC0
#define OPL2_TL_MASK             0x3F
#define OPL2_TL_MIN              0x3F
#define OPL2_FNUMLSB_MASK        0xFF
#define OPL2_FNUMMSB_MASK        0x03
#define OPL2_BLOCK_SHIFT         2

/* Synth constants */
#define IMS_SILENCE_NOTE         (-12)
#define IMS_MID_PITCH            0x2000u
#define IMS_MAX_VOLUME           0x7F
#define IMS_CARRIER_OFFSET       3
#define IMS_MAX_NOTES            96

/* Rhythm constants */
#define IMS_BASS_DRUM_CH         6
#define IMS_SNARE_CH             7
#define IMS_TOMTOM_CH            8
#define IMS_CYMBAL_CH            9
#define IMS_HIHAT_CH             10
#define IMS_TOMTOM_NOTE          24
#define IMS_TOMTOM_TO_SNARE      7
#define IMS_SNARE_NOTE           (IMS_TOMTOM_NOTE + IMS_TOMTOM_TO_SNARE)

#define OPL_WRITE(reg, val) g_roboplay_interface->opl4_write_fm_1((reg), (val))

/* =========================================================
 * Data structures
 * ========================================================= */

typedef struct {
    uint8_t ammulti, ksltl, ardr, slrr, fbc, waveform;
} IMS_OPL2Op;

typedef struct {
    IMS_OPL2Op mod, car;
} IMS_Instrument;

typedef struct {
    uint8_t  ins_index;
    uint8_t  volume;
    uint8_t  ksltl_cache;
    uint8_t  kon_oct_fnum;
    int8_t   note_cache;
    bool     key_on_cache;
    uint16_t current_pitch_bend;   /* NEW: proper per-channel pitch */
} IMS_Channel;

typedef struct {
    uint8_t  seg;
    uint16_t ofs;
} IMS_PTR;

/* Operator tables */
static const uint8_t g_op_table[9] = {0x00,0x01,0x02,0x08,0x09,0x0A,0x10,0x11,0x12};
static const uint8_t g_drum_op_table[4] = {0x14, 0x12, 0x15, 0x11};

/* IMS frequency table */
static const uint16_t g_fnum_table[192] = 
{
	0x02B2, 0x02B4, 0x02B7, 0x02B9, 0x02BC, 0x02BE, 0x02C1, 0x02C3, 0x02C6, 0x02C9,
	0x02CB, 0x02CE, 0x02D0, 0x02D3, 0x02D6, 0x02D8, 0x02DB, 0x02DD, 0x02E0, 0x02E3,
	0x02E5, 0x02E8, 0x02EB, 0x02ED, 0x02F0, 0x02F3, 0x02F6, 0x02F8, 0x02FB, 0x02FE,
	0x0301, 0x0303, 0x0306, 0x0309, 0x030C, 0x030F, 0x0311, 0x0314, 0x0317, 0x031A,
	0x031D, 0x0320, 0x0323, 0x0326, 0x0329, 0x032B, 0x032E, 0x0331, 0x0334, 0x0337,
	0x033A, 0x033D, 0x0340, 0x0343, 0x0346, 0x0349, 0x034C, 0x034F, 0x0352, 0x0356,
	0x0359, 0x035C, 0x035F, 0x0362, 0x0365, 0x0368, 0x036B, 0x036F, 0x0372, 0x0375,
	0x0378, 0x037B, 0x037F, 0x0382, 0x0385, 0x0388, 0x038C, 0x038F, 0x0392, 0x0395,
	0x0399, 0x039C, 0x039F, 0x03A3, 0x03A6, 0x03A9, 0x03AD, 0x03B0, 0x03B4, 0x03B7,
	0x03BB, 0x03BE, 0x03C1, 0x03C5, 0x03C8, 0x03CC, 0x03CF, 0x03D3, 0x03D7, 0x03DA,
	0x03DE, 0x03E1, 0x03E5, 0x03E8, 0x03EC, 0x03F0, 0x03F3, 0x03F7, 0x03FB, 0x03FE,
	0xFE01, 0xFE03, 0xFE05, 0xFE07, 0xFE08, 0xFE0A, 0xFE0C, 0xFE0E, 0xFE10, 0xFE12,
	0xFE14, 0xFE16, 0xFE18, 0xFE1A, 0xFE1C, 0xFE1E, 0xFE20, 0xFE21, 0xFE23, 0xFE25,
	0xFE27, 0xFE29, 0xFE2B, 0xFE2D, 0xFE2F, 0xFE31, 0xFE34, 0xFE36, 0xFE38, 0xFE3A,
	0xFE3C, 0xFE3E, 0xFE40, 0xFE42, 0xFE44, 0xFE46, 0xFE48, 0xFE4A, 0xFE4C, 0xFE4F,
	0xFE51, 0xFE53, 0xFE55, 0xFE57, 0xFE59, 0xFE5C, 0xFE5E, 0xFE60, 0xFE62, 0xFE64,
	0xFE67, 0xFE69, 0xFE6B, 0xFE6D, 0xFE6F, 0xFE72, 0xFE74, 0xFE76, 0xFE79, 0xFE7B,
	0xFE7D, 0xFE7F, 0xFE82, 0xFE84, 0xFE86, 0xFE89, 0xFE8B, 0xFE8D, 0xFE90, 0xFE92,
	0xFE95, 0xFE97, 0xFE99, 0xFE9C, 0xFE9E, 0xFEA1, 0xFEA3, 0xFEA5, 0xFEA8, 0xFEAA,
	0xFEAD, 0xFEAF
};

static const uint8_t g_note_div12[96] = 
{
	0,0,0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,3,3,3,3,
	4,4,4,4,4,4,4,4,4,4,4,4, 5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6, 7,7,7,7,7,7,7,7,7,7,7,7
};

static const uint8_t g_note_mod12[96] = 
{
	0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11
};

/* =========================================================
 * Globals
 * ========================================================= */

static char     g_tune_name[IMS_TUNE_NAME_SIZE + 1];
static uint8_t  g_tick_beat, g_sound_mode, g_num_channels;
static uint16_t g_basic_tempo;
static uint8_t  g_pitch_range;
static int32_t  g_data_size;

static IMS_PTR  g_midi_start, g_midi_cur, g_midi_end;

static uint32_t g_ticks_per_update, g_midi_counter, g_next_event_tick;
static uint8_t  g_running_status;
static bool     g_ended;


static IMS_Instrument g_instruments[IMS_MAX_INSTRUMENTS];
static uint8_t        g_num_instruments;

static char    g_ims_names[IMS_MAX_SONG_INSTRUMENTS][IMS_BNK_NAME_SIZE];
static uint8_t g_ims_num_instruments;

static int32_t  g_bnk_offset_names, g_bnk_offset_data;
static uint16_t g_bnk_num_total;
static bool     g_bnk_case_sensitive;

static IMS_Channel g_channels[IMS_MAX_CHANNELS];

static uint8_t  g_rhythm_mode, g_amvib_rhythm_cache;
static uint8_t  g_ims_to_slot[IMS_MAX_SONG_INSTRUMENTS];
static char     g_bnk_filename[64];

static uint8_t  g_segment_list[MAX_SEGMENTS];
static uint8_t  g_num_segments;
static uint8_t  g_current_segment;

/* =========================================================
 * Segment pointer
 * ========================================================= */

/* =========================================================
 * Segment selector
 *
 * Switch the current data segment if `seg_index` differs from
 * the active segment and inform the RoboPlay interface.
 * ========================================================= */
static void set_segment(uint8_t seg_index) {
    if (g_current_segment == seg_index) return;
    g_current_segment = seg_index;
    g_roboplay_interface->set_segment(g_segment_list[seg_index]);
}

/* =========================================================
 * Read byte from segment
 *
 * Read one byte from the segment pointer `p`, advancing the
 * cursor and handling segment wrap.
 * ========================================================= */
static uint8_t ptr_read_u8(IMS_PTR *p)
{
    uint8_t val;
    uint8_t *addr;
    set_segment(p->seg);
    addr = (uint8_t *)DATA_SEGMENT_BASE + p->ofs;
    val  = *addr;
    p->ofs++;
    if (p->ofs >= DATA_SEGMENT_SIZE) {
        p->ofs = 0;
        p->seg++;
    }
    return val;
}

/* =========================================================
 * Read 16-bit unsigned
 *
 * Read a little-endian 16-bit unsigned value from pointer `p`.
 * ========================================================= */
static uint16_t ptr_read_u16(IMS_PTR *p)
{
    uint8_t lo, hi;
    lo = ptr_read_u8(p);
    hi = ptr_read_u8(p);
    return (uint16_t)((uint16_t)hi << 8 | lo);
}

/* =========================================================
 * Read 16-bit signed
 *
 * Read a little-endian 16-bit signed value from pointer `p`.
 * ========================================================= */
static int16_t ptr_read_i16(IMS_PTR *p)
{
    return (int16_t)ptr_read_u16(p);
}

/* =========================================================
 * Read 32-bit signed
 *
 * Read a little-endian 32-bit signed value from pointer `p`.
 * ========================================================= */
static int32_t ptr_read_i32(IMS_PTR *p)
{
    uint32_t b0, b1, b2, b3;
    b0 = ptr_read_u8(p);
    b1 = ptr_read_u8(p);
    b2 = ptr_read_u8(p);
    b3 = ptr_read_u8(p);
    return (int32_t)(b0 | ((uint32_t)b1<<8) | ((uint32_t)b2<<16) | ((uint32_t)b3<<24));
}

/* =========================================================
 * Skip bytes in segment pointer
 *
 * Advance pointer `p` by `n` bytes across segment boundaries.
 * ========================================================= */
static void ptr_skip(IMS_PTR *p, uint16_t n)
{
    uint16_t space;
    while (n > 0) {
        space = DATA_SEGMENT_SIZE - p->ofs;
        if (n < space) { p->ofs += n; break; }
        n -= space;
        p->ofs = 0;
        p->seg++;
    }
}

/* =========================================================
 * Skip (32-bit) bytes in segment pointer
 *
 * Advance pointer `p` by up to 32-bit `n` bytes using safe chunks.
 * ========================================================= */
static void ptr_skip_32(IMS_PTR *p, int32_t n)
{
    while (n > 0) {
        uint16_t chunk = (n > 0x7FFF) ? 0x7FFF : (uint16_t)n;
        ptr_skip(p, chunk);
        n -= chunk;
    }
}

/* =========================================================
 * Refresh rate computation
 * ========================================================= */

 static void update_refresh(uint16_t multiplier_q8_8)
{
    uint32_t num;
    uint32_t result;

    num    = (uint32_t)g_basic_tempo * 65536UL / 3600UL;
    result = num * (uint32_t)g_tick_beat;
    result = (result / 256UL) * (uint32_t)multiplier_q8_8;

    g_ticks_per_update = result;
}

/* =========================================================
 * BNK file I/O helpers
 * ========================================================= */

/* =========================================================
 * BNK read helpers: u8
 *
 * Read one unsigned byte from the currently-open BNK file.
 * ========================================================= */
static uint8_t bnk_read_u8(void)
{
    uint8_t v;
    g_roboplay_interface->read(&v, 1);
    return v;
}

/* =========================================================
 * BNK read helpers: u16
 *
 * Read little-endian 16-bit unsigned value from BNK file.
 * ========================================================= */
static uint16_t bnk_read_u16(void)
{
    uint8_t buf[2];
    g_roboplay_interface->read(buf, 2);
    return (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
}

/* =========================================================
 * BNK read helpers: i32
 *
 * Read little-endian 32-bit signed value from BNK file.
 * ========================================================= */
static int32_t bnk_read_i32(void)
{
    uint8_t buf[4];
    g_roboplay_interface->read(buf, 4);
    return (int32_t)( (uint32_t)buf[0]
                    | ((uint32_t)buf[1] << 8)
                    | ((uint32_t)buf[2] << 16)
                    | ((uint32_t)buf[3] << 24));
}

/* =========================================================
 * Case-insensitive string compare
 *
 * Compare `a` and `b` without case; returns same semantics as strcmp.
 * ========================================================= */
static int ims_stricmp(const char *a, const char *b)
{
    uint8_t ca, cb;
    while (*a && *b) {
        ca = (uint8_t)*a;
        cb = (uint8_t)*b;
        if (ca >= 'a' && ca <= 'z') ca = (uint8_t)(ca - 32);
        if (cb >= 'a' && cb <= 'z') cb = (uint8_t)(cb - 32);
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

/* =========================================================
 * Read BNK header only
 *
 * Stores offsets for later single-pass scan.
 * ========================================================= */
static bool bnk_load_info(void)
{
    uint8_t  ver_major, ver_minor;
    char     sig[IMS_BNK_SIG_SIZE];
    uint16_t num_used;
    uint16_t i, found;
    uint16_t idx;
    uint8_t  used;
    char     name1[IMS_BNK_NAME_SIZE];
    char     name2[IMS_BNK_NAME_SIZE];

    ver_major = bnk_read_u8(); (void)ver_major;
    ver_minor = bnk_read_u8(); (void)ver_minor;
    g_roboplay_interface->read(sig, IMS_BNK_SIG_SIZE); (void)sig;

    num_used         = bnk_read_u16(); (void)num_used;
    g_bnk_num_total  = bnk_read_u16();
    g_bnk_offset_names = bnk_read_i32();
    g_bnk_offset_data  = bnk_read_i32();

    /* Auto-detect case sensitivity from first two used entries */
    g_bnk_case_sensitive = false;
    g_roboplay_interface->seek(g_bnk_offset_names, ROBOPLAY_SEEK_START);
    found = 0;
    name1[0] = '\0';
    name2[0] = '\0';

    for (i = 0; i < g_bnk_num_total && found < 2; i++) {
        idx  = bnk_read_u16(); (void)idx;
        used = bnk_read_u8();
        if (found == 0) {
            g_roboplay_interface->read(name1, IMS_BNK_NAME_SIZE);
            name1[IMS_BNK_NAME_SIZE-1] = '\0';
            if (used) found = 1;
        } else {
            g_roboplay_interface->read(name2, IMS_BNK_NAME_SIZE);
            name2[IMS_BNK_NAME_SIZE-1] = '\0';
            if (used) {
                if (ims_stricmp(name1, name2) > 0)
                    g_bnk_case_sensitive = true;
                found = 2;
            }
        }
    }

    return true;
}

/* =========================================================
 * Read one SFMOperator
 *
 * Convert 13 raw BNK bytes to packed OPL2 operator.
 * ========================================================= */
static void read_fm_operator(IMS_OPL2Op *out)
{
    uint8_t ksl, multi, feedback, attack, sustain, eg;
    uint8_t decay, release, outlevel, am, vib, ksr, fm_type;

    ksl      = bnk_read_u8();
    multi    = bnk_read_u8();
    feedback = bnk_read_u8();
    attack   = bnk_read_u8();
    sustain  = bnk_read_u8();
    eg       = bnk_read_u8();
    decay    = bnk_read_u8();
    release  = bnk_read_u8();
    outlevel = bnk_read_u8();
    am       = bnk_read_u8();
    vib      = bnk_read_u8();
    ksr      = bnk_read_u8();
    fm_type  = bnk_read_u8();

    out->ammulti = (uint8_t)((am<<7)|(vib<<6)|(eg<<5)|(ksr<<4)|multi);
    out->ksltl   = (uint8_t)((ksl<<6)|outlevel);
    out->ardr    = (uint8_t)((attack<<4)|decay);
    out->slrr    = (uint8_t)((sustain<<4)|release);
    out->fbc     = (uint8_t)((feedback<<1)|(fm_type^1));
}

/* =========================================================
 * Load BNK instrument
 *
 * Read instrument record at `record_index` into `g_instruments[slot]`.
 * ========================================================= */
static void bnk_read_instrument(uint8_t slot, uint16_t record_index)
{
    int32_t        seek_ofs;
    IMS_Instrument *ins;
    uint8_t        mode, voice_num;

    ins      = &g_instruments[slot];
    seek_ofs = g_bnk_offset_data + (int32_t)record_index * IMS_BNK_RECORD_SIZE;
    g_roboplay_interface->seek(seek_ofs, ROBOPLAY_SEEK_START);

    mode      = bnk_read_u8(); (void)mode;
    voice_num = bnk_read_u8(); (void)voice_num;

    read_fm_operator(&ins->mod);
    read_fm_operator(&ins->car);
    ins->mod.waveform = bnk_read_u8();
    ins->car.waveform = bnk_read_u8();
}

/* =========================================================
 * Resolve instrument names
 *
 * Single-pass scan of BNK name table; load matching instruments
 * and assign slot indices for all IMS instrument names.
 * ========================================================= */
static void resolve_instruments(void)
{
    uint16_t bnk_i;
    uint16_t rec_index;
    uint8_t  used;
    char     bnk_name[IMS_BNK_NAME_SIZE];
    uint8_t  ii;
    int      cmp;
    int32_t  name_pos;

    /* Slot 0 = silence */
    memset(&g_instruments[0], 0, sizeof(g_instruments[0]));
    g_num_instruments = 1;

    for (ii = 0; ii < g_ims_num_instruments; ii++)
        g_ims_to_slot[ii] = 0;  /* default: silence */

    g_roboplay_interface->seek(g_bnk_offset_names, ROBOPLAY_SEEK_START);

    for (bnk_i = 0; bnk_i < g_bnk_num_total; bnk_i++) {
        rec_index = bnk_read_u16();
        used      = bnk_read_u8();
        g_roboplay_interface->read(bnk_name, IMS_BNK_NAME_SIZE);
        bnk_name[IMS_BNK_NAME_SIZE-1] = '\0';

        if (!used) continue;

        /* Check each IMS instrument name for a match */
        for (ii = 0; ii < g_ims_num_instruments; ii++) {
            if (g_bnk_case_sensitive)
                cmp = memcmp(bnk_name, g_ims_names[ii], IMS_BNK_NAME_SIZE-1);
            else
                cmp = ims_stricmp(bnk_name, g_ims_names[ii]);

            if (cmp != 0) continue;

            /* Match: load instrument, save file position, then restore */
            if (g_num_instruments < IMS_MAX_INSTRUMENTS) {
                name_pos = g_bnk_offset_names + (int32_t)(bnk_i + 1) * 12;
                g_ims_to_slot[ii] = g_num_instruments;
                bnk_read_instrument(g_num_instruments, rec_index);
                g_num_instruments++;
                g_roboplay_interface->seek(name_pos, ROBOPLAY_SEEK_START);
            }

            /* Assign same slot to any other IMS entries with the same name */
            {
                uint8_t jj;
                for (jj = ii + 1; jj < g_ims_num_instruments; jj++) {
                    if (g_bnk_case_sensitive)
                        cmp = memcmp(g_ims_names[jj], g_ims_names[ii], IMS_BNK_NAME_SIZE-1);
                    else
                        cmp = ims_stricmp(g_ims_names[jj], g_ims_names[ii]);
                    if (cmp == 0)
                        g_ims_to_slot[jj] = g_ims_to_slot[ii];
                }
            }
            break;
        }
    }
}

/* =========================================================
 * OPL helpers
 * ========================================================= */

/* =========================================================
 * Compute KSL/TL
 *
 * Compute the carrier total level (KSL/TL) for channel `ch`
 * scaled by the current `volume` and cached KSL settings.
 * ========================================================= */
static uint8_t get_ksltl(uint8_t ch) {
    uint8_t  ksltl = g_channels[ch].ksltl_cache;
    uint8_t  volume = g_channels[ch].volume;
    uint16_t amplitude = OPL2_TL_MIN - (ksltl & OPL2_TL_MASK);
    uint16_t ksl_tl = (uint16_t)volume * amplitude;
    ksl_tl = ksl_tl + ksl_tl + IMS_MAX_VOLUME;
    ksl_tl = OPL2_TL_MIN - (ksl_tl / (2u * IMS_MAX_VOLUME));
    ksl_tl |= (ksltl & OPL2_KSL_MASK);
    return (uint8_t)ksltl;
}

/* =========================================================
 * Send operator regs
 *
 * Write all OPL operator registers for the instrument `ins`
 * mapped to channel `ch`.
 * ========================================================= */
static void send_operator(uint8_t ch, const IMS_Instrument *ins)
{
    uint8_t op;

    if (ch < IMS_SNARE_CH || !g_rhythm_mode) {
        if (ch >= 9) return;
        op = g_op_table[ch];

        OPL_WRITE(OPL2_AMMULTI  + op,                   ins->mod.ammulti);
        OPL_WRITE(OPL2_KSLTL    + op,                   ins->mod.ksltl);
        OPL_WRITE(OPL2_ARDR     + op,                   ins->mod.ardr);
        OPL_WRITE(OPL2_SLRR     + op,                   ins->mod.slrr);
        OPL_WRITE(OPL2_FEEDCON  + ch,                   ins->mod.fbc);
        OPL_WRITE(OPL2_WAVEFORM + op,                   ins->mod.waveform);

        g_channels[ch].ksltl_cache = ins->car.ksltl;

        OPL_WRITE(OPL2_AMMULTI  + op + IMS_CARRIER_OFFSET, ins->car.ammulti);
        OPL_WRITE(OPL2_KSLTL    + op + IMS_CARRIER_OFFSET, get_ksltl(ch));
        OPL_WRITE(OPL2_ARDR     + op + IMS_CARRIER_OFFSET, ins->car.ardr);
        OPL_WRITE(OPL2_SLRR     + op + IMS_CARRIER_OFFSET, ins->car.slrr);
        OPL_WRITE(OPL2_WAVEFORM + op + IMS_CARRIER_OFFSET, ins->car.waveform);
    } else {
        op = g_drum_op_table[ch - IMS_SNARE_CH];
        g_channels[ch].ksltl_cache = ins->mod.ksltl;

        OPL_WRITE(OPL2_AMMULTI  + op, ins->mod.ammulti);
        OPL_WRITE(OPL2_KSLTL    + op, get_ksltl(ch));
        OPL_WRITE(OPL2_ARDR     + op, ins->mod.ardr);
        OPL_WRITE(OPL2_SLRR     + op, ins->mod.slrr);
        OPL_WRITE(OPL2_WAVEFORM + op, ins->mod.waveform);
    }
}

/* =========================================================
 * Set frequency
 *
 * Compute OPL f-number/block for `note` on channel `ch` and
 * update OPL frequency registers; optionally key the note on.
 * ========================================================= */
static void set_freq(uint8_t ch, int8_t note, bool key_on) {
    int16_t tNote, signP, t2;
    uint16_t t1, tableOff;
    int16_t tblValue, block;
    uint8_t fnum_lo, kon_hi;

    if (note < 0) note = 0;
    if (note >= IMS_MAX_NOTES) note = IMS_MAX_NOTES - 1;

    /* Full AdLib pitch calculation */
    signP = (int16_t)(g_channels[ch].current_pitch_bend - IMS_MID_PITCH);
    t2 = signP >> 5;                    /* keep 8 MSB of pitch */
    t2 *= g_pitch_range;

    t1 = (uint16_t)note << 8;
    tNote = t1 + t2;

    tNote += (1 << 3);                  /* rounding */
    tNote >>= 4;                        /* 1/16 semitones */

    if (tNote < 0) tNote = 0;
    else if (tNote >= (IMS_MAX_NOTES << 4) - 1)
        tNote = (IMS_MAX_NOTES << 4) - 1;

    tableOff = g_note_mod12[tNote >> 4];
    tableOff <<= 5;
    tableOff += ((tNote << 1) & 31);

    tblValue = (int16_t)g_fnum_table[tableOff >> 1];

    block = (int16_t)g_note_div12[tNote >> 4] - 1;
    if (tblValue < 0) block++;
    if (block < 0) {
        block++;
        tblValue >>= 1;
    }

    g_channels[ch].note_cache = note;
    g_channels[ch].key_on_cache = key_on;

    fnum_lo = (uint8_t)(tblValue & 0xFF);
    kon_hi  = (uint8_t)((block << OPL2_BLOCK_SHIFT) | ((tblValue >> 8) & OPL2_FNUMMSB_MASK));
    g_channels[ch].kon_oct_fnum = kon_hi;

    OPL_WRITE(OPL2_FREQLO + ch, fnum_lo);
    OPL_WRITE(OPL2_KEYONFNUM + ch, (uint8_t)(kon_hi | (key_on ? OPL2_KEYON_MASK : 0)));
}

/* =========================================================
 * Pitch bend
 *
 * Update per-channel pitch-bend value and recompute frequency.
 * ========================================================= */
static void change_pitch(uint8_t ch, uint16_t pitch_bend) {
    if (ch >= IMS_BASS_DRUM_CH && g_rhythm_mode) return;
    g_channels[ch].current_pitch_bend = pitch_bend;
    set_freq(ch, g_channels[ch].note_cache, g_channels[ch].key_on_cache);
}

/* =========================================================
 * Set volume
 *
 * Update cached volume for channel `ch` and write the
 * carrier TL register accordingly.
 * ========================================================= */
static void set_volume(uint8_t ch, uint8_t volume)
{
    uint8_t op;
    if (ch >= 9 && !g_rhythm_mode) return;
    g_channels[ch].volume = volume;
    if (ch < IMS_SNARE_CH || !g_rhythm_mode)
        op = g_op_table[ch] + IMS_CARRIER_OFFSET;
    else
        op = g_drum_op_table[ch - IMS_SNARE_CH];
    OPL_WRITE(OPL2_KSLTL + op, get_ksltl(ch));
}

/* =========================================================
 * Note on (melodic)
 *
 * Trigger melodic note on channel `ch`. If `retrigger` is true
 * force a retrigger even if same note.
 * ========================================================= */
static void note_on_melodic(uint8_t ch, int8_t note, bool retrigger)
{
    if (ch >= 9) return;
    OPL_WRITE(OPL2_KEYONFNUM + ch, g_channels[ch].kon_oct_fnum & (uint8_t)~OPL2_KEYON_MASK);
    g_channels[ch].key_on_cache = false;
    if (note != IMS_SILENCE_NOTE || retrigger)
        set_freq(ch, note, true);
}

/* =========================================================
 * Note off (melodic)
 *
 * Silence melodic channel `ch` (turn key off).
 * ========================================================= */
static void note_off_melodic(uint8_t ch)
{
    if (ch >= 9) return;
    OPL_WRITE(OPL2_KEYONFNUM + ch, g_channels[ch].kon_oct_fnum & (uint8_t)~OPL2_KEYON_MASK);
    g_channels[ch].key_on_cache = false;
}

/* =========================================================
 * Note on (percussive)
 *
 * Trigger percussive instruments mapped to rhythm channels.
 * ========================================================= */
static void note_on_percussive(uint8_t ch, int8_t note)
{
    uint8_t ch_bit;
    ch_bit = (uint8_t)(1u << (4u - (uint8_t)(ch - IMS_BASS_DRUM_CH)));

    g_amvib_rhythm_cache &= (uint8_t)~ch_bit;
    OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
    g_channels[ch].key_on_cache = false;

    if (ch == IMS_TOMTOM_CH) {
        set_freq(IMS_TOMTOM_CH, note, false);
        set_freq(IMS_SNARE_CH,  (int8_t)(note + IMS_TOMTOM_TO_SNARE), false);
        set_freq(IMS_CYMBAL_CH, note, false);
        set_freq(IMS_HIHAT_CH,  (int8_t)(note + IMS_TOMTOM_TO_SNARE), false);
    } else if (ch == IMS_BASS_DRUM_CH) {
        set_freq(ch, note, false);
    }

    g_channels[ch].key_on_cache = true;
    g_amvib_rhythm_cache |= ch_bit;
    OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
}

/* =========================================================
 * Note off (percussive)
 *
 * Turn off the percussive note on rhythm channel `ch`.
 * ========================================================= */
static void note_off_percussive(uint8_t ch)
{
    uint8_t ch_bit;
    ch_bit = (uint8_t)(1u << (4u - (uint8_t)(ch - IMS_BASS_DRUM_CH)));
    g_amvib_rhythm_cache &= (uint8_t)~ch_bit;
    OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
    g_channels[ch].key_on_cache = false;
}

/* =========================================================
 * Rhythm mode
 *
 * Enable or disable OPL rhythm mode and initialise
 * default frequencies for drum channels when enabled.
 * ========================================================= */
static void set_rhythm_mode(uint8_t mode)
{
    if (mode) {
        g_amvib_rhythm_cache |= OPL2_RHYTHM_MASK;
        OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
        set_freq(IMS_TOMTOM_CH, IMS_TOMTOM_NOTE, false);
        set_freq(IMS_SNARE_CH,  IMS_SNARE_NOTE,  false);
        set_freq(IMS_CYMBAL_CH, IMS_TOMTOM_NOTE, false);
        set_freq(IMS_HIHAT_CH,  IMS_SNARE_NOTE,  false);
    } else {
        g_amvib_rhythm_cache &= (uint8_t)~OPL2_RHYTHM_MASK;
        OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
    }
    g_rhythm_mode = mode;
}

/* =========================================================
 * File loading
 * ========================================================= */

/* =========================================================
 * Load file into segments
 *
 * Read the entire IMS file into data segments for in-memory
 * parsing and playback. Manages segment allocation.
 * ========================================================= */
static bool load_file_data(void)
{
    uint8_t  *dest;
    uint16_t  page_left;
    uint16_t  bytes_read;
    uint16_t  remaining;
    uint16_t  chunk;
    uint8_t  *src;

    dest      = (uint8_t *)DATA_SEGMENT_BASE;
    page_left = DATA_SEGMENT_SIZE;

    g_num_segments    = 0;
    g_current_segment = 0;
    g_segment_list[g_num_segments] = START_SEGMENT_INDEX;
    g_roboplay_interface->set_segment(g_segment_list[g_num_segments]);
    g_num_segments++;

    do {
        bytes_read = g_roboplay_interface->read(
                         (void *)READ_BUFFER, READ_BUFFER_SIZE);
        remaining  = bytes_read;
        src        = (uint8_t *)READ_BUFFER;

        while (remaining > 0) {
            chunk = (remaining < page_left) ? remaining : page_left;
            memcpy(dest, src, chunk);
            src       += chunk;
            dest      += chunk;
            remaining -= chunk;
            page_left -= chunk;

            if (page_left == 0 && remaining > 0) {
                if (g_num_segments >= MAX_SEGMENTS) return false;
                g_segment_list[g_num_segments] =
                    g_roboplay_interface->get_new_segment();
                g_roboplay_interface->set_segment(
                    g_segment_list[g_num_segments]);
                g_num_segments++;
                dest      = (uint8_t *)DATA_SEGMENT_BASE;
                page_left = DATA_SEGMENT_SIZE;
            }
        }
    } while (bytes_read == READ_BUFFER_SIZE);

    g_current_segment = 0;
    g_roboplay_interface->set_segment(g_segment_list[0]);
    return true;
}

/* =========================================================
 * Build BNK filename
 *
 * Replace the extension of `ims_filename` with ".bnk" and
 * store the result in `g_bnk_filename`.
 * ========================================================= */
static void build_bnk_filename(const char *ims_filename)
{
    const char *p;
    const char *last_dot;
    const char *last_sep;
    uint8_t     base_len;

    p         = ims_filename;
    last_dot  = NULL;
    last_sep  = NULL;

    while (*p) {
        if (*p == '.') last_dot  = p;
        if (*p == '/' || *p == '\\') last_sep = p;
        p++;
    }

    /* Use dot position if it's after the last separator (i.e. in filename) */
    if (last_dot && last_dot > last_sep) {
        base_len = (uint8_t)(last_dot - ims_filename);
    } else {
        base_len = (uint8_t)(p - ims_filename);
    }

    if (base_len > (uint8_t)(sizeof(g_bnk_filename) - 5))
        base_len = (uint8_t)(sizeof(g_bnk_filename) - 5);

    memcpy(g_bnk_filename, ims_filename, base_len);
    memcpy(g_bnk_filename + base_len, ".bnk", 5);
}

/* =========================================================
 * Parse IMS
 *
 * Parse IMS file header, MIDI data boundaries and instrument
 * table from in-memory segment data.
 * ========================================================= */
static bool parse_ims(void)
{
    IMS_PTR  p;
    uint8_t  ver_major, ver_minor;
    uint8_t  i, k;
    uint8_t  sig1, sig2;
    uint16_t num_ins;

    p.seg = 0;
    p.ofs = 0;

    ver_major = ptr_read_u8(&p); (void)ver_major;
    ver_minor = ptr_read_u8(&p); (void)ver_minor;

    ptr_read_i32(&p);   /* tuneId */

    for (i = 0; i < IMS_TUNE_NAME_SIZE; i++)
        g_tune_name[i] = (char)ptr_read_u8(&p);
    g_tune_name[IMS_TUNE_NAME_SIZE] = '\0';

    g_tick_beat = ptr_read_u8(&p);
    ptr_read_u8(&p);    /* beatMeasure -- unused */
    ptr_read_i32(&p);   /* totalTick */
    g_data_size = ptr_read_i32(&p);
    ptr_read_i32(&p);   /* nrCommand */
    ptr_skip(&p, 8);    /* filler */
    g_sound_mode  = ptr_read_u8(&p);
    g_pitch_range = ptr_read_u8(&p);
    g_basic_tempo = ptr_read_u16(&p);
    ptr_skip(&p, 8);    /* filler2 */

    g_num_channels = g_sound_mode ? IMS_MAX_CHANNELS : IMS_MELODIC_CHANNELS;

    /* Save MIDI data start position */
    g_midi_start = p;
    g_midi_cur   = p;

    /* Skip to end of MIDI data (may be > 64KB) */
    ptr_skip_32(&p, g_data_size);
    g_midi_end = p;

    /* Parse IMS instrument table */
    sig1 = ptr_read_u8(&p);
    sig2 = ptr_read_u8(&p);
    if (sig1 != 0x77 || sig2 != 0x77) return false;

    num_ins = ptr_read_u16(&p);
    if (num_ins > IMS_MAX_SONG_INSTRUMENTS)
        num_ins = IMS_MAX_SONG_INSTRUMENTS;
    g_ims_num_instruments = (uint8_t)num_ins;

    for (i = 0; i < g_ims_num_instruments; i++) {
        for (k = 0; k < IMS_BNK_NAME_SIZE; k++)
            g_ims_names[i][k] = (char)ptr_read_u8(&p);
        g_ims_names[i][IMS_BNK_NAME_SIZE-1] = '\0';
    }

    return true;
}

/* =========================================================
 * MIDI event processing (called each tick when delay expires)
 *
 * Reads events from g_midi_cur until a non-zero delay is consumed
 * or the song ends.
 * ========================================================= */

/* =========================================================
 * Read next MIDI delay
 *
 * Consume MIDI delta-time bytes from `g_midi_cur` and update
 * `g_next_event_tick`. Returns false on end-of-data.
 * ========================================================= */
static bool read_next_delay(void)
{
    uint8_t b;

    for (;;) {
        if (g_midi_cur.seg > g_midi_end.seg ||
            (g_midi_cur.seg == g_midi_end.seg &&
             g_midi_cur.ofs >= g_midi_end.ofs)) {
            g_ended = true;
            return false;
        }

        b = ptr_read_u8(&g_midi_cur);

        if (b == MIDI_OVERFLOW) {
            g_next_event_tick += 240UL;
        } else {
            g_next_event_tick += (uint32_t)b;
            return true;
        }
    }
}

/* =========================================================
 * Dispatch MIDI event
 *
 * Handle one MIDI event whose command byte has already been read.
 * ========================================================= */
static void dispatch_event(uint8_t cmd, uint8_t ch, uint8_t p1)
{
    uint8_t  p2;
    uint16_t pitch_bend;
    uint8_t  slot;

    switch (cmd) {

    case MIDI_NOTE_OFF:
        p2 = ptr_read_u8(&g_midi_cur);
        if (ch < IMS_BASS_DRUM_CH || !g_rhythm_mode)
            note_off_melodic(ch);
        else
            note_off_percussive(ch);
        if (p2 > 0) {
            /* Note Retrigger: replay with new velocity */
            if (ch < IMS_BASS_DRUM_CH || !g_rhythm_mode)
                note_on_melodic(ch, (int8_t)p1, true);
            else
                note_on_percussive(ch, (int8_t)p1);
            set_volume(ch, (uint8_t)(p2 & IMS_MAX_VOLUME));
        }
        break;

    case MIDI_NOTE_ON:
        p2 = ptr_read_u8(&g_midi_cur);
        if (p2 == 0) {
            if (ch < IMS_BASS_DRUM_CH || !g_rhythm_mode)
                note_off_melodic(ch);
            else
                note_off_percussive(ch);
        } else {
            if (ch < IMS_BASS_DRUM_CH || !g_rhythm_mode)
                note_on_melodic(ch, (int8_t)p1, false);
            else
                note_on_percussive(ch, (int8_t)p1);
            set_volume(ch, (uint8_t)(p2 & IMS_MAX_VOLUME));
        }
        break;

    case MIDI_AFTERTOUCH:
        set_volume(ch, (uint8_t)(p1 & IMS_MAX_VOLUME));
        break;

    case MIDI_CTRL_CHANGE:
        ptr_read_u8(&g_midi_cur);   /* ignored */
        break;

    case MIDI_PROG_CHANGE:
        if (p1 < g_ims_num_instruments) {
            slot = g_ims_to_slot[p1];
            g_channels[ch].ins_index = slot;
            send_operator(ch, &g_instruments[slot]);
        }
        break;

    case MIDI_CHAN_PRESSURE:
        break;

    case MIDI_PITCH_BEND:
        p2 = ptr_read_u8(&g_midi_cur);
        pitch_bend = (uint16_t)p1 | ((uint16_t)p2 << 7);
        change_pitch(ch, pitch_bend);
        break;

    case MIDI_SYSEX:
        if (p1 == MIDI_ADLIB_CTRL) {
            uint8_t  ii, ff;
            uint16_t mult_q8_8;
            ptr_read_u8(&g_midi_cur);           /* sub-command 0x00 */
            ii        = ptr_read_u8(&g_midi_cur);
            ff        = ptr_read_u8(&g_midi_cur);
            ptr_read_u8(&g_midi_cur);           /* 0xF7 EOX */
            mult_q8_8 = (uint16_t)((uint16_t)ii << 8) | (uint16_t)(ff << 1);
            update_refresh(mult_q8_8);
        } else {
            do {
                p2 = ptr_read_u8(&g_midi_cur);
            } while (p2 != MIDI_EOX &&
                     !(g_midi_cur.seg > g_midi_end.seg ||
                       (g_midi_cur.seg == g_midi_end.seg &&
                        g_midi_cur.ofs >= g_midi_end.ofs)));
        }
        break;

    case MIDI_STOP:
        g_ended = true;
        break;

    default:
        break;
    }
}

/* =========================================================
 * Process pending MIDI events
 *
 * Process all MIDI events scheduled at or before the current
 * tick accumulator (`g_midi_counter`) and prefetch the next delay.
 * ========================================================= */
static void process_midi_events(void)
{
    uint8_t  byte, cmd, ch, p1;
    uint32_t cur_tick;

    cur_tick = g_midi_counter >> 16;   /* integer ticks elapsed */

    while (!g_ended && cur_tick >= g_next_event_tick) {
        /* Check end of data */
        if (g_midi_cur.seg > g_midi_end.seg ||
            (g_midi_cur.seg == g_midi_end.seg &&
             g_midi_cur.ofs >= g_midi_end.ofs)) {
            g_ended = true;
            return;
        }

        /* Read command byte (delay already consumed by read_next_delay) */
        byte = ptr_read_u8(&g_midi_cur);

        if (byte >= 0x80 && byte < 0xF0) {
            g_running_status = byte;
            cmd = byte & 0xF0;
            ch  = byte & 0x0F;
            p1  = ptr_read_u8(&g_midi_cur);
        } else if (byte < 0x80) {
            cmd = g_running_status & 0xF0;
            ch  = g_running_status & 0x0F;
            p1  = byte;
        } else if (byte == MIDI_SYSEX) {
            cmd = MIDI_SYSEX;
            ch  = 0;
            p1  = ptr_read_u8(&g_midi_cur);
        } else {
            cmd = byte;
            ch  = 0;
            p1  = 0;
        }

        if (cmd < 0xF0 && ch >= IMS_MAX_CHANNELS) {
            if (cmd == MIDI_NOTE_OFF  || cmd == MIDI_NOTE_ON ||
                cmd == MIDI_CTRL_CHANGE || cmd == MIDI_PITCH_BEND)
                ptr_read_u8(&g_midi_cur);
        } else {
            dispatch_event(cmd, ch, p1);
        }

        if (g_ended) return;

        /* Read delay before next event */
        if (!read_next_delay()) return;
    }
}

/* =========================================================
 * RoboPlay player interface
 * ========================================================= */

/* =========================================================
 * Player interface: rewind helper
 *
 * Reset playback state to the start of the song (subsong ignored).
 * ========================================================= */
static void ims_rewind(const uint8_t subsong);

/* =========================================================
 * Player interface: load
 *
 * Load an IMS file and its associated BNK, parse headers and
 * prepare instruments and channels for playback.
 * ========================================================= */
bool load(const char *file_name)
{
    uint8_t i;

    g_tune_name[0]    = '\0';
    g_num_instruments = 0;
    g_ims_num_instruments = 0;

    /* Load IMS into segments */
    g_roboplay_interface->open(file_name, false);
    if (!load_file_data()) {
        g_roboplay_interface->close();
        return false;
    }
    g_roboplay_interface->close();

    /* Parse header and instrument table from segments */
    if (!parse_ims()) return false;

    /* Try same-name .bnk first (e.g. YOUNG.bnk next to YOUNG.IMS).
     * If not found, fall back to standard.bnk from the framework folder. */
    build_bnk_filename(file_name);

    if (!g_roboplay_interface->exists(g_bnk_filename)) {
        memcpy(g_bnk_filename, "standard.bnk", 13);
        g_roboplay_interface->open(g_bnk_filename, true);
    } else {
        g_roboplay_interface->open(g_bnk_filename, false);
    }

    if (!bnk_load_info()) {
        g_roboplay_interface->close();
        return false;
    }
    resolve_instruments();
    g_roboplay_interface->close();

    /* Pre-load default instrument 0 into all channels */
    for (i = 0; i < IMS_MAX_CHANNELS; i++)
        g_channels[i].ins_index = 0;

    return true;
}

/* =========================================================
 * Player interface: update
 *
 * Advance playback state by one refresh tick and process
 * any MIDI events due at the new time.
 * ========================================================= */
bool update(void)
{
    if (g_ended) return false;

    /* Advance the tick accumulator by one update's worth of ticks */
    g_midi_counter += g_ticks_per_update;

    /* Process all events due at or before the current tick */
    process_midi_events();

    return !g_ended;
}

/* =========================================================
 * Player interface: rewind
 *
 * Public wrapper that resets playback to the start.
 * ========================================================= */
static void ims_rewind(const uint8_t subsong) {
    uint8_t i;
    (void)subsong;

    g_midi_cur = g_midi_start;
    g_midi_counter = 0;
    g_next_event_tick = 0;
    g_running_status = 0;
    g_ended = false;
    g_amvib_rhythm_cache = 0;
    g_rhythm_mode = 0;

    for (i = 0; i < IMS_MAX_CHANNELS; i++) {
        g_channels[i].volume = IMS_MAX_VOLUME;
        g_channels[i].ksltl_cache = 0;
        g_channels[i].kon_oct_fnum = 0;
        g_channels[i].note_cache = 0;
        g_channels[i].key_on_cache = false;
        g_channels[i].current_pitch_bend = IMS_MID_PITCH;
        g_channels[i].ins_index = 0;
    }

    g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL2);
    OPL_WRITE(OPL2_WAVE_CTRL, OPL2_ENABLE_WAVEFORM);
    set_rhythm_mode(g_sound_mode);

    update_refresh(0x0100u);
    read_next_delay();
}

/* =========================================================
 * Player interface: rewind (public)
 *
 * Public entry point to rewind playback to the beginning.
 * ========================================================= */
void rewind(const uint8_t subsong)
{
    ims_rewind(subsong);
}

/* =========================================================
 * Player interface: command
 *
 * Handle player-specific commands (currently unused).
 * ========================================================= */
void command(const uint8_t id)
{
    (void)id;
}

/* =========================================================
 * Player interface: get_refresh
 *
 * Return the nominal refresh rate in Hz used by the UI.
 * ========================================================= */
float get_refresh(void)
{
    return IMS_REFRESH_HZ;
}

/* =========================================================
 * Player interface: get_subsongs
 *
 * Return number of subsongs (not supported for IMS).
 * ========================================================= */
uint8_t get_subsongs(void)
{
    return 0;
}

/* =========================================================
 * Player interface: get_player_info
 *
 * Return a short string identifying this player implementation.
 * ========================================================= */
char *get_player_info(void)
{
    return "IMS (Iyagi Music Sound) player V1.0 by RoboSoft Inc.";
}

/* =========================================================
 * Player interface: get_title
 *
 * Return the current tune title parsed from the IMS header.
 * ========================================================= */
char *get_title(void)
{
    return g_tune_name;
}

/* =========================================================
 * Player interface: get_author
 *
 * Return author string (not provided by IMS format).
 * ========================================================= */
char *get_author(void)
{
    return "-";
}

/* =========================================================
 * Player interface: get_description
 *
 * Return a descriptive string for the currently-loaded song.
 * ========================================================= */
char *get_description(void)
{
    return g_bnk_filename;
}
