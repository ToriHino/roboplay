/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * devices.c
 */

#include "support/inc/printf_simple.h"

#include "device.h"
#include "dos.h"
#include "keyboard.h"

#define OPL4_TIMER1_COUNT  0x02
#define OPL4_TIMER2_COUNT  0x03
#define OPL4_TIMER_CONTROL 0x04

#define INTERRUPT_ADJUST_COUNT 5

extern bool g_force_msx_midi;
extern bool g_msx_midi_detected;

bool  g_opl4_detected = false;
float g_refresh = 0.0;

static uint8_t interrupt_count = 0;
static bool adjust_timing = false;

inline void wait_for_interrupt(void) { __asm__("ei"); __asm__("halt"); __asm__("di");}

/*
 * opl4_detected
 */
bool opl4_detected(void)
{
  return g_opl4_detected;
}

/*
 * init_opl4_device
 */
void init_opl4_device(void)
{
  printf_simple("\n\rDetecting OPL4      device");

  g_opl4_detected = opl4_detect();
  if(!g_opl4_detected)
  {   
    printf_simple(" ...Not Found, using VDP timing at ");

    if(get_vdp_refresh() == VDP_REFRESH_50_HZ)
      printf_simple("50");
    else
      printf_simple("60");

    printf_simple("Hz\n\r");
  }
  else
  {
    if(g_opl4_detected) opl4_reset();
    printf_simple(" ...Found, sample memory: %iK\n\r", opl4_sample_ram_banks() * 64);
  }
}

/*
 * init_scc_device
 */
void init_scc_device(void)
{
  printf_simple("Detecting SCC       device");

  if(!scc_find_slot())
  {
    printf_simple(" ...Not Found\n\r");
  }
  else
  {
    uint8_t scc_slot = scc_get_slot();
    if(scc_slot & 0x80)
      printf_simple(" ...Found, located in slot %i-%i\n\r", scc_slot & 0x03, (scc_slot & 0x0C) >> 2);
    else
      printf_simple(" ...Found, located in slot %i\n\r", scc_slot);

    scc_reset();
  }
}

/*
 * init_opm_device
 */
void init_opm_device(void)
{
  printf_simple("Detecting OPM       device");

  if(!opm_find_slot())
  {
    printf_simple(" ...Not Found\n\r");
  }
  else
  {
    uint8_t opm_slot = opm_get_slot();
    if(opm_slot & 0x80)
      printf_simple(" ...Found, located in slot %i-%i\n\r", opm_slot & 0x03, (opm_slot & 0x0C) >> 2);
    else
      printf_simple(" ...Found, located in slot %i\n\r", opm_slot);

    opm_reset();
  }
}

/*
 * init_midi_device
 */
void init_midi_device(void)
{
  printf_simple("Detecting MIDI      device");

  if(midi_pac_detect())
  {
      printf_simple(" ...Found, MIDI-PAC\n\r");
  }
  else if(msx_midi_detect())
  {
    printf_simple(" ...Found, MSX-MIDI\n\r");
  }
  else if(g_force_msx_midi)
  {
    printf_simple(" ...Forced to MSX-MIDI\n\r");
    g_msx_midi_detected = true;
  }
  else
    printf_simple(" ...Not Found\n\r");

}

/*
 * init_darky_device
 */
void init_darky_device(void)
{
  printf_simple("Detecting DARKY     device");

  if(!darky_detect())
  {
    printf_simple(" ...Not Found\n\r");
  }
  else
  {
    darky_reset();
    printf_simple(" ...Found\n\r");
  }
}

/*
 * init_soundstar_device
 */
void init_soundstar_device(void)
{
  printf_simple("Detecting SOUNDSTAR device");

  if(!soundstar_find_slot())
  {
    printf_simple(" ...Not Found\n\r");
  }
  else
  {
    uint8_t soundstar_slot = soundstar_get_slot();
    if(soundstar_slot & 0x80)
      printf_simple(" ...Found, located in slot %i-%i, version %i.%i\n\r", soundstar_slot & 0x03, (soundstar_slot & 0x0C) >> 2, soundstar_get_major_version(), soundstar_get_minor_version());
    else
      printf_simple(" ...Found, located in slot %i, version %i.%i\n\r", soundstar_slot, soundstar_get_major_version(), soundstar_get_minor_version());

    soundstar_reset();
  }
}

/*
 * init_msx_audio_device
 */
void init_msx_audio_device(void)
{
  printf_simple("Detecting MSX-AUDIO device");

  if(!msx_audio_detect())
  {
    printf_simple(" ...Not Found\n\r");
  }
  else
  {
    printf_simple(" ...Found\n\r");
  }
}

