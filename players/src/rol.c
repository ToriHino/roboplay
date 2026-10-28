/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * rol.c
 *
 * ROL: AdLib Visual Composer music format
 */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "player_interface.h"

/* =========================================================
 * Constants
 * ========================================================= */

#define ROL_MAX_VOICES          11
#define ROL_MELODIC_VOICES      9
#define ROL_MAX_TEMPO_EVENTS    64
#define ROL_MAX_INSTRUMENTS     64
#define ROL_MAX_INSTR_EVENTS    16
#define BNK_MAX_NAME_ENTRIES    672     /* STANDARD.BNK entry count, for reference */
#define MAX_SEGMENTS            8

/* ROL header skip sizes */
#define ROL_COMMENT_SIZE        40
#define ROL_UNUSED1_SIZE        1
#define ROL_UNUSED2_SIZE        90
#define ROL_FILLER0_SIZE        38
#define ROL_FILLER1_SIZE        15
#define ROL_FILLER_SIZE         15
#define ROL_INSTR_FILLER_SIZE   3       /* 1 filler + 2 unused after instName */

/* BNK constants */
#define BNK_INS_NAME_SIZE       9
#define BNK_SIG_SIZE            6
#define BNK_RECORD_SIZE         30      /* mode(1)+voicenum(1)+2*SFMOperator(13)+2*waveform(1) */

/* OPL2 register addresses */
#define OPL2_WAVE_CTRL          0x01
#define OPL2_AMMULTI            0x20
#define OPL2_KSLTL              0x40
#define OPL2_ARDR               0x60
#define OPL2_SLRR               0x80
#define OPL2_FREQLO             0xA0
#define OPL2_KEYONFNUM          0xB0
#define OPL2_AMVIB_RHYTHM       0xBD
#define OPL2_FEEDCON            0xC0
#define OPL2_WAVEFORM           0xE0

/* OPL2 masks / values */
#define OPL2_KEYON_MASK         0x20
#define OPL2_RHYTHM_MASK        0x20
#define OPL2_ENABLE_WAVEFORM    0x20
#define OPL2_KSL_MASK           0xC0
#define OPL2_TL_MASK            0x3F
#define OPL2_TL_MIN             0x3F
#define OPL2_FNUMLSB_MASK       0xFF
#define OPL2_FNUMMSB_MASK       0x03
#define OPL2_BLOCK_SHIFT        2

/* Synth constants */
#define ROL_VERSION_MAJOR       0
#define ROL_VERSION_MINOR       4
#define ROL_SILENCE_NOTE        (-12)
#define ROL_MID_PITCH           0x2000u
#define ROL_MAX_VOLUME          0x7F
#define ROL_MAX_TICK_BEAT       60
#define ROL_CARRIER_OFFSET      3
#define ROL_NR_STEP_PITCH       25
#define ROL_MAX_NOTES           96

/* Rhythm-mode channel constants */
#define ROL_BASS_DRUM_CH        6
#define ROL_SNARE_CH            7
#define ROL_TOMTOM_CH           8
#define ROL_TOMTOM_NOTE         24
#define ROL_TOMTOM_TO_SNARE     7
#define ROL_SNARE_NOTE          (ROL_TOMTOM_NOTE + ROL_TOMTOM_TO_SNARE)

/* op_table[9]: melodic voice -> OPL modulator register offset */
static const uint8_t g_op_table[9] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12
};

/* drum_op_table[4]: rhythm voice (7-10) -> operator register offset
 * indexed by voice - ROL_SNARE_CH */
static const uint8_t g_drum_op_table[4] = { 0x14, 0x12, 0x15, 0x11 };

/* g_note_octave[96]: OPL block number for each note index */
static const uint8_t g_note_octave[ROL_MAX_NOTES] = {
    0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7
};

/* g_note_index[96]: semitone within octave (0-11) */
static const uint8_t g_note_index[ROL_MAX_NOTES] = {
    0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11
};

/* g_fnum_notes[25][12]: F-number table */
static const uint16_t g_fnum_notes[ROL_NR_STEP_PITCH][12] = {
    {343,364,385,408,433,459,486,515,546,579,614,650},
    {344,365,387,410,434,460,488,517,548,581,615,652},
    {345,365,387,410,435,461,489,518,549,582,617,653},
    {346,366,388,411,436,462,490,519,550,583,618,655},
    {346,367,389,412,437,463,491,520,551,584,619,657},
    {347,368,390,413,438,464,492,522,553,586,621,658},
    {348,369,391,415,439,466,493,523,554,587,622,660},
    {349,370,392,415,440,467,495,524,556,589,624,661},
    {350,371,393,416,441,468,496,525,557,590,625,663},
    {351,372,394,417,442,469,497,527,558,592,627,665},
    {351,372,395,418,443,470,498,528,559,593,628,666},
    {352,373,396,419,444,471,499,529,561,594,630,668},
    {353,374,397,420,445,472,500,530,562,596,631,669},
    {354,375,398,421,447,473,502,532,564,597,633,671},
    {355,376,398,422,448,474,503,533,565,599,634,672},
    {356,377,399,423,449,475,504,534,566,600,636,674},
    {356,378,400,424,450,477,505,535,567,601,637,675},
    {357,379,401,425,451,478,506,537,569,603,639,677},
    {358,379,402,426,452,479,507,538,570,604,640,679},
    {359,380,403,427,453,480,509,539,571,606,642,680},
    {360,381,404,428,454,481,510,540,572,607,643,682},
    {360,382,405,429,455,482,511,541,574,608,645,683},
    {361,383,406,430,456,483,512,543,575,610,646,685},
    {362,384,407,431,457,484,513,544,577,611,648,687},
    {363,385,408,432,458,485,514,545,578,612,649,688}
};

