#include <stdio.h>
#include <string.h>
#include <sys/kbdscan.h>
#include <sys/nls.h> /* for getkey() */
#include "Edlin.h"

void EscEdlin::putchr(int c)
{
  putc(c,fp);
}

void EscEdlin::putel()
{
  fputs( "\x1B[K" , fp );
}

void EscEdlin::putbs(int n)
{
  /* BackSpaceならば、前の行へも移動できる。*/
  while( n-- > 0 ){
    putc('\b',fp);
  }
}

static int get_key(int wait)
{
  extern int get86key(int wait);

  int ch = (get86key(wait) & 0xFF );
  if( ch == 0 )
    ch = (get86key(wait)|0x100);
  else if( _nls_is_dbcs_lead(ch & 255) )
    ch = ((ch << 8)|(get86key(wait) & 0xFF));
  return ch;
}

int EscEdlin::getkey(int wait)
{
  if( cursor_on == NULL ){
    fflush(fp);
    return get_key(wait);
  }

  int ch;

  if( pos == len ){
    fprintf(fp," \b\x1b[%sm \b" , cursor_on );

    fflush(fp);
    ch=get_key(wait);
    
    fprintf(fp,"\x1b[%sm \b",cursor_off);
  }else if( atrbuf[pos] == DBC1ST ){
    fprintf(fp,"\x1b[%sm%c%c\b\b" , cursor_on , strbuf[pos] , strbuf[pos+1] );

    fflush(fp);
    ch=get_key(wait);

    fprintf(fp,"\x1b[%sm%c%c\b\b" , cursor_off , strbuf[pos] , strbuf[pos+1] );
  }else{
    fprintf(fp,"\x1b[%sm%c\b" , cursor_on , strbuf[pos] );

    fflush(fp);
    ch=get_key(wait);

    fprintf(fp,"\x1b[%sm%c\b" , cursor_off , strbuf[pos] );
  }
  return ch;
}
