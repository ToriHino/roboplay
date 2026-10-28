/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mid.c
 *
 * MID: Standard MIDI file player on OPL4
 */

#include <string.h>

#include "data/inc/yrw801.h"
#include "drivers/inc/opl4_defines.h"

#include "player_interface.h"
#include "mid.h"

/* Macros for efficient big-endian word reading on Z80 */
#define BE_WORD(ptr) (((uint16_t)((uint8_t*)(ptr))[0] << 8) | ((uint8_t*)(ptr))[1])
#define BE_DWORD(ptr) (((uint32_t)((uint8_t*)(ptr))[0] << 24) | ((uint32_t)((uint8_t*)(ptr))[1] << 16) | ((uint32_t)((uint8_t*)(ptr))[2] << 8) | ((uint8_t*)(ptr))[3])

/* End of the data window of a mapper segment */
#define SEGMENT_END ((uint8_t *)(DATA_SEGMENT_BASE + DATA_SEGMENT_SIZE))

bool load(const char* file_name)
{
    g_roboplay_interface->open(file_name, false);

    /* Load the MIDI file header */
    uint8_t *file_data = (uint8_t *)READ_BUFFER;
    g_roboplay_interface->read(file_data, MIDI_HEADER_SIZE);
    if(file_data[0] != 'M' || file_data[1] != 'T' || file_data[2] != 'h' || file_data[3] != 'd' ||
       file_data[4] != 0 || file_data[5] != 0 || file_data[6] != 0 || file_data[7] != 6 )
    {
        return false;
    }
    if(file_data[8] != 0) return false;
    g_header.file_format = file_data[9];
    if(g_header.file_format > 2) return false;

    g_header.number_of_tracks = BE_WORD(&file_data[10]);
    g_header.ticks_per_qnote = BE_WORD(&file_data[12]);

    /* Reject SMPTE time division (bit 15 set) and files with more tracks than we can hold */
    if(g_header.ticks_per_qnote < 1 || (g_header.ticks_per_qnote & 0x8000)) return false;
    if(g_header.number_of_tracks > MAX_NR_OF_TRACKS) return false;

    /* Load MIDI track data */
    load_track_data();

    g_roboplay_interface->close();

    /* Load YRW801 wave table */
    load_wave_data();

    return true;
}

bool update(void)
{
    bool song_finished = true;
    TRACK_DATA *t = g_track_data;

    {
        uint16_t sum = (uint16_t)g_ticks_fraction + g_ticks_fraction_step;
        g_ticks_fraction = (uint8_t)sum;
        g_MIDI_counter += g_ticks_per_update + (sum >> 8);
    }
    for(g_track = 0; g_track < g_header.number_of_tracks; g_track++, t++)
    {
        if(t->track_finished) continue;

        /* Only touch the mapper when this track actually has an event due */
        if(g_MIDI_counter >= t->waiting_for)
        {
            g_cur_track = t;
            g_ptr = t->track_data;
            g_seg = t->segment;
            g_roboplay_interface->set_segment(g_seg);

            do
            {
                handle_track_event();
                if(t->track_finished) break;  /* don't read a delta time past the end of the track */
                get_delta_time();
            } while(g_MIDI_counter >= t->waiting_for);

            t->track_data = g_ptr;
            t->segment = g_seg;
        }

        if(!t->track_finished) song_finished = false;
    }

    return !song_finished;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    /* Reset tempo to 120BPM */
    g_qnote_duration = 500000;

    g_MIDI_counter = 0;
    g_voice_age = 0;
    g_ticks_fraction = 0;
    g_volume_boost = 16;

    for(g_track = 0; g_track < g_header.number_of_tracks; g_track++)
    {
        TRACK_DATA *t = &g_track_data[g_track];

        t->segment = t->start_segment;
        t->track_data = t->start_track_data;

        t->track_finished = false;
        t->waiting_for = 0;
        t->last_command = 0xFF;

        g_cur_track = t;
        g_ptr = t->track_data;
        g_seg = t->segment;
        g_roboplay_interface->set_segment(g_seg);
        get_delta_time();

        t->track_data = g_ptr;
        t->segment = g_seg;
    }

    for(uint8_t channel = 0; channel < NR_OF_MIDI_CHANNELS; channel++)
    {
        g_midi_channel_data[channel].instrument = 0;
        g_midi_channel_data[channel].panpot     = 0;
        g_midi_channel_data[channel].vibrato    = 0;
        g_midi_channel_data[channel].drum_channel = (channel == DRUM_CHANNEL);
    }

    for(uint8_t voice = 0; voice < NR_OF_WAVE_CHANNELS; voice++)
    {
        g_voice_data[voice].number = voice;
        g_voice_data[voice].is_active = false;
        g_voice_data[voice].activated = 0;
        g_voice_data[voice].midi_channel = NULL;
        g_voice_data[voice].note = 0;
        g_voice_data[voice].velocity = 0;
        g_voice_data[voice].wave_data = NULL;
        g_voice_data[voice].level_direct = 0;
        g_voice_data[voice].reg_f_number = 0;
        g_voice_data[voice].reg_octave = 0;
        g_voice_data[voice].reg_misc = 0;
        g_voice_data[voice].reg_lfo_vibrato = 0;
    }
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    /* Use a fixed replay rate of 60 Hz */
    static const float rate = 60.0;
    
    /* Ticks per update = ticks_per_qnote * 1000000 / 60 / qnote_duration, kept in 24.8 fixed
       point so tempos that don't divide evenly don't drift. Computed as
       ((1000000 / 60 * 256 * 512) / qnote_duration * ticks_per_qnote) / 512
       so nothing overflows 32 bits: clamping at 32768 us per quarter note (~1800 BPM) and
       SMPTE division being rejected in load() guarantee that. */
    uint32_t qnote = g_qnote_duration;
    uint32_t steps;

    if(qnote < 32768UL) qnote = 32768UL;

    steps = (4266667UL * 512UL + qnote / 2) / qnote;
    steps = (steps * g_header.ticks_per_qnote + 256UL) >> 9;

    g_ticks_per_update = steps >> 8;
    g_ticks_fraction_step = (uint8_t)steps;

    return rate;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "Standard MIDI file player (Wave) V1.2 by RoboSoft Inc.";
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
    switch(g_header.file_format)
    {
        case 0x00:
            return "SMF format 0";
            break;

        case 0x01:
            return "SMF format 1";
            break;

        case 0x02:
            return "SMF format 2";
            break;

        default:
            return "-";            
    }

}

