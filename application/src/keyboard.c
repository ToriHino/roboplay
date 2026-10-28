/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * keyboard.c
 */

#include "keyboard.h"

__sfr __at 0xA0 ppi_register_write;
__sfr __at 0xA1 ppi_value_write;
__sfr __at 0xA2 ppi_value_read;


__sfr __at 0xA8 ppi_reg_a;
__sfr __at 0xA9 ppi_reg_b;
__sfr __at 0xAA ppi_reg_c;

bool is_key_pressed_ESC(void)
{
  ppi_reg_c = (ppi_reg_c & 0xF0) | 0x07;
  bool result = (ppi_reg_b & 0x04) ? false : true;

  if(!result)
  {
    ppi_register_write = 0x0f;
    ppi_value_write = (ppi_value_read & 0xbf);

    ppi_register_write = 0x0e;
    if(!(ppi_value_read & 0x20)) 
    {
      result = true;
      while(!(ppi_value_read & 0x20));
    }

    if(!result)
    {
      ppi_register_write = 0x0f;
      ppi_value_write = (ppi_value_read | 0x40);

      ppi_register_write = 0x0e;
      if(!(ppi_value_read & 0x20)) 
      {
        result = true;
        while(!(ppi_value_read & 0x20));
      }
    }
  }

  return result;
}

bool is_key_pressed_LEFT(void)
{
    bool result = false;

    ppi_reg_c = (ppi_reg_c & 0xF0) | 0x08;
    if(!(ppi_reg_b & 0x10))
    {
        result = true;
        while(!(ppi_reg_b & 0x10));
    }

    return result;
}

bool is_key_pressed_RIGHT(void)
{
    bool result = false;

    ppi_reg_c = (ppi_reg_c & 0xF0) | 0x08;
    if(!(ppi_reg_b & 0x80))
    {
        result = true;
        while(!(ppi_reg_b & 0x80));
    }

    return result;
}

bool is_key_pressed_SPACE(void)
{
  bool result = false;

  ppi_reg_c = (ppi_reg_c & 0xF0) | 0x08;
  if(!(ppi_reg_b & 0x01))
  {
    result = true;
    while(!(ppi_reg_b & 0x01));
  }

  if(!result)
  {
    ppi_register_write = 0x0f;
    ppi_value_write = (ppi_value_read & 0xbf);

    ppi_register_write = 0x0e;
    if(!(ppi_value_read & 0x10)) 
    {
      result = true;
      while(!(ppi_value_read & 0x10));
    }

    if(!result)
    {
      ppi_register_write = 0x0f;
      ppi_value_write = (ppi_value_read | 0x40);

      ppi_register_write = 0x0e;
      if(!(ppi_value_read & 0x10)) 
      {
        result = true;
        while(!(ppi_value_read & 0x10));
      }
    }
  }

  return result;
}

bool is_number_key_pressed(const uint8_t number)
{
    bool result = false;

    ppi_reg_c = (ppi_reg_c & 0xF0) | 0x00;
    if(!(ppi_reg_b & (1 << number)))
    {
        result = true;
        while(!(ppi_reg_b & (1 << number)));
    }

    return result;
}
