#include <assert.h>
#include <io.h>
#include <sys/types.h>

#include <string.h> /* for memset */
#include <stdlib.h> /* for _osmode */
#include <dos.h>    /* for int86 */

#include <sys/kbdscan.h> /* for translate CSI sequence */
#include <ctype.h>

#include <termios.h>
#include <termio.h>

#include "macros.h"

#define KEY(a)	(K_##a | 0x100)

int option_esc_key_sequences=0;
int option_direct_key=0;

#ifndef S2NYAOS
static int tty=-255;
static struct termios orig,s;
#endif

void raw_mode(void)
{
#ifndef S2NYAOS
  if( _osmode == DOS_MODE  || option_direct_key )
#endif
    return;
#ifndef S2NYAOS

  if( tty == -255 ){
    tcgetattr(tty=0,&s);
    orig = s;
  }else{
    tcgetattr(tty,&s);
  }

  /* 機能を off にする */
  s.c_iflag &= ~(  ISTRIP /* Clear bit 7 of all input characters */
		 | INLCR  /* Translate linefeed into carriage return */
		 | IGNCR  /* Ignore carrige return */
		 | ICRNL  /* Translate carriage return into linefeed */
		 | IUCLC  /* Convert upper case letters into lower case */
		 );

  /* 機能を off にする */
  s.c_lflag &= ~(  ICANON /* line editing */
		 | ECHO   /* echo input */
		 | ECHOK  /* echo CR/LF after VKILL */
		 | ISIG   /* Enable signal processing */
		 | ECHONL /* not support ? */
		 );
  /* emxでは、c_oflagがサポートされていない…互換性のためにヘッダだけは定義さ
   * れている…ので、以下のc_oflagを指定する必要性はない。→ 参照: termios (3)
   */
  s.c_oflag |=	(  TAB3	  /* expands tabs to spaces			     */
		 | OPOST  /* enable implementation-defined output processing */
		 | ONLCR  /* map NL to CR-NL on output			     */
		 );
  s.c_oflag &= ~(  OCRNL  /* map CR to NL on output	 */
		 | ONOCR  /* don't output CR at column 0 */
		 | ONLRET /* don't output CR             */
		 );

  /* 一文字単位で、0秒で反応させる */
  s.c_cc[VMIN] = 1;
  s.c_cc[VTIME] = 0;
  
  tcsetattr(tty,TCSADRAIN,&s);
#endif
}

void cocked_mode(void)
{
#ifndef S2NYAOS
  if( tty != -255 &&  _osmode != DOS_MODE  &&  option_direct_key==0 ){
    tcsetattr(tty,TCSANOW,&orig);
    tty = -255;
  }
#endif
}

int get86key(void)
{
#ifndef S2NYAOS
  if( _osmode == DOS_MODE ){
    /* DOSでは_read_kbdで、漢字の第二バイト目が取得できない。*/
    union REGS regs;
    regs.h.ah = 0x7;
    return _int86( 0x21, &regs , &regs ) & 0xFF ;
  }else if( option_direct_key ){
#endif
    return _read_kbd(0,1,0);
#ifndef S2NYAOS
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
#endif
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
  switch(ch){
  default: 
    if( is_kanji(ch) )
      ch = ((ch << 8)|(get86key() & 0xFF)) & 0xFFFF;
    break;

  case 0:  
    ch = (get86key()|0x100);
    break;
    
  case 27:
    if(!option_esc_key_sequences)
      return ch;	/* ← ディフォルトでは、ここで返る */

    /*
     * ESCキー・シーケンスをrawレベルに戻すのは、アルゴとして
     * 美しくないけど、手っとり早い (^^;
     */
    
    int par;
    switch(ch=get86key()){
    case '[':
      
      /*
       * CSI sequences
       */
      for(par=0;;){
	/*
	 * CSIパラメタの取得
	 */
	while(isdigit(ch=get86key()))
	  par=par*10+ch-'0';
	if(ch!=';')
	  break;
	/*
	 * 複数のパラメタがあったら、無効な値をセットして
	 * 無視させる
	 */
	par=500;
      }
      if(par<=1/*パラメタ1が1か省かれた時*/)
	switch(ch){
	case 'A':
	  ch=KEY(UP); break;
	case 'B':
	case 'e':
	  ch=KEY(DOWN);  break;
	case 'C':
	case 'a':
	  ch=KEY(RIGHT); break;
	case 'D': ch=KEY(LEFT);  break;
	case 'H':
	case 'f': ch=KEY(HOME);  break;
	case 'F': /* 本来のCSI Fと意味が違うが、実際に
		   * ENDキーを押してみると CSI Fが来ちゃう
		   */
	  ch=KEY(END);   break;
	default: ch=0;
	}
      else if(ch=='~')
	switch(par){
	case  2:
	  ch=KEY(INS); break;
	case  3: ch=KEY(DEL); break;
	  /*
	   * 他にもCSI n~で来るキーがいっぱいあるけど
	   * nyaosのために使うことは滅多にないだろうし
	   * 面倒くさいから無視
	   */
	default:
	  ch=0;
	}
      else
	ch=0; /* 上に該当しないCSIシーケンスを無視 */
      break;
      
    case '#': /* DEC test sequences			      */
    case '%': /* UTF control sequences		      */
    case '(': /* G0 sequences		 これらのシーケンスは */
    case ')': /* G1 sequences		 後に1文字が続く      */
      get86key();	/*	 それを無視する	      */
    default:
      ch=0;
      break;
    }
    break;
  }
  return ch & 0xFFFF;
}
