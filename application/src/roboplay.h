/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * roboplay.h
 */

#pragma once

#include <stdint.h>

typedef enum
{
    PLAY_MODE_SINGLE_FILE,
    PLAY_MODE_M3U,
} PLAY_MODE;

void show_info(void);

void check_arguments(char **argv, int argc);
void find_player_name(void);

void find_extension(char *name);

bool play_song(void);
