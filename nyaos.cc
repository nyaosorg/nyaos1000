#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define VERSION "1.31"

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

#define RED "" /*"\x1B[31m"*/
#define WHITE "" /*"\x1B[37m"*/

int do_rexx( const char *progname , LONG argc , RXSTRING *rx_argv );

extern int nhistories;

int screen_width=80;
int screen_height=25;
int option_vio_cursor_control=1;
int cursor_start;
int cursor_end;
char *cursor_on_color_str=NULL;
char *cursor_off_color_str=NULL;
int option_nyaos_rc=1;

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

void setprompt(const char *promptenv,char *dp,ShellEdlin &edlin)
{
  const char *sp;
  time_t now;
  time( &now );
  struct tm *thetime = localtime( &now );
  edlin.using_i_mark=0;
  int a;
  
  while( *promptenv != '\0' ){
    if( *promptenv == '$' ){
      switch( promptenv++ , to_upper(*promptenv) ){
	
      case '!':
	dp += sprintf(dp,"%d",nhistories );
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
	edlin.using_i_mark = 1;
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
	  edlin.using_i_mark = 1;
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

  if( isatty(fileno(stdin)) && !quite_mode ){
    printf("\x1b[2J\x1b[1m"
	   "\n"
	   RED"  oo  oo oo  oo  oooo   oooo   ooooo   "
	   WHITE"   Free Software     \n"
	   RED"  ooo oo oo  oo oo  oo oo  oo oo    o  "
	   WHITE"Nihongo Yet Another  \n"
	   RED"  oooooo  oooo  oooooo oo  oo   ooo    "
	   WHITE" Os/2 Shell "VERSION"\n"
	   RED"  oo ooo   oo   oo  oo oo  oo o    oo  "
	   WHITE"       (C)           \n"
	   RED"  oo  oo   oo   oo  oo  oooo   ooooo   "
	   WHITE"1996,97 HAYAMA,Kaoru \n"
	   "                                                            \n"
	   "    This version is compiled on " __DATE__ " " __TIME__"    \n"
	   "    Comments, suggestions, and bug reports are welcome.     \n"
	   "    Please mail to kaoru@ferrari6.cheme.kyoto-u.ac.jp       \n"
	   "\x1b[0m\n"
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
    while( fgets_chop(cmdlin,sizeof(cmdlin),stdin) != NULL 
	  && execute(stdin,cmdlin) != RC_QUIT )
      ;
    return 0;
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

    setprompt(promptenv , promptstr , edlin );
    
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
      if( cmdlin[0] != '\0' && execute(stdin,cmdlin) == RC_QUIT ){
	// --- exitコマンドなどによる終了 ----
	fputs("Good bye.\n",stderr);
	return 0;
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
