#include <sys/kbdscan.h>
#include <stdlib.h>
#include <ctype.h>
#include <io.h>	/* for access() */

#include <canna/jrkanji.h>

#define INCL_DOSMODULEMGR
#include <os2.h>

#include "smartptr.h"
#include "macros.h"
#include "Edlin.h"
#include "nyaos.h"

#define TOP_CLEAN_STR "\x1b[s\x1b[H\x1b[K\x1b[u"
#define KEY(x) (0x100 | K_##x )

int option_honest = 0;

/* ~/.canna が存在すれば 0 さもなければ 1 */
static int access_home_canna()
{
  char fname[ FILENAME_MAX ];
  const char *home=getenv("HOME");
  if( home == NULL  || *home=='\0' )
    return 1;
  sprintf( fname , "%s/.canna" , home );
  return access( fname, 0);
}

/* %SCRIPTDRIVE%:/usr/local/canna/lib/default.canna が存在すれば 0 */
static int access_script_canna()
{
  static char fname[] = "?:/usr/local/canna/lib/default.canna";
  const char *drv=getenv("SCRIPTDRIVE");
  if( drv == NULL || *drv == '\0' )
    return -1;
  fname[ 0 ] = *drv;
  if( access( fname , 0 )==0 )
    return *drv;
  else
    return -1;
}

static void euc2sjis(const char *sp , char *dp )
{
  while( *sp != '\0' ){
    if( (*sp & 255) == 0x8E ){ /* 半角カナ */
      ++sp; /* Prefix文字を読みとばす */
      *dp++ = *sp++;
    }else if( *sp & 0x80 ){ /* 漢字 */
      int c1=*sp++ & 0x7F;
      int c2=*sp++ & 0x7F;
      
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
    }else{
      *dp++ = *sp++;
    }
  }
  *dp = '\0';
}

/* -------- CANNA Dynamic Load ------- */

static int (*DLL_jrKanjiString )(int,int,char*,int,jrKanjiStatus *) = 0;
static int (*DLL_jrKanjiControl)(int,int,char*) = 0;

static HMODULE module_handle;
static void release_canna()
{
  char **warning;
  (*DLL_jrKanjiControl)(0,KC_FINALIZE,(char*)&warning);
  if( warning  &&  option_honest ){
    while( *warning != NULL ){
      char buffer[256];
      euc2sjis( *warning++ , buffer );
      fputs( buffer , stderr);
      putc( '\n' , stderr );
    }
  }
  DosFreeModule(module_handle);
}

static int canna_loaded=0;
int canna_init()
{
  /* ------- DLL Loading ------ */

  UCHAR errmsg[100];

  if(   DosLoadModule(errmsg,sizeof(errmsg),(UCHAR*)"canna",&module_handle )
     || DosQueryProcAddr(  module_handle , 0 ,(UCHAR*)"jrKanjiString"
			 , (PFN*)&DLL_jrKanjiString )
     || DosQueryProcAddr(  module_handle , 0 , (UCHAR*)"jrKanjiControl"
			 , (PFN*)&DLL_jrKanjiControl )){
    return 1;
  }

  atexit(release_canna);
  fputs("canna.dll loaded.\n",stdout);  
  canna_loaded = 1;  

  /* ------ CANNA customize file ----- */

  char **warning;
  char buffer[FILENAME_MAX];

  { /* -- かんな初期化の際に /usr/local/canna/lib のあるドライブに移動する--
     * set cannya=ドライブ[,初期化ファイル]
     * --------------------------------------------------------------------*/
    
    char *dotcanna=getenv("CANNYA");
    int orgdrv = _getdrive();
    int drv;

    if( dotcanna != NULL  &&  *dotcanna != '\0' ){
      if( *dotcanna != ',' ){
	_chdrive( *dotcanna++ );
	if( *dotcanna == ':' )
	  ++dotcanna;
      }
      if( *dotcanna == ',' )
	(*DLL_jrKanjiControl)(0 , KC_SETINITFILENAME , ++dotcanna);
      
    }else if( access_home_canna() != 0 && (drv=access_script_canna()) != -1 ){
      /* ホームディレクトリに .canna が無くて、
       * %SCRIPTDRIVE%:/usr/local/canna/lib/default.canna
       * が存在する場合、ドライブを一次的に変更する。ああ、こそく...
       */
      
      _chdrive(drv);
    }
    (*DLL_jrKanjiControl)( 0 , KC_INITIALIZE , (char*)&warning );
    _chdrive(orgdrv);
  }
  
  if( warning != NULL ){
    for( ; *warning != NULL ; warning ++ ){
      char buffer[256];
      euc2sjis(*warning , buffer );
      fputs(buffer,stderr);
      putc('\n',stderr);
    }
    return 1;
  }
  /* (*DLL_jrKanjiControl)( 0 , KC_SETMODEINFOSTYLE , (char*)1 ); */

  return 0;
}

