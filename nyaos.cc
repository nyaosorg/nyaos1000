#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define VERSION "1.33"

// #define INCL_WINWINDOWMGR
#define INCL_DOSFILEMGR
#define INCL_RXSUBCOM
#include <os2.h>

#include "edlin.h"
#include "nyaos.h"
#include "complete.h"
#include "finds.h"

#define USE_VIDEO_H  1

#if USE_VIDEO_H
#  include <sys/video.h>
#endif

#define RED	"" /*"\x1B[31m"*/
#define WHITE	"" /*"\x1B[37m"*/

int do_rexx( const char *progname , LONG argc , RXSTRING *rx_argv );

extern int nhistories;

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

#undef CACHE
#ifdef CACHE
PathCache *script_cache=NULL;
#endif

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
#if 0
static char **env2argv(const char *envname)
{
  const char *envstr=getenv(envname);
  if( envstr == NULL )
    return NULL;

  int len=strlen(envstr);
  char *base=malloc(len+1);
  if( base == NULL )
    return NULL;
  strcpy(base,envstr);

  char **argv=(char **)malloc(sizeof(char*)*(len+1));
  int i=0;
  char *token=strtok(base," \t\r");
  while( token != NULL ){
    argv[ i++ ] = token;
    token = strtok(NULL," \t\r");
  }
  argv[ i ] = NULL;
  argv = (char**)realloc( argv , sizeof(char*)*(i+1) );
  return argv;
}
#endif

char *getcwd_case(char *dst)
{
  char cwd[ FILENAME_MAX ];

  *dst++ = _getdrive();
  *dst++ = ':';
  char *dp=dst;

  if( _getcwd( cwd , sizeof(cwd) ) == NULL )
    return dp;
  
  /* 「x:\」までをコピーする。*/
  char *token=strtok(cwd+1,"\\/");
  if( token != NULL ){
    do{
      *dp++ = '\\';
      char *p=dp;
      while( *token != '\0' )
	*p++ = *token++;
      *p = '\0';

      Dir dir;
      if( dir._findfirst(dst) == 0 ){
	const char *q=dir.get_name();
	while( *q != '\0' )
	  *dp++ = *q++;
	*dp = '\0';
      }else{
	dp = p;
      }
    }while((token=strtok(NULL,"\\/"))!=NULL );
  }else{
    /* ルートディレクトリー only */
    *dp++ = '\\';
  }
  *dp = '\0';
  return dp;
}

