#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <sys/video.h>

#include "edlin.h"
#include "nyaos.h"
#include "complete.h"
#include "smartptr.h"

#define RED	"" /*"\x1B[31m"*/
#define WHITE	"" /*"\x1B[37m"*/

int do_rexx( const char *progname , LONG argc , RXSTRING *rx_argv );

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

char comspec[128]="COMSPEC=";
char *cmdexe_path=comspec+8;

int execute_result=0;

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

// ---- fgets と基本は同じ。ただ、末尾の「\n」を読み込まない点が異なる ----
char *fgets_chop(char *dp, int max, FILE *fp)
{
  int ch;
  while( max-- >= 0  &&  (ch=getc(fp)) != '\n' ){
    if( ch==EOF ){
      *dp = '\0';
      return NULL;
    }
    *dp++ = ch;
  }
  *dp = '\0';
  return dp;
}

#ifdef USE_SET_WIN_TITLE
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
  _THUNK_C_FLAT ( title );
  _THUNK_C_CALL ( WinSetTitle );
}
#endif

int main(int argc, char **argv)
{
  if( _osmode != OS2_MODE ){
    fputs("NYAOS : Current Version of NYAOS does not support DOS/VDM.\n"
	  , stderr );
    return -1;
  }
#ifdef USE_SET_WIN_TITLE
  set_win_title( "Nihongo Yet Another Os/2 Shell "VERSION );
#endif
  
  char directory[FILENAME_MAX];
  char thename[FILENAME_MAX];

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

  int quite_mode=0;
  for(int i=1;i<argc;i++){
    if( argv[i][0] == '-' || argv[i][0] == '/' ){
      switch(argv[i][1]){

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
	    return 1;
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
      }
    }else{
      if( _chdir2(argv[i]) != 0 ){
	fprintf(stderr,"%s: %s:invalid argument.\n",argv[0],argv[i]);
	return -1;
      }
    }
  }

  /* 
   */
     
  if( isatty(fileno(stdin)) && !quite_mode ){
    extern int get_current_cp(void);
    const char *term;

    printf("\x1b[2J\x1b[1m");
    int cp=get_current_cp();
    if( cp==932 || cp==942 || cp==943 ){
      printf(
	     "\n  ┏┓┳┳  ┳┏━┓┏━┓┏━┓  " 
	     "\n  ┃┃┃┗━┫┣━┫┃  ┃┗━┓  "
	     "\n  ┻┗┛┗━┛┻  ┻┗━┛┗━┛  "
	     );
    }else{
      printf("\n   // /// //  //  ////   ////   /////"
	     "\n  /// // ////// //  // //  // ///   "
	     "\n // ///     // ////// //  //    /// "
	     "\n/// //  ///// //  //  ////  /////   ");
    }
    
    printf("\n          Free Software           "
	   "\n- Nihongo Yet Another Os/2 Shell -"
	   "\n   1996,97,98 (c) HAYAMA,Kaoru    "
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

  char cmdlin[1024]="";

  // ---------------------------------------------------------
  // 標準入力が、リダイレクトされている場合の処理(ここで完結)
  // ---------------------------------------------------------
  if( ! isatty(fileno(stdin)) ){
    for(;;){
      if( option_prompt_even_piped ){
	char promptstr[2048];
	const char *promptenv=getenv("NYAOSPROMPT");
	if( promptenv==NULL && (promptenv=getenv("PROMPT")) == NULL )
	  promptenv = "$p$g";
	setprompt(promptenv , promptstr , NULL );
	fputs( promptstr , stdout );
	fflush(stdout);
      }
      if( fgets_chop(cmdlin,sizeof(cmdlin),stdin) == NULL 
	 || execute(stdin,cmdlin) == RC_QUIT )
	return 0;
    }
  }

  // -------- 入力オブジェクト edlin を用意する ---------------
  // ここで、用意するのは、ループの内部に置いて
  // 何回もコンストラクタ・デストラクタを呼ぶコストを省くため。
  // prompt は、この時点では未定なので、ダミーを放り込んでおく。
  // ----------------------------------------------------------

  extern int killAllPublicHistory(void);

  killAllPublicHistory();
  extern int canna_init();
  canna_init();
  
  ShellEdlin edlin("NYAOS>",cmdlin,sizeof(cmdlin) );
  Shell shell(edlin);
  
  // ======================== コマンド毎のループ =========================
  for(;;){
    // ----- ここから、えんえんと、プロンプト関係の処理がつづく -----

    char promptstr[2048];
    const char *promptenv=getenv("NYAOSPROMPT");
    if( promptenv==NULL && (promptenv=getenv("PROMPT")) == NULL )
      promptenv = "$p$g";
    
    setprompt(promptenv , promptstr , &edlin );
    
    // ------------------------------------------------------------------
    // 入力オブジェクト edlin に擬似カーソルの為のエスケープシーケンスを
    // 伝えておく(って、いちいち、ここで何回もさせることでもないが...)
    // ------------------------------------------------------------------

    // ---- カーソルを BOX 型にする ----
    if( option_vio_cursor_control ){
      if( v_hardware() == V_COLOR_12 )
	v_ctype( 11 , 0 );
      else
	v_ctype( 7 , 0 );
    }

    edlin.setcursor( cursor_on_color_str , cursor_off_color_str );
    
    // ================== 実際の入力 =======================
    //  _osmode によって処理を変えているのは、DOS では ^H で
    // 前の行に遡れないため、次の行に繰り越させず、入力文字列を
    // スクロールさせる。この時に一度に表示できる文字数を与えて
    // いるわけである。 OS/2 では、そのようなことを気にする必要
    // がないため ∞ を与える。
    
    int rc=shell.line_input(promptstr
			    ,_osmode==OS2_MODE ? 32767 :screen_width-1 );
    
    // ============== コマンドの実行 ====================

    // ---------------------------------------------------------
    // コマンドを実行し、「終了」の帰り値だったら、終了する。
    // 実行は、高機能system である execute がよしなにしてくれる。
    // ---------------------------------------------------------

    if( rc >= 0 ){
      putchar('\n');
      char *top=cmdlin;
      while( *top != '\0' && is_space(*top) )
	++top;
      if( top[0] != '\0' ){
	execute_result = execute(stdin,top);
	if( execute_result == RC_QUIT ){
	  // --- exitコマンドなどによる終了 ----
	  fputs("Good bye.\n",stderr);
	  return 0;
	}
	if( option_cmdlike_crlf )
	  putchar('\n');
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
	fputs("^C\n",stdout);
	break;

      defalt:
	fputs("\nUnknown error occuerd.\n"
	      "Please mail to hayama@karl.tis.co.jp\n"
	      , stdout );
	break;
      }
    }

  }// ============ コマンド毎のループの末尾 ===========
}
