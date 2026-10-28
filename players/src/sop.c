/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * sop.c
 *
 * SOP: Note OPL3 Music Sequencer player
 */

#include <string.h>

#include "player_interface.h"
#include "sop.h"

bool load(const char *file_name)
{
    init_volume_table();

    g_roboplay_interface->open(file_name, false);
    g_roboplay_interface->read(&g_sop_header, sizeof(SOP_HEADER));
    if(strncmp(g_sop_header.signature,"sopepos", 7) != 0)
    {
        g_roboplay_interface->close();
        return false;
    }

    if((g_sop_header.major_version != '\0') || (g_sop_header.minor_version != '\1'))
    {
        g_roboplay_interface->close();
        return false;
    }

    if(!g_sop_header.basic_tempo) g_sop_header.basic_tempo = SOP_DEF_TEMPO;

    g_roboplay_interface->read(g_channel_mode, g_sop_header.n_tracks);

    load_sop_instruments();
    load_sop_track_data();

    g_roboplay_interface->close(); 

    return true; 
}

bool update(void)
{
    bool songEnd = true;

    for(uint8_t i = 0; i < g_sop_header.n_tracks + 1; i++)
    {
        if(g_tracks[i].dur)
        {
            songEnd = false;
            if(!--g_tracks[i].dur) note_off(i);
        }

        if(g_tracks[i].current_event >= g_tracks[i].number_of_events) continue;
        songEnd = false;

        if(!g_tracks[i].counter)
        {
            if(g_tracks[i].current_event == 0 && g_tracks[i].ticks)
            {
                g_tracks[i].ticks++;
            }
        }

        if(++g_tracks[i].counter >= g_tracks[i].ticks)
        {
            g_tracks[i].counter = 0;
            g_roboplay_interface->set_segment(g_tracks[i].current_segment);
            while(g_tracks[i].current_event < g_tracks[i].number_of_events)
            {
                execute_sop_command(i);
                if(g_tracks[i].current_event >= g_tracks[i].number_of_events) break;
                else
                {
                    g_tracks[i].ticks = get_track_data(i);
                    uint16_t data = get_track_data(i);
                    g_tracks[i].ticks |= (data << 8);
                }
                if(g_tracks[i].ticks) break;
            }
        }
    }

    return !songEnd;
}

void rewind(const uint8_t subsong)
{
    /* No subsongs for this format */
    subsong;

    /* Set to OPL3 mode */
    g_roboplay_interface->opl4_set_mode(ROBOPLAY_OPL_MODE_OPL3);

    /* Set default tempo */
    set_tempo(g_sop_header.basic_tempo);

    for(uint8_t i = 0; i < YMB_SIZE; i++)
    {
        ymbuf[i] = 0;
        ymbuf[i + YMB_SIZE] = 0;
    }

    for (uint8_t i = 0; i < 20; i++) 
    {
        g_voice_pitch_bend[i] = 100;
        g_voice_key_on[i] = 0;
        g_voice_note[i] = MID_C;
        g_voice_volume[i] = 0;
        g_ksl[i] = 0;
        g_ksl2[i] = 0;
        g_ksl2v[i] = 0;
        g_op4_mode[i] = 0;
        g_stereo[i] = 0x30;
    }

    g_op_mask = 0;

    for(uint8_t i = 0; i < g_sop_header.n_tracks + 1; i++)
    {
        g_tracks[i].current_segment = g_tracks[i].start_segment;
        g_tracks[i].current_data = g_tracks[i].start_data;

        g_tracks[i].current_event = 0;
        g_tracks[i].dur = 0;
        g_tracks[i].counter = 0;

        g_roboplay_interface->set_segment(g_tracks[i].current_segment);
        g_tracks[i].ticks = get_track_data(i);
        g_tracks[i].ticks |= get_track_data(i) << 8;

        if(g_channel_mode[i] == SOP_CHAN_4OP)
        {
            set_4op_mode(i, 1);
        }
    }

    for(uint8_t i = 0; i < SOP_MAX_TRACK; i++)
    {
        g_volume[i] = 0;
        g_last_volume[i] = 0;
    }
    g_master_volume = SOP_MAX_VOL;

    set_percussion_mode(g_sop_header.percussive);
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    return g_interval;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "Note OPL3 Music Sequencer player V1.1 by RoboSoft Inc.";
}

char* get_title(void)
{
    return g_sop_header.title;
}

char* get_author(void)
{
    return "-";
}

char* get_description(void)
{
    return g_sop_header.comment;
}