void load_track_data(void)
{
    uint8_t  *destination     = (uint8_t *)DATA_SEGMENT_BASE;
    uint16_t  page_left       = DATA_SEGMENT_SIZE;
    uint8_t   current_segment = START_SEGMENT_INDEX;

    for(uint8_t i = 0; i < g_header.number_of_tracks; i++)
    {
        g_track_data[i].start_segment = current_segment;
        g_track_data[i].start_track_data = destination;

        uint8_t track_buffer[4];
        g_roboplay_interface->read(track_buffer, 4);  /* MTrk */
        g_roboplay_interface->read(track_buffer, 4);  /* Header Length */

        g_track_data[i].length = BE_DWORD(track_buffer);

        uint32_t data_size = g_track_data[i].length;
        while(data_size > 0)
        {
            uint16_t read_size = (data_size < READ_BUFFER_SIZE) ? data_size : READ_BUFFER_SIZE;
            read_size = (read_size < page_left) ? read_size : page_left;

            /* It's not possible to read directly to non-primary mapper memory segments,
               so use a buffer inbetween. */
            uint16_t bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, read_size);
            memcpy(destination, (void*)READ_BUFFER, bytes_read);

            data_size -= bytes_read;
            destination += bytes_read;
            page_left -= bytes_read;
            if(page_left == 0)
            {
                current_segment = g_roboplay_interface->get_new_segment();
                g_roboplay_interface->set_segment(current_segment);

                page_left = DATA_SEGMENT_SIZE;
                destination = (uint8_t *)DATA_SEGMENT_BASE;
            }
        }
    }
}

void load_wave_data(void)
{
    g_wave_segment = g_roboplay_interface->get_new_segment();
    g_roboplay_interface->set_segment(g_wave_segment);

    g_roboplay_interface->open("YRW801.DAT", true);

    uint8_t *destination = (uint8_t *)DATA_SEGMENT_BASE;
    uint16_t bytes_read = 0;
    do
    {
        /* It's not possible to read directly to non-primary mapper memory segments,
           so use a buffer inbetween. */
        bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, READ_BUFFER_SIZE);
        if(bytes_read)
        {
            memcpy(destination, (void*)READ_BUFFER, bytes_read);
            destination += bytes_read;
        }
    } while(bytes_read);

    g_roboplay_interface->close();
}

