/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mus_org.c
 *
 * MUS_ORG: FAC SoundTracker on original device(s)
 */


#include <string.h>

#include "player_interface.h"
#include "mus_org.h"

bool load(const char *file_name)
{
    g_roboplay_interface->open(file_name, false);

    g_roboplay_interface->read((void *)DATA_SEGMENT_BASE, 7);
    g_roboplay_interface->read((void *)DATA_SEGMENT_BASE, DATA_SEGMENT_SIZE);

    g_roboplay_interface->close();

    g_mus_header = (void *)(DATA_SEGMENT_BASE + MAX_SONG_SIZE);

    if(g_mus_header->version == 0x03)
    {
        g_pattern_data = (uint8_t *)DATA_SEGMENT_BASE;

        uint8_t value = 0x00;
        while((uint16_t)g_pattern_data < (DATA_SEGMENT_BASE + MAX_SONG_SIZE))
        {
            *g_pattern_data ^= value;
            value++;
            g_pattern_data++;
        }
    }

    uint8_t length = (g_mus_header->version > 0x02) ? (SONG_NAME_LENGTH - 5) : SONG_NAME_LENGTH;
    strncpy(g_song_name, g_mus_header->song_name, length);
    g_song_name[length] = '\0';

    length = (g_mus_header->version > 0x02) ? AUTHOR_NAME_LENGTH - 5 : SONG_NAME_LENGTH;
    strncpy(g_author, g_mus_header->author, length);
    g_author[length] = '\0';

    uint16_t song_length = (g_mus_header->number_of_tracks + 1) * STEP_DATA_SIZE * PATTERN_SIZE;
    if(song_length > MAX_SONG_SIZE) song_length = MAX_SONG_SIZE;
    
    g_song_end = (uint8_t *)(DATA_SEGMENT_BASE + song_length);

    /* Enable sample RAM write */
    g_roboplay_interface->msx_audio_write(0x07, 0x60);

    /* Set sample RAM address */
    g_roboplay_interface->msx_audio_write(0x09, 0x00);
    g_roboplay_interface->msx_audio_write(0x0a, 0x00);
    g_roboplay_interface->msx_audio_write(0x0b, 0xff);
    g_roboplay_interface->msx_audio_write(0x0c, 0x1f);

    g_play_samples = false;
    g_play_samples = load_sample_kit(file_name, ".SM1");
    g_play_samples = g_play_samples && load_sample_kit(file_name, ".SM2");

    return true;
}

bool update(void)
{
    g_speed_count++;
    if(g_speed_count == g_speed)
    {
        g_speed_count = 0;

        /* Play next step line */
        for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
        {
            uint8_t data = *g_pattern_data++;

            if(data && (g_mus_header->device_id == DEVICE_AUDIO || (g_mus_header->device_id == DEVICE_MUSIC && i < NR_OF_CHANNELS - NR_OF_DRUM_CHANNELS)))
            {
                if(data < NOTE_OFF)
                {
                    note_on(i, data);
                }
                else
                {
                    if(g_mus_header->version <= 1)
                    {
                        /* SoundTracker V1.0 */
                        if(data & 0x40) note_off(i);
                    }
                    else
                    {
                        /* SoundTracker V2.0 or higher */
                        if(data == NOTE_OFF) note_off(i);
                        else if(data == DETUNE_UP) detune_up(i);
                        else if(data == DETUNE_DOWN) detune_down(i);
                        else if(data < PITCH_BEND_DOWN) pitch_bend_up(i, data);
                        else if(data < TEMPO_CHANGE) pitch_bend_down(i, data);
                        else if(data < VOLUME_CHANGE) change_tempo(data);
                        else if(data  < BRIGHTNESS_CHANGE) change_volume(i, data);
                        else change_brightness(i, data);
                    }
                }
            }
        }

        if(g_mus_header->device_id == DEVICE_MUSIC)
        {
            /* Skip frequency */
            g_pattern_data ++;

            uint8_t volume = *g_pattern_data++;
            uint8_t drum_data = *g_pattern_data++;
            if(g_mus_header->version >= 0x02) drum_data >>= 4;

            if(drum_data)
            {
              g_percussion_data[9].data = g_drum_presets[drum_data - 1] | 0x20;
              if(g_percussion_data[9].data & 0x08) g_percussion_data[7].data = 0x05;

              volume >>= 4;
              g_percussion_data[3].data = ((g_percussion_data[3].data & 0xf0) | volume) ^ 0x0f;
              g_percussion_data[4].data = ((g_percussion_data[4].data & 0xf0) | volume) ^ 0x0f;
            }
        }
        else
        {
            if(g_play_samples && *g_pattern_data)
            {
                uint8_t frequency = *g_pattern_data++;
                uint8_t volume = *g_pattern_data++;
                uint8_t sample_number = *g_pattern_data++;

                if(g_mus_header->version >= 0x02) sample_number &= 0x0F;

                play_sample(frequency, volume, sample_number);
            }
            else
            {
                /* Skip empty sample data */
                g_pattern_data += 3;
            }
        }
    }
    else
    {
        handle_pitch_bend();
    }

    /* Handle FM percussion */
    for(uint8_t i = 0; i < 10; i++)
    {
      g_roboplay_interface->msx_music_write(g_percussion_data[i].reg, g_percussion_data[i].data);
    }

    if(g_percussion_data[7].data != 0x01) g_percussion_data[7].data -= 0x02;
    g_percussion_data[9].data = 0x20;

    if(g_pattern_data >= g_song_end) return false;

    return true;
}

