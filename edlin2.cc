#include <stdio.h>
#include <string.h>
#include <sys/kbdscan.h>
#include <stdlib.h>

#ifdef WITH_CANNA
#  include <canna/jrkanji.h>
#endif

#include "smartptr.h"
#include "macros.h"
#include "Edlin.h"

#define TOP_CLEAN_STR "\x1b[s\x1b[H\x1b[K\x1b[u"
#define KEY(x) (0x100 | K_##x )

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

static void euc2jms(int c1,int c2,SmartPtr &dp)
{
  if( (c1 & 255) == 0x8E ){ /* 半角かな処理 */
    *dp++ = c2;
    return;
  }

  c1 &= 0x7F; c2 &= 0x7F;

  if( c1 & 1 ){
    c1 = (c1 >> 1 ) + 0x71;
    c2 += 0x1f;
    if( c2 >= 0x7f )
      c2++;
  }else{
    c1 = (c1 >> 1 ) + 0x70;
    c2 += 0x7e;
  }
  if( c1 > 0x9F )
    c1 += 0x40;

  *dp++ = c1;
  *dp++ = c2;
}

int Edlin2::getkey_with_cursor()
{
  int key;
  if( cursor_on == NULL ){
    fflush(fp);
    return ::getkey();
  }

  if( pos == len ){
    fprintf(fp," \b\x1b[%sm \b" , cursor_on );
    
    fflush(fp);
    key=::getkey();
    if( cursor_off != NULL)
      fprintf(fp,"\x1b[%sm \b",cursor_off);
  }else if( atrbuf[pos] == DBC1ST ){
    fprintf(fp,"\x1b[%sm%c%c\b\b" , cursor_on 
	    , strbuf[pos] , strbuf[pos+1] );
    
    fflush(fp);
    key=::getkey();
    if( cursor_off != NULL )
      fprintf(fp,"\x1b[%sm%c%c\b\b" , cursor_off 
	      , strbuf[pos] , strbuf[pos+1] );
  }else{
    fprintf(fp,"\x1b[%sm%c\b" , cursor_on , strbuf[pos] );
    
    fflush(fp);
    key=::getkey();
    
    if( cursor_off != NULL )
      fprintf(fp,"\x1b[%sm%c\b" , cursor_off , strbuf[pos] );
  }
  return key;
}

enum{ PREFIX = -1 };
#define CAN2NYA(c,n)  case CANNA_KEY_##c: *dp++=PREFIX;*dp++ = K_##n;break
#define NYA2CAN(n,c)  case KEY(n): key= CANNA_KEY_##c ; break

int Edlin2::canna_inited=0;

void Edlin2::canna_to_alnum()
{
#ifdef WITH_CANNA
  /* かんなが初期化されている時のみ「英数モード」へ戻す。*/
  if( canna_inited ){
    jrKanjiStatusWithValue ksv;
    unsigned char buffer[256];
    jrKanjiStatus ks;

    ksv.val = CANNA_MODE_AlphaMode;
    ksv.buffer = buffer;
    ksv.bytes_buffer = sizeof(buffer);
    ksv.ks = &ks;
    jrKanjiControl( 0 , KC_CHANGEMODE , (char*)&ksv );
  }
#endif
}

int Edlin2::option_canna=1;