void setprompt(const char *promptenv,char *dp,ShellEdlin *edlin=NULL)
{
  const char *sp;
  time_t now;
  time( &now );
  struct tm *thetime = localtime( &now );
  if( edlin != NULL )
    edlin->using_i_mark=0;
  int a;
  
  while( *promptenv != '\0' ){
    if( *promptenv == '$' ){
      switch( promptenv++ , to_upper(*promptenv) ){
	
      case '!': /* ヒストリ番号 */
	dp += sprintf(dp,"%d",nhistories );
	break;
      case '@': /* ボリュームラベル */
	sp = _getvol(0);
	if( sp != NULL ){
	  while( *sp != '\0' )
	    *dp++ = *sp++;
	}
	break;

      case '$': *dp++ = '$';	  break;
      case '_': *dp++ = '\n';	  break;
      case 'A': *dp++ = '&';	  break;
      case 'B': *dp++ = '|';	  break;
      case 'C': *dp++ = '(';	  break;
	
      case 'D':/* 現在の日付 */
	dp += sprintf(dp,"%4d-%02d-%02d" ,
		      thetime->tm_year+1900 ,
		      thetime->tm_mon+1 ,
		      thetime->tm_mday );
	break;
	
      case 'E': *dp++ = '\x1b'; break;
      case 'F': *dp++ = ')';	  break;
      case 'G': *dp++ = '>';	  break;
      case 'H': *dp++ = '\b';	  break;
	
      case 'I':
	if( option_vio_cursor_control )
	  a = v_getattr();
	
	dp += sprintf(dp,"\x1B[s\x1B[1;44;37m\x1B[H%-*s\x1B[m\x1B[u"
		      , screen_width ,
		      " Nihongo Yet Another Os/2 Shell "VERSION
		      " (c) 1996,97 HAYAMA,Kaoru "
		      );
	if( edlin != NULL )
	  edlin->using_i_mark = 1;
	if( option_vio_cursor_control )
	  v_attrib(a);
	break;

      case '{':
	{
	  int curdrv=_getdrive();
	  if( option_vio_cursor_control )
	    a = v_getattr();
	  
	  dp += sprintf(dp,"\x1b[s\x1B[H" );
	  for(int length=0; *++promptenv != '}' && *promptenv != '\0'
	      && length < screen_width-1 ;){
	    if( isalpha(*promptenv) ){
	      int drv=toupper(*promptenv);
	      dp += sprintf(dp,"\x1B[1;%s;37m%c:"
			    ,(drv==curdrv ? "41" : "44")
			    ,drv);
	      length += 3;
	      
	      _getcwd1(dp,drv);
	      int len=strlen(dp);
	      if( length + len < screen_width-1 ){
		length += len;
		dp += len;
	      }else{
		for(int i=length ; i<screen_width-4 ; i++ ){
		  if( is_kanji(*dp) ){
		    ++dp;
		    ++i;
		  }
		  ++dp;
		}
		if( length < 74 ){
		  *dp++ = '.';
		  *dp++ = '.';
		  *dp++ = '.';
		}
		dp += sprintf(dp,"\x1b[0m ");
		while( *promptenv != '}' && *promptenv != '\0' )
		  ++promptenv;
		goto driveloop;
	      }
	      dp += sprintf(dp,"\x1b[0m ");
	    }
	  }
	driveloop:
	  dp += sprintf(dp,"\x1b[K\x1b[u");
	  if( edlin != NULL )
	    edlin->using_i_mark = 1;
	  if( option_vio_cursor_control )
	    v_attrib(a);
	  
	  if( *promptenv == '\0' )
	    goto promptend;
	}
	break;
	
      case 'L': *dp++ = '<';	  break;
	
      case 'N':/* カレントドライブ */
	*dp++ = _getdrive();
	break;
	
      case 'P':/* カレントディレクトリ */
#if 0
	*dp++ = _getdrive();
	*dp++ = ':';
	/* unsigned */ char cwd[256];
	
	/* ULONG bufsize;
	 * bufsize=sizeof(cwd);
	 * if( DosQueryCurrentDir(0,cwd,&bufsize) == 0 ){
	 */
	if( (sp=_getcwd(cwd,sizeof(cwd))) != NULL ){
	  /* char *sp=(char*)cwd; */
	  while( *sp != '\0' )
	    *dp++ = *sp++;
	}
#endif
	dp = getcwd_case(dp);
	break;
	
      case 'Q': *dp++ = '=';	  break;
      case 'S': *dp++ = ' ';	  break;
	
      case 'T':/* 現在の時刻 */
	dp += sprintf(dp,"%02d:%02d:%02d",
		      thetime->tm_hour ,
		      thetime->tm_min ,
		      thetime->tm_sec );
	break;
      case 'V':/* OS/2のバージョン */
	if( _osmode == OS2_MODE )
	  dp += sprintf(dp,"The Operating System/2 Version is %d.%d"
			, _osmajor/10 , _osminor );
	else
	  dp += sprintf(dp,"PC DOS Version is %d.%d"
			, _osmajor , _osminor );
	break;
      }
      promptenv++;
    }else{
      *dp++ = *promptenv++;
    }
  }
 promptend:    
    *dp = '\0';
}


