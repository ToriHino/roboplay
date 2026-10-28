/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * darky.c
 */

#include "darky.h"

#define DARKY_DEVICE_ID          0xAA

#define DARKY_READ_BACK_REGISTER 0x60

/*
 * OUT ports used for Darky
*/
__sfr __at 0x40 darky_switched_io_port;
__sfr __at 0x41 darky_cpld_control_register_port;
__sfr __at 0x42 darky_register_port;

__sfr __at 0x44 darky_epsg1_index_port;
__sfr __at 0x45 darky_epsg1_write_port;
__sfr __at 0x46 darky_epsg1_read_port;

__sfr __at 0x4c darky_epsg2_index_port;
__sfr __at 0x4d darky_epsg2_write_port;
__sfr __at 0x4e darky_epsg2_read_port;

inline void darky_delay(void) { __asm__("ex (sp), ix"); __asm__("ex (sp), ix"); }

bool darky_detect(void)
{
  /* Enable Darky on default swiched I/O address */
  darky_switched_io_port = DARKY_DEVICE_ID;

  return darky_switched_io_port == (uint8_t)~DARKY_DEVICE_ID;
}

void darky_wait_busy(void)
{
  do darky_delay(); while (darky_cpld_control_register_port & 0x40);
}

void darky_write_register_index(uint8_t index)
{
  darky_register_port = (index | 0x80);
  darky_delay();
}

void darky_write_register_data(uint8_t data)
{
  darky_register_port = (data & 0x7f);
  darky_delay();
  darky_delay();
}

uint8_t darky_read_internal_register(uint8_t reg)
{
  /* Set readback register */
  darky_write_register_index(DARKY_READ_BACK_REGISTER);

  /* Start readback command */
  darky_write_register_data(reg);

  /* Read the data */
  darky_wait_busy();
  return darky_register_port;
}

uint8_t darky_read_register(uint8_t reg)
{
  /* Set register number */
  darky_write_register_index(reg);

  /* Read the data */
  return darky_register_port;
}

void darky_write_register(uint8_t reg, uint8_t data)
{
  /* Set register number */
  darky_write_register_index(reg);

  /* Write the data */
  darky_write_register_data(data);
}

uint8_t darky_get_version(void)
{
  return darky_read_register(0x7d);
}

void darky_reset(void)
{
  darky_write_register(0x40, 0x00);

  /* Set master volume */
  darky_write_register(0x0b, 63);
  darky_write_register(0x1b, 63);

  /* Set default channels ePSG1 to LEFT MIDDLE RIGHT: ABC  */
  darky_write_register(0x00, 15);
  darky_write_register(0x01, 7);
  darky_write_register(0x02, 0);

  darky_write_register(0x10, 0);
  darky_write_register(0x11, 7);
  darky_write_register(0x12, 15);

  /* Set default channels ePSG2 to LEFT MIDDLE RIGHT: ABC  */
  darky_write_register(0x03, 15);
  darky_write_register(0x04, 7);
  darky_write_register(0x05, 0);

  darky_write_register(0x13, 0);
  darky_write_register(0x14, 7);
  darky_write_register(0x15, 15);

  /* Reset all ePSG registers */
  for(uint8_t i = 8; i < 11; i++)
  {
    epsg1_write(i, 0x00);
    epsg2_write(i, 0x00);
  }

  for(uint8_t i = 0; i < 14; i++)
  {
    epsg1_write(i, 0x00);
    epsg2_write(i, 0x00);
  }
}

void darky_restore(void)
{
  if(darky_detect())
    darky_write_register(0x40, 0x03);
}

void epsg1_write(const uint8_t reg, const uint8_t value)
{
  darky_epsg1_index_port = reg;
  darky_epsg1_write_port = value;
}

uint8_t epsg1_read(const uint8_t reg)
{
  darky_epsg1_index_port = reg;
  return darky_epsg1_read_port;
}

void epsg2_write(const uint8_t reg, const uint8_t value)
{
  darky_epsg2_index_port = reg;
  darky_epsg2_write_port = value;
}

uint8_t epsg2_read(const uint8_t reg)
{
  darky_epsg2_index_port = reg;
  return darky_epsg2_read_port;
}
