/*
 * Frequency to key conversion routine used from VGMPlay by Grauw
 *
 */

#include "opm_frequency2key.h"

uint8_t g_key_code;
uint8_t g_key_fraction;

void opm_frequency2key(uint8_t msb, uint8_t lsb) __naked
{
    msb;
    lsb;

    __asm

    ld      h, a
    call    block_fnum_to_float
    call    math_log2
    ld      de,#31941
    call    offset_OPN_to_OPM
    call    to_key_code_fraction
    ld      a,b;                  /* Key code */
    ld      (_g_key_code),a
    ld      a,c;                  /* Key fraction */
    ld      (_g_key_fraction),a

    ret

block_fnum_to_float:
    xor     a
    add     hl,hl
    add     hl,hl
    add     hl,hl
    rla
    add     hl,hl
    rla
    add     hl,hl
    rla
    ld      b,a
    ret
    
math_log2:
    and     a
    adc     hl,hl
    jr      c,look_up
    jr      z,negative_infinity
leading_zeroes_loop:
    dec     b
    add     hl,hl
    jr nc,leading_zeroes_loop
look_up:
    ld      a,l
    srl h
    rra
    sla     h
    ld      l,h
    ld      h,#0xC0                 /* Start of Log2 table */
    ld      e,(hl)
    inc     l
    ld      d,(hl)
    inc     l
    ld      c,(hl)
    inc     l
    ld      h,(hl)
    ld      l,c
    sbc     hl,de
    ex      de,hl
interpolate:
    srl     d
    rr      e
    add     a,a
    jr      nc,no_add_half
    add     hl,de
no_add_half:
    srl     d
    rr      e
    add     a,a
    jr      nc,no_add_quarter
    add     hl,de
no_add_quarter:
    add     a,a
    ret     nc
    srl     d
    rr      e
    add     hl,de
    ret
negative_infinity:
    ld      b,#-16
    ret

offset_OPN_to_OPM:
    add     hl,de
    ret     nc
    inc     b
    ret    

to_key_code_fraction:
    ld      a,b
    cp      #8
    jr      nc,out_of_range
    ld      e,l
    ld      d,h
    srl     d
    rr      e
    srl     d
    rr      e
    sbc     hl,de  ; * 3/4
    add     hl,hl
    rl      b
    add     hl,hl
    rl      b
    add     hl,hl
    rl      b
    add     hl,hl
    rl      b
    ld      c,h
    ld      a,b  ; remap
    and     #0x0F
    cp      #03
    ret     c
    inc     b
    cp      #06
    ret     c
    inc     b
    cp      #09
    ret     c
    inc     b
    ret
out_of_range:
    jp p,overflow
underflow:
    ld      bc,#0
    ret
overflow:
    ld      bc,#0x7EFC
    ret

    __endasm;

}