void rewind(int8_t subsong)
{
    /* No subsongs in this format */
    subsong;

    for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
    {
        if(g_mus_header->device_id == DEVICE_AUDIO)
        {
            for(uint8_t j = 0; j < INSTRUMENT_DATA_SIZE - 1; j++)
            {
                g_roboplay_interface->msx_audio_write(g_instrument_registers[i][j], g_mus_header->voice_data_audio[i][j]);
            }
            g_roboplay_interface->msx_audio_write(g_instrument_registers[i][INSTRUMENT_DATA_SIZE - 1], g_mus_header->voice_data_audio[i][INSTRUMENT_DATA_SIZE - 1] | PAN_SETTING_LEFT);
        }
        else if(i < FIRST_DRUM_CHANNEL)           
        {
            g_roboplay_interface->msx_music_write(0x30 + i, g_mus_header->instrument_volume_music[i]);
        }

        g_mus_status_table[i].note_active = false;
        g_mus_status_table[i].note_frequency.low  = 0;
        g_mus_status_table[i].note_frequency.high = 0;

        g_mus_status_table[i].pitch_bend_value_up   = 0;
        g_mus_status_table[i].pitch_bend_value_down = 0;
        g_mus_status_table[i].detune_value          = 0;
    }

    if(g_mus_header->device_id == DEVICE_MUSIC)
    {
        /* Set MSX-MUSIC to percussion mode */
        g_roboplay_interface->msx_music_write(0x0E, 0x20);

        /* Set initial percussion data*/
        g_percussion_data[0].reg  = 0x16;
        g_percussion_data[0].data = 0x20;
        g_percussion_data[1].reg  = 0x17;
        g_percussion_data[1].data = 0x57;
        g_percussion_data[2].reg  = 0x18;
        g_percussion_data[2].data = 0xaa;
        g_percussion_data[3].reg  = 0x36;
        g_percussion_data[3].data = 0xf0;
        g_percussion_data[4].reg  = 0x37;
        g_percussion_data[4].data = 0x70;
        g_percussion_data[5].reg  = 0x38;
        g_percussion_data[5].data = 0x0f;
        g_percussion_data[6].reg  = 0x26;
        g_percussion_data[6].data = 0x05;
        g_percussion_data[7].reg  = 0x27;
        g_percussion_data[7].data = 0x01;
        g_percussion_data[8].reg  = 0x28;
        g_percussion_data[8].data = 0x00;
        g_percussion_data[9].reg  = 0x0e;
        g_percussion_data[9].data = 0x20;

        /* Set user tone settings */
        for(uint8_t i = 0; i < 8; i++)
        {
          g_roboplay_interface->msx_music_write(i, g_mus_header->original_data_music[i]);
        }
    }
    else
    {
        g_roboplay_interface->msx_audio_write(0xBD, g_mus_header->sustain_audio);
    }

    g_speed = g_mus_header->speed;
    g_speed_count = g_speed - 1;

    g_pattern_data = (uint8_t *)DATA_SEGMENT_BASE;
}

