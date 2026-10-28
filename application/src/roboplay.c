/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 * 
 * roboplay.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
 
#include "support/inc/printf_simple.h"
#include "players/inc/player_interface.h"

#include "device.h"
#include "file.h"
#include "m3u.h"
#include "dos.h"
#include "memory.h"
#include "keyboard.h"
#include "player.h"
#include "roboplay.h"

inline void enable_interrupt(void) { __asm__("ei"); }
inline void disable_interrupt(void) { __asm__("di"); }

#define JIFFY 0xFC9E

static PLAY_MODE g_play_mode = PLAY_MODE_SINGLE_FILE;

char* g_extension;

static bool g_use_wildcard;
static bool g_use_fixed_player;
static bool g_randomize;

bool g_wave_opl_drums;
bool g_force_msx_midi;

static uint32_t g_current_tick;
static uint32_t g_maximum_ticks;
static uint16_t g_maximum_time_in_s;
static uint8_t  g_maximum_loops;

/*
 * show_info
 */
void show_info(void)
{
  printf_simple("RoboPlay v2.1 - MSX multi music format player\n\r");
  printf_simple("Created by RoboSoft Inc. in 2026\n\r");
}

/*
 * find_extension
 */
void find_extension(char *name)
{
  uint8_t found = strlen(name) - 1;
  while(name[found] != '.' && found > 0) found--;

  g_extension = &name[found + 1];
}

/*
 * check_arguments
 */
void check_arguments(char **argv, int argc)
{
  g_use_wildcard = false;
  g_use_fixed_player = false;
  g_randomize = false;
  g_wave_opl_drums = false;
  g_force_msx_midi = false;
  g_maximum_loops = 0;

  if (!argc)
  {
    printf_simple("\n\rUsage:\n\r  RoboPlay [/T <seconds>] [/L <loops>] [/P] [<player>.PLY] <song>.EXT\n\r");
    printf_simple("or\n\r  RoboPlay [/T <seconds>] [/L <loops>] [/R] <name>.M3U\n\r");
    printf_simple("\n\rWildcards in the song name are allowed.");
    printf_simple("\n\rUse the '/R' option to randomize the M3U playlist");
    printf_simple("\n\rUse the '/T' option to set maximum song play time");
    printf_simple("\n\rUse the '/L' option to set the number of loops playing a song\n\r");
    printf_simple("\n\rUse the '/W' option to use Wave drums for OPL rhythm mode");
    printf_simple("\n\rUse the '/P' option to force MIDI out to MSX-MIDI or MSX-Pico\n\r");
    printf_simple("\n\rWhen no player is provided, the song file extension is used for identification\n\r");
    dos_exit(0);
  }

  for(uint8_t i = 0; i < argc; i++)
  {
    find_extension(argv[i]);

    for (uint8_t j = 0; j < strlen(argv[i]); j++)
      argv[i][j] = toupper(argv[i][j]);
    
    if(!strcmp(g_extension, "T"))
    {
      g_maximum_time_in_s = atoi(argv[++i]);
    }
    else if(!strcmp(g_extension, "L")) 
      g_maximum_loops = atoi(argv[++i]);
    else if(!strcmp(g_extension, "R"))
      g_randomize = true;
    else if(!strcmp(g_extension, "W"))
      g_wave_opl_drums = true;
    else if(!strcmp(g_extension, "M"))
      g_force_msx_midi = true;
    else if(!strcmp(g_extension, "PLY"))
    {
      g_use_fixed_player = true;
      get_program_path(g_current_player_name);
      strcat(g_current_player_name, argv[i]);
    }
    else if (!strcmp(g_extension, "M3U"))
    {
      g_play_mode = PLAY_MODE_M3U;

      strcpy(g_m3u_name, argv[i]);
    }
    else
    {
      for (uint8_t j = 0; j < strlen(argv[i]); j++)
        if(argv[i][j] == '*' || argv[i][j] == '?') g_use_wildcard = true;
    
      strcpy(g_current_song_name, argv[i]);
    }

  }
}

/*
 * show_song_info
 */
void show_song_info(void)
{
    printf_simple("\n\r");
    printf_simple("Title       : %s\n\r", g_roboplay_interface->get_title());
    printf_simple("Author      : %s\n\r", g_roboplay_interface->get_author());
    printf_simple("Description : %s\n\r", g_roboplay_interface->get_description());

    if(g_roboplay_interface->get_subsongs() > 1)
    {
        printf_simple("Number of subsongs: %d\n\r", g_roboplay_interface->get_subsongs());
    }
        
    printf_simple("\nNow playing ...ESC to stop");

    if(g_roboplay_interface->get_subsongs() > 1)
    {
        printf_simple(", LEFT/RIGHT for subsong");
    }

     if (g_play_mode == PLAY_MODE_M3U || g_use_wildcard)
     {
        printf_simple(", SPACE for next song");
     }
    printf_simple("\n\r");
}