/* =========================================================
 * Segment-aware pointer and accessors
 * ========================================================= */

typedef struct {
    uint8_t  seg;
    uint16_t ofs;
} ROL_PTR;

static uint8_t  g_segment_list[MAX_SEGMENTS];
static uint8_t  g_num_segments;
static uint8_t  g_current_segment;

static void set_segment(uint8_t seg_index)
{
    if (g_current_segment == seg_index) return;
    g_current_segment = seg_index;
    g_roboplay_interface->set_segment(g_segment_list[seg_index]);
}

/* Read one byte from ptr and advance */
static uint8_t ptr_read_u8(ROL_PTR *p)
{
    uint8_t val;
    uint8_t *addr;
    set_segment(p->seg);
    addr = (uint8_t *)DATA_SEGMENT_BASE + p->ofs;
    val = *addr;
    p->ofs++;
    if (p->ofs >= DATA_SEGMENT_SIZE) {
        p->ofs = 0;
        p->seg++;
    }
    return val;
}

static uint16_t ptr_read_u16(ROL_PTR *p)
{
    uint8_t lo, hi;
    lo = ptr_read_u8(p);
    hi = ptr_read_u8(p);
    return (uint16_t)((uint16_t)hi << 8 | lo);
}

static int16_t ptr_read_i16(ROL_PTR *p)
{
    return (int16_t)ptr_read_u16(p);
}

static uint32_t ptr_read_float_bits(ROL_PTR *p)
{
    uint32_t b0, b1, b2, b3;
    b0 = ptr_read_u8(p);
    b1 = ptr_read_u8(p);
    b2 = ptr_read_u8(p);
    b3 = ptr_read_u8(p);
    return b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
}

static void ptr_skip(ROL_PTR *p, uint16_t n)
{
    uint16_t space;
    while (n > 0) {
        space = DATA_SEGMENT_SIZE - p->ofs;
        if (n < space) {
            p->ofs += n;
            break;
        }
        n -= space;
        p->ofs = 0;
        p->seg++;
    }
}

/* =========================================================
 * IEEE-754 single -> fixed-point conversion
 * No C float runtime used anywhere in the tick path.
 * ========================================================= */

/* Convert IEEE-754 bits to Q8.8 fixed-point.
 * value * 256 = mantissa * 2^(exponent - 142) */
static uint16_t float_bits_to_q8_8(uint32_t bits)
{
    uint8_t  exponent;
    uint32_t mantissa;
    int8_t   shift;
    uint32_t result;

    if ((bits & 0x7F800000UL) == 0) return 0;

    exponent = (uint8_t)((bits >> 23) & 0xFF);
    mantissa = (bits & 0x007FFFFFUL) | 0x00800000UL;

    shift = (int8_t)exponent - 142;
    if (shift >= 0) {
        if (shift >= 16) return 0xFFFF;
        result = mantissa << (uint8_t)shift;
    } else {
        shift = (int8_t)(-shift);
        if (shift >= 24) return 0;
        result = mantissa >> (uint8_t)shift;
    }

    if (result > 0xFFFF) return 0xFFFF;
    return (uint16_t)result;
}

/* Float bits -> uint8_t scaled by 'scale' (e.g. 127 for volume) */
static uint8_t float_bits_to_scaled_u8(uint32_t bits, uint8_t scale)
{
    uint16_t q;
    uint32_t v;
    q = float_bits_to_q8_8(bits);
    v = (uint32_t)q * scale;
    v >>= 8;
    if (v > scale) v = scale;
    return (uint8_t)v;
}

/* Float bits -> pitch bend value [0..0x3FFF]. */
static uint16_t float_bits_to_pitch(uint32_t bits)
{
    uint16_t q;
    uint32_t v;
    if (bits == 0x3F800000UL) return (uint16_t)ROL_MID_PITCH;
    q = float_bits_to_q8_8(bits);
    v = (uint32_t)q * 8191UL;
    v >>= 8;
    if (v > 0x3FFF) v = 0x3FFF;
    return (uint16_t)v;
}

/* =========================================================
 * Data structures
 * ========================================================= */

typedef struct {
    uint8_t ammulti;    /* reg 0x20 */
    uint8_t ksltl;      /* reg 0x40 */
    uint8_t ardr;       /* reg 0x60 */
    uint8_t slrr;       /* reg 0x80 */
    uint8_t fbc;        /* reg 0xC0 (channel-level) */
    uint8_t waveform;   /* reg 0xE0 */
} ROL_OPL2Op;

typedef struct {
    ROL_OPL2Op mod;
    ROL_OPL2Op car;
} ROL_Instrument;

/* Tempo event: stored in a flat array (small, <=64 events) */
typedef struct {
    int16_t  tick;
    uint16_t multiplier_q8_8;
} ROL_TempoEvent;

/* Per-voice playback state.
 * Note/volume/pitch event data lives in mapped segments; we store cursors.
 * Instrument events are pre-resolved at load time into g_instr_events[][]. */
typedef struct {
    /* Segment cursors -- point to the NEXT unread event in each track */
    ROL_PTR note_ptr;
    ROL_PTR volume_ptr;
    ROL_PTR pitch_ptr;
    ROL_PTR instr_ptr_saved;    /* saved at load time for resolve_instruments() */

    /* Remaining event counts */
    uint16_t notes_left;
    uint8_t  instrs_left;   /* counts down through g_instr_events[voice][] */
    uint8_t  volumes_left;
    uint8_t  pitches_left;

    /* Note timing state */
    int16_t  note_duration;
    int16_t  note_elapsed;
    bool     force_note;

    /* End-of-track flags */
    bool     note_end;
    bool     instr_end;
    bool     volume_end;
    bool     pitch_end;

    /* Lookahead tick for volume and pitch (read-ahead to avoid re-reading) */
    int16_t  next_volume_tick;
    int16_t  next_pitch_tick;

    /* OPL state cache */
    uint8_t  ksltl_cache;
    uint8_t  volume_cache;
    uint8_t  kon_oct_fnum;
    int8_t   note_cache;
    bool     key_on_cache;
    int16_t  half_tone_offset;
    uint8_t  fnum_row;
} ROL_Voice;

