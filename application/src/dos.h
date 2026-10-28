/*
 * RoboPlay for MSX
 * Created by RoboSoft Inc. in 2026
 *
 * dos.h
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* standard descriptors */
#define  STDIN   0
#define  STDOUT  1
#define  STDERR  2
#define  AUX     3
#define  PRN     4

/* open/create flags */
#define  O_RDONLY   0x01
#define  O_WRONLY   0x02
#define  O_RDWR     0x00
#define  O_INHERIT  0x04

/* create attributes */
#define  ATTR_RDONLY  0x01
#define  ATTR_HIDDEN  0x02
#define  ATTR_SYSTEM  0x04
#define  ATTR_VOLUME  0x08
#define  ATTR_FOLDER  0x10
#define  ATTR_ARCHIV  0x20
#define  ATTR_DEVICE  0x80

/* seek whence */
#define  SEEK_SET  0
#define  SEEK_CUR  1
#define  SEEK_END  2

/* MSX_DOS error codes */
#define _OK    0x00

#define _INTER 0xDF    /* Internal error */
#define _NORAM 0xDE    /* Not Enough Memory */
#define _IBDOS 0xDC    /* Invalid MSX-DOS Call */  
#define _IDRV  0xDB    /* Invalid drive */
#define _IFNM  0xDA    /* Invalid filename */ 
#define _IPATH 0xD9    /* Invalid pathname  */
#define _PLONG 0xD8    /* Pathname too long */
#define _NOFIL 0xD7    /* File not found */
#define _NODIR 0xD6    /* Directory not found  */
#define _DRFUL 0xD5    /* Root directory full */
#define _DKFUL 0xD4    /* Disk full */
#define _DUPF  0xD3    /* Duplicate filename  */
#define _DIRE  0xD2    /* Invalid directory move */
#define _FILRO 0xD1    /* Read only file */
#define _DINE  0xD0    /* Directory not empty */
#define _IATTR 0xCF    /* Invalid attributes */
#define _DOT   0xCE    /* Invalid . or .. operation */
#define _SYSX  0xCD    /* System file exists */
#define _DIRX  0xCC    /* Directory exists */
#define _FILEX 0xCB    /* File exists */
#define _FOPEN 0xCA    /* File already in use  */
#define _OV64K 0xC9    /* Cannot transfer above 64K */
#define _FILE  0xC8    /* File allocation error */
#define _EOF   0xC7    /* End of file  */
#define _ACCV  0xC6    /* File access violation  */
#define _IPROC 0xC5    /* Invalid process id  */
#define _NHAND 0xC4    /* No spare file handles */
#define _IHAND 0xC3    /* Invalid file handle */
#define _NOPEN 0xC2    /* File handle not open */
#define _IDEV  0xC1    /* Invalid device operation */
#define _IENV  0xC0    /* Invalid environment string */
#define _ELONG 0xBF    /* Environment string too long */
#define _IDATE 0xBE    /* Invalid date */
#define _ITIME 0xBD    /* Invalid time */
#define _RAMDX 0xBC    /* RAM disk (drive H:) already exists */
#define _NRAMD 0xBB    /* RAM disk does not exist */
#define _HDEAD 0xBA    /* File handle has been deleted */
#define _ISBFN 0xB8    /* Invalid sub-function number */

/* MSX_DOS function calls */
#define _FFIRST 0x40
#define _FNEXT  0x41
#define _OPEN   0x43
#define _CLOSE  0x45
#define _READ   0x48
#define _SEEK   0x4a
#define _TERM   0x62
#define _GENV   0x6b

typedef struct
{
  uint8_t  jump;
  uint16_t routine_address;
} MAPPER_ROUTINE;

typedef enum
{
  ALL_SEG,            /* Allocate a 16k segment */
  FRE_SEG,         	  /* Free a 16k segment */
   RD_SEG,         	  /* Read byte from address A:HL to A */
   WR_SEG,         	  /* Write byte from E to address A:HL */
  CAL_SEG,         	  /* Inter-segment call.  Address in IYh:IX */
    CALLS,         	  /* Inter-segment call.  Address in line after the call instruction */
   PUT_PH,            /* Put segment into page (HL) */
   GET_PH,            /* Get current segment for page (HL) */
   PUT_P0,            /* Put segment into page 0 */
   GET_P0,            /* Get current segment for page 0 */
   PUT_P1,            /* Put segment into page 1 */
   GET_P1,            /* Get current segment for page 1 */
   PUT_P2,            /* Put segment into page 2 */
   GET_P2,            /* Get current segment for page 2 */
   PUT_P3,            /* Not supported since page-3 must never be changed.  Acts like a "NOP" if called */
   GET_P3,            /* Get current segment for page 3 */
} MAPPER_SUPPORT_ROUTINES;

typedef struct
{
  uint8_t  dummy;
  char     name[13];
  uint8_t  attributes[2];
  uint8_t  time_modified[2];
  uint8_t  date_modified[2];
  uint8_t  start_cluster[2];
  uint8_t  size[4];
  uint8_t  drive;
  uint8_t  internal[37];
} FILE_INFO_BLOCK;

typedef struct {
	uint8_t allocatedSegmentNumber;
	uint8_t slotAddressOfMapper;
	bool    carryFlag;
} SEGMENTSTATUS;

extern FILE_INFO_BLOCK g_file_info_block;

void dos_exit(uint8_t error);
bool dos_get_environment(const char* env, char* value);

int8_t dos_open(const char* name, uint8_t mode);
int8_t dos_close(uint8_t handle);

int16_t  dos_read(uint8_t handle, uint8_t* destination, uint16_t size);
uint32_t dos_lseek(int8_t handle, int32_t offset, uint8_t whence);

int8_t dos_find_first_entry(char *name);
int8_t dos_find_next_entry(void);

uint8_t dos_init_ram_mapper_info(void);

SEGMENTSTATUS* dos_allocate_segment(uint8_t segment_type, uint8_t slot_address);

uint8_t dos_get_p2(void);
void    dos_put_p2( uint8_t segment);