void next_segment(void)
{
    g_seg++;
    g_roboplay_interface->set_segment(g_seg);
    g_ptr = (uint8_t *)DATA_SEGMENT_BASE;
}

uint8_t read_byte(void)
{
    uint8_t data = *g_ptr++;

    if(g_ptr == SEGMENT_END) next_segment();

    return data;
}

void skip_bytes(uint32_t count)
{
    while(count)
    {
        uint16_t room = (uint16_t)(SEGMENT_END - g_ptr);
        uint16_t step = (count < room) ? (uint16_t)count : room;

        g_ptr += step;
        count -= step;
        if(g_ptr == SEGMENT_END) next_segment();
    }
}

uint32_t get_variable_len(void)
{
    uint8_t  data = read_byte();
    uint16_t value;

    /* 1 byte: by far the most common case */
    if(!(data & 0x80)) return data;

    /* 2 bytes (up to 16383) still fit in 16 bits */
    value = data & 0x7F;
    data = read_byte();
    value = (value << 7) | (data & 0x7F);
    if(!(data & 0x80)) return value;

    /* 3 or 4 bytes: rare */
    {
        uint32_t big = value;

        do
        {
            data = read_byte();
            big = (big << 7) | (data & 0x7F);
        } while(data & 0x80);

        return big;
    }
}

void get_delta_time(void)
{
    g_cur_track->waiting_for += get_variable_len();
}

void change_speed(uint32_t speed)
{
    g_qnote_duration = speed;
    g_roboplay_interface->update_refresh();
}

void handle_track_event(void)
{
    TRACK_DATA *t = g_cur_track;
    uint8_t data_byte_1, data_byte_2, data_byte_3;

    data_byte_1 =  read_byte();

    /* If MSB is not set, this is a running status */
    if((data_byte_1 & 128) != 0)
    {
        t->last_command = data_byte_1;
        data_byte_2 = read_byte();
    }
    else
    {
        data_byte_2 = data_byte_1;
        data_byte_1 = t->last_command;
    }

    if(data_byte_1 == 0xFF)
        handle_meta_event(data_byte_2);
    else if(data_byte_1 == 0xF0 || data_byte_1 == 0xF7)
        handle_sys_ex_event(data_byte_2);
    else
    {
        uint8_t midi_channel = data_byte_1 & 0x0F;
        uint8_t midi_command = data_byte_1 >> 4;

        switch(midi_command)
        {
            case 0x8:  /* Note OFF */
                data_byte_3 = read_byte();
                note_off(data_byte_2 & 0x7F, data_byte_3, &g_midi_channel_data[midi_channel]);
                break;
            case 0x9:  /* Note ON (velocity 0 means Note OFF) */
                data_byte_3 = read_byte();
                if(data_byte_3)
                    note_on(data_byte_2 & 0x7F, data_byte_3, &g_midi_channel_data[midi_channel]);
                else
                    note_off(data_byte_2 & 0x7F, 0, &g_midi_channel_data[midi_channel]);
                break;
            case 0xA:  /* Key after-touch */
                data_byte_3 = read_byte();
                break;
            case 0xB:  /* Control change */
                data_byte_3 = read_byte();
                control_change(midi_channel, data_byte_2, data_byte_3);
                break;
            case 0xC:  /* Program change */
                program_change(midi_channel, data_byte_2 & 0x7F);
                break;
            case 0xD:  /* Channel after-touch: single data byte, already read */
                break;
            case 0xE:  /* Pitch wheel */
                data_byte_3 = read_byte();
                {
                    uint16_t pitch_value = ((uint16_t)data_byte_3 << 7) | data_byte_2;
                    pitch_wheel(midi_channel, pitch_value);
                }
                break;
        }
    }
}

void handle_meta_event(uint8_t sub_type)
{
    uint32_t length = get_variable_len();
    switch(sub_type)
    {
        case 0x2f:
            g_cur_track->track_finished = true;
            break;
        case 0x51:
            g_qnote_duration = read_byte();
            g_qnote_duration = (g_qnote_duration << 8) + read_byte();
            g_qnote_duration = (g_qnote_duration << 8) + read_byte();
            g_roboplay_interface->update_refresh();
            break;
        default:
            skip_bytes(length);
    }
}

