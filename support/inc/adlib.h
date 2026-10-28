#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Parameters of each voice */
#define nbLocParam  14

#define prmKsl      0
#define prmMulti    1
#define prmFeedBack 2   /* use for opr. 0 only */
#define prmAttack   3
#define prmSustain  4
#define prmStaining 5   /* Sustaining ... */
#define prmDecay    6
#define prmRelease  7
#define prmLevel    8
#define prmAm       9
#define prmVib      10
#define prmKsr      11
#define prmFm       12  /* use for opr. 0 only */
#define prmWaveSel  13  /* wave select */

 /* Globals parameters */
#define prmAmDepth    14
#define prmVibDepth   15
#define prmNoteSel    16
#define prmPercussion 17

 /* Melodic voice numbers */
#define vMelo0    0
#define vMelo1    1
#define vMelo2    2
#define vMelo3    3
#define vMelo4    4
#define vMelo5    5
#define vMelo6    6
#define vMelo7    7
#define vMelo8    8

 /* Percussive voice numbers */
#define BD        6
#define SD        7
#define TOM       8
#define CYMB      9
#define HIHAT     10

#define NR_OF_SLOTS           18
#define NR_OF_OPERATORS       2
#define MAX_FM_VOICES         9
#define MAX_PERCUSSION_VOICES 5

#define MAX_VOICES  11
#define MAX_VOLUME  0x7f
#define MAX_PITCH   0x3fff
#define MID_PITCH   0x2000

#define MID_C       60                            /* MIDI standard mid C */
#define CHIP_MID_C  48			                      /* sound chip mid C */
#define NR_NOTES    96                            /* # of notes we can play on chip */

#define TOM_PITCH   24                            /* best frequency, in range of 0 to 95 */
#define TOM_TO_SD   7                             /* 7 half-tones between voice 7 & 8 */
#define SD_PITCH    (TOM_PITCH + TOM_TO_SD)

#define NR_STEP_PITCH   25                        /* 25 steps within a half-tone for pitch bend */
#define OCTAVE          12                        /* half-tone by octave */
#define ADLIB_OPER_LEN  13                        /* operator length */
#define ADLIB_INST_LEN  (ADLIB_OPER_LEN * 2 + 2)  /* modulator, carrier, mod/car wave select */

#define GetLocPrm(slot, prm) ( (uint8_t)g_paramSlot[slot][prm] )

/* Definition of the ELECTRIC-PIANO voice (opr0 & opr1) */
const uint8_t g_pianoParamsOp0[nbLocParam] =
	{ 1, 1, 3, 15, 5, 0, 1, 3, 15, 0, 0, 0, 1, 0 };
const uint8_t g_pianoParamsOp1[nbLocParam] =
	{ 0, 1, 1, 15, 7, 0, 2, 4,  0, 0, 0, 1, 0, 0 };

/* Definition of default percussive voices */
const uint8_t g_bdOpr0[nbLocParam] =
	{ 0,  0, 0, 10,  4, 0, 8, 12, 11, 0, 0, 0, 1, 0 };
const uint8_t g_bdOpr1[nbLocParam] =
	{ 0,  0, 0, 13,  4, 0, 6, 15,  0, 0, 0, 0, 1, 0 };
const uint8_t g_sdOpr[nbLocParam] =
	{ 0, 12, 0, 15, 11, 0, 8,  5,  0, 0, 0, 0, 0, 0 };
const uint8_t g_tomOpr[nbLocParam] =
	{ 0,  4, 0, 15, 11, 0, 7,  5,  0, 0, 0, 0, 0, 0 };
const uint8_t g_cymbOpr[nbLocParam] =
	{ 0,  1, 0, 15, 11, 0, 5,  5,  0, 0, 0, 0, 0, 0 };
const uint8_t g_hhOpr[nbLocParam] =
	{ 0,  1, 0, 15, 11, 0, 7,  5,  0, 0, 0, 0, 0, 0 };

