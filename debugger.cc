#ifndef NDEBUG

#include <stdio.h>
#include <stdarg.h>
#include <sys/video.h>

void debugger(const char *s,...)
{
  char buffer[256];
  char vbuf[512];

  va_list varptr;
  va_start(varptr,s);
  vsprintf(buffer,s,varptr);
  va_end(varptr);
  
  const char *sp=buffer;
  char *dp=vbuf;
  int count=0;
  while( *sp != '\0' ){
    *dp++ = *sp++;
    *dp++ = 0xF0;
    count++;
  }
  v_putline( vbuf , 0 , 0 , count );
}

#endif
