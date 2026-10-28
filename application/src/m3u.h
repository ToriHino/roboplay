/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * m3u.h
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

void m3u_open_file(char *const name);
void m3u_close_file(void);

bool m3u_get_next_song(char *song_name);
void m3u_get_random_song(char* song_name);