/* Instrument event: pre-resolved at load time, indexed during playback */
typedef struct {
    int16_t tick;
    uint8_t ins_index;
} ROL_InstrEvent;

/* Collected instrument name: read from segments before BNK file is opened */
#define MAX_COLLECTED_INSTRS  (ROL_MAX_VOICES * ROL_MAX_INSTR_EVENTS)

typedef struct {
    int16_t tick;
    char    name[BNK_INS_NAME_SIZE];
} CollectedInstr;

/* =========================================================
 * Player globals
 * ========================================================= */

/* ROL header runtime fields */
static uint16_t g_ticks_per_beat;
static uint16_t g_basic_tempo_q8_8;
static uint8_t  g_mode;
static uint8_t  g_num_voices;
static int16_t  g_time_of_last_note;

/* Tempo track (small, stored flat) */
static ROL_TempoEvent g_tempo_events[ROL_MAX_TEMPO_EVENTS];
static uint8_t        g_num_tempo_events;
static uint8_t        g_next_tempo_event;

/* Voice playback state */
static ROL_Voice g_voices[ROL_MAX_VOICES];

/* Resolved instrument table */
static ROL_Instrument g_instruments[ROL_MAX_INSTRUMENTS];
static uint8_t        g_num_instruments;

/* BNK header offsets (read once, used during resolve) */
static int32_t  g_bnk_offset_names;
static int32_t  g_bnk_offset_data;
static uint16_t g_bnk_num_total;       /* total entries in name table */
static bool     g_bnk_case_sensitive;

/* Playback state */
static int16_t  g_curr_tick;
static bool     g_ended;

/* OPL rhythm state */
static uint8_t  g_rhythm_mode;
static uint8_t  g_amvib_rhythm_cache;

/* Refresh rate Q8.8 */
static uint16_t g_refresh_q8_8;

/* Filenames */
static char g_bnk_filename[64];
static char g_comment[ROL_COMMENT_SIZE];

/* Instrument event tables (populated at load time, used during playback) */
static ROL_InstrEvent g_instr_events[ROL_MAX_VOICES][ROL_MAX_INSTR_EVENTS];
static uint8_t        g_instr_event_count[ROL_MAX_VOICES];
static uint8_t        g_collected_base[ROL_MAX_VOICES];

/* g_bnk_names and g_collected_instrs are NOT in static BSS.
 * They use READ_BUFFER (0xC000) as load-time scratch. */
#define INSTR_SCRATCH  ((CollectedInstr *)READ_BUFFER)

/* =========================================================
 * Convenience macro
 * ========================================================= */

#define OPL_WRITE(reg, val)  g_roboplay_interface->opl4_write_fm_1((reg), (val))

/* =========================================================
 * BNK file helpers (direct file I/O, used only during load)
 * ========================================================= */

static uint8_t bnk_read_u8(void)
{
    uint8_t v;
    g_roboplay_interface->read(&v, 1);
    return v;
}

static uint16_t bnk_read_u16(void)
{
    uint8_t buf[2];
    g_roboplay_interface->read(buf, 2);
    return (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
}

static int32_t bnk_read_i32(void)
{
    uint8_t buf[4];
    g_roboplay_interface->read(buf, 4);
    return (int32_t)(  (uint32_t)buf[0]
                     | ((uint32_t)buf[1] << 8)
                     | ((uint32_t)buf[2] << 16)
                     | ((uint32_t)buf[3] << 24));
}

/* =========================================================
 * Case-insensitive string compare
 * ========================================================= */

static int rol_stricmp(const char *a, const char *b)
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
 * BNK loading
 * ========================================================= */

/* Read BNK header only -- stores offsets for later single-pass scan. */
static bool bnk_load_info(void)
{
    uint8_t  ver_major, ver_minor;
    char     sig[BNK_SIG_SIZE];
    uint16_t num_used;

    ver_major = bnk_read_u8();
    ver_minor = bnk_read_u8();
    (void)ver_major; (void)ver_minor;

    g_roboplay_interface->read(sig, BNK_SIG_SIZE);
    (void)sig;

    num_used             = bnk_read_u16(); (void)num_used;
    g_bnk_num_total      = bnk_read_u16();
    g_bnk_offset_names   = bnk_read_i32();
    g_bnk_offset_data    = bnk_read_i32();

    /* Detect case sensitivity by sampling the first two used entries.
     * Read the first BNK_INS_NAME_SIZE*2+6 bytes of the name table.
     * If the second name sorts before the first case-insensitively,
     * the BNK uses case-sensitive sorting. */
    {
        uint8_t  used1, used2;
        char     name1[BNK_INS_NAME_SIZE];
        char     name2[BNK_INS_NAME_SIZE];
        uint16_t i;
        uint8_t  found;

        g_bnk_case_sensitive = false;
        g_roboplay_interface->seek(g_bnk_offset_names, ROBOPLAY_SEEK_START);

        found = 0;
        name1[0] = '\0';
        name2[0] = '\0';

        for (i = 0; i < g_bnk_num_total && found < 2; i++) {
            bnk_read_u16();   /* index */
            used1 = bnk_read_u8();
            if (found == 0) {
                g_roboplay_interface->read(name1, BNK_INS_NAME_SIZE);
                name1[BNK_INS_NAME_SIZE-1] = '\0';
                if (used1) found = 1;
            } else {
                g_roboplay_interface->read(name2, BNK_INS_NAME_SIZE);
                name2[BNK_INS_NAME_SIZE-1] = '\0';
                used2 = used1;
                if (used2) {
                    if (rol_stricmp(name1, name2) > 0)
                        g_bnk_case_sensitive = true;
                    found = 2;
                }
            }
        }
    }

    return true;
}

/* Convert one SFMOperator (13 raw bytes from BNK) to packed SOPL2Op. */
static void read_fm_operator(ROL_OPL2Op *out)
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
    out->fbc     = (uint8_t)((feedback<<1)|(fm_type^1)); /* connection bit inverted */
}