void handle_sys_ex_event(uint8_t first_length_byte)
{
    /* The byte already read is the first byte of the variable-length size */
    uint32_t length = first_length_byte & 0x7F;
    uint8_t  data = first_length_byte;

    while(data & 0x80)
    {
        data = read_byte();
        length = (length << 7) | (data & 0x7F);
    }

    skip_bytes(length);
}

VOICE_DATA* get_voice(YRW801_WAVE_DATA *wave_data)
{
    VOICE_DATA *free_voice = NULL;
    VOICE_DATA *oldest_voice = g_voice_data;
    VOICE_DATA *voice = g_voice_data;
    uint16_t    oldest_time = g_voice_data[0].activated;
    uint8_t     count;

    /* Single pass: find best free voice and oldest active voice.
       Ages are 16-bit and compared with a signed difference, so wrap-around is harmless. */
    for(count = NR_OF_WAVE_CHANNELS; count; count--, voice++)
    {
        if(voice->is_active)
        {
            /* Track oldest active voice for voice stealing */
            if((int16_t)(voice->activated - oldest_time) < 0)
            {
                oldest_voice = voice;
                oldest_time = voice->activated;
            }
        }
        else
        {
            /* Prefer a free voice with matching wave data */
            if(voice->wave_data == wave_data)
            {
                free_voice = voice;
                break;  /* Found exact match, use it immediately */
            }

            /* Track best free voice (least recently used) */
            if(free_voice == NULL || (int16_t)(voice->activated - free_voice->activated) < 0)
                free_voice = voice;
        }
    }

    /* If no suitable free voice, use oldest active voice */
    if(free_voice == NULL)
    {
        free_voice = oldest_voice;
        free_voice->activated = 0;
        free_voice->reg_misc &= ~OPL4_KEY_ON_BIT;
        g_roboplay_interface->opl4_write_wave(OPL4_REG_MISC + free_voice->number, free_voice->reg_misc);
    }

    return free_voice;
}

void note_off(uint8_t note, uint8_t velocity, MIDI_CHANNEL_DATA *midi_channel)
{
    VOICE_DATA *voice = g_voice_data;
    uint8_t     count;

    /* Velocity not used */
    velocity;

    for(count = NR_OF_WAVE_CHANNELS; count; count--, voice++)
    {
        /* Only voices that are still sounding need a key-off write */
        if(voice->is_active && voice->note == note && voice->midi_channel == midi_channel)
        {
            voice->is_active = false;

            voice->reg_misc &= ~OPL4_KEY_ON_BIT;
            g_roboplay_interface->opl4_write_wave(OPL4_REG_MISC + voice->number, voice->reg_misc);
        }
    }
}