void execute_sop_command(uint8_t track)
{
  uint8_t value = 0;

  uint8_t event = get_track_data(track);

  switch(event)
  {
    case SOP_EVNT_NOTE:
      value = get_track_data(track);
      g_tracks[track].dur = get_track_data(track) + (get_track_data(track) << 8);
      note_on(track, value);
      break;
    case SOP_EVNT_VOL:
      value = get_track_data(track);          
      g_last_volume[track] = value;
      value = value * g_master_volume / SOP_MAX_VOL;
      if(value != g_volume[track])
      {
        set_voice_volume(track, value);
        g_volume[track] = value;
      }
      break;
    case SOP_EVNT_PITCH:
      value = get_track_data(track);
      set_voice_pitch(track, value);
      break;
    case SOP_EVNT_INST:
      value = get_track_data(track);
      set_voice_timbre(track, g_instruments[value].inst_data);
      break;
    case SOP_EVNT_PAN:
      value = get_track_data(track);
      set_stereo_pan(track, value);
      break;
    case SOP_EVNT_TEMPO:
      value = get_track_data(track);
      set_tempo(value);
      break;
    case SOP_EVNT_MVOL:

      value = get_track_data(track);
      g_master_volume = value;
      for(uint8_t i = 0; i < g_sop_header.n_tracks; i++)
      {
        value = g_last_volume[i] * g_master_volume / SOP_MAX_VOL;
        if(value != g_volume[i])
        {
          set_voice_volume(i, value);
          g_volume[i] = value;
        }
      }

      g_roboplay_interface->set_segment(g_tracks[track].current_segment);

      break;
  }

  g_tracks[track].current_event++;
}

void init_volume_table(void)
{
    g_volume_table = (uint8_t *)VOLUME_TABLE_BASE;
    for(uint8_t i = 0; i < 64; i++)
    {
        for(uint8_t j = 0; j < 128; j++)
        {
            g_volume_table[i * 128 + j] = (i * j + (MAX_VOLUME +1) /2) >> LOG2_VOLUME;
        }
    }
}

void set_tempo(uint8_t tempo)
{  
    if(!tempo) tempo = g_sop_header.basic_tempo;
    
    uint16_t t = tempo * g_sop_header.tick_beat;
    g_interval = t / 60.0;

    g_roboplay_interface->update_refresh();
}

bool load_sop_instruments(void)
{
    g_inst_vol_segment = START_SEGMENT_INDEX;

    g_instruments = (SOP_INST*)INSTRUMENTS_BASE;
    for(uint8_t i = 0; i < g_sop_header.n_insts; i++)
    {
        g_roboplay_interface->read(&g_instruments[i], 1+8+19);  // inst_type + shortName + longName
        switch(g_instruments[i].inst_type)
        {
            case INSTRUMENT_TYPE_UNUSED:
                break;
            case INSTRUMENT_TYPE_MELODY_4OP:
                g_roboplay_interface->read(&g_instruments[i].inst_data, INSTRUMENT_DATA_SIZE_4OP);
                break;
            default:
                g_roboplay_interface->read(&g_instruments[i].inst_data, INSTRUMENT_DATA_SIZE_2OP);
        }
    }

    return true;
}

bool load_sop_track_data(void)
{
    uint32_t data_size = 0;
    uint16_t read_size = 0;
    uint16_t page_left  = DATA_SEGMENT_SIZE;
    uint16_t bytes_read = 0;

    uint8_t* destination = (uint8_t *)SOP_EVNT_BASE;
    uint8_t  current_segment = g_roboplay_interface->get_new_segment();

    g_roboplay_interface->set_segment(current_segment);
    for(uint8_t i = 0; i < g_sop_header.n_tracks + 1; i++)
    {
        g_tracks[i].start_segment = current_segment;
        g_tracks[i].start_data = destination;

        g_roboplay_interface->read(&g_tracks[i].number_of_events, sizeof(g_tracks[i].number_of_events));
        g_roboplay_interface->read(&data_size, sizeof(data_size));

        while(data_size > 0)
        {
            read_size = (data_size < READ_BUFFER_SIZE) ? data_size : READ_BUFFER_SIZE;
            read_size = (read_size < page_left) ? read_size : page_left;

            /* It's not possible to read directly to non-primary mapper memory segments,
               so use a buffer inbetween. */
            bytes_read = g_roboplay_interface->read((void*)READ_BUFFER, read_size);
            memcpy(destination, (void*)READ_BUFFER, bytes_read);

            data_size -= bytes_read;
            destination += bytes_read;
            page_left -= bytes_read;
            if(page_left == 0)
            {
                current_segment = g_roboplay_interface->get_new_segment();
                g_roboplay_interface->set_segment(current_segment);

                page_left = DATA_SEGMENT_SIZE;
                destination = (uint8_t *)SOP_EVNT_BASE;
            }
        }
    }

    return true;
}