/* Read one BNK instrument record at the given record index.
 * Seeks to the correct position and reads into g_instruments[ins_slot]. */
static void bnk_read_instrument(uint8_t ins_slot, uint16_t record_index)
{
    int32_t        seek_ofs;
    ROL_Instrument *ins;
    uint8_t        mode, voice_num;

    ins      = &g_instruments[ins_slot];
    seek_ofs = g_bnk_offset_data + (int32_t)record_index * BNK_RECORD_SIZE;
    g_roboplay_interface->seek(seek_ofs, ROBOPLAY_SEEK_START);

    mode      = bnk_read_u8(); (void)mode;
    voice_num = bnk_read_u8(); (void)voice_num;

    read_fm_operator(&ins->mod);
    read_fm_operator(&ins->car);
    ins->mod.waveform = bnk_read_u8();
    ins->car.waveform = bnk_read_u8();
}

/* =========================================================
 * OPL2 register write helpers
 * ========================================================= */

/* Volume-scaled carrier total level. */
static uint8_t get_ksltl(uint8_t voice)
{
    uint8_t  ksltl, volume;
    uint16_t amplitude, ksl_tl;

    ksltl    = g_voices[voice].ksltl_cache;
    volume   = g_voices[voice].volume_cache;
    amplitude = (uint16_t)(OPL2_TL_MIN - (ksltl & OPL2_TL_MASK));
    ksl_tl    = (uint16_t)volume * amplitude;
    ksl_tl    = ksl_tl + ksl_tl + ROL_MAX_VOLUME;
    ksl_tl    = OPL2_TL_MIN - (ksl_tl / (2u * ROL_MAX_VOLUME));
    ksl_tl   |= (ksltl & OPL2_KSL_MASK);
    return (uint8_t)ksl_tl;
}

/* Write all OPL2 operator registers for one voice. */
static void send_operator(uint8_t voice, const ROL_Instrument *ins)
{
    uint8_t op;

    if (voice < ROL_SNARE_CH || !g_rhythm_mode) {
        if (voice >= ROL_MELODIC_VOICES) return;
        op = g_op_table[voice];

        OPL_WRITE(OPL2_AMMULTI  + op,                   ins->mod.ammulti);
        OPL_WRITE(OPL2_KSLTL    + op,                   ins->mod.ksltl);
        OPL_WRITE(OPL2_ARDR     + op,                   ins->mod.ardr);
        OPL_WRITE(OPL2_SLRR     + op,                   ins->mod.slrr);
        OPL_WRITE(OPL2_FEEDCON  + voice,                ins->mod.fbc);
        OPL_WRITE(OPL2_WAVEFORM + op,                   ins->mod.waveform);

        g_voices[voice].ksltl_cache = ins->car.ksltl;

        OPL_WRITE(OPL2_AMMULTI  + op + ROL_CARRIER_OFFSET, ins->car.ammulti);
        OPL_WRITE(OPL2_KSLTL    + op + ROL_CARRIER_OFFSET, get_ksltl(voice));
        OPL_WRITE(OPL2_ARDR     + op + ROL_CARRIER_OFFSET, ins->car.ardr);
        OPL_WRITE(OPL2_SLRR     + op + ROL_CARRIER_OFFSET, ins->car.slrr);
        OPL_WRITE(OPL2_WAVEFORM + op + ROL_CARRIER_OFFSET, ins->car.waveform);
    } else {
        op = g_drum_op_table[voice - ROL_SNARE_CH];
        g_voices[voice].ksltl_cache = ins->mod.ksltl;

        OPL_WRITE(OPL2_AMMULTI  + op, ins->mod.ammulti);
        OPL_WRITE(OPL2_KSLTL    + op, get_ksltl(voice));
        OPL_WRITE(OPL2_ARDR     + op, ins->mod.ardr);
        OPL_WRITE(OPL2_SLRR     + op, ins->mod.slrr);
        OPL_WRITE(OPL2_WAVEFORM + op, ins->mod.waveform);
    }
}

/* Set OPL frequency registers for a voice. */
static void set_freq(uint8_t voice, int8_t note, bool key_on)
{
    int16_t  biased;
    uint16_t freq;
    uint8_t  oct, kon_hi;

    biased = (int16_t)note + g_voices[voice].half_tone_offset;
    if (biased < 0)              biased = 0;
    if (biased >= ROL_MAX_NOTES) biased = ROL_MAX_NOTES - 1;

    freq   = g_fnum_notes[g_voices[voice].fnum_row][g_note_index[biased]];
    oct    = g_note_octave[biased];

    g_voices[voice].note_cache   = note;
    g_voices[voice].key_on_cache = key_on;

    kon_hi = (uint8_t)((oct << OPL2_BLOCK_SHIFT)
                      | ((freq >> 8) & OPL2_FNUMMSB_MASK));
    g_voices[voice].kon_oct_fnum = kon_hi;

    OPL_WRITE(OPL2_FREQLO    + voice, (uint8_t)(freq & OPL2_FNUMLSB_MASK));
    OPL_WRITE(OPL2_KEYONFNUM + voice, (uint8_t)(kon_hi | (key_on ? OPL2_KEYON_MASK : 0)));
}

