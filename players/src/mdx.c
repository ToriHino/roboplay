/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mdx.c
 *
 * MDX: MXDRV music format for the Sharp X68000
 */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "player_interface.h"

/* =========================================================
 * Limits
 * ========================================================= */

#define MDX_MAX_CHANNELS    16          /* 8 FM + 1 PCM + 7 Mercury ADPCM */
#define MDX_FM_CHANNELS     8
#define MAX_SEGMENTS        8           /* 8 * 16KB = 128KB max file size */

#define PDX_NUM_SAMPLES     96
#define OPL_WAVE_MEMORY     0x200000

#define MDX_TITLE_END_0     0x0D
#define MDX_TITLE_END_1     0x0A
#define MDX_TITLE_END_2     0x1A

/* Pitch fixed point: semitone index in bits [14:8], fraction in [7:0].
 * Matches the reference's "(5 + (note<<6) + detune) << 8" scale, where
 * note<<6 (i.e. *64) is itself already 1/64-semitone-per-detune-unit
 * pre-multiplied; the final <<8 here additionally gives headroom for
 * portamento accumulation at finer-than-OPM-KF resolution. */
#define PITCH_NOTE_BIAS     5

/* =========================================================
 * OPM register addresses
 * ========================================================= */

#define OPM_NOISE           0x0F
#define OPM_KEYON           0x08
#define OPM_LFRQ            0x18
#define OPM_PMD_AMD         0x19
#define OPM_CT_W            0x1B
#define OPM_RL_FB_CON       0x20
#define OPM_KC              0x28
#define OPM_KF              0x30
#define OPM_PMS_AMS         0x38
#define OPM_DT1_MUL         0x40
#define OPM_TL              0x60
#define OPM_KS_AR           0x80
#define OPM_AME_D1R         0xA0
#define OPM_DT2_D2R         0xC0
#define OPM_D1L_RR          0xE0

/* Carrier operator bitmask per CON algorithm (0-7).
 * Bit i corresponds to OPM operator i (stride-8 order), matching
 * the con_masks[] table in fm_opm_driver_set_tl(). */
static const uint8_t g_carrier_mask[8] = {
    0x08, 0x08, 0x08, 0x08,      /* CON 0-3: op3 only */
    0x0C,                        /* CON 4:   op2+op3 */
    0x0E, 0x0E,                  /* CON 5-6: op1+op2+op3 */
    0x0F,                        /* CON 7:   all */
};

/* MDX MML volume (0-15) to OPM TL attenuation lookup, plus an extended
 * 128+ range treated as a direct TL value (vol - 128). Matches
 * mdx_volume_to_opm() in the reference driver exactly. */
static const uint8_t g_volume_to_tl[16] = {
    0x2A, 0x28, 0x25, 0x22,
    0x20, 0x1D, 0x1A, 0x18,
    0x15, 0x12, 0x10, 0x0D,
    0x0A, 0x08, 0x05, 0x02,
};

/* =========================================================
 * Sample table
 * ========================================================= */

/* Header-table slot (in OPL4's 384-entry instrument header area) for
 * each of the 96 possible PDX samples; 0xFFFF marks an empty/unused
 * slot (zero-length in the PDX file). Indexed directly by MDX note
 * value when the PCM channel (P) gets a note-on. */
static uint16_t g_pdx_header_slot[PDX_NUM_SAMPLES];
static bool     g_pdx_loaded;

static uint8_t volume_to_tl(uint8_t vol)
{
    if (vol <= 15u)       return g_volume_to_tl[vol];
    else if (vol >= 128u) return (uint8_t)(vol - 128u);
    return 0u;
}

/* =========================================================
 * Segment-aware byte pointer
 * ========================================================= */

typedef struct {
    uint8_t  seg;
    uint16_t ofs;
} MDX_PTR;

/* =========================================================
 * Software LFO state (pitch/vibrato or amplitude/tremolo)
 * ========================================================= */

typedef struct {
    uint8_t  enable;
    uint8_t  waveform;     /* 0=sawtooth, 1=square, 2=triangle */
    uint16_t period;
    int16_t  amplitude;
    uint16_t phase;
    int16_t  pitch;        /* current LFO output value */
} MDX_LFO;

static void lfo_init(MDX_LFO *lfo, uint8_t waveform, uint16_t period, int16_t amplitude)
{
    lfo->enable    = 1;
    lfo->waveform  = waveform;
    lfo->period    = period;
    lfo->amplitude = amplitude;
}

static void lfo_start(MDX_LFO *lfo)
{
    lfo->phase = 0;
    lfo->pitch = 0;
    if (lfo->waveform == 0u || lfo->waveform == 2u) {
        lfo->phase = lfo->period / 2u;
    }
    if (lfo->waveform == 2u) {
        lfo->pitch = (int16_t)(lfo->amplitude * 2);
    }
}

static int16_t lfo_tick(MDX_LFO *lfo)
{
    if (lfo->enable && lfo->period > 0u) {
        uint8_t phase_reset = 0;
        lfo->phase++;
        if (lfo->phase >= lfo->period) {
            phase_reset = 1;
            lfo->phase  = 0;
        }
        if (lfo->waveform == 0u) {              /* sawtooth */
            if (phase_reset)
                lfo->pitch = (int16_t)(-lfo->amplitude * (int16_t)(lfo->period / 2u));
            else
                lfo->pitch += lfo->amplitude;
        } else if (lfo->waveform == 1u) {        /* square */
            lfo->pitch = lfo->amplitude;
            if (phase_reset) lfo->amplitude = (int16_t)(-lfo->amplitude);
        } else if (lfo->waveform == 2u) {        /* triangle */
            if (phase_reset) lfo->amplitude = (int16_t)(-lfo->amplitude);
            lfo->pitch += lfo->amplitude;
        }
    }
    return lfo->pitch;
}

/* =========================================================
 * Per-channel playback state
 * ========================================================= */

typedef struct {
    MDX_PTR  pc;
    int32_t  pitch;                  /* fixed point, see PITCH_NOTE_BIAS comment */
    int16_t  detune;
    int16_t  portamento;
    int16_t  ticks_remaining;
    int16_t  staccato_counter;
    uint8_t  staccato;               /* MML q# value, default 8 */
    uint8_t  key_on_delay;
    uint8_t  key_on_delay_counter;
    uint8_t  volume;                 /* MML 0-15, or extended 128+ */
    uint8_t  opm_volume;             /* cached volume_to_tl(volume) */
    uint8_t  pan;
    int8_t   voice_num;              /* -1 = none selected yet */
    int8_t   note;                   /* -1 = no note currently held */
    uint8_t  skip_note_off;
    uint8_t  skip_note_on;
    uint8_t  slot_mask;
    uint8_t  con;
    uint8_t  fl_con_reg;
    uint8_t  tl[4];                  /* base TL values from the loaded voice */
    MDX_LFO  pitch_lfo;
    MDX_LFO  amplitude_lfo;
    uint8_t  lfo_delay;
    uint8_t  lfo_delay_counter;
    uint8_t  mhon;                   /* OPM hardware LFO enabled for channel */
    uint8_t  keysync;
    uint8_t  pms_ams;
    uint8_t  waiting;                /* parked at sync-wait (0xEE) */
    uint8_t  ended;
    uint8_t  used;
    uint8_t  effects_active;
} MDX_CHANNEL;

