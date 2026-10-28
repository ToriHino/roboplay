/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * zsm.c
 *
 * ZSM: Commander X16 zsound player
 *
 * VERA PSG (16 voices) is emulated on the OPL4 FM section, the YM2151 stream goes to the OPM.
 */

#include <string.h>

#include "player_interface.h"
#include "zsm.h"

#define NOISE_FV_DEFAULT 3333
#define NOISE_FV_MIN     1000
#define NOISE_FV_MAX     8000

/* VERA frequency word -> OPL4 FM fnum:
     f_vera = fv * 48828.125 / 2^17            (VERA PSG rate = 25 MHz / 512, / 2^17)
     fnum   = f * 2^(20 - block) / R            (R = OPL4 FM sample rate, MULT = 1)
            = fv * 7.88891 / 2^block
   The OPL4 FM section runs at 33868800 / 684 = 49515.8 Hz (the figure openMSX
   models), which is 0.4 % (7 cents) lower than the 49716 Hz of a real OPL3 -
   not 44100 Hz, which is the rate of the wave table part.
   FNUM_BASE is 7.88891 * 2^12. The highest note reachable (block 7, fnum 1023)
   is about 6.2 kHz (fv ~ 16600); higher values are clamped.

   ZSM_TUNE_PPM shifts all VERA voices by a few parts per million (+ = higher)
   to line them up with the OPM when the real chip clocks are not the nominal
   ones: ppm = (f_opm / f_vera_voice - 1) * 1e6, about 578 ppm per cent. */
#ifndef ZSM_TUNE_PPM
#define ZSM_TUNE_PPM 0
#endif

#define FNUM_BASE 32313L
#define FNUM_MUL  (FNUM_BASE + (FNUM_BASE * (ZSM_TUNE_PPM)) / 1000000L)

/* Highest fv for which the fnum still fits in 10 bits, per block 0..6.
   Choosing the lowest fitting block keeps fnum as large (precise) as possible.
   (Untuned: 129, 259, 518, 1037, 2075, 4151, 8303) */
#define BLOCK_LIMIT(b) ((uint16_t)(((2047UL << (11 + (b))) - 1UL) / (unsigned long)FNUM_MUL))

static const uint16_t g_block_limit[7] =
{
  BLOCK_LIMIT(0), BLOCK_LIMIT(1), BLOCK_LIMIT(2), BLOCK_LIMIT(3),
  BLOCK_LIMIT(4), BLOCK_LIMIT(5), BLOCK_LIMIT(6)
};

/* Operator register offset of the modulator within an FM bank (carrier = +3).
   VERA voices 0..8 are OPL4 FM channels 0..8 (bank 1), voices 9..15 are
   channels 0..6 of bank 2. */
static const uint8_t g_op_offset[NR_OF_PSG_CHANNELS] =
{
  0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12,
  0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10
};

/* Channel number within the bank (for registers 0xA0 / 0xB0 / 0xC0) */
static const uint8_t g_chan_index[NR_OF_PSG_CHANNELS] =
{
  0, 1, 2, 3, 4, 5, 6, 7, 8,
  0, 1, 2, 3, 4, 5, 6
};

/* VERA volume (0..63) -> OPL TL (0.75 dB per step).
   Derived from the VERA PSG volume curve as implemented in the X16 emulator
   (vera_psg.c volume_lut, 0..511): TL = round(20 * log10(511 / lut[v]) / 0.75),
   clamped to 63. Volume 0 is handled by keying the channel off. */
static const uint8_t g_vol_to_tl[64] =
{
  63, 56, 48, 43, 40, 39, 39, 38, 37, 36, 36, 35, 34, 34, 33, 32,
  32, 31, 30, 30, 29, 28, 28, 27, 26, 26, 25, 24, 24, 23, 22, 21,
  21, 20, 19, 19, 18, 17, 17, 16, 15, 15, 14, 13, 13, 12, 11, 11,
  10,  9,  9,  8,  7,  7,  6,  5,  5,  4,  3,  3,  2,  1,  1,  0
};

/* Extra attenuation (in TL steps) per waveform, indexed by WAVEFORM_x >> 6.
   All VERA waveforms have the same peak level, but the FM approximations do
   not have the same loudness. Tune by ear; positive = quieter. */
static const uint8_t g_wave_bias[4] =
{
  0, 0, 0, 0
};

