/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * mid.h
 *
 * MID: Standard MIDI file player on external device
 */

#pragma once

#include <stdint.h>

#define MIDI_HEADER_SIZE 14
#define MAX_NR_OF_TRACKS 64

typedef struct 
{
    uint8_t   file_format;
    uint16_t  number_of_tracks;
    uint16_t  ticks_per_qnote;
} MIDI_HEADER;

typedef struct 
{
    bool      track_finished;
    uint32_t  waiting_for;
    uint16_t  length;

    uint8_t   last_command;

    uint8_t   start_segment;
    uint8_t  *start_track_data;
    uint8_t   segment;
    uint8_t  *track_data;
} TRACK_DATA;

typedef struct
{
    uint32_t ticks_per_qnote;
    uint32_t clock_ticks;
    uint32_t tick_counter;
    uint32_t midi_counter;
} TIME_DATA;

MIDI_HEADER g_header;
TRACK_DATA  g_track_data[MAX_NR_OF_TRACKS];

uint8_t g_number_of_tracks;
uint8_t g_track;

uint8_t g_volume_boost;

uint32_t g_qnote_duration;

uint32_t g_ticks_per_update;
uint32_t g_MIDI_counter;

void load_track_data(void);

uint8_t read_byte(void);
uint32_t get_variable_len(void);
void get_delta_time(void);

void handle_track_event(void);
void handle_meta_event(uint8_t sub_type);
void handle_sys_ex_event(void);