int Edlin2::getkey()
{
#ifdef WITH_CANNA
  if( option_canna == 0 )
#endif
    return getkey_with_cursor();
#ifdef WITH_CANNA

  /* 前回の呼び出しで確定している文字列がある場合、
   * それらを順次、呼び出しの度に返す必要がある。
   */
  static char kakbuf[256];
  static int kakpos=0; /* この変数、メンバ変数にすべきかなぁ...やっぱり */

  if( kakpos > 0  &&  kakbuf[kakpos] != '\0' ){
    if( kakbuf[kakpos] == PREFIX ){
      kakpos++;
      return kakbuf[kakpos++] | 0x100;
    }else if( is_kanji(kakbuf[kakpos] & 255) ){
      int rc=(kakbuf[kakpos] << 8) | (kakbuf[kakpos+1] & 255 );
      kakpos += 2;
      return rc & 0xFFFF;
    }else{
      return kakbuf[kakpos++] & 0xFF;
    }
  }
  jrKanjiStatus status;

  char localbuf[256]="\0";
  
  /* 始めて呼び出された時に「かんな」を初期化する。*/
  if( canna_inited==0 ){
    char **warning;
    jrKanjiControl( 0 , KC_INITIALIZE , (char*)&warning );
    if( warning != NULL ){
      char buffer[256];
      SmartPtr dp(buffer,sizeof(buffer));
      for(const char *sp=*warning ; *sp != '\0' ; sp++ ){
	if( *sp & 0x80 ){
	  euc2jms( sp[0] , sp[1] , dp );
	  ++sp;
	}else{
	  *dp++ = *sp;
	}
      }
      message( "!!! %s !!!",buffer );
      sleep(2);
      cleanmsg();
      return ::getkey();
    }
    canna_inited = 1;
    jrKanjiControl(0,KC_SETMODEINFOSTYLE,(char*)1);
  }

  int use_top_line = 0;
  /* 「かんな」の変換ループ */
  for(;;){
    int orgkey,key;

    /* ローカルバッファが空の時はカーソルを表示して、
     * キー入力を行う */
    if( localbuf[0] == '\0' && cursor_on != NULL ){
      orgkey = getkey_with_cursor();
    }else{
      fflush(fp);
      orgkey=::getkey();
    }

    /* IME からの入力があった場合などは、即確定させて、
     * その文字列を確定バッファに放り込む */
    if( orgkey > 0x200 ){
      jrKanjiStatusWithValue ksv;
      ksv.ks = &status;
      ksv.buffer = (unsigned char *)kakbuf;
      ksv.bytes_buffer = sizeof(kakbuf);
      jrKanjiControl( 0 , KC_KAKUTEI , (char*)&ksv );
      
      kakbuf[ ksv.val   ] = orgkey >> 8;
      kakbuf[ ksv.val+1 ] = orgkey & 255;
      kakbuf[ ksv.val+2 ] = 0;
      
      cleanmsg();
      if( is_kanji(kakbuf[kakpos=0] & 0xFF ) ){
	int rc=(kakbuf[kakpos] << 8 )|(kakbuf[kakpos+1]& 255 );
	kakpos += 2;
	return rc & 0xFFFF;
      }else{
	return kakbuf[kakpos++] & 0xFF;
      }
    }

    /* ローカルバッファが空で、特殊キーが入力されたら、
     * そのキーコードをそのまま返す。*/
    if( localbuf[0] == '\0' && orgkey > 0xFF ){
      if( use_top_line )
	fputs(TOP_CLEAN_STR,stdout);
      return orgkey;
    }

    /* NYAOS式キーコードを CANNA式キーコードに変換する */
    switch( orgkey ){
      NYA2CAN(UP,Up);
      NYA2CAN(DOWN,Down);
      NYA2CAN(LEFT,Left);
      NYA2CAN(RIGHT,Right);
      NYA2CAN(INS,Insert);
      NYA2CAN(HOME,Home);
      NYA2CAN(END,End);
      NYA2CAN(F1,F1);
      NYA2CAN(CTRL_DOWN,Cntrl_Down);
      NYA2CAN(CTRL_UP,Cntrl_Up);
      NYA2CAN(CTRL_LEFT,Cntrl_Left);
      NYA2CAN(CTRL_RIGHT,Cntrl_Right);
      NYA2CAN(PAGEUP,PageUp);
      NYA2CAN(PAGEDOWN,PageDown);

    case 0x0A:
      key = 0x0d;	break;

    case KEY(DEL):
      key = 0x7F;	break;

    default:
      key = orgkey;	break;
    }

    /* 「かんな」にお任せ */
    char eucbuf[256];
    int kakutei=jrKanjiString( 0 , key , eucbuf ,sizeof(eucbuf),&status );

    /* 確定文字列の処理 */
    if( kakutei > 0 ){
      cleanmsg();
      
      SmartPtr dp(kakbuf,sizeof(kakbuf));
      for(int i=0 ; i<kakutei ; i++ ){
	if( eucbuf[i] & 0x80 ){

	  switch( 0xFF & eucbuf[i] ){
	    CAN2NYA(Up,UP);
	    CAN2NYA(Down,DOWN);
	    CAN2NYA(Left,LEFT);
	    CAN2NYA(Right,RIGHT);
	    CAN2NYA(Insert,INS);
	    CAN2NYA(Home,HOME);
	    CAN2NYA(End,END);
	    CAN2NYA(F1,F1);
	    CAN2NYA(Cntrl_Down,CTRL_DOWN);
	    CAN2NYA(Cntrl_Up,CTRL_UP);
	    CAN2NYA(Cntrl_Left,CTRL_LEFT);
	    CAN2NYA(Cntrl_Right,CTRL_RIGHT);
	    CAN2NYA(PageUp,PAGEUP);
	    CAN2NYA(PageDown,PAGEDOWN);

	  case 0xFF:
	    *dp++ = 0xFF;
	    *dp++ = 0xFF;
	    break;

	  case 0x7F:
	    *dp++ = 0xFF;
	    *dp++ = K_DEL;
	    break;
	    
	  default:
	    euc2jms(eucbuf[i],eucbuf[i+1],dp);
	    i++;
	    break;
	  }
	}else{
	  *dp++ = eucbuf[i];
	}
      }
      *dp = '\0';
      
      /* jrKanjiControl( 0 ,KC_FINALIZE , NULL ); */
      cleanmsg();
      if( kakbuf[kakpos=0] == PREFIX ){
	kakpos++;
	return kakbuf[kakpos++] | 0x100;
      }else if( is_kanji(kakbuf[kakpos=0] & 0xFF) ){
	int rc=(kakbuf[kakpos] << 8)|(kakbuf[kakpos+1] & 255 );
	kakpos += 2;
	return rc & 0xFFFF;
      }else{
	return kakbuf[kakpos++] & 0xFF;
      }
    }

    if( status.length <= 0 )
      continue;

    /* local echo */

    SmartPtr dp(localbuf,sizeof(localbuf));
    int quote=0;
    for(int i=0; i < status.length; i++ ){
      if( i== status.revPos ){
	if( cursor_on != NULL ){
	  *dp++ = '\x1b';
	  *dp++ = '[';
	  for(const char *sp=cursor_on ; *sp != '\0' ; sp++ )
	    *dp++ = *sp;
	  *dp++ = 'm';
	}else{
	  *dp++ = '<';
	}
	quote = 1;
      }else if( i == status.revPos + status.revLen ){
	if( cursor_off != NULL ){
	  *dp++ = '\x1b';
	  *dp++ = '[';
	  for( const char *sp=cursor_off ; *sp != '\0' ; sp++ )
	    *dp++ = *sp;
	  *dp++ = 'm';
	}else{
	  *dp++ = '>';
	}
	quote = 0;
      }

      if( status.echoStr[i] & 0x80 ){
	euc2jms( status.echoStr[i] , status.echoStr[i+1] , dp );
	i++;
      }else{
	*dp++ = status.echoStr[i];
      }
    }
    if( quote ){
      if( cursor_on != NULL ){
	*dp++ = '\x1b';
	*dp++ = '[';
	for(const char *sp=cursor_off ; *sp != '\0' ; sp++ )
	  *dp++ = *sp;
	*dp++ = 'm';
      }else{
	*dp++ = '>';
      }
    }
    *dp = '\0';
    message( "|%s|",localbuf);

    if(   (status.info & KanjiGLineInfo) != 0
       &&  status.gline.length > 0  &&  status.gline.line != NULL ){
      fputs("\x1b[s\x1b[H",stdout);
      int column=0;
      for(const unsigned char *p=status.gline.line ; *p != '\0' ; p++ ){
	if( column == status.gline.revPos ){
	  if( cursor_on != NULL ){
	    printf("\x1B[%sm",cursor_on );
	  }else{
	    putchar('<');
	  }
	}else if( column == status.gline.revPos + status.gline.revLen ){
	  if( cursor_off != NULL ){
	    printf("\x1B[%sm",cursor_off );
	  }else{
	    putchar('>');
	  }
	}
	if( *p & 0x80 ){
	  char buf[10];
	  SmartPtr tmp(buf,10);
	  euc2jms( p[0] , p[1] , tmp );
	  putchar( buf[0] );
	  putchar( buf[1] );
	  p++;
	  column += 2;
	}else{
	  putchar( *p );
	  column++;
	}
      }
      fputs("\x1b[K\x1b[u",stdout);
      use_top_line = 1;
    }else{
      fputs(TOP_CLEAN_STR,stdout);
      use_top_line = 0;
    }
  }
#endif
}