/* Pulse width (0..63) -> modulator TL.
   VERA's duty cycle is (width + 1) / 128, so 0..50 %. The pulse is made by a
   half-sine modulator (waveform 1) that phase-modulates a square carrier
   (waveform 6); the modulator TL sets how much the falling edge is delayed.
   Measured on the OPL3 core (Nuked): TL 38..63 give a plain 50 % square, TL 20..37
   give a clean 10..38 % pulse in 0.75 dB steps, below that the result is distorted.
   The table maps each width to the TL whose measured duty is closest, so the
   64 widths share about 20 distinct settings. */
static const uint8_t g_pulse_width_tl[64] =
{
  10, 10, 10, 10, 10, 10, 10, 12, 12, 12, 20, 20, 21, 22, 22, 23,
  24, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32,
  32, 33, 33, 33, 34, 34, 34, 34, 35, 35, 35, 35, 35, 36, 36, 36,
  36, 36, 36, 36, 36, 36, 36, 36, 37, 37, 37, 37, 37, 37, 37, 37
};

/* Operator settings per waveform. Level (TL) registers are written separately. */
typedef struct
{
  uint8_t mod_20, mod_60, mod_80, mod_e0;
  uint8_t car_20, car_60, car_80, car_e0;
  uint8_t fb_conn;
} WAVE_PATCH;

static const WAVE_PATCH g_patch[4] =
{
  /* pulse    */ { 0x21, 0xf0, 0x0e, 0x01,   0x21, 0xf0, 0x0e, 0x06,   0x00 },
  /* sawtooth */ { 0x21, 0xfe, 0x0e, 0x00,   0x21, 0xfe, 0x0e, 0x07,   0x09 },
  /* triangle */ { 0x21, 0xf0, 0x0e, 0x04,   0x21, 0xf0, 0x0e, 0x00,   0x07 },
  /* noise    */ { 0xe5, 0xf0, 0x0e, 0x02,   0x21, 0xf0, 0x0e, 0x02,   0x0e }
};

static uint32_t u24(const uint8_t *p)
{
  return ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}

bool load(char *const file_name)
{
  uint32_t data_size;
  uint32_t loop_offset;
  uint16_t page_left;
  uint16_t read_size;
  uint16_t bytes_read;
  uint8_t  segment;
  uint8_t *destination;

  g_has_loop = false;

  g_roboplay_interface->open(file_name, false);

  if(g_roboplay_interface->read(&g_header, sizeof(ZSM_HEADER)) != sizeof(ZSM_HEADER) ||
     g_header.magic_header[0] != 'z' || g_header.magic_header[1] != 'm')
  {
    g_roboplay_interface->close();
    return false;
  }

  g_refresh = g_header.tick_rate ? (float)g_header.tick_rate : 60.0f;

  /* The music stream ends where the PCM block starts (or at the end of the file) */
  data_size = u24(g_header.pcm_offset);
  if(data_size == 0)
    data_size = 0x400000UL;
  else if(data_size > sizeof(ZSM_HEADER))
    data_size -= sizeof(ZSM_HEADER);
  else
  {
    g_roboplay_interface->close();
    return false;
  }

  /* Segment 0 starts with room for the header, so a file offset maps directly
     onto (offset / DATA_SEGMENT_SIZE, offset % DATA_SEGMENT_SIZE). */
  g_segment_index = 0;
  g_segment_list[0] = START_SEGMENT_INDEX;
  g_roboplay_interface->set_segment(START_SEGMENT_INDEX);

  page_left = DATA_SEGMENT_SIZE - sizeof(ZSM_HEADER);
  destination = (uint8_t *)(DATA_SEGMENT_BASE + sizeof(ZSM_HEADER));

  while(data_size > 0)
  {
    if(page_left == 0)
    {
      /* Out of list space: keep what we have, the stream just ends early */
      if(g_segment_index == ZSM_MAX_SEGMENTS - 1)
        break;

      segment = g_roboplay_interface->get_new_segment();
      if(segment == 0)
      {
        g_roboplay_interface->close();
        return false;
      }

      g_segment_list[++g_segment_index] = segment;
      g_roboplay_interface->set_segment(segment);

      page_left = DATA_SEGMENT_SIZE;
      destination = (uint8_t *)DATA_SEGMENT_BASE;
    }

    read_size = (data_size < READ_BUFFER_SIZE) ? (uint16_t)data_size : READ_BUFFER_SIZE;
    if(read_size > page_left)
      read_size = page_left;

    /* It's not possible to read directly to non-primary mapper memory segments,
       so use a buffer inbetween. */
    bytes_read = g_roboplay_interface->read((void *)READ_BUFFER, read_size);
    if(bytes_read == 0)
      break;

    memcpy(destination, (void *)READ_BUFFER, bytes_read);

    data_size   -= bytes_read;
    destination += bytes_read;
    page_left   -= bytes_read;
  }

  g_roboplay_interface->close();

  g_last_segment = g_segment_index;
  g_data_end = destination;

  /* Nothing but the header: no stream to play */
  if(g_last_segment == 0 && destination == (uint8_t *)(DATA_SEGMENT_BASE + sizeof(ZSM_HEADER)))
    return false;

  /* Loop point: file offset of the stream position to continue from at the end */
  loop_offset = u24(g_header.loop_point);
  if(ZSM_LOOP_COUNT > 0 && loop_offset >= sizeof(ZSM_HEADER))
  {
    uint32_t loop_segment = loop_offset / DATA_SEGMENT_SIZE;
    uint16_t loop_in_seg  = (uint16_t)(loop_offset % DATA_SEGMENT_SIZE);

    if(loop_segment < g_last_segment ||
       (loop_segment == g_last_segment && (uint8_t *)(DATA_SEGMENT_BASE + loop_in_seg) < g_data_end))
    {
      g_has_loop = true;
      g_loop_segment = (uint8_t)loop_segment;
      g_loop_offset = loop_in_seg;
    }
  }

  return true;
}