void write_opl_1(uint8_t addr, uint8_t value)
{
    if(addr >= 0xB0) ymbuf[addr - 0xB0] = value;
    g_roboplay_interface->opl4_write_fm_1(addr, value);
}

void write_opl_2(uint8_t addr, uint8_t value)
{
    if(addr >= 0xB0) ymbuf[YMB_SIZE - 0xB0 + addr] = value;
    g_roboplay_interface->opl4_write_fm_2(addr, value);
}

void set_4op_mode(uint8_t channel, uint8_t value)
{
    if(g_slot_x[channel + 20] <= 2)
    {
        g_op4_mode[channel] = value;

        if(value)
        {
            if(channel > 10) 
                g_op_mask |= (0x01 << (channel - 11 + 3));
            else 
                g_op_mask |= (0x01 << channel);
        }
        else
        {
            if(channel > 10) 
                g_op_mask &= (0xFF - (0x01 << (channel - 11 + 3)));
            else 
                g_op_mask &= (0xFF -(0x01 << channel));
        }

        g_roboplay_interface->opl4_write_fm_2(0x04, g_op_mask);
    }
}

void set_frequency(uint8_t voice, uint8_t note, int16_t pitch, uint8_t keyOn)
{
    uint16_t fN, divFactor, fNIndex;

    int16_t temp = (int16_t)((pitch - 100) / 3.125) + ((note - 12) << LOG_NB_STEP_PITCH);

    if (temp < 0)
        temp = 0;
    else {
        if (temp >= ((NB_NOTES << LOG_NB_STEP_PITCH) - 1))
            temp = (NB_NOTES << LOG_NB_STEP_PITCH) - 1;
    }

    fNIndex = (g_mod12[(temp >> 5)] << 5) + (temp & (NB_STEP_PITCH - 1));

    fN = g_fnum_table[fNIndex];

    divFactor = g_div12[(temp >> 5)];

    if (voice <= 10)
        write_opl_1(0xA0 + voice, (fN & 0xFF));
    else
        write_opl_2(0xA0 + voice - 11, (fN & 0xFF));

    fN = (((fN >> 8) & 0x03) | (divFactor << 2) | keyOn) & 0xFF;

    if (voice <= 10)
        write_opl_1(0xB0 + voice, fN);
    else
        write_opl_2(0xB0 + voice - 11, fN);
}

void set_percussion_mode(bool mode)
{
    g_percussion_mode = mode;

    if (g_percussion_mode) 
    {
        /* set the frequency for the last 4 percussion voices */
        g_voice_note[TOM] = SOP_TOM_PITCH;
        g_voice_pitch_bend[TOM] = 100;
        set_frequency(TOM, g_voice_note[TOM], g_voice_pitch_bend[TOM], 0);

        g_voice_note[SD] = SOP_SD_PITCH;
        g_voice_pitch_bend[SD] = 100;
        set_frequency(SD, g_voice_note[SD], g_voice_pitch_bend[SD], 0);
    }

    write_opl_1(0xBD, (g_percussion_mode ? 0x20 : 0));
}

void note_on(uint8_t channel, uint8_t pitch)
{
    if(g_percussion_mode && channel >= BD && channel <= HIHAT)
    {
        if(channel == BD)
        {
            g_voice_note[BD] = pitch;
            set_frequency(BD, g_voice_note[BD], g_voice_pitch_bend[BD], 0);
        }
        else
        {
            if(channel == TOM && (unsigned)g_voice_note[TOM] != pitch)
            {
                g_voice_note[SD] = (g_voice_note[TOM] = pitch) + TOM_TO_SD;
                set_frequency(TOM, g_voice_note[TOM], 100, 0);
                set_frequency(SD, g_voice_note[SD], 100, 0);
            }
        }
        write_opl_1(0xBD, ymbuf[0x0D] | (0x10 >> (channel - BD)));
    }
    else
    {
        g_voice_note[channel] = pitch;
        g_voice_key_on[channel] = 0x20;

        set_frequency(channel, pitch, g_voice_pitch_bend[channel], 0x20);
    }
}

void note_off(uint8_t channel)
{
    g_voice_key_on[channel] = 0;

    if(g_percussion_mode && channel >= BD && channel <= HIHAT)
        write_opl_1(0xBD, ymbuf[0x0D] & (0xFF - (0x10 >> (channel -BD))));
    else
    {
        if(channel < HIHAT)
            write_opl_1(0xB0 + channel, ymbuf[channel] & 0xDF);
        else
            write_opl_2(0xB0 - 11 + channel, ymbuf[channel - 11 + YMB_SIZE] & 0xDF);
    }
}