/* ------------------------------------- */

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

int Edlin2::getkey_with_cursor()
{
#if 0
  int key;
  if( cursor_on == NULL ){
#endif
    fflush(fp);
    return ::getkey();
#if 0
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
#endif
}

enum{ PREFIX = -1 };
#define CAN2NYA(c,n)  case CANNA_KEY_##c: *dp++=PREFIX;*dp++ = K_##n;break
#define NYA2CAN(n,c)  case KEY(n): key= CANNA_KEY_##c ; break

int Edlin2::canna_inited=0;

void Edlin2::canna_to_alnum()
{
  /* かんなが初期化されている時のみ「英数モード」へ戻す。*/
  if( canna_loaded ){
    jrKanjiStatusWithValue ksv;
    unsigned char buffer[256];
    jrKanjiStatus ks;

    ksv.val = CANNA_MODE_AlphaMode;
    ksv.buffer = buffer;
    ksv.bytes_buffer = sizeof(buffer);
    ksv.ks = &ks;
    (*DLL_jrKanjiControl)( 0 , KC_CHANGEMODE , (char*)&ksv );
  }
}

int Edlin2::option_canna=1;

int are_spaces(const char *s)
{
  while( *s != '\0' ){
    if( ! isspace(*s & 255) )
      return 0;
    ++s;
  }
  return 1;
}

/* message が NULL なら *mark を、さもなければ message の内容を dp にコピー */
static void copy_message( const char *message , int mark , SmartPtr &dp )
{
  if( message != NULL ){
    *dp++ = '\033';
    *dp++ = '[';
    while( *message != '\0' )
      *dp++ = *message++;
    *dp++ = 'm';
  }else{
    *dp++ = mark ;
  }
}

int Edlin2::print_henkan_koho( jrKanjiStatus &status , const char *mode_string )
{
  /* 何らかの表示を行ったら 文字数、さもなければ 0 を表示する。 */
  if(  (status.info & KanjiGLineInfo)==0
     || status.gline.length <= 0 || status.gline.line == NULL )
    return 0;

  int column=0;
  int standout=0;

  char *buffer=(char*)alloca(status.gline.length+10);
  SmartPtr dp(buffer,status.gline.length+10);

  for( const unsigned char *sp=status.gline.line; *sp != '\0' ; ++sp ){
      /* 反転部分の処理 */
    if( column == status.gline.revPos ){
      copy_message( cursor_on , '<' , dp );
      standout = 1;
    }else if( column == status.gline.revPos + status.gline.revLen ){
      copy_message( cursor_off , '>' , dp );
      standout = 0;
    }
#if 0
    if( *sp & 0x80 ){
      euc2jms( sp[0] , sp[1] , dp );
      sp++;
      column += 2;
    }else{
#endif
      *dp++ = *sp;
      column++;
#if 0
    }
#endif
  }
  if( standout )
    copy_message( cursor_off , '>' , dp );
  *dp = '\0';
  euc2sjis(buffer,buffer);

  if( mode_string == NULL )
    mode_string = "\0";

  bottom_message("%s%s", mode_string , buffer );
  return column;
}

int Edlin2::getkey()
{
  if( !canna_loaded )
    return getkey_with_cursor();

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

  static const unsigned char *mode_string=NULL;
  static char mode_buf[20];
  int use_bottom=0;

  char localbuf[256]="\0";

  if(   mode_string != NULL  &&  mode_string[0] != '\0'
     && ! are_spaces((const char*)mode_string) )
    bottom_message( "%s",mode_string );
  
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
    if( orgkey >= 0x200 ){
      jrKanjiStatusWithValue ksv;
      ksv.ks = &status;
      ksv.buffer = (unsigned char *)kakbuf;
      ksv.bytes_buffer = sizeof(kakbuf);

      if( (*DLL_jrKanjiControl)( 0 , KC_KAKUTEI , (char*)&ksv ) != -1 ){
	/* ksvの値は正常終了の時のみ、あてになるとする */
	kakbuf[ ksv.val   ] = orgkey >> 8;
	kakbuf[ ksv.val+1 ] = orgkey & 255;
	kakbuf[ ksv.val+2 ] = 0;
      }else{
	kakbuf[ 0 ] = orgkey >> 8;
	kakbuf[ 1 ] = orgkey & 255;
	kakbuf[ 2 ] = 0;
      }
      
      cleanmsg();
      if( is_kanji(kakbuf[kakpos=0] & 0xFF ) ){
	int rc=(kakbuf[kakpos] << 8 )|(kakbuf[kakpos+1]& 255 );
	kakpos += 2;
	return rc & 0xFFFF;
      }else{
	return kakbuf[kakpos++] & 0xFF;
      }
    }else if( (orgkey & 0x180) == 0x80 ){
      // 半角カナならば...(漢字の場合は、getkey がすでに2bytes化している。
      return orgkey & 0xFF;
    }

    /* ローカルバッファが空で、特殊キーが入力されたら、
     * そのキーコードをそのまま返す。*/
    if( localbuf[0] == '\0' && orgkey > 0xFF ){
      if( mode_string != NULL ){
	if( !are_spaces(mode_string) )
	  bottom_message( "%s",mode_string );
      }else{
	clean_bottom();
      }
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
    int kakutei=(*DLL_jrKanjiString)(0 ,key ,eucbuf ,sizeof(eucbuf),&status );

    /* 何らかのエラーが発生した時は、元のキーをそのまま返す。*/
    if( kakutei == -1 )
      return orgkey;
    
    /* モードが変更されているならば、それを表示する */
    if( status.info & KanjiModeInfo ){
      euc2sjis( (const char *)status.mode ,mode_buf);
      bottom_message( "%s", mode_string = (unsigned char*)mode_buf );
    }

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
	    {
	      char tinybuf[3]={ eucbuf[i] , eucbuf[i+1] , 0 };
	      euc2sjis( tinybuf , tinybuf );
	      *dp++ = tinybuf[0];
	      if( tinybuf[1] != '\0' )
		*dp++ = tinybuf[1];
	    }
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
    
    if( status.length <= 0 ){
      cleanmsg();
      continue;
    }

    /* local echo */

    SmartPtr dp(localbuf,sizeof(localbuf));
    int quote=0;
    for(int i=0; i < status.length; i++ ){
      if( i== status.revPos ){
	copy_message( cursor_on , '<' , dp );
	quote = 1;
      }else if( i == status.revPos + status.revLen ){
	copy_message( cursor_off , '>' , dp );
	quote = 0;
      }
#if 0
      if( status.echoStr[i] & 0x80 ){
	euc2jms( status.echoStr[i] , status.echoStr[i+1] , dp );
	i++;
      }else{
#endif
	*dp++ = status.echoStr[i];
#if 0
      }
#endif
    }
    if( quote )
      copy_message( cursor_off , '>' , dp );

    *dp = '\0';
    euc2sjis( localbuf , localbuf );
    message( "|%s|",localbuf);
    
    /* 画面最下段に、変換候補などを表示する */
    if( print_henkan_koho(status,(char*)mode_string) <= 0  &&  use_bottom ){
      if( mode_string != NULL  ){
	bottom_message("%s" , mode_string);
      }else{
	clean_bottom();
      }
      use_bottom=0;
    }else{
      use_bottom=1;
    }
  }/* 「かんな」の変換ループ */
}