/* Recompute ch->effects_active from current portamento/LFO state.
 * Call after anything that can turn an effect on or off: 0xF2
 * (portamento), 0xEC/0xEB (LFO enable/disable), note-on (which can
 * clear portamento via ticks_remaining reaching 0 next tick), and
 * channel init. */
static void update_effects_active(MDX_CHANNEL *ch)
{
    ch->effects_active = (uint8_t)(ch->portamento != 0
                                 || ch->pitch_lfo.enable
                                 || ch->amplitude_lfo.enable);
}

/* =========================================================
 * Player globals
 * ========================================================= */

static uint8_t      g_segment_list[MAX_SEGMENTS];
static uint8_t      g_num_segments;
static uint8_t      g_current_segment;

static MDX_PTR      g_voice_pc;             /* base position of voice table */
static uint8_t      g_num_voices;           /* number of 27-byte entries in table */
static uint32_t     g_file_size;            /* total bytes loaded across all segments */

static uint8_t      g_num_channels;
static MDX_CHANNEL  g_ch[MDX_MAX_CHANNELS];
static uint8_t      g_ended;                 /* 1 once every used channel has ended */

/* Timing: get_refresh() returns the live MXDRV tick rate, derived from
 * the Timer B byte set by the MDX tempo command (0xFF), using the real
 * X68000 OPM Timer B hardware formula:
 *   rate_hz = 4000000 / (1024 * (256 - timer_b))
 */
static uint8_t      g_timer_b;

static char        *g_title_str;
static char        *g_pdx_filename;   /* points into loaded MDX data, segment 0 */
static char         g_empty_str[1];

/* =========================================================
 * Convenience macro
 * ========================================================= */

#define OPM_WRITE(reg, val)  g_roboplay_interface->opm_write((reg), (val))

/* =========================================================
 * Segment switching
 * ========================================================= */

static void set_segment(uint8_t seg_index)
{
    if (g_current_segment == seg_index) return;
    g_current_segment = seg_index;
    g_roboplay_interface->set_segment(g_segment_list[seg_index]);
}

/* =========================================================
 * Segment-aware byte access
 * ========================================================= */

/* Read the byte at ptr without advancing it. */
static uint8_t peek_byte(const MDX_PTR *ptr)
{
    set_segment(ptr->seg);
    return *((uint8_t *)DATA_SEGMENT_BASE + ptr->ofs);
}

/* Read the byte at ptr and advance it by one. */
static uint8_t fetch_byte(MDX_PTR *ptr)
{
    uint8_t value = peek_byte(ptr);
    ptr->ofs++;
    if (ptr->ofs >= DATA_SEGMENT_SIZE) {
        ptr->ofs = 0;
        ptr->seg++;
    }
    return value;
}

/* Read a big-endian 16-bit value, advancing ptr by 2 bytes. */
static uint16_t fetch_uint16_local(MDX_PTR *ptr)
{
    uint16_t hi = fetch_byte(ptr);
    uint16_t lo = fetch_byte(ptr);
    return (uint16_t)((hi << 8) | lo);
}

/* Write a byte at ptr without advancing it */
static void poke_byte(const MDX_PTR *ptr, uint8_t value)
{
    set_segment(ptr->seg);
    *((uint8_t *)DATA_SEGMENT_BASE + ptr->ofs) = value;
}

static void ptr_offset(const MDX_PTR *ptr, int32_t delta, MDX_PTR *out)
{
    int32_t abs_ofs = ((int32_t)ptr->seg << 14)                 /* seg * 0x4000 */
                    + (int32_t)ptr->ofs
                    + delta;
    out->seg = (uint8_t)(abs_ofs >> 14);                        /* / 0x4000 */
    out->ofs = (uint16_t)(abs_ofs & (DATA_SEGMENT_SIZE - 1u));  /* % 0x4000 */
}

static void ptr_advance(MDX_PTR *ptr, int32_t delta)
{
    MDX_PTR result;
    ptr_offset(ptr, delta, &result);
    ptr->seg = result.seg;
    ptr->ofs = result.ofs;
}

#define PDX_OCT  0x0Fu   /* raw nibble for signed octave -1 (-1 & 0xF) */
#define PDX_FN   425u
 
/* Fixed register 0x38 value: PDX_OCT/PDX_FN never change between
 * notes (every PDX sample plays at the same derived rate) */
#define PDX_REG_0x38  ((uint8_t)((PDX_OCT << 4) | ((PDX_FN >> 7) & 0x07u)))
 
bool pdx_note_on(uint8_t opl4_channel, uint8_t note)
{
    uint16_t wave_num_lo;
 
    if (note >= PDX_NUM_SAMPLES) return false;
    wave_num_lo = g_pdx_header_slot[note];
    if (wave_num_lo == 0xFFFFu) return false;
 
    /* wave_num_lo is the low 8 bits of the wave/instrument number
     * assigned to this sample at load time (0x80-0xFF, since sample
     * numbers start just past the YRW801 ROM bank). The high bit
     * (always 1 for any RAM sample) is asserted separately as a
     * hardcoded constant in the register 0x20 write below, so the
     * full 9-bit wave number is never assembled here. */
 
    /* Required write order per OPL4 wave-channel programming:
     * 1) octave + F-number high bits  (0x38+channel)
     * 2) F-number low bits + wave-number MSB (0x20+channel)
     * 3) wave-number low byte (0x08+channel) -- this write triggers
     *    the actual instrument load, so it must come last. */
    g_roboplay_interface->opl4_write_wave(
        (uint8_t)(0x38u + opl4_channel), PDX_REG_0x38);
 
    /* Bit 0 here is wave-number bit 8, which is always 1 for every
     * valid PDX wave number (any RAM sample, 384-511, falls above the
     * 256 boundary where that bit turns on) -- hardcoded rather than
     * computed per note-on, matching mod.c's own set_sample_number()
     * convention, which makes the same assumption for its own (also
     * always-RAM-range) sample numbers. */
    g_roboplay_interface->opl4_write_wave(
        (uint8_t)(0x20u + opl4_channel),
        (uint8_t)(0x01u | ((PDX_FN << 1) & 0xFEu)));
 
    g_roboplay_interface->opl4_write_wave(
        (uint8_t)(0x08u + opl4_channel),
        (uint8_t)(wave_num_lo & 0xFFu));
 
    g_roboplay_interface->opl4_wait_for_load();
 
    g_roboplay_interface->opl4_write_wave((uint8_t)(0x68u + opl4_channel), 0x80u);
 
    return true;
}
 
void pdx_note_off(uint8_t opl4_channel)
{
    g_roboplay_interface->opl4_write_wave((uint8_t)(0x68u + opl4_channel), 0x00u);
}
 
/* =========================================================
 * Pitch application
 * ========================================================= */

#define NOTE_TABLE_SIZE 192u