void command(const uint8_t id)
{
    /* No additional commmands supported */
    id;
}

float get_refresh(void)
{
    /* Fixed replay rate of 60Hz */
    return 60.0;
}

uint8_t get_subsongs(void)
{
    return 0;
}

char* get_player_info(void)
{
    return "FAC SoundTracker player on MSX-AUDIO/MSX-MUSIC V1.0 by RoboSoft Inc.";
}

char* get_title(void)
{
    return g_song_name;
}

char* get_author(void)
{
    return g_author;
}

char* get_description(void)
{
    switch(g_mus_header->version)
    {
        case 0x00:
        case 0x01:
            if(g_mus_header->device_id == DEVICE_MUSIC)
                return "FAC SoundTracker v1.0  6CH MSX-MUSIC + PERCUSSION";
            else
                return "FAC SoundTracker v1.0  9CH MSX-AUDIO";
        case 0x02:
            if(g_mus_header->device_id == DEVICE_MUSIC)
                return "FAC SoundTracker v2.0  6CH MSX-MUSIC + PERCUSSION";
            else
                return "FAC SoundTracker v2.0  9CH MSX-AUDIO";
        case 0x03:
            if(g_mus_header->device_id == DEVICE_MUSIC)
                return "FAC SoundTracker PRO  6CH MSX-MUSIC + PERCUSSION";
            else
                return "FAC SoundTracker PRO  9CH MSX-AUDIO";
        default:
            return "Unknown tracker version";
    }
}

bool load_sample_kit(const char* file_name, const char* extension)
{
    bool result = false;

    char* sample_kit_name = (char *)READ_BUFFER;

    strcpy(sample_kit_name, file_name);
    uint8_t found = strlen(sample_kit_name) - 1;
    while(sample_kit_name[found] != '\\' && sample_kit_name[found] != ':' && found > 0) found--;
    if(found) 
        sample_kit_name[found + 1] = '\0';
    else
        sample_kit_name[found] = '\0';

    uint8_t l = strlen(sample_kit_name);
    uint8_t c = 0;

    while(g_mus_header->sample_kit_name[c] != ' ' && c < 8)
        sample_kit_name[l + c] = g_mus_header->sample_kit_name[c++];
    sample_kit_name[l + c] = '\0';

    strcat(sample_kit_name, extension);

    if(!g_roboplay_interface->exists(sample_kit_name))
    {
        /* Try to load the drum kit based on the file name */
        strcpy(sample_kit_name, file_name);
        sample_kit_name[strlen(sample_kit_name) - 4] = '\0';
        strcat(sample_kit_name, extension);
    }

    if(g_roboplay_interface->exists(sample_kit_name))
    {
        result = true;

        g_roboplay_interface->open(sample_kit_name, false);
        g_roboplay_interface->read((void *)READ_BUFFER, 7);

        for(int i = 0; i < 4; i++)
        {
            g_roboplay_interface->read((uint8_t *)READ_BUFFER, READ_BUFFER_SIZE);
            g_roboplay_interface->msx_audio_write_adpcm_data((uint8_t*)READ_BUFFER, READ_BUFFER_SIZE);
        }
        g_roboplay_interface->close();
    }

    return result;
}