/* Slot numbers as a function of the voice and the operator (melodic only) */
const uint8_t g_slotVoice[MAX_FM_VOICES][NR_OF_OPERATORS] = 
{
	{ 0, 3 },   /* voice 0 */
	{ 1, 4 },   /* 1 */
	{ 2, 5 },   /* 2 */
	{ 6, 9 },   /* 3 */
	{ 7, 10 },  /* 4 */
	{ 8, 11 },  /* 5 */
	{ 12, 15 }, /* 6 */
	{ 13, 16 }, /* 7 */
	{ 14, 17 }  /* 8 */
};

/* Slot numbers for the percussive voices. 0 indicates that there is only one slot */
const uint8_t g_slotPerc[MAX_PERCUSSION_VOICES][NR_OF_OPERATORS] = 
{
	{ 12, 15 },   /* Bass Drum: slot 12 and 15 */
	{ 16, 0 },    /* SD:        slot 16 */
	{ 14, 0 },    /* TOM:       slot 14 */
	{ 17, 0 },    /* TOP-CYM:   slot 17 */
	{ 13, 0 }     /* HH:        slot 13 */
};

/* This table gives the offset of each slot within the chip: offset = fn( slot) */
const uint8_t g_offsetSlot[NR_OF_SLOTS] = 
{
   0,  1,  2,  3,  4,  5,
   8,  9, 10, 11, 12, 13,
  16, 17, 18, 19, 20, 21
};

/* This table indicates if the slot is a modulator (0) or a carrier (1): opr = fn( slot) */
const uint8_t g_operSlot[NR_OF_SLOTS] = 
{
  0, 0, 0,    /*  1  2  3 */
  1, 1, 1,    /*  4  5  6 */
  0, 0, 0,    /*  7  8  9 */
  1, 1, 1,    /* 10 11 12 */
  0, 0, 0,    /* 13 14 15 */
  1, 1, 1,    /* 16 17 18 */
};

/* This table gives the voice number associated with each slot (melodic mode only):	voice = fn( slot) */
const uint8_t g_voiceSlot[NR_OF_SLOTS] = 
{
  0, 1, 2,
  0, 1, 2,
  3, 4, 5,
  3, 4, 5,
  6, 7, 8,
  6, 7, 8,
};

const uint8_t g_mod12[OCTAVE * 11] =
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

const uint8_t g_div12[OCTAVE * 8] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

const uint8_t g_percMasks[5] =
{
  0x10, 0x08, 0x04, 0x02, 0x01
};

uint8_t  g_pitchRange;              /* Pitch variation, half-tone [+1,+12] */
uint16_t g_pitchRangeStep;          /* == g_pitchRange * NR_STEP_PITCH */
uint8_t  g_modeWaveSel;             /* != 0 if used with the 'wave-select' parameters */

uint8_t g_percBits;                 /* Control bits of percussive voices */
uint8_t g_notePitch[MAX_VOICES];    /* Pitch of last note-on of each voice */
uint8_t g_voiceKeyOn[MAX_VOICES];   /* State of keyOn bit of each voice */

uint8_t g_amDepth;			/* Chip global parameters .. */
uint8_t g_vibDepth;			/* ... */
uint8_t g_noteSel;			/* ... */
uint8_t g_percussion;		/* Percussion mode parameter */

typedef uint8_t SLOT_PARAM;
SLOT_PARAM g_paramSlot[NR_OF_SLOTS][nbLocParam];	  /* All the parameters of slots */

uint8_t g_slotRelVolume[NR_OF_SLOTS];		            /* Relative volume of slots */

void set_percussion_mode(bool mode);
void set_wave_sel(bool state);
void set_pitch_range(uint8_t range);
void set_global_params(uint8_t amDepth, uint8_t vibDepth, uint8_t noteSel);

void set_voice_timbre(uint8_t voice, int16_t* paramArray);
void set_voice_volume(uint8_t voice, uint8_t volume);
void set_voice_pitch(uint8_t voice, uint16_t pitchBend);

void note_on(uint8_t voice, uint16_t pitch);
void note_off(uint8_t voice);