/* Apply pitch bend to a voice, recomputing half_tone_offset and fnum_row. */
static void change_pitch(uint8_t voice, uint16_t pitch_bend)
{
    int32_t pbl;
    int16_t step_dir, delta, step_down;

    if (voice >= ROL_BASS_DRUM_CH && g_rhythm_mode) return;

    pbl      = (int32_t)((int16_t)(pitch_bend - ROL_MID_PITCH)) * ROL_NR_STEP_PITCH;
    step_dir = (int16_t)(pbl / (int32_t)ROL_MID_PITCH);

    if (step_dir < 0) {
        step_down = (int16_t)(ROL_NR_STEP_PITCH - 1 - step_dir);
        g_voices[voice].half_tone_offset = (int16_t)(-(step_down / ROL_NR_STEP_PITCH));
        delta = (int16_t)((step_down - ROL_NR_STEP_PITCH + 1) % ROL_NR_STEP_PITCH);
        if (delta) delta = (int16_t)(ROL_NR_STEP_PITCH - delta);
    } else {
        g_voices[voice].half_tone_offset = (int16_t)(step_dir / ROL_NR_STEP_PITCH);
        delta = (int16_t)(step_dir % ROL_NR_STEP_PITCH);
    }

    g_voices[voice].fnum_row = (uint8_t)delta;
    set_freq(voice, g_voices[voice].note_cache, g_voices[voice].key_on_cache);
}

/* =========================================================
 * Note on / off
 * ========================================================= */

static void note_on_melodic(uint8_t voice, int8_t note)
{
    if (voice >= ROL_MELODIC_VOICES) return;
    OPL_WRITE(OPL2_KEYONFNUM + voice,
              g_voices[voice].kon_oct_fnum & (uint8_t)~OPL2_KEYON_MASK);
    g_voices[voice].key_on_cache = false;
    if (note != ROL_SILENCE_NOTE)
        set_freq(voice, note, true);
}

static void note_on_percussive(uint8_t voice, int8_t note)
{
    uint8_t ch_bit;
    ch_bit = (uint8_t)(1u << (4u - (uint8_t)(voice - ROL_BASS_DRUM_CH)));

    g_amvib_rhythm_cache &= (uint8_t)~ch_bit;
    OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
    g_voices[voice].key_on_cache = false;

    if (note != ROL_SILENCE_NOTE) {
        if (voice == ROL_TOMTOM_CH) {
            set_freq(ROL_TOMTOM_CH, note, false);
            set_freq(ROL_SNARE_CH, (int8_t)(note + ROL_TOMTOM_TO_SNARE), false);
        } else if (voice == ROL_BASS_DRUM_CH) {
            set_freq(voice, note, false);
        }
        g_voices[voice].key_on_cache = true;
        g_amvib_rhythm_cache |= ch_bit;
        OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
    }
}

static void voice_note_on(uint8_t voice, int8_t note)
{
    if (voice < ROL_BASS_DRUM_CH || !g_rhythm_mode)
        note_on_melodic(voice, note);
    else
        note_on_percussive(voice, note);
}

/* =========================================================
 * Volume
 * ========================================================= */

static void set_volume(uint8_t voice, uint8_t volume)
{
    uint8_t op;
    if (voice >= ROL_MELODIC_VOICES && !g_rhythm_mode) return;
    g_voices[voice].volume_cache = volume;
    if (voice < ROL_SNARE_CH || !g_rhythm_mode)
        op = g_op_table[voice] + ROL_CARRIER_OFFSET;
    else
        op = g_drum_op_table[voice - ROL_SNARE_CH];
    OPL_WRITE(OPL2_KSLTL + op, get_ksltl(voice));
}

/* =========================================================
 * Rhythm mode
 * ========================================================= */

static void set_rhythm_mode(uint8_t mode)
{
    if (mode) {
        g_amvib_rhythm_cache |= OPL2_RHYTHM_MASK;
        OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
        set_freq(ROL_TOMTOM_CH, ROL_TOMTOM_NOTE, false);
        set_freq(ROL_SNARE_CH,  ROL_SNARE_NOTE,  false);
    } else {
        g_amvib_rhythm_cache &= (uint8_t)~OPL2_RHYTHM_MASK;
        OPL_WRITE(OPL2_AMVIB_RHYTHM, g_amvib_rhythm_cache);
    }
    g_rhythm_mode = mode;
}

/* =========================================================
 * Refresh rate
 *
 * Computed in Q8.8 fixed-point throughout.
 * ========================================================= */

static void update_refresh(uint16_t multiplier_q8_8)
{
    uint16_t tick_beat;
    uint32_t result;

    tick_beat = g_ticks_per_beat;
    if (tick_beat > ROL_MAX_TICK_BEAT) tick_beat = ROL_MAX_TICK_BEAT;

    /* result = (tick_beat * basic_tempo_q8_8 * multiplier_q8_8) / (60 * 65536)
     *   i.e. frequency in Hz = (BPM / 60) * tickBeat * mult
     * Fixed-point: BPM_q88 * tickBeat * mult_q88 / (60 << 16) */
    result = (uint32_t)tick_beat
           * (uint32_t)g_basic_tempo_q8_8
           * (uint32_t)multiplier_q8_8;

    /* Divide by 60*65536 = 3932160 with rounding.
     * First >>8 to keep numbers manageable, then / (60*256). */
    result >>= 8;  /* now effectively divided by 256 */
    result = (result + 7680u) / 15360u;   /* / (60*256) with rounding */

    g_refresh_q8_8 = (result > 0xFFFF) ? 0xFFFF : (uint16_t)result;
    g_roboplay_interface->update_refresh();
}