void play_sample(uint8_t frequency, uint8_t volume, uint8_t sample_number)
{
  sample_number--;
  frequency--;

  g_roboplay_interface->msx_audio_write(0x12, volume);

  g_roboplay_interface->msx_audio_write(0x10, g_sample_delta_n[frequency].delta_n_low);
  g_roboplay_interface->msx_audio_write(0x11, g_sample_delta_n[frequency].delta_n_high);

  g_roboplay_interface->msx_audio_write(0x09, g_sample_blocks[sample_number].start_address_low);
  g_roboplay_interface->msx_audio_write(0x0a, g_sample_blocks[sample_number].start_address_high);

  g_roboplay_interface->msx_audio_write(0x0b, g_sample_blocks[sample_number].end_address_low);
  g_roboplay_interface->msx_audio_write(0x0c, g_sample_blocks[sample_number].end_address_high);

  g_roboplay_interface->msx_audio_write(0x07, 0x01);
  g_roboplay_interface->msx_audio_write(0x07, 0xa0);
}

void frequency_up(uint8_t channel, uint8_t value)
{
    if(!(g_mus_status_table[channel].note_frequency.high & 0x01))
    {
        g_mus_status_table[channel].note_frequency.low += value;
        if(g_mus_status_table[channel].note_frequency.low > 0xB2)
        {
            g_mus_status_table[channel].note_frequency.low -= 0xB2;
            g_mus_status_table[channel].note_frequency.low += 0x59;
            g_mus_status_table[channel].note_frequency.high += 0x03;
        }
    }
    else
    {
        if(g_mus_status_table[channel].note_frequency.low + value > 255)
            g_mus_status_table[channel].note_frequency.high += 0x01;

        g_mus_status_table[channel].note_frequency.low += value;
    }

    if(g_mus_header->device_id == DEVICE_AUDIO)
    {
      g_roboplay_interface->msx_audio_write(0xA0 + channel, g_mus_status_table[channel].note_frequency.low);
      g_roboplay_interface->msx_audio_write(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);
    }
    else
    {
      uint16_t freq = ((uint16_t)(g_mus_status_table[channel].note_frequency.high << 8) + g_mus_status_table[channel].note_frequency.low) >> 1;

      g_roboplay_interface->msx_music_write(0x10 + channel, freq & 0xff);
      g_roboplay_interface->msx_music_write(0x20 + channel, (freq >> 8) | 0x10);
    }
}

void frequency_down(uint8_t channel, uint8_t value)
{
    if(g_mus_status_table[channel].note_frequency.high & 0x01)
    {
        g_mus_status_table[channel].note_frequency.low -= value;
        if(g_mus_status_table[channel].note_frequency.low < 0x46)
        {
            g_mus_status_table[channel].note_frequency.low -= 0x45;
            g_mus_status_table[channel].note_frequency.low += 0x8B;
            g_mus_status_table[channel].note_frequency.high -= 0x03;
        }
    }
    else
    {
        if(g_mus_status_table[channel].note_frequency.low < value)
            g_mus_status_table[channel].note_frequency.high -= 0x01;

        g_mus_status_table[channel].note_frequency.low -= value;
    }

    if(g_mus_header->device_id == DEVICE_AUDIO)
    {
      g_roboplay_interface->msx_audio_write(0xA0 + channel, g_mus_status_table[channel].note_frequency.low);
      g_roboplay_interface->msx_audio_write(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);
    }
    else
    {
      uint16_t freq = ((uint16_t)(g_mus_status_table[channel].note_frequency.high << 8) + g_mus_status_table[channel].note_frequency.low) >> 1;

      g_roboplay_interface->msx_music_write(0x10 + channel, freq & 0xff);
      g_roboplay_interface->msx_music_write(0x20 + channel, (freq >> 8) | 0x10);
    }
}

void handle_pitch_bend(void)
{
    for(uint8_t i = 0; i < NR_OF_CHANNELS; i++)
    {
        if(g_mus_status_table[i].pitch_bend_value_up)
            frequency_up(i, g_mus_status_table[i].pitch_bend_value_up);

        if(g_mus_status_table[i].pitch_bend_value_down)
            frequency_down(i, g_mus_status_table[i].pitch_bend_value_down);
    }
}

