/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * file.c
 */

#include <stdio.h>
#include <string.h>

#include "support/inc/printf_simple.h"
#include "players/inc/player_interface.h"

#include "dos.h"
#include "memory.h"
#include "file.h"

#define MAX_PLAYER_SIZE 0x4000
#define PLAYER_SIGNATURE "ROBOPLAY"

static int8_t  g_file_handle = -1;
static uint8_t g_counter;

static FILE_INFO_BLOCK g_local_file_info_block;

void get_program_path(char *path)
{
    path[0] = '\0';

    if(dos_get_environment("PROGRAM", path))
    {
      uint8_t found = strlen(path) - 1;
      while(path[found] != '\\' && path[found] != ':' && found > 0) found--;
      if(found) path[found + 1] = '\0';
    }
}

bool find_next_file(const bool first_file)
{
  bool result = false;

  if(first_file)
    strcpy(work_buffer, g_current_song_name);

  uint8_t found = strlen(g_current_song_name) - 1;
  while(g_current_song_name[found] != '\\' && g_current_song_name[found] != ':' && found > 0) found--;
  if(found) 
      g_current_song_name[found + 1] = '\0';
  else
      g_current_song_name[0] = '\0';

  if(first_file)
    result = (dos_find_first_entry(work_buffer) == _OK);
  else
  {
    memcpy(&g_file_info_block, &g_local_file_info_block, sizeof(FILE_INFO_BLOCK));
    result = (dos_find_next_entry() == _OK);
  }

  if(result)
  {
    memcpy(&g_local_file_info_block, &g_file_info_block, sizeof(FILE_INFO_BLOCK));
    strcat(g_current_song_name, g_file_info_block.name);
  }

  return result;
}

void load_player(char *const name)
{
  printf_simple("\nLoading player: %s", name);

  int8_t player_file_handle = dos_open(name, O_RDONLY);
  dos_read(player_file_handle, (uint8_t *)g_roboplay_interface, MAX_PLAYER_SIZE);
  dos_close(player_file_handle);

  if (strncmp(g_roboplay_interface->signature, PLAYER_SIGNATURE, 8))
  {
    printf_simple(" ...Error\n\r");
    printf_simple("\n\rNot a RoboPlay player file\n\r");
    dos_exit(_INTER);
  }

  if( !(g_roboplay_interface->major_version == INTERFACE_MAJOR_VERSION &&
        g_roboplay_interface->minor_version == INTERFACE_MINOR_VERSION))
  {
    printf_simple(" ...Error\n\r");
    printf_simple("\n\rWrong version of RoboPlay player file\n\r");
    dos_exit(_INTER);
  }

  printf_simple(" ...Done\n\r");
}

void file_open(char *const name, const bool roboplay_file)
{
  if(roboplay_file)
  {
      get_program_path(work_buffer);
      strcat(work_buffer, name);
  }
  else
  {
      strcpy(work_buffer, name);
  }

  if(g_file_handle == -1)
      printf_simple("Loading file: %s ", work_buffer);
  else
      dos_close(g_file_handle);

  g_file_handle = dos_open(work_buffer, O_RDONLY);

  g_counter = 0;
}

void file_close(void)
{
  dos_close(g_file_handle);
  g_file_handle = -1;

  for(uint8_t i = 0; i < g_counter; i++)
      printf_simple("\b \b");
  printf_simple("...Done\n\r");
}

uint16_t file_read(void *const destination, const uint16_t size)
{
#ifndef __DEBUG
  if(g_counter < 3)
      printf_simple(".");
  else
      printf_simple("\b\b\b   \b\b\b");
  g_counter = (g_counter < 3) ? g_counter + 1 : 0;
#endif

  int16_t bytes_read = dos_read(g_file_handle, destination, size);
  return bytes_read;
}

bool file_exists(char *const name)
{
  return (dos_find_first_entry(name) == _OK);
}

uint16_t file_size(char *const name)
{
  int8_t file_handle = dos_open(name, O_RDONLY);
  int16_t bytes_read = dos_read(file_handle, (uint8_t *)g_roboplay_interface, MAX_PLAYER_SIZE);
  dos_close(file_handle);

  return bytes_read;
}

void file_seek(int32_t offset, roboplay_seek whence)
{
  uint8_t dos_whence = SEEK_SET;

  switch(whence)
  {
    case ROBOPLAY_SEEK_CURRENT:
      dos_whence = SEEK_CUR;
      break;
    case ROBOPLAY_SEEK_END:
      dos_whence = SEEK_END;
      break;
  }

  dos_lseek(g_file_handle, offset, dos_whence);
}