bool update(void)
{
  uint8_t data;

  if(g_delay_counter)
    g_delay_counter--;

  while(!g_delay_counter)
  {
    data = get_next_data_byte();

    if(data < 0x40)
      psg_write(data);
    else if(data == 0x40)
      ext_cmd();
    else if(data < ZSM_END_OF_DATA)
      fm_write(data);
    else if(data == ZSM_END_OF_DATA)
    {
      if(g_has_loop && g_loops_done < ZSM_LOOP_COUNT)
      {
        g_loops_done++;
        seek_to(g_loop_segment, g_loop_offset);
        continue;
      }

      flush_channels();
      return false;
    }
    else
      g_delay_counter = (data & 0x7F);
  }

  flush_channels();
  return true;
}

void rewind(int8_t subsong)
{
  uint8_t i;

  /* No subsongs in this format */
  (void)subsong;

  seek_to(0, sizeof(ZSM_HEADER));

  g_delay_counter = 0;
  g_loops_done = 0;
  g_freq_dirty = 0;
  g_key_dirty = 0;

  /* Silence both chips: a song that ended (or was restarted) mid-note must not hang */
  for(i = 0; i < NR_OF_PSG_CHANNELS; i++)
  {
    g_psg_channels[i].freq_low = 0;
    g_psg_channels[i].freq_high = 0;
    g_psg_channels[i].pan_volume = 0;
    g_psg_channels[i].wave_pulse_width = 0;

    g_opl_channels[i].wave = WAVEFORM_NONE;
    g_opl_channels[i].pulse_width = 0;
    g_opl_channels[i].volume = 0;
    g_opl_channels[i].pan = 0;
    g_opl_channels[i].fb_conn = 0;
    g_opl_channels[i].fnum_low = 0;
    g_opl_channels[i].blk_hi = 0;
    g_opl_channels[i].reg_a0 = 0;
    g_opl_channels[i].reg_b0 = 0;
    g_opl_channels[i].mod_tl = 0xFF;
    g_opl_channels[i].car_tl = 0xFF;

    opl_out(i, 0xA0 + g_chan_index[i], 0x00);
    opl_out(i, 0xB0 + g_chan_index[i], 0x00);
  }

  /* OPM key-on register: slot mask 0 = all slots off */
  for(i = 0; i < 8; i++)
    g_roboplay_interface->opm_write(0x08, i);
}

void command(const uint8_t id)
{
  /* No additional commmands supported */
  (void)id;
}

float get_refresh(void)
{
  return g_refresh;
}

uint8_t get_subsongs(void)
{
  return 0;
}

char* get_player_info(void)
{
  return "Commander X16 zsound player V1.2 by RoboSoft Inc.";
}

char* get_title(void)
{
  return "-";
}

char* get_author(void)
{
  return "-";
}

char* get_description(void)
{
  return "-";
}

/* ---- Song data ---- */

void seek_to(uint8_t segment_index, uint16_t offset)
{
  g_segment_index = segment_index;
  g_roboplay_interface->set_segment(g_segment_list[segment_index]);

  g_song_data = (uint8_t *)(DATA_SEGMENT_BASE + offset);
  g_song_limit = (segment_index == g_last_segment)
    ? g_data_end
    : (uint8_t *)(DATA_SEGMENT_BASE + DATA_SEGMENT_SIZE);
}

