/* -*- c++ -*-
 *
 * If you will compile NYAOS without CANNA library/header files ,
 * Please write `#define ICANNA' or add option -DICANNA to gcc.exe .
 *
 */

#undef DEBUG

#ifdef DEBUG
#  define DEBUG1(x) (x)
#else
#  define DEBUG1(x)
#endif

#ifdef ICANNA
#  define CANNA 0
#endif
#ifndef CANNA
#  define CANNA 1
#endif

enum{ TIMEOUT_SECOND = 5 };
#define CANNA_MODULE ((PUCHAR)"canna")
/* マルチスレッド化する場合、"cannamt" にしなくてはいけない */

typedef unsigned long u_long;

#include <setjmp.h>
#include <signal.h>
#include <netdb.h>
#include <sys/kbdscan.h>
#include <stdlib.h>
#include <ctype.h>
#include <io.h>	/* for access() */
#if CANNA
#  include <canna/jrkanji.h>
#endif

#define INCL_DOSMODULEMGR
#include <os2.h>

#include "strbuffer.h"
#include "heapptr.h"
#include "smartptr.h"
#include "macros.h"
#include "Edlin.h"
#include "nyaos.h"

#define TOP_CLEAN_STR "\x1b[s\x1b[H\x1b[K\x1b[u"
#define KEY(x) (0x100 | K_##x )

int option_honest = 0;

const char *getShellEnv(const char *);

#if CANNA
/* ~/.canna が存在すれば 0 さもなければ 1 */
static bool access_home_canna()
{
  char fname[ FILENAME_MAX ];
  const char *home=getShellEnv("HOME");
  if( home == NULL  || *home=='\0' )
    return true;
  sprintf( fname , "%s/.canna" , home );
  return access( fname, 0) != 0;
}

/* %SCRIPTDRIVE%:/usr/local/canna/lib/default.canna が存在すれば
 * そのドライブ番号、さもなければ、-1 を返す。
 */
static int access_script_canna()
{
  static char fname[] = "?:/usr/local/canna/lib/default.canna";
  const char *drv=getShellEnv("SCRIPTDRIVE");
  if( drv == NULL || *drv == '\0' )
    return -1;
  fname[ 0 ] = *drv;
  if( access( fname , 0 )==0 )
    return *drv;
  else
    return -1;
}

/* euc2sjis 系関数の下請け関数
 * EUC の漢字部分の 2bytes を SJIS の 2bytes に置き帰る。
 * 2bytes コードでない場合の動作は未定義
 * 
 * in	c1 EUC上位 1byte
 *	c2 EUC下位 1byte
 * out	jms_c1 SJIS上位 1byte
 *	jms_c2 SJIS下位 1byte (半角カナの場合、書かれない)
 * return
 *	1 : 半角カナ
 *	2 : それ以外
 */
static int euc2sjis_oneword(int c1,int c2,char &jms_c1,char &jms_c2)
{
  if( (c1 & 255) == 0x8E ){
    jms_c1 = c2;
    return 1;
  }
  c1 &= 0x7F;c2 &= 0x7F;

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
  
  jms_c1 = c1;
  jms_c2 = c2;
  return 2;
}

/* EUC文字列を ShiftJIS へ変換する。
 * in	sp EUC 文字列
 * out	dp ShiftJIS 文字列
 */
static void euc2sjis(const char *sp , char *dp )
{
  while( *sp != '\0' ){
    if( *sp & 0x80 ){ /* 2bytes コード */
      dp += euc2sjis_oneword(sp[0],sp[1],dp[0],dp[1]);
      sp += 2;
    }else{
      *dp++ = *sp++;
    }
  }
  *dp = '\0';
}

/* -------- CANNA Dynamic Load ------- */

static int (*DLL_jrKanjiString )(int,int,char*,int,jrKanjiStatus *) = 0;
static int (*DLL_jrKanjiControl)(int,int,const char*) = 0;

static HMODULE module_handle;
static int canna_loaded=0;

static void print_warning( char **warning )
{
  if( warning != NULL ){
    for( ; *warning != NULL ; warning++ ){
      char buffer[256];
      euc2sjis(*warning , buffer );
      fputs(buffer,stderr);
      putc('\n',stderr);
    }
  }
}
#endif

static jmp_buf here;
static void timeout(int)
{
  longjmp( here , 1 );
}

extern bool option_quite_mode;