static const uint8_t g_note_to_kc[NOTE_TABLE_SIZE] = {
    /* octave 0 */ 0x00,0x01,0x02,0x04,0x05,0x06,0x08,0x09,0x0A,0x0C,0x0D,0x0E,
    /* octave 1 */ 0x10,0x11,0x12,0x14,0x15,0x16,0x18,0x19,0x1A,0x1C,0x1D,0x1E,
    /* octave 2 */ 0x20,0x21,0x22,0x24,0x25,0x26,0x28,0x29,0x2A,0x2C,0x2D,0x2E,
    /* octave 3 */ 0x30,0x31,0x32,0x34,0x35,0x36,0x38,0x39,0x3A,0x3C,0x3D,0x3E,
    /* octave 4 */ 0x40,0x41,0x42,0x44,0x45,0x46,0x48,0x49,0x4A,0x4C,0x4D,0x4E,
    /* octave 5 */ 0x50,0x51,0x52,0x54,0x55,0x56,0x58,0x59,0x5A,0x5C,0x5D,0x5E,
    /* octave 6 */ 0x60,0x61,0x62,0x64,0x65,0x66,0x68,0x69,0x6A,0x6C,0x6D,0x6E,
    /* octave 7 */ 0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
    /* octave 8  (clamped beyond here in practice; kept for headroom) */
    0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
    /* octave 9 */
    0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
    /* octave 10 */
    0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
    /* octave 11 */
    0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
    /* octave 12 */
    0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
    /* octave 13 */
    0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
    /* octave 14 */
    0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
    /* octave 15 */
    0x70,0x71,0x72,0x74,0x75,0x76,0x78,0x79,0x7A,0x7C,0x7D,0x7E,
};

/* Convert a note index (0-based semitone) to OPM KC byte via direct
 * table lookup. Out-of-range values are clamped to the table edges
 * rather than risking an out-of-bounds read. */
static uint8_t mdx_note_to_opm(int16_t note)
{
    if (note < 0) note = 0;
    if (note >= (int16_t)NOTE_TABLE_SIZE) note = (int16_t)NOTE_TABLE_SIZE - 1;
    return g_note_to_kc[note];
}

/* Write KC and KF registers from a fixed-point pitch value.
 * Matches fm_opm_driver_set_pitch() exactly:
 *   KC = mdx_note_to_opm(pitch >> 14)
 *   KF = (pitch >> 6) & 0xfc  (top 6 bits of the 8-bit fraction) */
static void apply_pitch(uint8_t ci, int32_t pitch)
{
    OPM_WRITE(0x28 + ci, mdx_note_to_opm((int16_t)(pitch >> 14)));
    OPM_WRITE(0x30 + ci, (uint8_t)((pitch >> 6) & 0xfc));
}

/* =========================================================
 * Tempo
 * ========================================================= */

static void set_tempo(uint8_t timer_b)
{
    g_timer_b = timer_b;
    g_roboplay_interface->update_refresh();
}

/* =========================================================
 * Voice loading and volume application
 * ========================================================= */

/* Read one 27-byte voice entry at table slot `slot` into a local buffer,
 * using segment-aware access (the entry may straddle a 16KB boundary). */
static void read_voice_entry(uint8_t slot, uint8_t *out27)
{
    MDX_PTR p;
    uint8_t k;
    int32_t delta = (int32_t)slot * 27;

    ptr_offset(&g_voice_pc, delta, &p);
    for (k = 0; k < 27u; k++) {
        out27[k] = fetch_byte(&p);
    }
}

/* Find the voice table slot whose stored voice_id byte matches voice_id.
 * The 0xFD command argument is an id to look up, not a direct zero-based
 * slot index (ids are typically 1-based and not necessarily contiguous,
 * matching mdxplay's behaviour against real MDX files). Falls back to
 * slot 0 if no match is found, to fail safe rather than read garbage. */
static void voice_load(uint8_t ci, uint8_t voice_id)
{
    MDX_CHANNEL *ch = &g_ch[ci];
    uint8_t v[27];
    uint8_t slot, i;
    uint8_t found = 0;

    for (slot = 0; slot < g_num_voices; slot++) {
        read_voice_entry(slot, v);
        if (v[0] == voice_id) { found = 1; break; }
    }
    if (!found) {
        if (g_num_voices == 0) return;
        read_voice_entry(0, v);
    }

    /* v layout (27 bytes):
     *   [0]    voice id
     *   [1]    FL/CON  (feedback + algorithm)
     *   [2]    slot mask
     *   [3..6] DT1/MUL  op0..op3
     *   [7..10] TL       op0..op3
     *   [11..14] KS/AR   op0..op3
     *   [15..18] AME/D1R op0..op3
     *   [19..22] DT2/D2R op0..op3
     *   [23..26] D1L/RR  op0..op3
     * OPM operator registers use stride 8: base + i*8 + channel
     * Matches fm_opm_driver_load_voice() exactly. */

    ch->slot_mask  = v[2];
    ch->con        = v[1] & 0x07u;
    ch->fl_con_reg = v[1];
    for (i = 0; i < 4; i++) ch->tl[i] = v[7 + i];

    for (i = 0; i < 4; i++) OPM_WRITE(0x40 + i * 8 + ci, v[ 3 + i]); /* DT1/MUL */
    /* TL written via apply_tl() after voice_load() so volume is applied */
    for (i = 0; i < 4; i++) OPM_WRITE(0x80 + i * 8 + ci, v[11 + i]); /* KS/AR   */
    for (i = 0; i < 4; i++) OPM_WRITE(0xa0 + i * 8 + ci, v[15 + i]); /* AME/D1R */
    for (i = 0; i < 4; i++) OPM_WRITE(0xc0 + i * 8 + ci, v[19 + i]); /* DT2/D2R */
    for (i = 0; i < 4; i++) OPM_WRITE(0xe0 + i * 8 + ci, v[23 + i]); /* D1L/RR  */
    OPM_WRITE(0x20 + ci, (uint8_t)((ch->pan << 6) | ch->fl_con_reg)); /* PAN/FL/CON */
}

/* Apply an OPM-scale volume attenuation to carrier operators only.
 * Carrier mask bits [3:0] correspond to operators 0..3 directly */
static void apply_tl(uint8_t ci, int16_t opm_vol)
{
    const MDX_CHANNEL *ch = &g_ch[ci];
    uint8_t i;
    uint8_t mask = g_carrier_mask[ch->con];
    for (i = 0; i < 4; i++) {
        int16_t tl = (int16_t)ch->tl[i];
        if (mask & (1u << i)) {
            tl += opm_vol;
            if (tl > 127) tl = 127;
            if (tl < 0)   tl = 0;
        }
        OPM_WRITE(0x60 + i * 8 + ci, (uint8_t)tl);
    }
}

static void volume_inc(MDX_CHANNEL *ch)
{
    if (ch->volume < 15u)       ch->volume++;
    else if (ch->volume > 128u) ch->volume--;
}

static void volume_dec(MDX_CHANNEL *ch)
{
    if (ch->volume < 16u && ch->volume > 0u)        ch->volume--;
    else if (ch->volume >= 128u && ch->volume < 255u) ch->volume++;
}

