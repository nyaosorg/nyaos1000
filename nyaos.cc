#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <sys/video.h>

#define USE_SET_WIN_TITLE 0

#define INCL_VIO
#define INCL_WIN
#define INCL_RXSUBCO
#define INCL_RXFUNC
#define INCL_DOSPROCESS

#include <os2.h>

#include "edlin.h"
#include "nyaos.h"
#include "complete.h"
#include "smartptr.h"
#include "errmsg.h"
#include "prompt.h"
#include "heapptr.h"
#include "shared.h"
#include "remote.h"

HAB   hab, hmq;

int prompt_myself=1;
int screen_width=80;
int screen_height=25;
int option_vio_cursor_control=1;
int option_prompt_even_piped=1;
int cursor_start;
int cursor_end;
char *cursor_on_color_str=NULL;
char *cursor_off_color_str=NULL;
int option_nyaos_rc=1;
int option_cmdlike_crlf=0;
int option_icanna=0;

char comspec[128]="COMSPEC=";
char *cmdexe_path=comspec+8;

int execute_result=0;
extern int printexitvalue;
int option_noclobber=0;
char *check_redirect_to_exist_file(const char *sp);
extern int killAllPublicHistory(void);

int do_rexx( const char *progname , LONG argc , RXSTRING *rx_argv );

/* 「VER」コマンド： 
 * nyaos.cc さえ再コンパイルすれば、NYAOS で表示される全ての
 * バージョンナンバが更新されるよう、あえてここに置いている。
 */
int cmd_ver( FILE *source , Parse &argv )
{
  fputs("Nihongo Yet Another Os/2 Shell is "VERSION
#ifdef S2NYAOS
	"\n (static linked version)"
#endif
	"\ncompiled on "__DATE__
	, stdout );
  fflush(stdout);
  return RC_HOOK;
}


/* VIOプログラムから、内部的に PM アプリケーションに化ける
 * これによって、PM のクリップボードの読み書きを可能とする。
 */
static int pretend_pm_application()
{
  PTIB  ptib = NULL;
  PPIB  ppib = NULL;
  APIRET rc = DosGetInfoBlocks(&ptib, &ppib);
  if (rc != 0)
    return rc;
  ppib->pib_ultype = PROG_PM;
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);
  if (hmq == NULLHANDLE)
    return rc;
  return 0;
}

/* 環境変数より、画面サイズを取得する。
 * 取得できなかった場合は、80x25 となる。
 *	wh[0] 桁数
 *	wh[1] 行数
 */
static void get_scrsize_with_env(int *wh)
{
  const char *env;

  env=getShellEnv("COLUMNS");
  if( env==NULL || (wh[0]=atoi(env)) <= 1 ) 
    wh[0] = 80;

  env=getShellEnv("LINES");
  if( env==NULL || (wh[1]=atoi(env)) <= 1 )
    wh[1] = 25;
}

/* ストリーム f の画面サイズを取得する。
 * f がファイルの場合は、環境変数より取得、
 * 端末の場合は、_scrsize関数を用いて取得する。
 *
 *	wh[0] 桁数
 *	wh[1] 行数
 *	f ストリーム
 */
void get_scrsize(int *wh,FILE *f)
{
  if( f==0 )
    f=stdout;
  
  if( !isatty(fileno(f)) ){
    /* ファイル出力ならば、環境変数だけを頼りにする */
    get_scrsize_with_env(wh);
  }else{
    /* 端末ならば、_scrsize関数を使ってみる。*/
    _scrsize(wh);
    if( wh[0] <= 1  ||  wh[1] <= 1 )
      get_scrsize_with_env(wh);
  }
}

/* fgets と基本は同じ。ただし
 *	・末尾の「\n」を読み込まない。
 *	・「^\n」は無視する(行継続文字だとみなす)
 */
char *fgets_chop(char *dp, int max, FILE *fp)
{
  bool kanji=false;    /* 前のcharが、漢字の第1バイトだった。*/
  int ch=0;

  for(;;){
    if( max-- <= 1  ||  (ch=getc(fp)) == EOF ){
      *dp = '\0';
      return NULL;
    }
    if( ch == '\n' )
      break;

    if( ch == '^'  &&  !kanji  ){
      if( (ch=getc(fp) == '\n' ) ){
	ch = getc(fp); /* そのまま「^\n」を無視する。
			* 「^\n^\n」と連続すると対応できないが、
			* そんなことする人間はいないだろう…多分 */
      }else{
	ungetc( ch , fp );
	ch = '^';
      }
    }
    if( ch==EOF ){
      *dp = '\0';
      return NULL;
    }
    if( kanji ){
      kanji = false;
    }else if( is_kanji(ch) ){
      kanji = true;
    }
    *dp++ = ch;
  }
  *dp = '\0';
  return dp;
}

