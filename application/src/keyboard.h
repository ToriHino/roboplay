/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * keyboard.h
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool is_key_pressed_ESC(void);
bool is_key_pressed_LEFT(void);
bool is_key_pressed_RIGHT(void);
bool is_key_pressed_SPACE(void);

bool is_number_key_pressed(const uint8_t number);