/* =========================================================
 * Key on / off
 * ========================================================= */

static void opm_key_on(uint8_t ci, uint8_t slot_mask)
{
    OPM_WRITE(OPM_KEYON,
              (uint8_t)((slot_mask & 0x0Fu) << 3) |
              (ci & 0x07u));
}

static void opm_key_off(uint8_t ci)
{
    OPM_WRITE(OPM_KEYON, ci & 0x07u);
}

/* Write pitch/TL, then (unless this note-on is being skipped due to legato)
 *  load OPM LFO state and key the note on. */
static void channel_note_on(uint8_t ci)
{
    MDX_CHANNEL *ch = &g_ch[ci];

    if (ch->voice_num < 0) return;

    apply_pitch(ci, ch->pitch);
    apply_tl(ci, (int16_t)ch->opm_volume);

    if (ch->skip_note_on) {
        ch->skip_note_on = 0;
        return;
    }

    if (ch->mhon) {
        OPM_WRITE(OPM_PMS_AMS + ci, ch->pms_ams);
        /* keysync (phase reset) has no dedicated OPM register write
         * beyond the key-on itself on real hardware; omitted. */
    }

    if (ch->pitch_lfo.enable) {
        if (ch->lfo_delay) {
            ch->lfo_delay_counter = ch->lfo_delay;
        } else {
            lfo_start(&ch->pitch_lfo);
        }
    }
    if (ch->amplitude_lfo.enable && !ch->lfo_delay) {
        lfo_start(&ch->amplitude_lfo);
    }

    opm_key_on(ci, ch->slot_mask);
}

/* Honour legato (skip_note_off) by deferring suppression to the *next* 
 * note-on instead of keying off now. */
static void channel_note_off(uint8_t ci)
{
    MDX_CHANNEL *ch = &g_ch[ci];

    if (ch->note < 0) return;

    if (ci >= MDX_FM_CHANNELS) {
        /* PCM channel: PDX samples are one-shot per the format, but
         * still honour an explicit rest/note-change as a stop, in
         * case a sample is longer than the time allotted to it in
         * the MML and the next command needs to cut it short. */
        if (g_pdx_loaded) {
            pdx_note_off((uint8_t)(ci - MDX_FM_CHANNELS));
        }
        ch->note = -1;
        return;
    }

    if (ch->skip_note_off) {
        ch->skip_note_on  = 1;
        ch->skip_note_off = 0;
    } else {
        opm_key_off(ci);
    }
    ch->note = -1;
}

/* =========================================================
 * Channel end-of-data bookkeeping
 * ========================================================= */

static void channel_end(uint8_t ci)
{
    MDX_CHANNEL *ch = &g_ch[ci];
    uint8_t i;

    ch->ended = 1;
    g_ended   = 1;
    for (i = 0; i < g_num_channels; i++) {
        if (g_ch[i].used && !g_ch[i].ended) {
            g_ended = 0;
            break;
        }
    }
}

/* =========================================================
 * File loading (multi-segment)
 * ========================================================= */

static bool load_file_data(void)
{
    uint8_t  *destination = (uint8_t *)DATA_SEGMENT_BASE;
    uint16_t  page_left   = DATA_SEGMENT_SIZE;
    uint16_t  bytes_read;

    g_num_segments = 0;
    g_file_size    = 0;
    g_segment_list[g_num_segments] = START_SEGMENT_INDEX;
    g_roboplay_interface->set_segment(g_segment_list[g_num_segments]);
    g_num_segments++;

    do {
        bytes_read = g_roboplay_interface->read(
            (void *)READ_BUFFER, READ_BUFFER_SIZE);

        g_file_size += bytes_read;

        uint16_t remaining = bytes_read;
        uint8_t *src       = (uint8_t *)READ_BUFFER;

        while (remaining > 0) {
            uint16_t chunk = (remaining < page_left) ? remaining : page_left;
            memcpy(destination, src, chunk);

            src         += chunk;
            destination += chunk;
            remaining   -= chunk;
            page_left   -= chunk;

            if (page_left == 0 && remaining > 0) {
                if (g_num_segments >= MAX_SEGMENTS) return false;
                g_segment_list[g_num_segments] =
                    g_roboplay_interface->get_new_segment();
                g_roboplay_interface->set_segment(
                    g_segment_list[g_num_segments]);
                g_num_segments++;

                destination = (uint8_t *)DATA_SEGMENT_BASE;
                page_left   = DATA_SEGMENT_SIZE;
            }
        }
    } while (bytes_read == READ_BUFFER_SIZE);

    g_current_segment = 0;
    g_roboplay_interface->set_segment(g_segment_list[0]);
    return true;
}

/* =========================================================
 * Header parser
 * ========================================================= */

static bool parse_header(void)
{
    uint8_t       *p   = (uint8_t *)DATA_SEGMENT_BASE;
    const uint8_t *end = p + DATA_SEGMENT_SIZE;
    uint8_t        i;

    g_title_str = (char *)p;
    while (p + 2 < end) {
        if (p[0] == MDX_TITLE_END_0 &&
            p[1] == MDX_TITLE_END_1 &&
            p[2] == MDX_TITLE_END_2) {
            *p = '\0';
            p += 3;
            break;
        }
        p++;
    }
    if (p >= end) return false;

    /* PDX filename: null-terminated string, same capture pattern as
     * the title above. Empty string (just a single 0x00 byte) means
     * the song has no PDX sample bank. */
    g_pdx_filename = (char *)p;
    while (p < end && *p) p++;
    if (p >= end) return false;
    p++;

    const uint8_t *base = p;
    uint16_t base_ofs   = (uint16_t)(base - (uint8_t *)DATA_SEGMENT_BASE);

    if (p + 2 > end) return false;
    uint16_t voice_off = ((uint16_t)p[0] << 8) | p[1];
    uint32_t voice_abs = (uint32_t)base_ofs + voice_off;
    g_voice_pc.seg = (uint8_t)(voice_abs / DATA_SEGMENT_SIZE);
    g_voice_pc.ofs = (uint16_t)(voice_abs % DATA_SEGMENT_SIZE);

    /* Voice count: the voice table is the last item in the file, so its
     * size is exactly (total file size - absolute voice table offset). */
    if (voice_abs < g_file_size)
        g_num_voices = (uint8_t)((g_file_size - voice_abs) / 27u);
    else
        g_num_voices = 0;
    p += 2;

    if (p + 2 > end) return false;
    uint16_t first_mml = ((uint16_t)p[0] << 8) | p[1];
    g_num_channels = (uint8_t)((first_mml >> 1) - 1u);
    if (g_num_channels > MDX_MAX_CHANNELS) g_num_channels = MDX_MAX_CHANNELS;

    for (i = 0; i < g_num_channels; i++) {
        if (p + 2 > end) return false;
        uint16_t off = ((uint16_t)p[0] << 8) | p[1];
        p += 2;

        uint32_t abs = (uint32_t)base_ofs + off;
        g_ch[i].pc.seg = (uint8_t)(abs / DATA_SEGMENT_SIZE);
        g_ch[i].pc.ofs = (uint16_t)(abs % DATA_SEGMENT_SIZE);
    }

    return true;
}