uint8_t get_next_data_byte(void)
{
  if(g_song_data == g_song_limit)
  {
    /* Ran out of data without an end marker (truncated file): act as if it was there */
    if(g_segment_index >= g_last_segment)
      return ZSM_END_OF_DATA;

    seek_to(g_segment_index + 1, 0);
  }

  return *g_song_data++;
}

void ext_cmd(void)
{
  /* Extension data (PCM / expansion chips) is skipped */
  uint8_t count = get_next_data_byte() & 0x3f;

  while(count--)
    get_next_data_byte();
}

void fm_write(uint8_t data)
{
  uint8_t count = data & 0x3f;
  uint8_t reg;
  uint8_t val;

  while(count--)
  {
    reg = get_next_data_byte();
    val = get_next_data_byte();

    g_roboplay_interface->opm_write(reg, val);
  }
}

/* ---- VERA PSG -> OPL4 ---- */

void opl_out(uint8_t channel, uint8_t reg, uint8_t value)
{
  if(channel < 9)
    g_roboplay_interface->opl4_write_fm_1(reg, value);
  else
    g_roboplay_interface->opl4_write_fm_2(reg, value);
}

void write_c0(uint8_t channel)
{
  OPL_REGISTERS *o = &g_opl_channels[channel];

  opl_out(channel, 0xC0 + g_chan_index[channel], o->pan | o->fb_conn);
}

static uint16_t noise_fv(uint16_t fv)
{
#if ZSM_NOISE_TRACK
  if(fv < NOISE_FV_MIN) return NOISE_FV_MIN;
  if(fv > NOISE_FV_MAX) return NOISE_FV_MAX;
  return fv;
#else
  (void)fv;
  return NOISE_FV_DEFAULT;
#endif
}

void psg_write(uint8_t data)
{
  uint8_t channel = data >> 2;
  uint8_t value = get_next_data_byte();
  uint16_t bit = (uint16_t)(1u << channel);

  ZSM_PSG_REGISTERS *p = &g_psg_channels[channel];
  OPL_REGISTERS *o = &g_opl_channels[channel];

  switch(data & 0x03)
  {
    case FREQUENCY_LOW:
      p->freq_low = value;
      g_freq_dirty |= bit;
      break;

    case FREQUENCY_HIGH:
      p->freq_high = value;
      g_freq_dirty |= bit;
      break;

    case PAN_VOLUME:
    {
      /* VERA: bit 7 = right, bit 6 = left. OPL4 0xC0: bit 5 = CHB (right), bit 4 = CHA (left) */
      uint8_t pan = (value & 0xC0) >> 2;

      p->pan_volume = value;
      o->volume = value & 0x3F;
      g_key_dirty |= bit;

      if(o->wave != WAVEFORM_NONE)
      {
        if(pan != o->pan)
        {
          o->pan = pan;
          write_c0(channel);
        }
        update_level(channel);
      }
      else
        o->pan = pan;

      break;
    }

    case WAVE_PULSE_WIDTH:
    {
      uint8_t wave = value & 0xC0;

      p->wave_pulse_width = value;
      o->pulse_width = value & 0x3F;

      if(wave != o->wave)
      {
        /* Key off while the operators are reprogrammed; update_frequency() keys on again */
        o->reg_b0 &= 0x1F;
        opl_out(channel, 0xB0 + g_chan_index[channel], o->reg_b0);

        select_wave(channel, wave);
        g_freq_dirty |= bit;
      }
      else if(wave == WAVEFORM_PULSE)
        update_level(channel);

      break;
    }
  }
}

void select_wave(uint8_t channel, uint8_t waveform)
{
  const WAVE_PATCH *p = &g_patch[waveform >> 6];
  OPL_REGISTERS *o = &g_opl_channels[channel];
  uint8_t op = g_op_offset[channel];

  o->wave = waveform;
  o->fb_conn = p->fb_conn;

  opl_out(channel, op + 0x20, p->mod_20);
  opl_out(channel, op + 0x60, p->mod_60);
  opl_out(channel, op + 0x80, p->mod_80);
  opl_out(channel, op + 0xE0, p->mod_e0);

  opl_out(channel, op + 0x23, p->car_20);
  opl_out(channel, op + 0x63, p->car_60);
  opl_out(channel, op + 0x83, p->car_80);
  opl_out(channel, op + 0xE3, p->car_e0);

  write_c0(channel);

  /* Force the levels to be written for the new instrument */
  o->mod_tl = 0xFF;
  o->car_tl = 0xFF;
  update_level(channel);
}

