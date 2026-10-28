/*
   Simplified printf for SDCC+Z80 based on code from Konamiman - www.konamiman.com

   Supported format specifiers:

   %d or %i: signed int
   %ud or %ui: unsigned int
   %x: hexadecimal int
   %c: character
   %s: string
   %%: a % character
*/

#include <stdarg.h>

extern void __uitoa(int val, char* buffer, char base);
extern void __itoa(int val, char* buffer, char base);

static int format_string(const char *fmt, va_list ap);

int printf_simple(const char *fmt, ...)
{
  va_list arg;
  va_start(arg, fmt);
  return format_string(fmt, arg);
}

static void do_char(char c) __naked
{
  c;

  __asm

  ld  e,a
  ld  c,#2
  jp  5

  __endasm;
}

#define do_char_inc(c) {do_char(c); count++;}

static int format_string(const char *fmt, va_list ap)
{
  char *fmtPnt;
  char base;
  char isUnsigned;
  char *strPnt;
  long val;
  static char buffer[16];
  char theChar;
  int count=0;

  fmtPnt = (char *)fmt;

  while((theChar = *fmtPnt)!=0)
  {
    isUnsigned = 0;
    base = 10;

    fmtPnt++;

    if(theChar != '%') {
      do_char_inc(theChar);
      continue;
    }

    theChar = *fmtPnt;
    fmtPnt++;

    if(theChar == 's')
    {
      strPnt = va_arg(ap, char *);
      while((theChar = *strPnt++) != 0) 
        do_char_inc(theChar);

      continue;
    } 

    if(theChar == 'c')
    {
      val = va_arg(ap, int);
      do_char_inc((char) val);

      continue;
    } 

    if(theChar == 'u') {
      isUnsigned = 1;
    }
    else if(theChar == 'x') {
      base = 16;
    }
    else if(theChar != 'd' && theChar != 'i') {
      do_char_inc(theChar);
      continue;
    }

    val = va_arg(ap, int);
    
    if(isUnsigned)
      __uitoa(val, buffer, base);
    else
      __itoa(val, buffer, base);

    strPnt = buffer;
    while((theChar = *strPnt++) != 0) 
      do_char_inc(theChar);
  }

  return count;
}
