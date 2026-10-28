/*
 * Convert ADPCM to PCM
 *
 * Original code by XelaSoft
 *
 * destination: DATA_SEGMENT_BASE
 * source     : READ_BUFFER
 * size       : READ_BUFFER_SIZE
 *
 */

#include <stdint.h>
#include "players/inc/player_interface.h"
#include "adpcm2pcm.h"

static uint16_t g_sample;
static uint16_t g_delta_n;

static const uint8_t g_factors[] = {0x39, 0x39, 0x39, 0x39, 0x4D, 0x66, 0x80, 0x99};

void adpcm2pcm_init(void)
{
    g_sample = 0x8000;
    g_delta_n = 0x7f;
}

void adpcm2pcm(void) __naked
{
     __asm

 .macro divhl2
     srl     h
     rr      l
 .endm

 .macro ld_de_hl
     ld      e,l
     ld      d,h
 .endm

 adp2pcm::
     ld      de,#DATA_SEGMENT_BASE
     ld      hl,#READ_BUFFER
     ld      bc,#READ_BUFFER_SIZE

 adp2pcm2::
     ld      a,(hl)          ; get ADPCM code
     rrca
     rrca
     rrca
     rrca                    ; put 1st ADPCM code in LSB
     exx                     ; save registers
     call    adpcm_pcm       ; convert into PCM
     exx
     add     #128
     ld      (de),a          ; and store in memory
     inc     de
     ld      a,(hl)          ; get ADPCM code again
     inc     hl
     exx
     call    adpcm_pcm       ; convert into PCM
     exx
     add     #128
     ld      (de),a          ; and store again
     inc     de
     dec     bc
     ld      a,b
     or      c
     jr      nz,adp2pcm2     ; continue with the remainder

     ret

 adpcm_pcm::
     and     #15
     ld      c,a             ; remember PCM code
     ld      hl,(_g_delta_n)
     ld_de_hl                ; DE := HL = delta_n (dn for short)
     divhl2                  ; HL = delta_n/2
     rra                     ; examine l1 (bit 0 of ADPCM code)
     jr      nc,.+3
     add     hl,de           ; HL = l1 * dn + dn/2
     divhl2                  ; HL = l1*dn/2 + dn/4
     rra                     ; examine l2 (bit 1 of ADPCM code)
     jr      nc,.+3
     add hl,de               ; HL = l2*dn + l1*dn/2 + dn/4
     divhl2                  ; HL = l2*dn/2 + l1*dn/4 + dn/8
     rra                     ; examine l3 (bit 2 of ADPCM code)
     jr      nc,.+3
     add     hl,de           ; HL = l3*dn + l2*dn/2 + l1*dn/4 + dn/8
     ld      de,(_g_sample)    ; DE = previous sample value
     and     #1              ; examine l4
     jr      nz,adpcm_pmin   ; negative ==> substract
     add     hl,de
     jp      nc,adpcm_p2     ; JP is faster then JR
     ld      hl,#0xffff      ; value was to large ==> take maximum
     jp      adpcm_p2
 adpcm_pmin:
     ex      de,hl           ; DE = (2*(l and 7)+1)*dn/8, HL = samp.
     sbc     hl,de
     jp      nc,adpcm_p2
     ld      hl,#0
 adpcm_p2:
     ld      (_g_sample),hl     ; store new sample value
     res     3,c             ; C = abs(l) = abs(ADPCM code)
     ld      hl,#_g_factors
     ld      b,#0
     add     hl,bc           ; HL = addr of correct factor
     ld      a,(hl)          ; A = f(l) * #40
     ld      h,b
     ld      l,b             ; HL = 0
     ld      de,(_g_delta_n)    ; start calculation of dn = dn*f(l)
     ld      b,#6            ; use 6 bit fixed point arithmetic
 adpcm_p3:
     rra
     jr      nc,.+3
     add     hl,de
     divhl2
     djnz    adpcm_p3        ; HL = delta_n * (f(l) and 63)
     rra
     jr      nc,.+3
     add hl,de               ; HL = delta_n * (f(l) and 127)
     rra
     jr      nc,.+4
     add     hl,de
     add     hl,de           ; HL = delta_n * f(l)
     ex      de,hl           ; DE = delta_n * f(l)
     ld      hl,#0xa000
     add     hl,de           ; DE < #6000 => this is allowed
     jr      c,adpcm_p4      ; error => DE > #6000 ==> to large
     ld      hl,#0xff81      ; - 127
     add     hl,de           ; check if DE < 127
     jp      c,adpcm_p5      ; no => everything ok
     ld      de,#127         ; use minimum value
     jp      adpcm_p5
 adpcm_p4:   
     ld      de,#0x6000      ; use maximum value
 adpcm_p5:   
     ld      (_g_delta_n),de   ; store new delta_n
     ld      a,(_g_sample+1)   ; A = msb of sample = PCM value
     ret    

     __endasm;
 }