/* =========================================================
 * ROL file loading into segments
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
 * ROL file parsing
 * ========================================================= */

 static bool parse_rol(void)
{
    ROL_PTR  p;
    uint16_t ver_major, ver_minor;
    uint8_t  voice, i;
    uint16_t n;
    int16_t  time_last, total_dur;
    int16_t  dur;
    ROL_Voice *v;

    p.seg = 0;
    p.ofs = 0;

    ver_major = ptr_read_u16(&p);
    ver_minor = ptr_read_u16(&p);
    if (ver_major != ROL_VERSION_MAJOR || ver_minor != ROL_VERSION_MINOR)
        return false;

    /* Comment */
    for (i = 0; i < ROL_COMMENT_SIZE; i++)
        g_comment[i] = (char)ptr_read_u8(&p);
    g_comment[ROL_COMMENT_SIZE - 1] = '\0';

    g_ticks_per_beat = ptr_read_u16(&p);
    ptr_read_u16(&p);                       /* beats_per_measure */
    ptr_read_u16(&p);                       /* edit_scale_y */
    ptr_read_u16(&p);                       /* edit_scale_x */
    ptr_skip(&p, ROL_UNUSED1_SIZE);         /* 1 byte */
    g_mode = ptr_read_u8(&p);
    ptr_skip(&p, ROL_UNUSED2_SIZE);         /* 90 bytes */
    ptr_skip(&p, ROL_FILLER0_SIZE);         /* 38 bytes */
    ptr_skip(&p, ROL_FILLER1_SIZE);         /* 15 bytes (not on wiki) */

    g_basic_tempo_q8_8 = float_bits_to_q8_8(ptr_read_float_bits(&p));
    g_num_voices = g_mode ? ROL_MELODIC_VOICES : ROL_MAX_VOICES;

    /* --- Tempo track --- */
    n = ptr_read_u16(&p);
    if (n > ROL_MAX_TEMPO_EVENTS) n = ROL_MAX_TEMPO_EVENTS;
    g_num_tempo_events = (uint8_t)n;
    for (i = 0; i < g_num_tempo_events; i++) {
        g_tempo_events[i].tick            = ptr_read_i16(&p);
        g_tempo_events[i].multiplier_q8_8 = float_bits_to_q8_8(ptr_read_float_bits(&p));
    }

    g_time_of_last_note = 0;

    /* --- Voice tracks --- */
    for (voice = 0; voice < g_num_voices; voice++) {
        v = &g_voices[voice];

        /* ---- Note track ---- */
        ptr_skip(&p, ROL_FILLER_SIZE);      /* 15-byte track name */
        time_last = ptr_read_i16(&p);
        total_dur = 0;
        v->notes_left = 0;

        if (time_last != 0) {
            /* Save cursor at first note event */
            v->note_ptr = p;

            /* Walk through to count events and advance p past them */
            do {
                ptr_read_i16(&p);           /* note number */
                dur = ptr_read_i16(&p);
                total_dur += dur;
                v->notes_left++;
            } while (total_dur < time_last);

            if (time_last > g_time_of_last_note)
                g_time_of_last_note = time_last;
        }
        ptr_skip(&p, ROL_FILLER_SIZE);      /* 15-byte trailing skip */

        /* ---- Instrument track ---- */
        n = ptr_read_u16(&p);
        v->instrs_left = (uint8_t)(n < ROL_MAX_INSTR_EVENTS ? n : ROL_MAX_INSTR_EVENTS);

        /* Save cursor at start of first event for resolve_instruments().
         * Then skip past all n events so p advances to the next track. */
        g_voices[voice].instr_ptr_saved = p;
        ptr_skip(&p, (uint16_t)n * (2u + BNK_INS_NAME_SIZE + ROL_INSTR_FILLER_SIZE));
        ptr_skip(&p, ROL_FILLER_SIZE);      /* 15-byte trailing skip */

        /* ---- Volume track ---- */
        n = ptr_read_u16(&p);
        v->volumes_left = (uint8_t)(n < 255 ? n : 255);

        if (n > 0) {
            v->volume_ptr = p;
            {
                ROL_PTR tmp;
                tmp = v->volume_ptr;
                v->next_volume_tick = ptr_read_i16(&tmp);
            }
            ptr_skip(&p, (uint16_t)v->volumes_left * 6u);
        } else {
            v->next_volume_tick = 0x7FFF;
        }
        ptr_skip(&p, ROL_FILLER_SIZE);      /* 15-byte trailing skip */

        /* ---- Pitch track ---- */
        n = ptr_read_u16(&p);
        v->pitches_left = (uint8_t)(n < 255 ? n : 255);

        if (n > 0) {
            v->pitch_ptr = p;
            {
                ROL_PTR tmp;
                tmp = v->pitch_ptr;
                v->next_pitch_tick = ptr_read_i16(&tmp);
            }
            ptr_skip(&p, (uint16_t)v->pitches_left * 6u);
        } else {
            v->next_pitch_tick = 0x7FFF;
        }
        /* No trailing skip for pitch track */
    }

    return true;
}

/* =========================================================
 * Instrument name collection and resolution.
 * ========================================================= */

/* Phase 1: collect tick+name pairs from segments into READ_BUFFER. */
static void collect_instr_names(void)
{
    CollectedInstr *scratch;
    uint8_t         voice, i, k;
    uint8_t         total;
    uint8_t         count;
    ROL_PTR         p;

    scratch = INSTR_SCRATCH;
    total   = 0;

    for (voice = 0; voice < g_num_voices; voice++) {
        count = g_voices[voice].instrs_left;
        if (count > ROL_MAX_INSTR_EVENTS) count = ROL_MAX_INSTR_EVENTS;

        g_collected_base[voice]    = total;
        g_instr_event_count[voice] = count;

        if (count == 0) continue;

        p = g_voices[voice].instr_ptr_saved;

        for (i = 0; i < count && total < MAX_COLLECTED_INSTRS; i++, total++) {
            scratch[total].tick = ptr_read_i16(&p);
            for (k = 0; k < BNK_INS_NAME_SIZE; k++)
                scratch[total].name[k] = (char)ptr_read_u8(&p);
            scratch[total].name[BNK_INS_NAME_SIZE - 1] = '\0';
            ptr_skip(&p, ROL_INSTR_FILLER_SIZE);
        }
    }
}