/*
 * play_song
 */
bool play_song(void)
{
  bool result = false;

  g_current_tick = 0;

  init_player();
  if(!g_roboplay_interface->load(g_current_song_name))
  {
    printf_simple("\n\rError: Not a valid file for this player\n\r");
    if(g_play_mode == PLAY_MODE_SINGLE_FILE) 
    {
      restore_ram_segment();
      dos_exit(_INTER);
    }
    else 
      return true;
  }

  show_song_info();

#ifndef __DEBUG
  disable_interrupt();
#endif

  uint8_t subsong = 0;
  g_roboplay_interface->rewind(subsong);

  set_refresh();
  g_maximum_ticks = g_maximum_time_in_s * (uint16_t)g_refresh;
  
  uint8_t loops = g_maximum_loops;

  while(!is_key_pressed_ESC())
  {
    if(g_maximum_ticks && g_current_tick > g_maximum_ticks)
    {
      result = true;
      break;
    }

    if(g_play_mode == PLAY_MODE_M3U && is_key_pressed_SPACE())
    {
      result = true;
      break;
    }

    if(g_use_wildcard && (is_key_pressed_SPACE()))
    {
      result = true;
      break;
    }

    for(uint8_t i = 1; i < 7; i++)
    {
      if(is_number_key_pressed(i)) g_roboplay_interface->command(i);
    }

    if(!g_roboplay_interface->update())
    {
      if(loops)
      {
        loops--;
        if(!loops) break;
      }

      g_roboplay_interface->rewind(subsong);
      set_refresh();
      g_maximum_ticks = g_maximum_time_in_s * (uint16_t)g_refresh;
    }

    if(g_roboplay_interface->get_subsongs() > 1)
    {
      if(is_key_pressed_RIGHT() && subsong < g_roboplay_interface->get_subsongs())
      {
        subsong++;
        g_roboplay_interface->rewind(subsong);
      }

      if(is_key_pressed_LEFT() && subsong > 0)
      {
        subsong--;
        g_roboplay_interface->rewind(subsong);
      }
    }

    g_current_tick++;
    wait_for_refresh();
  }

  reset_devices();
  free_used_ram_segments();

#ifndef __DEBUG
  enable_interrupt();
#endif

  return result;
}

 /*
  * main
  */
int main(char **argv, int argc)
{ 
  *g_current_song_name = '\0';
  *g_current_player_name = '\0';
  *g_m3u_name = '\0';

  g_maximum_time_in_s = 0;
  g_maximum_ticks = 0;
  g_current_tick  = 0;

  uint16_t* jiffy = (uint8_t *)JIFFY;
  srand(*jiffy);

  show_info();
  check_arguments(argv, argc);
  
  init_devices();

  if (g_play_mode == PLAY_MODE_M3U)
  {
      printf_simple("\n\rUsing M3U playlist: %s", g_m3u_name);
      if (g_randomize) printf_simple(", Randomized");

      m3u_open_file(g_m3u_name);
  }

  printf_simple("\n\rTotal free memory found: %dK\n\r", pre_allocate_ram_segments() * 16);
  if (g_maximum_time_in_s) printf_simple("Max play time:           %i seconds\n\r", g_maximum_time_in_s);
  if (g_maximum_loops) printf_simple    ("Max loops to play song:  %i\n\r", g_maximum_loops);

  bool next_song = true;
  bool first_file = true;
  do
  {
    if(g_use_wildcard)
      next_song = find_next_file(first_file);
    else if(g_play_mode == PLAY_MODE_M3U)
    {
      if(g_randomize)
        m3u_get_random_song(g_current_song_name);
      else
        next_song = m3u_get_next_song(g_current_song_name);
    }

    if(next_song && !g_use_fixed_player)
    {
        find_extension(g_current_song_name);
        find_player_name();
    } 

    first_file = false;
  } while(next_song && play_song() && (g_use_wildcard || g_play_mode == PLAY_MODE_M3U));

  if (g_play_mode == PLAY_MODE_M3U)
  {
    m3u_close_file();
  }

  restore_devices();
  restore_ram_segment();

  return 0;
}