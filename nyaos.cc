#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <sys/video.h>

#define USE_SET_WIN_TITLE 0

#define INCL_VIO
#define INCL_WIN
#define INCL_DOSPROCESS

#include <os2.h>

#include "edlin.h"
#include "nyaos.h"
#include "complete.h"
#include "smartptr.h"

#define RED	"" /*"\x1B[31m"*/
#define WHITE	"" /*"\x1B[37m"*/

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

extern int killAllPublicHistory(void);

int do_rexx( const char *progname , LONG argc , RXSTRING *rx_argv );

int cmd_ver( FILE *source , Parse &argv )
{
  spawnl(P_WAIT,cmdexe_path,"CMD","/C","ver",NULL);
  puts( "Nihongo Yet Another Os/2 Shell is "VERSION );
  return 0;
}

static void get_scrsize_with_env(int *wh)
{
  const char *env;

  env=getenv("COLUMNS");
  if( env==NULL || (wh[0]=atoi(env)) <= 1 ) 
    wh[0] = 80;

  env=getenv("LINES");
  if( env==NULL || (wh[1]=atoi(env)) <= 1 )
    wh[1] = 25;
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
extern "C" {
  void _THUNK_C_FUNCTION (WinSetTitle) (PSZ szTITLE);
}
void set_win_title( const char *title )
{
  _THUNK_C_PROLOG ( 4 );
  _THUNK_C_FLAT ( const_cast<char*>(title) );
  _THUNK_C_CALL ( WinSetTitle );
}

#endif

int main(int argc, char **argv)
{
  if( _osmode != OS2_MODE ){
    fputs(  "NYAOS : Current Version of NYAOS does not support DOS/VDM.\n"
	  , stderr );
    return -1;
  }
#if USE_SET_WIN_TITLE
  set_win_title( "Nihongo Yet Another Os/2 Shell "VERSION );
#endif
  
  // ---- DBCS table の初期化 ----
  if( dbcs_table_init() != 0 ){
    fprintf(stderr,"nyaos: DBCS init error\n");
    return -1;
  }
  
  // ---- 画面表示は、fflush せずとも、ただちにやれ！ -----
  setvbuf(stdout,NULL,_IOLBF,BUFSIZ);

  // ---- とりあえず、キーバインドを好評の tcsh ライクにする -----
  Shell::bindkey_nyaos();

  // ----------------------------------------
  // COMSPEC に、NYAOS自身が設定されていると、
  // 動作がおかしくなるので、
  // CMD.EXE に切り換えさせる。 
  // ----------------------------------------
  if( SearchEnv("CMD.EXE","PATH",cmdexe_path) == 0 ){
    fputs("nyaos: can not find cmd.exe.\n"
	  "       Please put cmd.exe on %PATH%\n",stderr );
    return -1;
  }
  putenv(comspec);

  // -------- オプション分析 ----------

  int quite_mode=0;	/* ロゴを表示しない */
  int warning_mode=0;	/* 警告あり：!0 で画面をクリアしない */

  for(int i=1;i<argc;i++){
    if( argv[i][0] == '-' || argv[i][0] == '/' ){
      switch(argv[i][1]){
      default:
	fprintf(stderr,"-%c : no such option.\n",argv[i][1]);
	warning_mode = 1;
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
	    fprintf(stderr,"nyaos: no geometry parameter for -g.\n");
	    warning_mode = 1;
	    break;
	  }

	  int x=0,y=0;

	  while( *p != '\0' && isdigit(*p & 255) )
	    x = x*10 + (*p++ -'0');

	  if( (*p != 'x' && *p != 'X' ) || x<80 || x>200 ){
	    fprintf(stderr,"nyaos: bad geometry parameter `%s'.\n"
		    , argv[i] );
	    return 1;
	  }
	  ++p;
	  while( *p !='\0' && isdigit(*p & 255) )
	    y = y*10 + (*p++ -'0');

	  if( y<20 || y>100 ){
	    fprintf(stderr,"nyaos: bad geometry parameter `%s'.\n"
		    , argv[i] );
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
	quite_mode = 1;
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
	    fprintf(stderr,"--%s: no such option.\n",buffer);
	    warning_mode = 1;
	  }
	}
	break;
      }
    }else{
      if( _chdir2(argv[i]) != 0 ){
	fprintf(stderr,"%s: %s:invalid argument.\n",argv[0],argv[i]);
	return -1;
      }
    }
  }
  pretend_pm_application();

  if( isatty(fileno(stdin)) && !quite_mode ){
    extern int get_current_cp(void);
    
    if( ! warning_mode )
      fputs("\x1b[2J\x1b[1m",stdout);

    int cp=get_current_cp();
    if( cp==932 || cp==942 || cp==943 ){
      printf("\n  ┏┓┳┳  ┳┏━┓┏━┓┏━┓  " 
	     "\n  ┃┃┃┗━┫┣━┫┃  ┃┗━┓  "
	     "\n  ┻┗┛┗━┛┻  ┻┗━┛┗━┛  "
	     );
    }else{
      printf("\n   //  // //  //  ////   ////   ////"
	     "\n  /// // ////// //  // //  // ///   "
	     "\n // ///    /// ////// //  //    /// "
	     "\n//  //  ////  //  //  ////  /////   ");
    }
    
    printf("\n          Free Software           "
	   "\n- Nihongo Yet Another Os/2 Shell -"
	   "\n  1996,97,98,99 (c) HAYAMA,Kaoru  "
	   "\n Ver."VERSION" compiled on "__DATE__
	   "\n\n\x1b[0m"
	   );
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
    }else if( (home=getenv("HOME"))!=NULL ){
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

  // ---------------------------------------------------------
  // 標準入力が、リダイレクトされている場合の処理(ここで完結)
  // ---------------------------------------------------------
  if( ! isatty(fileno(stdin)) ){
    char cmdlin[1024]="";
    for(;;){
      if( option_prompt_even_piped ){
	/* Mule 中から、NYAOS を利用する場合は、
	 * パイプされている場合でも、プロンプトを表示させなくては
	 * いけない */
	char promptstr[2048];
	const char *promptenv=getenv("NYAOSPROMPT");
	if( promptenv==NULL && (promptenv=getenv("PROMPT")) == NULL )
	  promptenv = "$p$g";
	(void)set_prompt( promptenv , promptstr , sizeof(promptstr) );
	fputs( promptstr , stdout );
	fflush(stdout);
      }
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
    fputs( "nyaos: memory allocation error for shell.\n" , stderr );
    return -1;
  }

  // ======================== コマンド毎のループ =========================
  for(;;){
    /* プロンプト文字列の作成 */
    
    char promptstr[256];
    const char *promptenv=getenv("NYAOSPROMPT");
    if( promptenv==NULL && (promptenv=getenv("PROMPT")) == NULL )
      promptenv = "$p$g";
    
    /* プロンプト文字列に、最上段を使用するものがあれば、
     * シェル(Shell)に、その使用を禁止させる。*/
    
    if( set_prompt(promptenv , promptstr , sizeof(promptstr) ) )
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
    // int rc=shell.line_input(promptstr);
    const char *top;
    int rc = shell.line_input(promptstr,">",&top);
    
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
	  fputs("Good bye.\n",stdout);
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
      case Shell::QUIT:
	// ---- CTRL-Z などによる終了 ----
	fputs("\nGood bye!\n",stdout);
	return 0;

      case RC_ABORT:
      case Shell::ABORT:
	// fputs("^C\n",stdout);
	break;

      case 0:
	putchar('\n');
	break;
	
      default:
	fputs("\nUnknown error occuerd.\n"
	      "Please mail to iya-hayamatta@ijk.com\n"
	      , stdout );
	break;
      }
    }
  }// ============ コマンド毎のループの末尾 ===========
}