/* Phase 2: single sequential scan of BNK name table.
 * For each BNK entry that matches a collected name, load the instrument
 * data once and assign its slot index to all matching events. */
static void resolve_instruments(void)
{
    CollectedInstr *scratch;
    uint8_t  total_collected;
    uint16_t bnk_i;
    uint16_t rec_index;
    uint8_t  used;
    char     bnk_name[BNK_INS_NAME_SIZE];
    uint8_t  voice, i, ci, ei;
    int      cmp;
    uint8_t  ins_slot;
    int32_t  name_pos;

    scratch = INSTR_SCRATCH;

    /* Count total collected entries */
    total_collected = 0;
    for (voice = 0; voice < g_num_voices; voice++)
        total_collected = (uint8_t)(total_collected + g_instr_event_count[voice]);

    /* Slot 0 = silence (zero-initialised instrument, written by memset) */
    memset(g_instruments, 0, sizeof(g_instruments[0]));
    g_num_instruments = 1;

    /* Copy ticks, default ins_index to 0 (silence) */
    for (voice = 0; voice < g_num_voices; voice++) {
        for (i = 0; i < g_instr_event_count[voice]; i++) {
            ci = g_collected_base[voice] + i;
            g_instr_events[voice][i].tick      = scratch[ci].tick;
            g_instr_events[voice][i].ins_index = 0;
        }
    }

    /* Single sequential scan of BNK name table */
    g_roboplay_interface->seek(g_bnk_offset_names, ROBOPLAY_SEEK_START);

    for (bnk_i = 0; bnk_i < g_bnk_num_total; bnk_i++) {
        rec_index = bnk_read_u16();
        used      = bnk_read_u8();
        g_roboplay_interface->read(bnk_name, BNK_INS_NAME_SIZE);
        bnk_name[BNK_INS_NAME_SIZE - 1] = '\0';

        if (!used) continue;

        /* Check if any collected entry matches this BNK name.
         * We break after the first match, so the instrument load
         * inside the loop body is reached at most once per BNK entry. */
        ins_slot = 0;

        for (ci = 0; ci < total_collected; ci++) {
            if (g_bnk_case_sensitive)
                cmp = memcmp(bnk_name, scratch[ci].name, BNK_INS_NAME_SIZE - 1);
            else
                cmp = rol_stricmp(bnk_name, scratch[ci].name);

            if (cmp != 0) continue;

            /* Load instrument data -- seek into data section and back.
             * Must save/restore position to preserve the name table scan.
             * Next entry starts at offset_names + (bnk_i+1)*12. */
            if (g_num_instruments < ROL_MAX_INSTRUMENTS) {
                name_pos = g_bnk_offset_names + (int32_t)(bnk_i + 1) * 12;

                ins_slot = g_num_instruments;
                bnk_read_instrument(ins_slot, rec_index);
                g_num_instruments++;

                g_roboplay_interface->seek(name_pos, ROBOPLAY_SEEK_START);
            }

            /* Assign slot to all events whose name matches this BNK entry */
            for (voice = 0; voice < g_num_voices; voice++) {
                for (i = 0; i < g_instr_event_count[voice]; i++) {
                    ei = g_collected_base[voice] + i;
                    if (g_bnk_case_sensitive)
                        cmp = memcmp(scratch[ei].name, bnk_name,
                                     BNK_INS_NAME_SIZE - 1);
                    else
                        cmp = rol_stricmp(scratch[ei].name, bnk_name);
                    if (cmp == 0)
                        g_instr_events[voice][i].ins_index = ins_slot;
                }
            }
            break;
        }
    }
}

/* =========================================================
 * Per-voice tick update
 * Event data is read live from segment memory via cursors.
 * ========================================================= */

