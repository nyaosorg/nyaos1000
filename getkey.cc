#include <assert.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h> /* for memset */
#include <stdlib.h> /* for _osmode */
#include <dos.h>    /* for int86 */

/* #include <sys/kbdscan.h> /*** for _read_kbd()         ***/

#include <termios.h>
#include <termio.h>

#include "macros.h"

static int tty=-255;
static struct termios orig,s;

void raw_mode(void)
{
  if( _osmode == DOS_MODE )
    return;

  if( tty == -255 ){
    tcgetattr(tty=2,&s);
    orig = s;
  }
  /* 機能を off にする */
  s.c_lflag &= ~(  ICANON /* line editing */
		 | ECHO   /* echo input */
		 | ECHOK  /* echo CR/LF after VKILL */
		 | ISIG   /* Enable signal processing */
		 | ECHONL /* not support ? */
		 );
  /* 以下の二行は謎 */
  s.c_oflag |=  ( TAB3 | OPOST | ONLCR );
  s.c_oflag &= ~( OCRNL | ONOCR | ONLRET );

  /* 一文字単位で、0秒で反応させる */
  s.c_cc[VMIN] = 1;
  s.c_cc[VTIME] = 0;
  
  tcsetattr(tty,TCSADRAIN,&s);
}

void cocked_mode(void)
{
  if( tty == 2  &&  _osmode != DOS_MODE )
    tcsetattr(tty,TCSADRAIN,&orig);
}

int get86key(void)
{
  if( _osmode == DOS_MODE ){
    /* DOSでは_read_kbdで、漢字の第二バイト目が取得できない。*/
    union REGS regs;
    regs.h.ah = 0x7;
    return _int86( 0x21, &regs , &regs ) & 0xFF ;
  }else{
    unsigned char key;
    int rc;

    enum{ READ_INTR = -2 };
    assert( tty != -255 );

    do{
      if( isatty( tty ) ){
	fd_set readfds;
	FD_ZERO( &readfds );
	FD_SET(tty,&readfds );
	if( select(tty+1,&readfds,NULL,NULL,NULL) == -1 )
	  return 0;
      }

      rc= read( tty , &key , sizeof(char) );
      if( rc < 0 )
	return 0;
    }while( rc != 1 );
    return key & 0xFF;
  }
  /* return _read_kbd(0,1,0); */
}

static int keybuf[16],left=0;

void ungetkey(int key)
{
  keybuf[ left++ ] = key;
}

int getkey(void)
{
  if( left > 0 )
    return keybuf[ --left ];

  int ch = (get86key() & 0xFF );
  if( ch == 0 )
    ch = (get86key()|0x100);
  else if( is_kanji(ch) )
    ch = ((ch << 8)|(get86key() & 0xFF));

  return ch;
}
