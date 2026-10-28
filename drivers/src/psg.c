/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * psg.c
 */

#include "psg.h"

__sfr __at 0xA0 psg_register_port;
__sfr __at 0xA1 psg_value_port;
__sfr __at 0xA2 psg_read_port;

__sfr __at 0x10 psg2_register_port;
__sfr __at 0x11 psg2_value_port;
__sfr __at 0x12 psg2_read_port;

void psg_reset(void)
{
  for(uint8_t i = 8; i < 11; i++)
  {
    psg_write(i, 0x00);
    psg2_write(i, 0x00);
  }

  for(uint8_t i = 0; i < 14; i++)
  {
    psg_write(i, 0x00);
    psg2_write(i, 0x00);
  }
}

void psg_write(const uint8_t reg, const uint8_t value)
{
  psg_register_port = reg;
  psg_value_port    = value;

}

uint8_t psg_read(const uint8_t reg)
{
  psg_register_port = reg;
  return psg_read_port;
}

void psg2_write(const uint8_t reg, const uint8_t value)
{
  psg2_register_port = reg;
  psg2_value_port    = value;
}

uint8_t psg2_read(const uint8_t reg)
{
  psg2_register_port = reg;
  return psg2_read_port;
}