void note_on(uint8_t note, uint8_t velocity, MIDI_CHANNEL_DATA *midi_channel)
{
  g_roboplay_interface->set_segment(g_wave_segment);

  YRW801_REGION_DATA **regions = (YRW801_REGION_DATA**)DATA_SEGMENT_BASE;
  YRW801_WAVE_DATA *wave_data[2];
  VOICE_DATA* voice[2];
  uint8_t i = 0, voices = 0;
#if MID_REUSE_LOADED_WAVE
  bool wave_loaded[2];
  bool load_pending = false;
#endif

  /* Determine voices and wave data */
  if(midi_channel->drum_channel)
  {
    YRW801_REGION_DATA *drum = regions[0x80];

    if(note >= DRUM_GAPLESS_FIRST)
    {
      /* Fast path: direct index. Notes above the bank have no sample and are ignored. */
      if(note <= DRUM_LAST_NOTE)
        wave_data[voices++] = &drum[note - DRUM_INDEX_OFFSET].wave_data;
    }
    else
    {
      /* Sparse part of the bank (keys 0x18..0x22): search by key. Notes without a sample are ignored. */
      while(drum->key_min != 0xFF)
      {
        if(note >= drum->key_min && note <= drum->key_max)
        {
          wave_data[voices++] = &drum->wave_data;
          break;
        }
        drum++;
      }
    }
  }
  else
  {
    YRW801_REGION_DATA *region = regions[midi_channel->instrument & 0x7F];
    while(region[i].key_min != 0xFF)
    {
      if(note >= region[i].key_min && note <= region[i].key_max)
      {
        wave_data[voices] = &region[i].wave_data;
        if(++voices >= 2) break;
      }
      i++;
    }
  }

  if(voices == 0)
  {
    g_roboplay_interface->set_segment(g_seg);
    return;
  }

  /* First pass: allocate and initialize voices, then set tone numbers */
  for(i = 0; i < voices; i++)
  {
    VOICE_DATA *current_voice = get_voice(wave_data[i]);
#if MID_REUSE_LOADED_WAVE
    wave_loaded[i] = (current_voice->wave_data == wave_data[i]);
#endif
    current_voice->is_active = true;
    if(++g_voice_age == 0) g_voice_age = 1;   /* 0 is reserved */
    current_voice->activated = g_voice_age;
    current_voice->midi_channel = midi_channel;
    current_voice->note = note;
    current_voice->velocity = velocity & 0x7F;
    current_voice->wave_data = wave_data[i];
    voice[i] = current_voice;

    current_voice->reg_f_number = (wave_data[i]->tone >> 8) & OPL4_TONE_NUMBER_BIT8;
#if MID_MERGE_FNUMBER
    /* Pitch is known up front, so the F-number register only needs to be written once */
    calc_pitch(current_voice);
#endif
    g_roboplay_interface->opl4_write_wave(OPL4_REG_F_NUMBER + current_voice->number, current_voice->reg_f_number);

    /* Set tone number immediately (triggers header loading) */
#if MID_REUSE_LOADED_WAVE
    if(!wave_loaded[i])
    {
      g_roboplay_interface->opl4_write_wave(OPL4_REG_TONE_NUMBER + current_voice->number, wave_data[i]->tone & 0xFF);
      load_pending = true;
    }
#else
    g_roboplay_interface->opl4_write_wave(OPL4_REG_TONE_NUMBER + current_voice->number, wave_data[i]->tone & 0xFF);
#endif
  }

  /* Second pass: set parameters which can be set while loading */
  for(i = 0; i < voices; i++)
  {
    voice[i]->reg_misc = OPL4_LFO_RESET_BIT;
    update_pan(voice[i]);
    update_pitch(voice[i]);
    voice[i]->level_direct = OPL4_LEVEL_DIRECT_BIT;
    update_volume(voice[i]);
  }

  /* Wait for completion of loading */
#if MID_REUSE_LOADED_WAVE
  if(load_pending)
#endif
    g_roboplay_interface->opl4_wait_for_load();

  /* Third pass: set remaining parameters */
  for(i = 0; i < voices; i++)
  {
#if MID_REUSE_LOADED_WAVE
    if(!wave_loaded[i])
#endif
    {
      /* A header load overwrites these registers, so they are only valid to skip when no load happened */
      g_roboplay_interface->opl4_write_wave(OPL4_REG_ATTACK_DECAY1 + voice[i]->number, voice[i]->wave_data->reg_attack_decay1);
      g_roboplay_interface->opl4_write_wave(OPL4_REG_LEVEL_DECAY2 + voice[i]->number, voice[i]->wave_data->reg_level_decay2);
      g_roboplay_interface->opl4_write_wave(OPL4_REG_RELEASE_CORRECTION + voice[i]->number, voice[i]->wave_data->reg_release_correction);
      g_roboplay_interface->opl4_write_wave(OPL4_REG_TREMOLO + voice[i]->number, voice[i]->wave_data->reg_tremolo);
      voice[i]->reg_lfo_vibrato = voice[i]->wave_data->reg_lfo_vibrato;
    }

    if(!midi_channel->drum_channel)
      update_vibrato_depth(voice[i]);
  }

  /* Fourth pass: activate all voices (key on) */
  for(i = 0; i < voices; i++)
  {
    voice[i]->reg_misc = (voice[i]->reg_misc & 0x1F) | OPL4_KEY_ON_BIT;
    g_roboplay_interface->opl4_write_wave(OPL4_REG_MISC + voice[i]->number, voice[i]->reg_misc);
  }

  g_roboplay_interface->set_segment(g_seg);
}

void pitch_wheel(uint8_t midi_channel, uint16_t value)
{
    midi_channel;
    value;
}
void control_change(uint8_t midi_channel, uint8_t id, uint8_t value)
{
    switch(id)
    {
        case 10:
            /* Change stereo panning */
            if(midi_channel != DRUM_CHANNEL)
                g_midi_channel_data[midi_channel].panpot = (value - 0x40) >> 3;
            break;
    }
}

void channel_pressure(uint8_t midi_channel, uint8_t pressure)
{
    midi_channel;
    pressure;
}

void key_pressure(uint8_t midi_channel, uint8_t note, uint8_t pressure)
{
    midi_channel;
    note;
    pressure;
}