void set_voice_volume(uint8_t chan, uint8_t vol)
{    
    uint16_t volume;
    int8_t KSL_value;

    if (chan > 2 && g_op4_mode[chan - 3])
        return;

    g_roboplay_interface->set_segment(g_inst_vol_segment);

    if (vol > MAX_VOLUME)
        vol = MAX_VOLUME;

    g_voice_volume[chan] = vol;

    if (g_ksl2v[chan]) {
        volume = 63 - g_volume_table[((63 - ((KSL_value = g_ksl2[chan]) & 0x3F)) << 7) + vol];

        if (chan >= 11)
            write_opl_2(vol_reg[chan - 11] - 3, KSL_value & 0xC0 | volume);
        else
            write_opl_1((g_percussion_mode ? vol_reg[chan + 11] : vol_reg[chan]) - 3, KSL_value & 0xC0 | volume);

        if (g_op4_mode[chan]) {
            chan += 3;
            volume = 63 - g_volume_table[((63 - ((KSL_value = g_ksl[chan]) & 0x3F)) << 7) + vol];

            if (chan >= 11)
                write_opl_2(vol_reg[chan - 11], KSL_value & 0xC0 | volume);
            else
                write_opl_1(vol_reg[chan], KSL_value & 0xC0 | volume);

            if (g_ksl2v[chan]) {
                volume = 63 - g_volume_table[((63 - ((KSL_value = g_ksl2[chan]) & 0x3F)) << 7) + vol];

                if (chan >= 11)
                    write_opl_2(vol_reg[chan - 11] - 3, KSL_value & 0xC0 | volume);
                else
                    write_opl_1(vol_reg[chan] - 3, KSL_value & 0xC0 | volume);
            }
        }
        else {
            volume = 63 - g_volume_table[((63 - ((KSL_value = g_ksl[chan]) & 0x3F)) << 7) + vol];

            if (chan >= 11)
                write_opl_2(vol_reg[chan - 11], KSL_value & 0xC0 | volume);
            else
                write_opl_1((g_percussion_mode ? vol_reg[chan + 11] : vol_reg[chan]), KSL_value & 0xC0 | volume);
        }
    }
    else {
        if (g_op4_mode[chan]) {
            volume = 63 - g_volume_table[((63 - ((KSL_value = g_ksl[chan + 3]) & 0x3F)) << 7) + vol];

            if (chan >= 11)
                write_opl_2(vol_reg[chan + 3 - 11], KSL_value & 0xC0 | volume);
            else
                write_opl_1(vol_reg[chan + 3], KSL_value & 0xC0 | volume);

            if (g_ksl2v[chan + 3]) {
                volume = 63 - g_volume_table[((63 - ((KSL_value = g_ksl[chan]) & 0x3F)) << 7) + vol];

                if (chan >= 11)
                    write_opl_2(vol_reg[chan - 11], KSL_value & 0xC0 | volume);
                else
                    write_opl_1(vol_reg[chan], KSL_value & 0xC0 | volume);
            }
        }
        else {
            volume = 63 - g_volume_table[((63 - ((KSL_value = g_ksl[chan]) & 0x3F)) << 7) + vol];

            if (chan >= 11)
                write_opl_2(vol_reg[chan - 11], KSL_value & 0xC0 | volume);
            else
                write_opl_1((g_percussion_mode ? vol_reg[chan + 11] : vol_reg[chan]), KSL_value & 0xC0 | volume);
        }
    }   

    g_roboplay_interface->set_segment(g_tracks[chan].current_segment);
}

void send_instrument(uint16_t base_addr, uint8_t* value, bool mode)
{
    for(uint8_t i = 0; i < 4; i++)
    {
        if(!mode) 
            write_opl_1(base_addr, *value++);
        else
            write_opl_2(base_addr, *value++);
        base_addr += 0x20;
    }
    base_addr += 0x40;
    if(!mode) 
        write_opl_1(base_addr, (*value) & 0x07);
    else
        write_opl_2(base_addr, (*value) & 0x07);
}

