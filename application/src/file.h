/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * file.h
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

void get_program_path(char *path);

bool find_next_file(const bool first_file);

void load_player(char *const name);

void     file_open(char *const name, const bool roboplay_file);
void     file_close(void);
uint16_t file_read(void *const destination, const uint16_t size);
bool     file_exists(char *const name);
uint16_t file_size(char *const name);
void     file_seek(int32_t offset, uint8_t whence);