void program_change(uint8_t midi_channel, uint8_t program)
{
    g_midi_channel_data[midi_channel].instrument = program;
}

void update_pan(VOICE_DATA *voice)
{
    int8_t pan = voice->wave_data->panpot;

    if(!voice->midi_channel->drum_channel)
        pan += voice->midi_channel->panpot;
    if(pan < -7)
        pan = -7;
    else if(pan > 7)
        pan = 7;

    voice->reg_misc = (voice->reg_misc & ~OPL4_PAN_POT_MASK) | (pan & OPL4_PAN_POT_MASK);
    g_roboplay_interface->opl4_write_wave(OPL4_REG_MISC + voice->number, voice->reg_misc);
}

void calc_pitch(VOICE_DATA *voice)
{
    int16_t  pitch;
    int32_t  total;
    uint16_t p;
    uint16_t fnumber;
    uint8_t  q, octave;

    /* 16-bit arithmetic for the common case; the 32-bit multiply/divide is only
       needed when a wave uses key scaling other than 100% */
    if(voice->midi_channel->drum_channel)
        pitch = 0;
    else
    {
        pitch = ((int16_t)voice->note - 60) << 7;
        if(voice->wave_data->key_scaling != 100)
            pitch = (int16_t)(((int32_t)pitch * voice->wave_data->key_scaling) / 100);
    }

    total = (int32_t)pitch + 7680 + voice->wave_data->pitch_offset;
    if(total < 0)
        total = 0;
    else if(total > 0x5FFF)
        total = 0x5FFF;
    p = (uint16_t)total;

    /* q = p / 0x600 without a divide: p >> 9 is 0..47 and (x * 171) >> 9 == x / 3 for that range */
    q = (uint8_t)((((uint8_t)(p >> 9)) * 171u) >> 9);
    octave = q - 8;
    fnumber = g_wave_pitch_map[p - (uint16_t)((q + (q << 1)) << 9)];   /* p % 0x600 */

    voice->reg_octave = (octave << 4) | ((fnumber >> 7) & 0x07);
    voice->reg_f_number = (voice->reg_f_number & OPL4_TONE_NUMBER_BIT8) | ((fnumber << 1) & OPL4_F_NUMBER_LOW_MASK);
}

void update_pitch(VOICE_DATA *voice)
{
#if !MID_MERGE_FNUMBER
    calc_pitch(voice);
#endif

    g_roboplay_interface->opl4_write_wave(OPL4_REG_OCTAVE + voice->number, voice->reg_octave);
#if !MID_MERGE_FNUMBER
    g_roboplay_interface->opl4_write_wave(OPL4_REG_F_NUMBER + voice->number, voice->reg_f_number);
#endif
}

void update_volume(VOICE_DATA *voice)
{
    int16_t att = voice->wave_data->tone_attenuate;

    att += g_volume_table[voice->velocity];
    /* A volume factor of 0xFE leaves att unchanged, so skip the divide */
    if(voice->wave_data->volume_factor != 0xFE)
    {
        int16_t d = 0x7F - att;

        /* d * volume_factor must fit in a 16-bit int (SDCC): that holds for d >= -128.
           att can reach 510 (tone_attenuate is uint8_t), so use 32 bits for the rare extreme case. */
        if(d >= -128)
            att = 0x7F - d * voice->wave_data->volume_factor / 0xFE;
        else
            att = 0x7F - (int16_t)(((int32_t)d * voice->wave_data->volume_factor) / 0xFE);
    }
    att -= g_volume_boost;
    if(att < 0)
        att = 0;
    else if(att > 0x7E)
        att = 0x7E;

    g_roboplay_interface->opl4_write_wave(OPL4_REG_LEVEL + voice->number, (att << 1) | voice->level_direct);
    voice->level_direct = 0;
}

void update_vibrato_depth(VOICE_DATA *voice)
{
    uint16_t depth;

    depth = (7 - voice->wave_data->vibrato) * (voice->midi_channel->vibrato & 0x7F);
    depth = (depth >> 7) + voice->wave_data->vibrato;
    voice->reg_lfo_vibrato &= ~OPL4_VIBRATO_DEPTH_MASK;
    voice->reg_lfo_vibrato |= depth & OPL4_VIBRATO_DEPTH_MASK;
    g_roboplay_interface->opl4_write_wave(OPL4_REG_LFO_VIBRATO + voice->number, voice->reg_lfo_vibrato);
}