/* =========================================================
 * Channel init
 * Mirrors mdx_driver_track_init().
 * ========================================================= */

static void channels_init(void)
{
    uint8_t i;
    g_ended = 0;

    for (i = 0; i < g_num_channels; i++) {
        MDX_CHANNEL *ch = &g_ch[i];
        uint8_t  saved_seg = ch->pc.seg;   /* preserve PC set by parse_header() */
        uint16_t saved_ofs = ch->pc.ofs;

        memset(ch, 0, sizeof(*ch));
        ch->pc.seg = saved_seg;
        ch->pc.ofs = saved_ofs;

        ch->staccato    = 8u;
        ch->volume      = 8u;
        ch->opm_volume  = volume_to_tl(ch->volume);
        ch->voice_num   = -1;
        ch->note        = -1;
        ch->pan         = 3u;
        ch->con         = 7u;
        ch->slot_mask   = 0x0Fu;  /* all 4 slots on, fallback before voice load */

        /* MDX_FM_CHANNELS, i.e. channel 8 / "P") is only active if a
         * PDX sample bank actually loaded successfully; otherwise it
         * stays silently skipped by channel_tick()'s !ch->used guard. 
         * Channels beyond that (Mercury Q-W) remain unused -- not implemented. */
        if (i < MDX_FM_CHANNELS) {
            ch->used = 1u;
        } else if (i == MDX_FM_CHANNELS) {
            ch->used = g_pdx_loaded ? 1u : 0u;
        } else {
            ch->used = 0u;
        }
        ch->ended       = ch->used ? 0u : 1u;  /* PCM stub starts "ended" */
    }
}

/* =========================================================
 * Sequencer: advance one channel by one command
 * Mirrors mdx_driver_track_advance().
 * ========================================================= */