int canna_init()
{
#if CANNA
  /* ホスト名を参照できない状況では、canna.dll 内で abnormal termination
   * を起こしてしまう。よって、canna.dll を呼び出す前に、ホスト名を参照
   * できるか否かをチェックしている */

  if( setjmp(here) == 0 ){
    char ownhost[40];
    
    signal( SIGALRM , &timeout );
    alarm( TIMEOUT_SECOND );
    gethostname(ownhost,sizeof(ownhost));
    struct hostent *hostinfo = gethostbyname(ownhost);
    if( hostinfo == NULL  ){
      alarm(0);
      return 0;
    }
  }else{
    alarm( 0 );
    return 0;
  }
  alarm(0);

  /* ------- DLL Loading ------ */

  UCHAR errmsg[100];

  if(   DosLoadModule(errmsg,sizeof(errmsg),CANNA_MODULE,&module_handle )
     || DosQueryProcAddr(  module_handle , 0 ,(UCHAR*)"jrKanjiString"
			 , (PFN*)&DLL_jrKanjiString )
     || DosQueryProcAddr(  module_handle , 0 , (UCHAR*)"jrKanjiControl"
			 , (PFN*)&DLL_jrKanjiControl )){
    return 1;
  }
  if( ! option_quite_mode )
    fputs("canna.dll loaded.\n",stdout);
  
  /* ------ CANNA customize file ----- */

  char **warning=NULL;
  /* -- かんな初期化の際に /usr/local/canna/lib のあるドライブに移動する--
   * set cannya=ドライブ[,初期化ファイル]
   * --------------------------------------------------------------------*/
  
  int orgdrv = _getdrive();
  int rc=0;
  
  if( setjmp(here) == 0 ){
    const char *dotcanna=getShellEnv("CANNYA");
    signal( SIGALRM , &timeout );
    alarm( TIMEOUT_SECOND );
    
    if( dotcanna != NULL  &&  *dotcanna != '\0' ){
      if( *dotcanna != ',' ){
	_chdrive( *dotcanna++ );
	if( *dotcanna == ':' )
	  ++dotcanna;
      }
      if( *dotcanna == ',' )
	(*DLL_jrKanjiControl)(0 , KC_SETINITFILENAME , ++dotcanna);
      
    }else if( access_home_canna() != 0  ){
      int drv=access_script_canna();
      if( drv != -1 ){
	/* ホームディレクトリに .canna が無くて、
	 * %SCRIPTDRIVE%:/usr/local/canna/lib/default.canna
	 * が存在する場合、ドライブを一次的に変更する。ああ、こそく...
	 */
	_chdrive(drv);
      }
    }
    rc=(*DLL_jrKanjiControl)( 0 , KC_INITIALIZE , (char*)&warning );
    alarm( 0 );
  }else{
    alarm( 0 );
    puts( "can not access to cannaserver in time." );
    _chdrive(orgdrv);
    return 0;
  }
  _chdrive(orgdrv);
  if( rc == -1 ){
    print_warning(warning);
    return 1;
  }

  if( warning != NULL ){
    print_warning(warning);
    return 1;
  }
  canna_loaded = 1;
#endif
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

enum{ PREFIX = -1 };
#define CAN2NYA(c,n)  case CANNA_KEY_##c: *dp++=PREFIX;*dp++ = K_##n;break
#define NYA2CAN(n,c)  case KEY(n): key= CANNA_KEY_##c ; break

int Edlin2::canna_inited=0;

void Edlin2::canna_to_alnum()
{
#if CANNA
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
#endif
}

#if CANNA
/* message が NULL なら *mark を、さもなければ message の内容を dp にコピー */
static void copy_message( const char *message , int mark , StrBuffer &buf )
{
  if( message != NULL && *message != '\0' ){
    buf << "\x033[" << (char)*message++ << 'm';
  }else{
    buf << (char)mark;
  }
}
#endif

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

#if CANNA
int Edlin2::print_henkan_koho( jrKanjiStatus &status ,const char *mode_string)
{
  /* 何らかの表示を行ったら 文字数、さもなければ 0 を表示する。 */
  if(  (status.info & KanjiGLineInfo)==0
     || status.gline.length <= 0 || status.gline.line == NULL )
    return 0;

  int column=0;
  int standout=0;

  StrBuffer buf;
  for( const unsigned char *sp=status.gline.line; *sp != '\0' ; ++sp ){
    /* 反転部分の処理 */
    if( column == status.gline.revPos ){
      copy_message( cursor_on , '<' , buf );
      standout = 1;
    }else if( column == status.gline.revPos + status.gline.revLen ){
      copy_message( cursor_off , '>' , buf );
      standout = 0;
    }
    buf << (char)*sp;
    column++;
  }
  if( standout )
    copy_message( cursor_off , '>' , buf );
  heapchar_t buffer(buf.finish());

  euc2sjis(buffer,buffer);

  if( mode_string == NULL )
    mode_string = "\0";

  bottom_message("%s%s", mode_string , (const char*)buffer );
  return column;
}
#endif

extern int option_icanna;

int Edlin2::getkey()
{
#if CANNA
  if( !canna_loaded || option_icanna ){
#endif
    fflush(fp);
    return ::getkey();
#if CANNA /* ================ CANNA ================ */
  }

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

  heapchar_t localbuf;

  if(   mode_string != NULL  &&  mode_string[0] != '\0'
     && ! are_spaces((const char*)mode_string) )
    bottom_message( "%s",mode_string );
  
  /* 「かんな」の変換ループ */
  for(;;){
    int orgkey,key;

    /* ローカルバッファが空の時はカーソルを表示して、
     * キー入力を行う */
    fflush(fp);
    orgkey=::getkey();

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
    if( localbuf == NULL  &&  orgkey > 0xFF ){
      if( mode_string != NULL ){
	if( !are_spaces((const char*)mode_string) )
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
      try{
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
	      dp += euc2sjis_oneword(eucbuf[i],eucbuf[i+1],dp[0],dp[1] );
	      ++i;
	      break;
	    }
	  }else{
	    *dp++ = eucbuf[i];
	  }
	}
	*dp = '\0';
      }catch(SmartPtr::BorderOut){
	dp.terminate();
      }
      
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

    StrBuffer buf;
    int quote=0;
    for(int i=0; i < status.length; i++ ){
      if( i== status.revPos ){
	copy_message( cursor_on , '<' , buf );
	quote = 1;
      }else if( i == status.revPos + status.revLen ){
	copy_message( cursor_off , '>' , buf );
	quote = 0;
      }
      buf << (char)status.echoStr[i];
    }
    if( quote )
      copy_message( cursor_off , '>' , buf );
    
    localbuf = buf.finish();
    euc2sjis( localbuf , localbuf );
    message( "|%s|",(const char*)localbuf);
    
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
#endif /* ================ CANNA ================ */
}
