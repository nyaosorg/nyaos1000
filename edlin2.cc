#include <stdio.h>
#include <string.h>
#include <sys/kbdscan.h>

#include "macros.h"
#include "Edlin.h"

void Edlin2::putchr(int c)
{
  putc(c,fp);
}

void Edlin2::putel()
{
  fputs( "\x1B[K" , fp );
}

void Edlin2::putbs(int n)
{
  /* BackSpaceならば、前の行へも移動できる。*/
  while( n-- > 0 )
    putc('\b',fp);
}

int Edlin2::getkey()
{
  if( cursor_on == NULL ){
    fflush(fp);
    return ::getkey();
  }

  int ch;
  
  if( pos == len ){
    fprintf(fp," \b\x1b[%sm \b" , cursor_on );

    fflush(fp);
    ch=::getkey();
    
    fprintf(fp,"\x1b[%sm \b",cursor_off);
  }else if( atrbuf[pos] == DBC1ST ){
    fprintf(fp,"\x1b[%sm%c%c\b\b" , cursor_on , strbuf[pos] , strbuf[pos+1] );

    fflush(fp);
    ch=::getkey();

    fprintf(fp,"\x1b[%sm%c%c\b\b" , cursor_off , strbuf[pos] , strbuf[pos+1] );
  }else{
    fprintf(fp,"\x1b[%sm%c\b" , cursor_on , strbuf[pos] );

    fflush(fp);
    ch=::getkey();

    fprintf(fp,"\x1b[%sm%c\b" , cursor_off , strbuf[pos] );
  }
  return ch;
}