static void channel_advance(uint8_t ci)
{
    MDX_CHANNEL *ch = &g_ch[ci];

    channel_note_off(ci);

    uint8_t cmd = fetch_byte(&ch->pc);

    /* --- Rest (0x00-0x7F) --- */
    if (cmd <= 0x7Fu) {
        ch->note            = -1;
        ch->ticks_remaining = (int16_t)cmd + 1;
        return;
    }

    /* --- Note on (0x80-0xDF) --- */
    if (cmd <= 0xDFu) {
        uint8_t dur = fetch_byte(&ch->pc);
        ch->ticks_remaining       = (int16_t)dur + 1;
        ch->key_on_delay_counter = 0;
        ch->note                  = (int8_t)(cmd & 0x7Fu);

        if (ci >= MDX_FM_CHANNELS) {
            /* PCM channel (P, and Mercury Q-W if ever supported):
             * the note value itself selects which of PDX's 96 samples
             * to trigger -- there is no pitch or key-on-delay handling
             * here, since PDX samples are one-shot and play at their
             * own fixed recorded pitch.
             *
             * staccato_counter must still be set explicitly: it is
             * decremented unconditionally every tick in channel_tick()
             * regardless of channel type, and channel_note_off() fires
             * the instant it reaches <= 0. Left unset, it would still
             * hold whatever stale value preceded this note (often 0 or
             * close to it), firing an early note-off within a tick or
             * two of the sample starting -- which is exactly the
             * "short tick" symptom this fixes. Setting it past
             * ticks_remaining means the staccato path never fires
             * before the channel's own rest/next-note-on naturally
             * calls channel_note_off() instead, letting the sample
             * play for its full MML-allotted duration uninterrupted. */
            ch->staccato_counter = (int16_t)(ch->ticks_remaining + 1);
            if (g_pdx_loaded) {
                pdx_note_on((uint8_t)(ci - MDX_FM_CHANNELS), (uint8_t)ch->note);
            }
            return;
        }
 

        /* (bias + note*64 + detune) fits in 16 bits for the full
         * practical range (note 0-127, detune bounded), so compute it
         * there and only widen to int32_t for the final <<8 -- avoids
         * doing the add/shift as 32-bit operations.
         */
        int16_t pitch16 = (int16_t)(PITCH_NOTE_BIAS
                                    + ((int16_t)ch->note << 6)
                                    + ch->detune);
        ch->pitch = (int32_t)pitch16 << 8;

        if (ch->staccato <= 8u) {
            /* staccato (0-8) * ticks_remaining fits in 16 bits for any
             * realistic note duration; /8 is a shift, not a divide. */
            ch->staccato_counter =
                (int16_t)(((int16_t)ch->staccato * ch->ticks_remaining) >> 3);
        } else {
            int16_t tail = (int16_t)(ch->ticks_remaining - (int16_t)(256 - ch->staccato));
            ch->staccato_counter = (tail < 0) ? 0 : tail;
        }

        if (ch->key_on_delay) {
            ch->key_on_delay_counter = ch->key_on_delay;
        } else {
            channel_note_on(ci);
        }
        return;
    }

    /* --- Control commands (0xE0-0xFF) --- */
    switch (cmd) {

    case 0xFF:
        set_tempo(fetch_byte(&ch->pc));
        return;

    case 0xFE: {
        uint8_t reg = fetch_byte(&ch->pc);
        uint8_t val = fetch_byte(&ch->pc);
        OPM_WRITE(reg, val);
        return;
    }

    case 0xFD: {
        ch->voice_num = (int8_t)fetch_byte(&ch->pc);
        if (ci < MDX_FM_CHANNELS)
            voice_load(ci, (uint8_t)ch->voice_num);
        return;
    }

    case 0xFC: {
        uint8_t pan = fetch_byte(&ch->pc);
        if (ci < MDX_FM_CHANNELS && ch->voice_num >= 0) {
            ch->pan = pan & 0x03u;
            OPM_WRITE(OPM_RL_FB_CON + ci,
                      (uint8_t)((ch->pan << 6) | ch->fl_con_reg));
        }
        return;
    }

    case 0xFB: {
        ch->volume     = fetch_byte(&ch->pc);
        ch->opm_volume = volume_to_tl(ch->volume);
        if (ci < MDX_FM_CHANNELS && ch->voice_num >= 0)
            apply_tl(ci, (int16_t)ch->opm_volume);
        return;
    }

    case 0xFA:
        volume_dec(ch);
        ch->opm_volume = volume_to_tl(ch->volume);
        if (ci < MDX_FM_CHANNELS && ch->voice_num >= 0)
            apply_tl(ci, (int16_t)ch->opm_volume);
        return;

    case 0xF9:
        volume_inc(ch);
        ch->opm_volume = volume_to_tl(ch->volume);
        if (ci < MDX_FM_CHANNELS && ch->voice_num >= 0)
            apply_tl(ci, (int16_t)ch->opm_volume);
        return;

    case 0xF8:
        ch->staccato = fetch_byte(&ch->pc);
        return;

    case 0xF7:
        /* Legato: defer key-off suppression to the next note-off/-on pair */
        if (ci < MDX_FM_CHANNELS) ch->skip_note_off = 1;
        return;

    case 0xF6: {
        /* Repeat start: copy the loop count into the padding byte slot,
         * which becomes the live counter mutated by 0xF5/0xF4. */
        uint8_t count = fetch_byte(&ch->pc);
        uint8_t pad_seg = ch->pc.seg;   /* points at the 0x00 padding byte */
        uint16_t pad_ofs = ch->pc.ofs;
        MDX_PTR pad;
        pad.seg = pad_seg;
        pad.ofs = pad_ofs;
        fetch_byte(&ch->pc);            /* consume the padding byte */
        poke_byte(&pad, count);
        return;
    }

    case 0xF5: {
        /* Repeat end: jump back ofs bytes; the counter byte sits at
         * (destination - 1) relative to the jump target, mirroring the
         * reference's track->data[pos + ofs - 1]. */
        int16_t ofs = (int16_t)fetch_uint16_local(&ch->pc);
        MDX_PTR counter_pos;
        ptr_offset(&ch->pc, (int32_t)ofs - 1, &counter_pos);
        uint8_t counter = peek_byte(&counter_pos);
        if (counter > 1u) {
            poke_byte(&counter_pos, counter - 1u);
            ptr_advance(&ch->pc, ofs);
        }
        return;
    }

    case 0xF4: {
        /* Repeat escape: peek ahead to the loop's own counter byte;
         * if this is the last pass, skip forward past the loop body.
         */
        int16_t ofs = (int16_t)fetch_uint16_local(&ch->pc);
        MDX_PTR tmp;
        ptr_offset(&ch->pc, ofs, &tmp);
        uint16_t start_ofs = (uint16_t)fetch_byte(&tmp) << 8;
        start_ofs         |= fetch_byte(&tmp);
        MDX_PTR counter_pos;
        ptr_offset(&ch->pc, (int32_t)ofs + (int16_t)start_ofs + 1, &counter_pos);
        uint8_t counter = peek_byte(&counter_pos);
        if (counter <= 1u) {
            ptr_advance(&ch->pc, (int32_t)ofs + 2);
        }
        return;
    }

    case 0xF3: {
        int16_t nn = (int16_t)fetch_uint16_local(&ch->pc);
        if (ci < MDX_FM_CHANNELS) ch->detune = nn;
        return;
    }

    case 0xF2: {
        int16_t nn = (int16_t)fetch_uint16_local(&ch->pc);
        if (ci < MDX_FM_CHANNELS) {
            ch->portamento = nn;
            update_effects_active(ch);
        }
        return;
    }

    case 0xF1: {
        uint8_t hi = fetch_byte(&ch->pc);
        if (hi == 0u) {
            channel_end(ci);
        } else {
            uint8_t lo = fetch_byte(&ch->pc);
            int16_t ofs = (int16_t)(((uint16_t)hi << 8) | lo);
            ptr_advance(&ch->pc, ofs);
        }
        return;
    }

    case 0xF0:
        ch->key_on_delay = fetch_byte(&ch->pc);
        return;

    case 0xEF: {
        uint8_t target = fetch_byte(&ch->pc);
        if (target < g_num_channels) g_ch[target].waiting = 0;
        return;
    }

    case 0xEE:
        ch->waiting = 1;
        return;

    case 0xED: {
        uint8_t freq = fetch_byte(&ch->pc);
        (void)freq;  /* noise frequency: OPM noise output not wired up */
        return;
    }

    case 0xEC: {
        uint8_t sub = fetch_byte(&ch->pc);
        if (sub == 0x80u) {
            if (ci < MDX_FM_CHANNELS) {
                ch->pitch_lfo.enable = 0;
                update_effects_active(ch);
            }
        } else if (sub == 0x81u) {
            if (ci < MDX_FM_CHANNELS) {
                ch->pitch_lfo.enable = 1;
                update_effects_active(ch);
            }
        } else {
            uint8_t  p_hi = fetch_byte(&ch->pc);
            uint8_t  p_lo = fetch_byte(&ch->pc);
            uint8_t  a_hi = fetch_byte(&ch->pc);
            uint8_t  a_lo = fetch_byte(&ch->pc);
            if (ci < MDX_FM_CHANNELS) {
                lfo_init(&ch->pitch_lfo, sub,
                         (uint16_t)(((uint16_t)p_hi << 8) | p_lo),
                         (int16_t)(((uint16_t)a_hi << 8) | a_lo));
                update_effects_active(ch);
            }
        }
        return;
    }

    case 0xEB: {
        uint8_t sub = fetch_byte(&ch->pc);
        if (sub == 0x80u) {
            if (ci < MDX_FM_CHANNELS) {
                ch->amplitude_lfo.enable = 0;
                update_effects_active(ch);
            }
        } else if (sub == 0x81u) {
            if (ci < MDX_FM_CHANNELS) {
                ch->amplitude_lfo.enable = 1;
                update_effects_active(ch);
            }
        } else {
            uint8_t  p_hi = fetch_byte(&ch->pc);
            uint8_t  p_lo = fetch_byte(&ch->pc);
            uint8_t  a_hi = fetch_byte(&ch->pc);
            uint8_t  a_lo = fetch_byte(&ch->pc);
            if (ci < MDX_FM_CHANNELS) {
                lfo_init(&ch->amplitude_lfo, sub,
                         (uint16_t)(((uint16_t)p_hi << 8) | p_lo),
                         (int16_t)(((uint16_t)a_hi << 8) | a_lo));
                update_effects_active(ch);
            }
        }
        return;
    }

    case 0xEA: {
        uint8_t sub = fetch_byte(&ch->pc);
        if (sub == 0x80u) {
            ch->mhon = 0;
        } else if (sub == 0x81u) {
            ch->mhon = 1;
        } else {
            uint8_t n = fetch_byte(&ch->pc);
            uint8_t o = fetch_byte(&ch->pc);
            uint8_t p = fetch_byte(&ch->pc);
            uint8_t q = fetch_byte(&ch->pc);
            ch->keysync = sub & 0x40u;
            ch->pms_ams = q;
            ch->mhon    = 1;
            OPM_WRITE(OPM_CT_W,         sub & 0x03u);
            OPM_WRITE(OPM_LFRQ,         n);
            OPM_WRITE(OPM_PMD_AMD,      (uint8_t)(0x80u | o));
            OPM_WRITE(OPM_PMD_AMD,      p & 0x7Fu);
        }
        return;
    }

    case 0xE9:
        ch->lfo_delay = fetch_byte(&ch->pc);
        return;

    case 0xE8:
        /* Enable PCM8 mode: no argument bytes; no ADPCM on SFG-05 */
        return;

    case 0xE7: {
        uint8_t sub = fetch_byte(&ch->pc);
        if (sub == 0x01u) fetch_byte(&ch->pc);
        return;
    }

    default:
        /* Unknown/unhandled opcode: consume only the opcode byte */
        return;
    }
}

/* =========================================================
 * Per-channel tick
 * ========================================================= */