#if USE_SET_WIN_TITLE
/* フラグ : FCF_TASKLIST が VIO ウインドウで立っている場合、
 * 「NYAOS.EXE」の代わりに set_win_title の引数がウインドウタイトルになる。
 * あいにく「start nyaos.exe」で起動した時か、アイコンにタイトルが無い
 * 時しか、FCF_TASKLIST は立たない。
 */
extern "C" void _THUNK_C_FUNCTION(WinSetTitle)(PSZ szTITLE);

void set_win_title( const char *title )
{
  _THUNK_C_PROLOG ( 4 );
  _THUNK_C_FLAT ( const_cast<char*>(title) );
  _THUNK_C_CALL ( WinSetTitle );
}

#endif

static void nyaosAtExit()
{
  const char *envAtExit=getShellEnv("ATEXIT");
  if( envAtExit != NULL ){
    execute(stdin,envAtExit);
  }else{
    fputs("\nGood bye!\n",stdout);
  }
}

bool option_quite_mode=0;	/* ロゴを表示しない */

int main(int argc, char **argv)
{
  if( _osmode != OS2_MODE ){
    ErrMsg::say( ErrMsg::NotSupportVDM , "nyaos",0 );
    return -1;
  }
#if USE_SET_WIN_TITLE
  set_win_title( "Nihongo Yet Another Os/2 Shell "VERSION );
#endif
  
  // ---- DBCS table の初期化 ----

  if( dbcs_table_init() != 0 ){
    ErrMsg::say( ErrMsg::DBCStableCantGet , "nyaos" , 0 );
    return -1;
  }

  resetCWD();				/* シェル変数 CWD にカレント
					 * ディレクトリを設定する */
  setvbuf(stdout,NULL,_IOLBF,BUFSIZ);	/* 画面表示は改行するまで
					 * バッファリングする。*/
  Shell::bindkey_nyaos();		/* キーバインドを設定する */

  // ----------------------------------------
  // COMSPEC に、NYAOS自身が設定されていると、
  // 動作がおかしくなるので、
  // CMD.EXE に切り換えさせる。 
  // ----------------------------------------
  if( SearchEnv("CMD.EXE","PATH",cmdexe_path) == 0 ){
    ErrMsg::say( ErrMsg::WhereIsCmdExe , "nyaos" , 0 );
    return -1;
  }
  putenv(comspec);

  // -------- オプション分析 ----------

  int warning_mode=0;	/* 警告あり：!0 で画面をクリアしない */

  for(int i=1;i<argc;i++){
    if( argv[i][0] == '-' || argv[i][0] == '/' ){
      switch(argv[i][1]){
      default:
	ErrMsg::say(ErrMsg::UnknownOption , argv[i] , 0 );
	warning_mode = 1;
	break;

      case 'z':
      case 'Z': /* Vz ライクキーモード */
	Shell::bindkey_wordstar();
	break;

      case 'g': /* ウインドウサイズ指定 */
      case 'G':
	{
	  const char *p;

	  if( argv[i][2] != '\0' ){
	    p = &argv[i][2];
	  }else if( i+1 < argc ){
	    p = argv[++i];
	  }else{
	    ErrMsg::say(ErrMsg::TooFewArguments,"nyaos -g",0);
	    warning_mode = 1;
	    break;
	  }

	  int x=0,y=0;

	  while( *p != '\0' && isdigit(*p & 255) )
	    x = x*10 + (*p++ -'0');

	  if( (*p != 'x' && *p != 'X' ) || x<80 || x>200 ){
	    ErrMsg::say(ErrMsg::BadParameter,"nyaos -g",argv[i],0);
	    return 1;
	  }
	  ++p;
	  while( *p !='\0' && isdigit(*p & 255) )
	    y = y*10 + (*p++ -'0');

	  if( y<20 || y>100 ){
	    ErrMsg::say(ErrMsg::BadParameter,"nyaos -g",argv[i],0);
	    return 2;
	  }
	  
	  char buffer[40];
	  sprintf(buffer,"co%d,%d", screen_width=x , screen_height=y );
	  spawnlp(P_WAIT,"CMD.EXE","CMD.EXE","/C","MODE",buffer,NULL);

	  static char env_columns[20];
	  sprintf(env_columns,"COLUMNS=%d",x);
	  putenv(env_columns);

	  static char env_lines[20];
	  sprintf(env_lines,"LINES=%d",y);
	  putenv(env_lines);
	}
	break;

      case 'h':
      case 'H':
	if( i+1 < argc ){
	  char *home;
	  int len=strlen(argv[++i]);
	  if( (home = new char[len+7]) == NULL ){
	    perror( argv[0] );
	    return -1;
	  }
	  sprintf( home , "HOME=%s" , argv[i] );
	  putenv( home );
	}
	break;
	
      case 'C':
      case 'c':
      case 'K':
      case 'k':
      case 'e':
	if( i+1 < argc ){
	  char oneline[1024];
	  SmartPtr dp(oneline,sizeof(oneline));
	  try{
	    for(int j=i+1;;){
	      int quote=0;
	      
	      for(const char *p=argv[j] ; *p != '\0' ; p++ ){
		if( isspace(*p & 255) || *p == '^' || *p == '!' )
		  quote = 1;
	      }
	      if( quote ) *dp++ = '"';
	      
	      for( const char *sp=argv[j] ; *sp != '\0' ; sp++ )
		*dp++ = *sp;
	      
	      if( quote ) *dp++ = '"';
	      
	      if( ++j >= argc )  break;
	      *dp++ = ' ';
	    }
	    *dp = '\0';
	  }catch( SmartPtr::BorderOut ){
	    dp.terminate();
	  }

	  int rc=execute(stdin,oneline);
	  if( argv[i][1] == 'c' || argv[i][1] == 'C' || argv[i][1] == 'e' )
	    return rc;
	}
	goto end_argv;
	
      case 'f':
	option_nyaos_rc = 0;
	break;

      case 'q':
	option_quite_mode = 1;
	break;

      case '-':
	{
	  extern int set_option(const char *name,int flag);
	  
	  const char *sp=&argv[i][2];
	  char buffer[40],*dp=buffer;
	  int flag=1;
	  while( *sp != '\0' &&  dp < buffer+sizeof(buffer)-1 ){
	    if( *sp == '-' ){
	      flag = 0;
	      break;
	    }else if( *sp == '+' ){
	      break;
	    }
	    *dp++ = *sp++;
	  }
	  *dp = '\0';
	  
	  if( set_option(buffer,flag) != 0 ){
	    ErrMsg::say(ErrMsg::UnknownOption,"nyaos" , buffer , 0 );
	    warning_mode = 1;
	  }
	}
	break;
      }
    }else{
      if( changeDir(argv[i]) != 0 ){
	ErrMsg::say(ErrMsg::BadParameter,argv[0],argv[i],0);
	return -1;
      }
    }
  }
  pretend_pm_application();

  if( isatty(fileno(stdin)) && !option_quite_mode ){
    extern int get_current_cp(void);
    
    if( ! warning_mode )
      fputs("\x1b[2J\x1b[1m",stdout);

    int cp=get_current_cp();
    if( cp==932 || cp==942 || cp==943 ){
      // Japanese Message.
      fputs("\n  ┏┓┳┳  ┳┏━┓┏━┓┏━┓" 
	    "\n  ┃┃┃┗━┫┣━┫┃  ┃┗━┓"
	    "\n  ┻┗┛┗━┛┻　┻┗━┛┗━┛"
	    ,stdout );
    }else{
      // English Message.
      fputs("\n   //  // //  //  ////   ////   //// "
	    "\n  /// // ////// //  // //  // ///    "
	    "\n // ///    /// ////// //  //    ///  "
	    "\n//  //  ////  //  //  ////  /////    "
	    ,stdout);
    }
    
    fputs("\n     The Open Source Software     "
	  "\n- Nihongo Yet Another Os/2 Shell -"
	  "\n    1996-2001 (c) HAYAMA,Kaoru  "
#ifdef S2NYAOS
	  "\n   Static linked version "VERSION
	  "\n     compiled on "__DATE__
#else
	  "\n Ver."VERSION" compiled on "__DATE__
#endif
	  "\n\n\x1b[0m"
	  , stdout );
  }

 end_argv:
  if( option_nyaos_rc ){
    char buffer[FILENAME_MAX];
    char *path=NULL;
    const char *home;

    if( access(".nyaos",0)==0 ){
      path = ".nyaos";
    }else if( access("_nyaos",0)==0 ){
      path = "_nyaos";
    }else if( access("nyaos.rc",0)==0 ){
      path = "nyaos.rc";
    }else if( (home=getShellEnv("HOME"))!=NULL ){
      char *dp = path = buffer;
      int lastchar=0;
      
      while( *home != '\0' ){
	if( is_kanji(lastchar=*home) )
	  *dp++ = *home++;
	*dp++ = *home++;
      }
      if( lastchar != '\\' && lastchar != '/' && lastchar != ':' )
	*dp++ = '\\';
      
      strcpy(dp,".nyaos");
      if( access(buffer,0) != 0 ){
	strcpy(dp,"_nyaos");
	if( access( buffer,0) != 0 ){
	  strcpy(dp,"nyaos.rc");
	  if( access(buffer,0) != 0 )
	    path = NULL;
	}
      }
    }

    /* ----------------------------------------------------------------
     * 「.nyaos」の処理
     * ----------------------------------------------------------------
     */
    FILE *fp;
    if(  path != NULL  && (fp=fopen(path,"r")) != NULL ){
      char buffer[256];
      const char *rc=fgets_chop(buffer,sizeof(buffer),fp);

      if( rc != NULL && buffer[0]=='/' && buffer[1]=='*' ){
	// REXX モード
	fclose(fp);
	
	RXSTRING rx_argv[1];
	MAKERXSTRING(rx_argv[0],"-",1);
	do_rexx( path , 1 , rx_argv );
      }else{
	// コマンドモード
	while( rc != NULL ){
	  if( execute(fp,buffer) == RC_QUIT )
	    break;
	  rc = fgets_chop(buffer,sizeof(buffer),fp);
	}
	fclose(fp);
      }
    }
  }

  if( option_vio_cursor_control )
    v_init();

  /* ----------------------------------------------------------------
   * 標準入力が、リダイレクトされている場合の処理(ここで完結)
   * ----------------------------------------------------------------
   */
  if( ! isatty(fileno(stdin)) ){
    for(;;){
      if( option_prompt_even_piped ){
	/* Mule 中から、NYAOS を利用する場合は、
	 * パイプされている場合でも、プロンプトを表示させなくては
	 * いけない */

	const char *promptenv=getShellEnv("NYAOSPROMPT");
	if( promptenv==NULL && (promptenv=getShellEnv("PROMPT")) == NULL )
	  promptenv = "$p$g";

	Prompt prompt( promptenv );
	fputs( prompt.get2() , stdout );
	fflush(stdout);
      }
      char cmdlin[1024]="";
      if( fgets_chop(cmdlin,sizeof(cmdlin),stdin) == NULL 
	 || execute(stdin,cmdlin) == RC_QUIT )
	return 0;
    }
  }

  killAllPublicHistory();

  if( option_icanna == 0 ){
    extern int canna_init();
    canna_init();
  }

  Shell shell;
  if( !shell ){
    ErrMsg::say(ErrMsg::MemoryAllocationError,"nyaos",0);
    return -1;
  }

  // ======================== コマンド毎のループ =========================
  for(;;){
    /* プロンプト文字列の作成 */
    
    const char *promptenv=getShellEnv("NYAOSPROMPT");
    if( promptenv==NULL && (promptenv=getShellEnv("PROMPT")) == NULL )
      promptenv = "$p$g";
    
    /* プロンプト文字列に、最上段を使用するものがあれば、
     * シェル(Shell)に、その使用を禁止させる。*/

    Prompt prompt( promptenv );
    if( prompt.isTopUsed() )
      shell.forbid_use_topline();
    else
      shell.allow_use_topline();

    /* カーソルを BOX 型にする。
     */
    if( option_vio_cursor_control ){
      VIOCURSORINFO info;

      if( shell.isOverWrite() ){ /* 上書きモードの時は半分サイズ */
	info.yStart = (unsigned short)-50;
      }else{			 /* 挿入モードの時はフルサイズ */
	info.yStart = 0;
      }
      info.cEnd   = (unsigned short)-100;
      info.cx = 0;
      info.attr = 0;
      
      VioSetCurType( &info , 0 );
    }
    
    /* 一行入力 */
    const char *top;

    // pretend_pm_application();
    int rc = shell.line_input(prompt.get2() ,">",&top);
    
    // ============== コマンドの実行 ====================

    // ---------------------------------------------------------
    // コマンドを実行し、「終了」の帰り値だったら、終了する。
    // 実行は、高機能system である execute がよしなにしてくれる。
    // ---------------------------------------------------------
    
    if( rc > 0 ){
      putchar('\n');
      while( *top != '\0' && is_space(*top) )
	++top;
      if( top[0] != '\0' ){
	execute_result = execute(stdin,top);
	
	if( execute_result == RC_QUIT ){
	  // --- exitコマンドなどによる終了 ----
	  nyaosAtExit();
	  return 0;
	}
	if( option_cmdlike_crlf )
	  putchar('\n');
	
	if( execute_result != 0 && printexitvalue !=0 ){
	  printf("Exit %i\n",execute_result);
	}
      }else{
	execute_result = 0;
      }
    }else{
      switch( rc ){
      case Edlin::QUIT:
	// ---- CTRL-Z などによる終了 ----
	nyaosAtExit();
	return 0;

      case RC_ABORT:
      case Edlin::ABORT:
	fputs("^C\n",stdout);
	break;

      case 0:
	putchar('\n');
	break;
	
      default:
	fputs("\nUnknown error occuerd.\n"
	      "Please mail to iyamatta.hayama@nifty.ne.jp\n"
	      , stdout );
	break;
      }
    }
  }// ============ コマンド毎のループの末尾 ===========
}