static void update_voice(uint8_t voice)
{
    ROL_Voice    *v;
    uint8_t       idx;
    int8_t        note;
    uint32_t      fbits;
    uint8_t       volume;
    uint16_t      pitch;
    uint8_t       ei;
    ROL_PTR       tmp;

    v = &g_voices[voice];
    if (v->notes_left == 0 || v->note_end) return;

    /* --- Instrument event --- */
    if (!v->instr_end) {
        if (v->instrs_left > 0) {
            ei = g_instr_event_count[voice] - v->instrs_left;
            if (g_instr_events[voice][ei].tick == g_curr_tick) {
                idx = g_instr_events[voice][ei].ins_index;
                send_operator(voice, &g_instruments[idx]);
                v->instrs_left--;
            }
        } else {
            v->instr_end = true;
        }
    }

    /* --- Volume event --- */
    if (!v->volume_end) {
        if (v->volumes_left > 0 && v->next_volume_tick == g_curr_tick) {
            ptr_read_i16(&v->volume_ptr);   /* consume tick field */
            fbits  = ptr_read_float_bits(&v->volume_ptr);
            volume = float_bits_to_scaled_u8(fbits, ROL_MAX_VOLUME);
            set_volume(voice, volume);
            v->volumes_left--;
            if (v->volumes_left > 0) {
                tmp = v->volume_ptr;
                v->next_volume_tick = ptr_read_i16(&tmp);
            } else {
                v->volume_end = true;
            }
        }
    }

    /* --- Note advance --- */
    if (v->force_note || (v->note_elapsed >= v->note_duration)) {
        /* On force_note (first tick): play note 0 without considering "previous note done".
         * On subsequent advances: the previous note is done; if no notes remain, key off. */
        if (!v->force_note) {
            if (v->notes_left == 0) {
                voice_note_on(voice, ROL_SILENCE_NOTE);
                v->note_end = true;
                return;
            }
        }

        /* Read next note from segment cursor */
        note             = (int8_t)(ptr_read_i16(&v->note_ptr) + ROL_SILENCE_NOTE);
        v->note_duration = ptr_read_i16(&v->note_ptr);
        v->notes_left--;
        voice_note_on(voice, note);
        v->note_elapsed = 0;
        v->force_note   = false;
    }

    /* --- Pitch event --- */
    if (!v->pitch_end) {
        if (v->pitches_left > 0 && v->next_pitch_tick == g_curr_tick) {
            ptr_read_i16(&v->pitch_ptr);    /* consume tick field */
            fbits = ptr_read_float_bits(&v->pitch_ptr);
            pitch = float_bits_to_pitch(fbits);
            change_pitch(voice, pitch);
            v->pitches_left--;
            if (v->pitches_left > 0) {
                tmp = v->pitch_ptr;
                v->next_pitch_tick = ptr_read_i16(&tmp);
            } else {
                v->pitch_end = true;
            }
        }
    }

    v->note_elapsed++;
}

/* =========================================================
 * Voice reset
 * ========================================================= */

static void reset_voices(void)
{
    uint8_t    i;
    ROL_Voice *v;

    for (i = 0; i < g_num_voices; i++) {
        v = &g_voices[i];
        /* note_ptr, volume_ptr, pitch_ptr, instr_ptr_saved are restored
         * by parse_rol() which is called before reset_voices() on rewind. */
        v->instrs_left      = g_instr_event_count[i];
        v->note_duration    = 0;
        v->note_elapsed     = 0;
        v->force_note       = true;
        v->note_end         = false;
        v->instr_end        = (g_instr_event_count[i] == 0);
        v->volume_end       = false;
        v->pitch_end        = false;
        v->ksltl_cache      = 0;
        v->volume_cache     = ROL_MAX_VOLUME;
        v->kon_oct_fnum     = 0;
        v->note_cache       = 0;
        v->key_on_cache     = false;
        v->half_tone_offset = 0;
        v->fnum_row         = 0;
    }
}

/* =========================================================
 * RoboPlay player interface
 * ========================================================= */

static void rol_rewind(const uint8_t subsong);

bool load(const char *file_name)
{
    g_comment[0]      = '\0';
    g_num_instruments = 0;
    g_num_voices      = 0;

    /* Load ROL into segments */
    g_roboplay_interface->open(file_name, false);
    if (!load_file_data()) {
        g_roboplay_interface->close();
        return false;
    }
    g_roboplay_interface->close();

    /* Parse ROL structure from segments -- no file I/O after this point
     * until we open the BNK. Sets up voice cursors and instr_ptr_saved. */
    if (!parse_rol()) return false;

    /* Collect instrument names from segment data into a flat list.
     * Must be done before opening BNK so segment reads don't interfere
     * with file I/O (both use page 2 on MSX). */
    collect_instr_names();

    /* Open BNK, load name table, resolve all instruments, close.
     * No segment reads (ptr_read_u8 etc.) happen inside this block. */
//    build_bnk_filename(file_name);
//    if (!g_roboplay_interface->exists(g_bnk_filename)) return false;

    g_roboplay_interface->open("STANDARD.BNK", true);
    if (!bnk_load_info()) {
        g_roboplay_interface->close();
        return false;
    }
    resolve_instruments();
    g_roboplay_interface->close();

    return true;
}

bool update(void)
{
    uint8_t i;

    if (g_ended) return false;

    /* Tempo event */
    if (g_next_tempo_event < g_num_tempo_events &&
        g_tempo_events[g_next_tempo_event].tick == g_curr_tick) {
        update_refresh(g_tempo_events[g_next_tempo_event].multiplier_q8_8);
        g_next_tempo_event++;
    }

    for (i = 0; i < g_num_voices; i++)
        update_voice(i);

    g_curr_tick++;

    if (g_curr_tick > g_time_of_last_note) {
        g_ended = true;
        return false;
    }

    return true;
}

static void rol_rewind(const uint8_t subsong)
{
    (void)subsong;

    g_curr_tick          = 0;
    g_next_tempo_event   = 0;
    g_ended              = false;
    g_amvib_rhythm_cache = 0;
    g_rhythm_mode        = 0;

    /* Re-parse to restore all voice cursors to start of each track */
    parse_rol();
    reset_voices();

    g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL2);
    OPL_WRITE(OPL2_WAVE_CTRL, OPL2_ENABLE_WAVEFORM);

    set_rhythm_mode(g_mode ^ 1);
    update_refresh(0x0100u);   /* multiplier = 1.0 in Q8.8 */
}

void rewind(const uint8_t subsong)
{
    rol_rewind(subsong);
}

void command(const uint8_t id)
{
    (void)id;
}

float get_refresh(void)
{
    return (float)(g_refresh_q8_8 >> 8);
}

uint8_t get_subsongs(void)
{
    return 0;
}

char *get_player_info(void)
{
    return "ROL (AdLib Visual Composer) player V1.0 by RoboSoft Inc.";
}

char *get_title(void)
{
    if (g_comment[0] == '\0' ||
        memcmp(g_comment, "\\roll\\default", 13) == 0)
        return "";
    return g_comment;
}

char *get_author(void)
{
    return "-";
}

char *get_description(void)
{
    return "-";
}