static void channel_tick(uint8_t ci)
{
    MDX_CHANNEL *ch = &g_ch[ci];

    if (!ch->used)            return;
    if (ch->ended || ch->waiting) return;

    if (ch->key_on_delay_counter > 0u) {
        ch->key_on_delay_counter--;
        if (ch->key_on_delay_counter == 0u && ch->staccato_counter == 0)
            channel_note_on(ci);
    }

    ch->staccato_counter--;
    if (ch->staccato_counter <= 0 && ch->key_on_delay_counter == 0u)
        channel_note_off(ci);

    ch->ticks_remaining--;
    if (ch->ticks_remaining == 0 && ch->portamento) {
        ch->portamento = 0;
        update_effects_active(ch);
    }

    if (ci < MDX_FM_CHANNELS && ch->effects_active) {
        uint8_t pitch_dirty = 0;
        uint8_t vol_dirty   = 0;

        if (ch->portamento) {
            ch->pitch  += ch->portamento;
            pitch_dirty = 1;
        }

        int32_t pitch = ch->pitch;
        if (ch->lfo_delay_counter > 0u) {
            ch->lfo_delay_counter--;
            if (ch->lfo_delay_counter == 0u) {
                if (ch->pitch_lfo.enable)     lfo_start(&ch->pitch_lfo);
                if (ch->amplitude_lfo.enable) lfo_start(&ch->amplitude_lfo);
                /* LFO just started: its first tick's output may be
                 * nonzero (e.g. triangle starts at 2x amplitude), so
                 * force a write even though the "enable && counter==0"
                 * check below would otherwise see counter==0 already. */
                pitch_dirty = 1;
                vol_dirty   = 1;
            }
        }

        if (ch->pitch_lfo.enable && ch->lfo_delay_counter == 0u) {
            pitch      += lfo_tick(&ch->pitch_lfo);
            pitch_dirty = 1;
        }

        if (pitch_dirty) apply_pitch(ci, pitch);

        int16_t opm_vol = (int16_t)ch->opm_volume;
        if (ch->amplitude_lfo.enable && ch->lfo_delay_counter == 0u) {
            opm_vol    = (int16_t)(opm_vol + lfo_tick(&ch->amplitude_lfo));
            vol_dirty   = 1;
        }

        if (vol_dirty && ch->voice_num >= 0) {
            apply_tl(ci, opm_vol);
        }
    }

    while (ch->ticks_remaining <= 0 && !ch->ended) {
        channel_advance(ci);
    }
}

/* =========================================================
 * ADPCM decoder
 * ========================================================= */

typedef struct {
    int16_t last;
    int16_t step_index;
} ADPCM_STATUS;

/* Dialogic/Oki step-size table */
static const int16_t g_step_size[49] = {
      16,   17,   19,   21,   23,   25,   28,   31,   34,   37,   41,
      45,   50,   55,   60,   66,   73,   80,   88,   97,  107,  118,
     130,  143,  157,  173,  190,  209,  230,  253,  279,  307,  337,
     371,  408,  449,  494,  544,  598,  658,  724,  796,  876,  963,
    1060, 1166, 1282, 1411, 1552
};

