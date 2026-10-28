#pragma once

/*
 * From ASMLIB - SDCC library for assembler and UNAPI interop v1.0 by Konamiman
 */

#include <stdint.h>

/* ---  Register detail levels  --- */

// This value tells which registers to pass in/out
// to the routine invoked by AsmCall, DosCall, BiosCall
// and UnapiCall.

typedef enum 
{
	REGS_NONE = 0,	//No registers at all
	REGS_AF = 1,	  //AF only
	REGS_MAIN = 2,	//AF, BC, DE, HL
	REGS_ALL = 3	  //AF, BC, DE, HL, IX, IY
} register_usage;


/* ---  Structure representing the Z80 registers  ---
        Registers can be accesses as:
        Signed or unsigned words (ex: regs.Words.HL, regs.UWords.HL)
        Bytes (ex: regs.Bytes.A)
        Flags (ex: regs.Flags.Z)
 */

typedef union 
{
	struct 
  {
    uint8_t F;
    uint8_t A;
    uint8_t C;
		uint8_t B;
		uint8_t E;
		uint8_t D;
		uint8_t L;
		uint8_t H;
		uint8_t IXl;
		uint8_t IXh;
		uint8_t IYl;
		uint8_t IYh;
  } Bytes;
	
  struct 
  {
	    int16_t AF;
	    int16_t BC;
	    int16_t DE;
	    int16_t HL;
	    int16_t IX;
	    int16_t IY;
  } Words;
	
  struct 
  {
	    uint16_t AF;
	    uint16_t BC;
	    uint16_t DE;
	    uint16_t HL;
	    uint16_t IX;
	    uint16_t IY;
  } UWords;
	
  struct 
  {
		uint8_t C:1;
		uint8_t N:1;
		uint8_t PV:1;
		uint8_t bit3:1;
		uint8_t H:1;
		uint8_t bit5:1;
		uint8_t Z:1;
		uint8_t S:1;
	} Flags;
} Z80_registers;

/* ---  Generic assembler interop functions  --- */

//* Invoke a generic assembler routine.
//  regs is used for both input and output registers of the routine.
//  Depending on the values of inRegistersDetail and outRegistersDetail,
//  not all the registers will be passed from regs to the routine
//  and/or copied back to regs when the routine returns.
extern void AsmCall(uint16_t address, Z80_registers* regs, register_usage inRegistersDetail, register_usage outRegistersDetail) __sdcccall(0);

//* Execute a DOS function call,
//  it is equivalent to invoking address 5 with function number in register C.
extern void DosCall(uint8_t function, Z80_registers* regs, register_usage inRegistersDetail, register_usage outRegistersDetail);

//* Invoke a MSX BIOS routine,
//  this is done via CALSLT to the slot specified in EXPTBL.
extern void BiosCall(uint16_t address, Z80_registers* regs, register_usage outRegistersDetail);