/*
 * init_msx_music_device
 */
void init_msx_music_device(void)
{
  printf_simple("Detecting MSX-MUSIC device");

  if(!msx_music_detect())
  {
    printf_simple(" ...Not Found\n\r");
  }
  else
  {
    uint8_t msx_music_slot = msx_music_get_slot();
    if(msx_music_slot & 0x80)
      printf_simple(" ...Found, located in slot %i-%i\n\r", msx_music_slot & 0x03, (msx_music_slot & 0x0C) >> 2);
    else
      printf_simple(" ...Found, located in slot %i\n\r", msx_music_slot);

    msx_music_reset();
  }
}

/*
 * init_sid_device
*/
void init_sid_device(void)
{
  printf_simple("Detecting PLAYSONIQ device");

  if(!sid_detect())
  {
    printf_simple(" ...Not found\n\r");
  }
  else
  {
    printf_simple(" ...Found, SID type %s\n\r", get_sid_type() == SID_TYPE_6581 ? "6581" : "8580");
  }
}

void init_devices(void)
{
  init_vdp_device();

  init_opl4_device();
  init_scc_device();
  init_opm_device();
  init_midi_device();
  init_darky_device();
  init_soundstar_device();
  init_msx_audio_device();
  init_msx_music_device();
  init_sid_device();

  reset_devices();
}

void reset_devices(void)
{
  if(g_opl4_detected) opl4_reset();
  
  opm_reset();
  scc_reset();
  psg_reset();
  midi_pac_reset();
  msx_midi_reset();
  darky_reset();
  soundstar_reset();
  msx_audio_reset();
  msx_music_reset();
  sid_reset();
}

void restore_devices(void)
{
  midi_pac_restore();
  msx_midi_restore();
  darky_restore();
}

void set_opl4_mode(const roboplay_opl_mode mode)
{
    switch(mode)
    {
        case ROBOPLAY_OPL_MODE_OPL2:
            opl4_write_fm_register_array_2(0x05, 0x00);
            break;
        case ROBOPLAY_OPL_MODE_OPL3:
            opl4_write_fm_register_array_2(0x05, 0x01);
            break;
        case ROBOPLAY_OPL_MODE_OPL4:
            opl4_write_fm_register_array_2(0x05, 0x03);
            break;
    }
}

void write_opl4_fm_1(const uint8_t reg, const uint8_t value)
{
#ifdef __DEBUG
    printf_simple("1:%x:%x ", reg, value);
#endif

    if(reg == 2 || reg == 3 || reg == 4) return;

    opl4_write_fm_register_array_1(reg, value);
}

void write_opl4_fm_2(const uint8_t reg, const uint8_t value)
{
#ifdef __DEBUG
    printf_simple("2:%x:%x ", reg, value);
#endif

    opl4_write_fm_register_array_2(reg, value);
}

void write_opm_fm(const uint8_t reg, const uint8_t value)
{
    if(reg == 0x11 || reg == 0x12 || reg == 0x13 || reg == 0x14) return;

    opm_write_register(reg, value);
}

void write_msx_audio_fm(const uint8_t reg, const uint8_t value)
{
  if(reg == 2 || reg == 3 || reg == 4) return;

#ifdef __DEBUG
    printf_simple("A %x:%x ", reg, value);
#endif

  msx_audio_write_register(reg, value);
}

void write_msx_music_fm(const uint8_t reg, const uint8_t value)
{
#ifdef __DEBUG
    printf_simple("M %x:%x ", reg, value);
#endif

  msx_music_write_register(reg, value);
}

void set_refresh(void)
{
    g_refresh = g_roboplay_interface->get_refresh();

#ifdef __DEBUG
    printf_simple("\n\rRefresh: %i\n\r", (uint16_t)g_refresh);
#else
  if(g_opl4_detected)
    opl4_set_refresh(g_refresh);
  else
  {
    adjust_timing = (get_vdp_refresh() == VDP_REFRESH_60_HZ && (uint8_t)g_refresh == 50);
  }
#endif
}

void wait_for_refresh(void)
{
#ifndef __DEBUG
  if(g_opl4_detected)
  {
    while(!(opl4_read_status_register() & 0x80));
    opl4_write_fm_register_array_1(0x04, 0x80);
  }
  else
  {
    if(adjust_timing &&  interrupt_count++ == INTERRUPT_ADJUST_COUNT)
    {
      interrupt_count = 0;
      wait_for_interrupt();
    }
    wait_for_interrupt();
  }
#else
  while(!is_key_pressed_RIGHT());
#endif        
}