void set_voice_timbre(uint8_t chan, uint8_t* array)
{
    uint16_t i;
    uint16_t Slot_Number, KSL_value;

    if (chan > 2 && g_op4_mode[chan - 3])
        return;

    g_roboplay_interface->set_segment(g_inst_vol_segment);

    if (!g_percussion_mode)
        Slot_Number = g_slot_x[chan];
    else
        Slot_Number = g_slot_x[chan + 20];

    g_ksl2v[chan] = ((KSL_value = (array[5] & 0x0F)) & 0x01);

    if (chan > 10) 
    {
        i = chan + 0xC0 - 11;

        write_opl_2(i, 0);

        send_instrument(0x20 + Slot_Number, array, true);
        send_instrument(0x23 + Slot_Number, &array[6], true);

        if (g_op4_mode[chan]) {
            write_opl_2(i + 3, 0);

            send_instrument(0x28 + Slot_Number, &((unsigned char*)array)[11], true);
            send_instrument(0x2B + Slot_Number, &((unsigned char*)array)[17], true);

            g_ksl[chan + 3] = *(array + 18);
            g_ksl2[chan + 3] = *(array + 12);
            g_ksl2v[chan + 3] = *(array + 16) & 1;

            write_opl_2(i + 3, (*(array + 16) & 0x0F) | g_stereo[chan]);
        }

        g_ksl[chan] = *(array + 7);
        g_ksl2[chan] = *(array + 1);
        g_ksl2v[chan] = *(array + 5) & 1;

        set_voice_volume(chan, g_voice_volume[chan]);
        write_opl_2(i, KSL_value | g_stereo[chan]);
    }
    else {
        if (chan > 8)
            i = 0xC0 + 17 - chan;
        else
            i = chan + 0xC0;

        write_opl_1(i, 0);

        send_instrument(0x20 + Slot_Number, array, false);

        if (g_percussion_mode && chan > BD)
        {
            g_ksl[chan] = *(array + 1);
            g_ksl2v[chan] = 0;
        }
        else
        {
            send_instrument(0x23 + Slot_Number, &array[6], false);

            g_ksl[chan] = *(array + 7);
            g_ksl2[chan] = *(array + 1);
            g_ksl2v[chan] = *(array + 5) & 1;
        }

        if (g_op4_mode[chan]) 
        {
            write_opl_1(i + 3, 0);

            send_instrument(0x28 + Slot_Number, &((unsigned char*)array)[11], false);
            send_instrument(0x2B + Slot_Number, &((unsigned char*)array)[17], false);

            g_ksl[chan + 3] = *(array + 18);
            g_ksl2[chan + 3] = *(array + 12);
            g_ksl2v[chan + 3] = *(array + 16) & 1;

            write_opl_1(i + 3, *(array + 16) & 0x0F | g_stereo[chan]);
        }

        set_voice_volume(chan, g_voice_volume[chan]);
        write_opl_1(i, KSL_value | g_stereo[chan]);
    }

    g_roboplay_interface->set_segment(g_tracks[chan].current_segment);
}

void set_stereo_pan(uint8_t chan, uint8_t value)
{
    uint8_t PAN[] = { 0xA0, 0x30, 0x50 };
    bool output3 = false;
    uint8_t addr = 0;
    uint8_t data;

    g_stereo[chan] = (uint8_t)(value = PAN[value]);

    if(chan < 9) addr = chan;
    else
    {
        if(chan < 11) addr = 17 - chan;
        else
        {
            addr = chan - 11;
            output3 = true;
        }
    }

    value |= ((chan >= 11) ? ymbuf[YMB_SIZE + 0x10 + addr] & 0x0F : ymbuf[addr + 0x10] & 0x0F);

    if(g_op4_mode[chan])
    {
        data = (value & 0xF0) | ((chan >= 11) ? ymbuf[YMB_SIZE + 0x13 + addr] : ymbuf[addr + 0x13]) & 0x0F;
        (output3) ? write_opl_2(addr + 0xC3, data) : write_opl_1(addr + 0xC3, data);
    }
    (output3) ? write_opl_2(addr + 0xC0, value) : write_opl_1(addr + 0xC0, value);
}

void set_voice_pitch(uint8_t chan, uint8_t pitch)
{
    g_voice_pitch_bend[chan] = pitch;

    if(!g_percussion_mode)
    {
        set_frequency(chan, g_voice_note[chan], pitch, g_voice_key_on[chan]);
    }
    else
    {
        if(chan <= BD || chan > HIHAT)
        {
            set_frequency(chan, g_voice_note[chan], pitch, g_voice_key_on[chan]);
        }
    }
}

uint8_t get_track_data(uint8_t track)
{   
    uint8_t data = *(g_tracks[track].current_data++);

    if(g_tracks[track].current_data == (uint8_t *)(SOP_EVNT_BASE + DATA_SEGMENT_SIZE))
    {
        g_tracks[track].current_segment++;
        g_roboplay_interface->set_segment(g_tracks[track].current_segment);
        g_tracks[track].current_data = (uint8_t*)SOP_EVNT_BASE;
    }
    return data;
}
