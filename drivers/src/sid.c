/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * sid.c
 */

 #include "sid.h"

__sfr __at 0x2A sid_register_port;
__sfr __at 0x2B sid_data_port;
__sfr __at 0x2C sid_read_port;

bool sid_detect(void)
{
  // TODO
  return false;
}

SID_TYPE get_sid_type(void)
{
  return ((sid_data_port & 1) == 1) ? SID_TYPE_8580 : SID_TYPE_6581;
}

void sid_reset(void)
{
  for(uint8_t i = 0; i < 25; i++)
  {
    sid_write(i, 0x00);
  }
}

void sid_write(const uint8_t reg, const uint8_t data)
{
  sid_register_port = reg;
  sid_data_port     = data;

}

uint8_t sid_read(const uint8_t reg)
{
  sid_register_port = reg;
  return sid_data_port;
}

