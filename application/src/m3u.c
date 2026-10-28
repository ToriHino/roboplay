/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * m3u.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "support/inc/printf_simple.h"

#include "dos.h"
#include "file.h"
#include "memory.h"
#include "m3u.h"

static int8_t   g_m3u_file_handle;
static uint16_t g_m3u_file_size;
static uint16_t g_m3u_previous_line;

void m3u_open_file(char *const name)
{
  g_m3u_previous_line = 0;
  g_m3u_file_size = file_size(name);
  g_m3u_file_handle = dos_open(name, O_RDONLY);
}

void m3u_close_file(void)
{
  dos_close(g_m3u_file_handle);
  g_m3u_file_handle = -1;
}

bool m3u_get_next_song(char *song_name)
{
  bool result = true;
  do
  {
    uint16_t i = 0;
    while(i < BUFFER_SIZE)
    {
      if(!dos_read(g_m3u_file_handle, &song_name[i], 1)) 
      {
        result = false;
        break;
      }
      if(song_name[i] == '\n') break;
      if(song_name[i] != '\r') i++;
    }
    song_name[i] = '\0';
  }  while(result && (strlen(song_name) && song_name[0] == '#'));

  return (strlen(song_name) > 0);
}

void m3u_get_random_song(char* song_name)
{
  do
  {
    uint16_t offset = rand() % g_m3u_file_size;
    dos_lseek(g_m3u_file_handle, offset, SEEK_SET);

    uint8_t data;
    int8_t  bytes_read;
    do
    {
      bytes_read = dos_read(g_m3u_file_handle, &data, 1);
    } while(bytes_read > 0 && data != '\n');
}
  while(!m3u_get_next_song(song_name));
}