void note_on(uint8_t channel, uint8_t data)
{
    g_mus_status_table[channel].note_active = true;
    g_mus_status_table[channel].pitch_bend_value_up = 0;
    g_mus_status_table[channel].pitch_bend_value_down = 0;

    g_mus_status_table[channel].note_frequency.low  = g_frequency_table[data - 1].low + g_mus_status_table[channel].detune_value;
    g_mus_status_table[channel].note_frequency.high = g_frequency_table[data - 1].high;

    if(g_mus_header->device_id == DEVICE_AUDIO)
    {
      g_roboplay_interface->msx_audio_write(0xA0 + channel, g_mus_status_table[channel].note_frequency.low);

      g_roboplay_interface->msx_audio_write(0xB0 + channel, g_mus_status_table[channel].note_frequency.high);
      g_roboplay_interface->msx_audio_write(0xB0 + channel, g_mus_status_table[channel].note_frequency.high | 0x20);
    }
    else
    {
      uint16_t freq = ((uint16_t)(g_mus_status_table[channel].note_frequency.high << 8) + g_mus_status_table[channel].note_frequency.low) >> 1;

      g_roboplay_interface->msx_music_write(0x10 + channel, freq & 0xff);

      g_roboplay_interface->msx_music_write(0x20 + channel, (freq >> 8));
      g_roboplay_interface->msx_music_write(0x20 + channel, (freq >> 8) | 0x10);
    }
}

void note_off(uint8_t channel)
{
    g_mus_status_table[channel].note_active = false;

    if(g_mus_header->device_id == DEVICE_AUDIO)
    {
      g_roboplay_interface->msx_audio_write(0xB0 + channel, 0x00);
    }
    else
    {
      g_roboplay_interface->msx_music_write(0x20 + channel, 0x00);
    }
}

void detune_up(uint8_t channel)
{
  g_mus_status_table[channel].detune_value++;

  if(g_mus_status_table[channel].note_active)
  {
    frequency_up(channel, 1);
  }
}

void detune_down(uint8_t channel)
{
  g_mus_status_table[channel].detune_value--;

  if(g_mus_status_table[channel].note_active)
  {
    frequency_down(channel, 1);
  }
}

void pitch_bend_up(uint8_t channel, uint8_t data)
{
  uint8_t value = data - PITCH_BEND_UP;
  g_mus_status_table[channel].pitch_bend_value_up   = value;
  g_mus_status_table[channel].pitch_bend_value_down = 0;

  if(g_mus_status_table[channel].note_active)
  {
    frequency_up(channel, g_mus_status_table[channel].pitch_bend_value_up);
  }
}

void pitch_bend_down(uint8_t channel, uint8_t data)
{
  uint8_t value = data - PITCH_BEND_DOWN;
  g_mus_status_table[channel].pitch_bend_value_up   = 0;
  g_mus_status_table[channel].pitch_bend_value_down = value;

  if(g_mus_status_table[channel].note_active)
  {
    frequency_down(channel, g_mus_status_table[channel].pitch_bend_value_down);
  }
}

void change_tempo(uint8_t data)
{
    g_speed = MAX_TEMPO - data;
}


void change_volume(uint8_t channel, uint8_t data)
{
    data &= 0x3F;
    data ^= 0x3F;

    uint8_t  value;
    if(g_mus_header->device_id == DEVICE_AUDIO)
    {
        value = (g_mus_header->voice_data_audio[channel][VOLUME_REGISTER_INDEX] & 0xC0) | data;
        g_roboplay_interface->msx_audio_write(g_instrument_registers[channel][VOLUME_REGISTER_INDEX], value);
    }
    else
    {
      value = (g_mus_header->instrument_volume_music[channel] & 0xf0) | (data >> 2);
      g_roboplay_interface->msx_music_write(0x30 + channel, value);
    }
}

void change_brightness(uint8_t channel, uint8_t data)
{
    if(g_mus_header->device_id == DEVICE_AUDIO)
    {
        data &= 0x3F;
        data ^= 0x3F;

        uint8_t  value = (g_mus_header->voice_data_audio[channel][BRIGHTNESS_REGISTER_INDEX] & 0xC0) | data;
        g_roboplay_interface->msx_audio_write(g_instrument_registers[channel][BRIGHTNESS_REGISTER_INDEX], value);
    }
}