static const int8_t g_step_adjust[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static void adpcm_init(ADPCM_STATUS *st)
{
    st->last       = 0;
    st->step_index = 0;
}

/* Per-nibble ADPCM decode step */
static int16_t adpcm_decode_step(uint8_t code, ADPCM_STATUS *st)
{
    int16_t ss = g_step_size[st->step_index];
    int16_t e  = ss >> 3;
    int16_t diff, samp;

    if (code & 0x01) e += ss >> 2;
    if (code & 0x02) e += ss >> 1;
    if (code & 0x04) e += ss;

    diff = (code & 0x08) ? (int16_t)(-e) : e;
    samp = (int16_t)(st->last + diff);

    if (samp > 2047)  samp = 2047;
    if (samp < -2048) samp = -2048;

    st->last = samp;
    st->step_index = (int16_t)(st->step_index + g_step_adjust[code & 0x07]);
    if (st->step_index < 0)  st->step_index = 0;
    if (st->step_index > 48) st->step_index = 48;

    return samp;
}

/* Decode one full ADPCM byte (both nibbles) and write the 3 OPL4-ready
 * packed bytes directly to out3
 *
 * Nibble order (low nibble first, then high) is mandatory
 *
 * Packing layout (byte0=V0[11:4], byte1=(V0[3:0]<<4)|V1[3:0],
 *
 * Silence-centering: OPL4's 12-bit wave format centers silence at 0,
 * the plain "& 0x0FFFu" mask (relying on int16_t's negative values 
 * wrapping correctly under truncation) is correct here. */
static void adpcm_decode_byte_packed(uint8_t byte, ADPCM_STATUS *st,
                                      uint8_t *out3)
{
    int16_t d0 = adpcm_decode_step(byte & 0x0F, st);
    int16_t d1 = adpcm_decode_step((byte >> 4) & 0x0F, st);

    uint16_t v0 = (uint16_t)d0 & 0x0FFFu;
    uint16_t v1 = (uint16_t)d1 & 0x0FFFu;

    out3[0] = (uint8_t)(v0 >> 4);
    out3[1] = (uint8_t)(((v0 & 0x0Fu) << 4) | (v1 & 0x0Fu));
    out3[2] = (uint8_t)(v1 >> 4);
}

/* =========================================================
 * Streaming decode-and-upload
 * ========================================================= */

/* How many ADPCM source bytes to decode per chunk before uploading.
 * Each source byte produces 2 decoded samples = 3 packed output
 * bytes, so this must stay small enough that 3*N comfortably fits
 * within READ_BUFFER_SIZE when doubled. */
#define PDX_DECODE_CHUNK 256u

/* Decode adpcm_len bytes of ADPCM data starting at file offset
 * file_ofs, uploading the packed 12-bit result to OPL4 wave memory
 * starting at *wave_addr (advanced as data is written). Reads the
 * source ADPCM bytes through the start of READ_BUFFER in chunks. */
static void pdx_decode_upload(uint32_t adpcm_len, uint32_t *wave_addr)
{
    ADPCM_STATUS st;
    uint8_t *buf = (uint8_t *)READ_BUFFER;
    uint32_t remaining = adpcm_len;

    adpcm_init(&st);

    while (remaining > 0) {
        uint16_t chunk = (remaining > PDX_DECODE_CHUNK) ? PDX_DECODE_CHUNK : (uint16_t)remaining;
        uint16_t bytes_read = g_roboplay_interface->read(buf, chunk);
        if (bytes_read == 0) break;

        uint8_t *dst = buf + PDX_DECODE_CHUNK;
        uint16_t i;

        for (i = 0; i < bytes_read; i++) {
            adpcm_decode_byte_packed(buf[i], &st, dst);
            dst += 3;
        }

        uint16_t pack_len = bytes_read * 3u;
        g_roboplay_interface->opl4_write_wave_data(buf + PDX_DECODE_CHUNK, pack_len);

        *wave_addr += pack_len;
        remaining -= bytes_read;
    }
}

/* =========================================================
 * Ensure PDX extension
 * ========================================================= */

static void ensure_pdx_extension(char *filename)
{
    /* Find the end of the string */
    char *end = filename;
    while (*end) end++;

    /* Check if it already ends with .pdx (case insensitive) */
    if (end - filename >= 4) {
        char *ext = end - 4;
        if ((ext[0] == '.' || ext[0] == 0) &&
            (ext[1] == 'p' || ext[1] == 'P') &&
            (ext[2] == 'd' || ext[2] == 'D') &&
            (ext[3] == 'x' || ext[3] == 'X')) {
            return;  /* already has .pdx */
        }
    }

    /* Append .pdx */
    *end++ = '.';
    *end++ = 'P';
    *end++ = 'D';
    *end++ = 'X';
    *end = '\0';
}
/* =========================================================
 * PDX loading
 * ========================================================= */

bool pdx_load(const char *pdx_file_name)
{
    uint8_t  table[PDX_NUM_SAMPLES * 8];
    uint16_t header_address = 0;
    uint16_t slot_index = 0x80u;
    uint32_t wave_address;
    uint8_t  i;

    g_pdx_loaded = false;
    for (i = 0; i < PDX_NUM_SAMPLES; i++) g_pdx_header_slot[i] = 0xFFFFu;

    if (!g_roboplay_interface->exists((char *)pdx_file_name)) return false;

    g_roboplay_interface->opl4_write_wave(0x02, 0x11);
    g_roboplay_interface->open(pdx_file_name, false);

    if (g_roboplay_interface->read(table, sizeof(table)) != sizeof(table)) {
        g_roboplay_interface->close();
        g_roboplay_interface->opl4_write_wave(0x02, 0x10);
        return false;
    }

    wave_address = OPL_WAVE_MEMORY + 384u * 12u;

    for (i = 0; i < PDX_NUM_SAMPLES; i++) {
        uint32_t ofs = ((uint32_t)table[i*8+0] << 24) | ((uint32_t)table[i*8+1] << 16)
                     | ((uint32_t)table[i*8+2] << 8)  | table[i*8+3];
        uint32_t len = ((uint32_t)table[i*8+4] << 24) | ((uint32_t)table[i*8+5] << 16)
                     | ((uint32_t)table[i*8+6] << 8)  | table[i*8+7];

        if (len == 0) continue;

        uint32_t num_samples = len * 2u;
        uint32_t packed_len  = len * 3u;
        uint16_t end         = (uint16_t)((num_samples - 1u) ^ 0xFFFFu);

        /* Header */
        g_roboplay_interface->opl4_write_wave(3, 0x20);
        g_roboplay_interface->opl4_write_wave(4, (uint8_t)((header_address >> 8) & 0xFFu));
        g_roboplay_interface->opl4_write_wave(5, (uint8_t)(header_address & 0xFFu));

        g_roboplay_interface->opl4_write_wave(6, (uint8_t)((0x01u << 6) | ((wave_address >> 16) & 0x3Fu)));
        g_roboplay_interface->opl4_write_wave(6, (uint8_t)((wave_address >> 8) & 0xFFu));
        g_roboplay_interface->opl4_write_wave(6, (uint8_t)(wave_address & 0xFFu));

        /* Loop & End (one-shot) */
        {
            uint16_t loop = (uint16_t)(num_samples - 1u);
            g_roboplay_interface->opl4_write_wave(6, (uint8_t)((loop >> 8) & 0xFFu));
            g_roboplay_interface->opl4_write_wave(6, (uint8_t)(loop & 0xFFu));
        }
        g_roboplay_interface->opl4_write_wave(6, (uint8_t)((end >> 8) & 0xFFu));
        g_roboplay_interface->opl4_write_wave(6, (uint8_t)(end & 0xFFu));

        g_roboplay_interface->opl4_write_wave(6, 0x00u); /* LFO/VIB */
        g_roboplay_interface->opl4_write_wave(6, 0xF0u);
        g_roboplay_interface->opl4_write_wave(6, 0xFFu);
        g_roboplay_interface->opl4_write_wave(6, 0x0Fu);
        g_roboplay_interface->opl4_write_wave(6, 0x00u); /* AM */

        g_pdx_header_slot[i] = slot_index;
        header_address += 12u;
        slot_index++;

        g_roboplay_interface->opl4_write_wave(3, (uint8_t)((wave_address >> 16) & 0x3Fu));
        g_roboplay_interface->opl4_write_wave(4, (uint8_t)((wave_address >> 8) & 0xFFu));
        g_roboplay_interface->opl4_write_wave(5, (uint8_t)(wave_address & 0xFFu));

        g_roboplay_interface->seek((int32_t)ofs, ROBOPLAY_SEEK_START);
        pdx_decode_upload(len, &wave_address);
    }

    g_roboplay_interface->close();
    g_roboplay_interface->opl4_write_wave(0x02, 0x10);

    g_pdx_loaded = true;
    return true;
}

/* =========================================================
 * RoboPlay player interface
 * ========================================================= */

bool load(char *const file_name)
{
    g_empty_str[0]  = '\0';
    g_title_str     = g_empty_str;
    g_pdx_filename  = g_empty_str;

    g_roboplay_interface->open(file_name, false);
    if (!load_file_data()) {
        g_roboplay_interface->close();
        return false;
    }
    g_roboplay_interface->close();

    if (!parse_header()) return false;

    /* Load the PDX sample bank, if this song references one. A failed
     * or absent PDX load is not fatal to the MDX song itself -- the
     * PCM channel simply stays unused (channels_init() checks
     * g_pdx_loaded to decide whether to activate it), exactly as
     * if the song had no PCM channel at all. */
    if (g_pdx_filename[0] != '\0') {
        ensure_pdx_extension(g_pdx_filename);
        pdx_load(g_pdx_filename);
    }

    return true;
}

bool update(void)
{
    if (g_ended) return false;

    uint8_t i;
    for (i = 0; i < g_num_channels; i++) {
        MDX_CHANNEL *ch = &g_ch[i];
        if (ch->used && !ch->ended && !ch->waiting)
            channel_tick(i);
    }

    return !g_ended;
}

void rewind(const int8_t subsong)
{
    (void)subsong;

    g_current_segment = 0;
    g_roboplay_interface->set_segment(g_segment_list[0]);

    channels_init();

    /* Default Timer B value: 216, matching bpm_tempo=120 default 
     * (256 - 78125/(16*120) = 216), until the song's own 0xFF tempo 
     * command overrides it -- which MDX files do almost immediately. */
    g_timer_b = 216u;
    g_roboplay_interface->update_refresh();
}

void command(const uint8_t id)
{
    (void)id;
}

float get_refresh(void)
{
    /* Rate_hz = 4000000 / (1024 * divisor), reduced to integer terms */
    uint16_t divisor = 256u - g_timer_b;
    if (divisor == 0u) divisor = 1u;   /* guard against timer_b == 256 (impossible for uint8_t, but stay safe) */
    uint32_t denom = (uint32_t)divisor * 4uL;
    uint32_t rate_hz = (15625uL + denom / 2uL) / denom;   /* round-to-nearest */
    return (float)rate_hz;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char *get_player_info(void)
{
    return "MDX (MXDRV) player V1.1 by RoboSoft Inc.";
}

char *get_title(void)
{
    return g_title_str;
}

char *get_author(void)
{
    return "-";
}

char *get_description(void)
{
    return g_pdx_filename;
}