/* Writes the TL registers for the current waveform and VERA volume.
     pulse:         modulator TL = pulse width, carrier TL = level
     sawtooth/tri:  both operators are audible (additive), both follow the level
     noise:         carrier TL = level; the modulator depth (the "noisiness")
                    only follows the level at a quarter of the steps, as before */
void update_level(uint8_t channel)
{
  OPL_REGISTERS *o = &g_opl_channels[channel];
  uint8_t op = g_op_offset[channel];
  uint8_t tl;
  uint8_t mod_tl;

  if(o->wave == WAVEFORM_NONE)
    return;

  tl = g_vol_to_tl[o->volume] + g_wave_bias[o->wave >> 6];
  if(tl > 63)
    tl = 63;

  switch(o->wave)
  {
    case WAVEFORM_PULSE:
      mod_tl = g_pulse_width_tl[o->pulse_width];
      break;
    case WAVEFORM_NOISE:
      mod_tl = tl >> 2;
      break;
    default:
      mod_tl = tl;
      break;
  }

  if(mod_tl != o->mod_tl)
  {
    o->mod_tl = mod_tl;
    opl_out(channel, op + 0x40, mod_tl);
  }

  if(tl != o->car_tl)
  {
    o->car_tl = tl;
    opl_out(channel, op + 0x43, tl);
  }
}

/* Returns register 0xB0 (without key-on) in the high byte, 0xA0 in the low byte */
uint16_t vera_to_opl_freq(uint16_t fv)
{
  uint32_t product;
  uint16_t fnum;
  uint8_t  block = 0;
  uint8_t  shift;

  while(block < 7 && fv > g_block_limit[block])
    block++;

  /* fnum = fv * FNUM_MUL / 2^(12 + block), rounded. The first 8 bits of the
     shift are a constant shift, which is cheap on a Z80. */
  product = ((uint32_t)fv * (uint32_t)FNUM_MUL) >> 8;
  shift = 4 + block;
  fnum = (uint16_t)((product + (1UL << (shift - 1))) >> shift);

  if(fnum > 1023)
    fnum = 1023;

  return ((uint16_t)((block << 2) | (fnum >> 8)) << 8) | (fnum & 0xFF);
}

/* Brings OPL registers 0xA0 / 0xB0 of one channel in line with the VERA state.
   'recompute' is false when only volume or pan changed (no pitch conversion). */
void update_frequency(uint8_t channel, bool recompute)
{
  OPL_REGISTERS *o = &g_opl_channels[channel];
  ZSM_PSG_REGISTERS *p = &g_psg_channels[channel];
  uint16_t fv;
  uint16_t a0b0;
  uint8_t  b0;

  /* Not set up yet: select_wave() will mark the channel dirty again */
  if(o->wave == WAVEFORM_NONE)
    return;

  fv = ((uint16_t)p->freq_high << 8) | p->freq_low;

  if(recompute)
  {
    a0b0 = vera_to_opl_freq((o->wave == WAVEFORM_NOISE) ? noise_fv(fv) : fv);
    o->fnum_low = (uint8_t)a0b0;
    o->blk_hi = (uint8_t)(a0b0 >> 8);
  }

  /* VERA is silent without volume, without pan (no output) and without frequency */
  b0 = o->blk_hi;
  if(o->volume && o->pan && fv)
    b0 |= 0x20;

  if(o->fnum_low != o->reg_a0)
  {
    o->reg_a0 = o->fnum_low;
    opl_out(channel, 0xA0 + g_chan_index[channel], o->reg_a0);
  }

  if(b0 != o->reg_b0)
  {
    o->reg_b0 = b0;
    opl_out(channel, 0xB0 + g_chan_index[channel], b0);
  }
}

/* Called once per tick, after all stream commands were processed: a low byte
   followed by a high byte of the same frequency causes one update, not two. */
void flush_channels(void)
{
  uint16_t freq_mask = g_freq_dirty;
  uint16_t any = freq_mask | g_key_dirty;
  uint8_t channel = 0;

  g_freq_dirty = 0;
  g_key_dirty = 0;

  while(any)
  {
    if(any & 1)
      update_frequency(channel, (freq_mask & 1) != 0);

    any >>= 1;
    freq_mask >>= 1;
    channel++;
  }
}