int main(int argc, char **argv)
{
  char directory[FILENAME_MAX];
  char thename[FILENAME_MAX];

  // ---- DBCS table の初期化 ----
  if( dbcs_table_init() != 0 ){
    fprintf(stderr,"nyaos: DBCS init error\n");
    return -1;
  }
  memset( alias_hashtable , 0 , sizeof(alias_hashtable) );
  
#ifdef CACHE
  script_cache = new PathCache;
  script_cache->rehash("SCRIPTPATH");
#endif

  // ---- 画面表示は、fflush せずとも、ただちにやれ！ -----
  setvbuf(stdout,NULL,_IOLBF,BUFSIZ);

  // ---- とりあえず、キーバインドを好評の tcsh ライクにする -----
  Shell::bindkey_tcshlike();

  // ----------------------------------------
  // COMSPEC に、NYAOS自身が設定されていると、
  // 動作がおかしくなるので、
  // CMD.EXE に切り換えさせる。 
  // ----------------------------------------
  const char *shellname=getenv("COMSPEC");
  if(   shellname != NULL
     && (   strstr(shellname,"nyaos") != NULL
	 || strstr(shellname,"NYAOS") != NULL )){

    static char comspec[256];
    auto char cmdexe_path[100];
    
    if( _path(cmdexe_path,"CMD.EXE") != 0 ){
      fprintf(stderr,"NYAOS: can not find cmd.exe.");
      return -1;
    }
    /* 念の為、forward-slash を back-slash に変えておく。*/
    for(char *p=cmdexe_path ; *p != '\0' ; p++ ){
      if( *p == '/' )
	*p == '\\';
      else if( is_kanji(*p) )
	++p;
    }
    sprintf(comspec,"COMSPEC=%s",cmdexe_path);
    putenv(comspec);
  }
  
  // -------- オプション分析 ----------

  int quite_mode=0;
  for(int i=1;i<argc;i++){
    if( argv[i][0] == '-' || argv[i][0] == '/' ){
      switch(argv[i][1]){
      case 'C':
      case 'c':
      case 'K':
      case 'k':
      case 'e':
	if( i+1 < argc ){
	  int length=0;
	  for(int j=i+1;j<argc;j++)
	    length += strlen(argv[j])+1;
	  
	  char *oneline=(char*)alloca(length);
	  char *dp=oneline;
	  
	  for(int j=i+1;;){
	    const char *sp=argv[j];
	    while( *sp != '\0' )
	      *dp++ = *sp++;
	    if( ++j >= argc )
	      break;
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
    if( get_current_cp() == 932 ){
      printf("\n  ┏┓┳┳  ┳┏━┓┏━┓┏━┓  " 
	     "\n  ┃┃┃┗━┫┣━┫┃  ┃┗━┓  "
	     "\n  ┻┗┛┗━┛┻  ┻┗━┛┗━┛  "
	     );
#if 0
    }else if( (term=getenv("TERM"))==NULL || strcmp(term,"xterm")!=0 ){
      /*      N                   Y                   A
       *      O               S    */
      printf("\n   "
	     "\xC9\xCD\xBB\x20\xCB\xCB\xCD\x20\xCD\xCB\xC9\xCD\xCD\xCD\xBB"
	     "\xC9\xCD\xCD\xCD\xBB\xC9\xCD\xCD\xCD\xBB"
	     "\n   "
	     "\xBA\x20\xBA\x20\xBA\xC8\xCD\xCD\xCD\xB9\xCC\xCD\xCD\xCD\xB9"
	     "\xBA\x20\x20\x20\xBA\xC8\xCD\xCD\xCD\xBB"
	     "\n   "
	     "\xCA\x20\xC8\xCD\xBC\xC8\xCD\xCD\xCD\xBC\xCA\xCD\x20\xCD\xCA"
	     "\xC8\xCD\xCD\xCD\xBC\xC8\xCD\xCD\xCD\xBC"
	     );
#endif
    }else{
      printf("\n   // /// //  //  ////   ////   /////"
	     "\n  /// // ////// //  // //  // ///   "
	     "\n // ///     // ////// //  //    /// "
	     "\n/// //  ///// //  //  ////  /////   ");
    }
    
    printf("\n          Free Software           "
	   "\n- Nihongo Yet Another Os/2 Shell -"
	   "\n     1996,97 (c) HAYAMA,Kaoru     "
	   "\n Ver."VERSION" compiled on "__DATE__
	   "\n\n"
	   );
  }

 end_argv:
  if( option_nyaos_rc ){
    char buffer[FILENAME_MAX];
    char *path=NULL;
    const char *home;

    if( access(".nyaos",0)==0 ){
      path = ".nyaos";
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
	strcpy(dp,"nyaos.rc");
	if( access(buffer,0) != 0 )
	  path = NULL;
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

    // ---------------------------------
    // カーソルを消去された場合にそなえ、
    // カーソルのサイズを保存しておく。
    // ---------------------------------
    
    if( option_vio_cursor_control )
      v_getctype( &cursor_start , &cursor_end );
    
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
	if( execute(stdin,top) == RC_QUIT ){
	  // --- exitコマンドなどによる終了 ----
	  fputs("Good bye.\n",stderr);
	  return 0;
	}
	if( option_cmdlike_crlf )
	  putchar('\n');
      }
    }else{
      switch( rc ){
      case Shell::QUIT:
	// ---- CTRL-Z などによる終了 ----
	fputs("\nGood bye!\n",stdout);
	return 0;
      case Shell::ABORT:
	fputs("^C\n",stdout);
	break;
      defalt:
	fputs("\nUnknown error occuerd.\n"
	      "Please mail to kaoru@ferrari6.cheme.kyoto-u.ac.jp\n"
	      , stdout );
	break;
      }
    }
    // ---- カーソルを元に戻す ----
    if( option_vio_cursor_control )
      v_ctype( cursor_start , cursor_end );

  }// ============ コマンド毎のループの末尾 ===========
}
