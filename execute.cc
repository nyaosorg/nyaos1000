#include <ctype.h>
#include <stdlib.h>
#include <process.h>
#include <signal.h>
#define INCL_DOSMISC
#include <os2.h>

#include "remote.h"
#include "hash.h"
#include "parse.h"
#include "nyaos.h"
#include "complete.h"
#include "errmsg.h"
#include "heapptr.h"

extern char *cmdexe_path; /* in nyaos.cc */
extern char drivealias[];
extern char prevdir[];
extern int echoflag;

int option_single_quote=0;
int option_backquote=1;
int option_backquote_in_quote=0;
int option_debug_echo;
extern option_vmax;

int cmd_ver   (FILE *source , Parse &params );
int cmd_exec  (FILE *source , Parse &params );
int cmd_mode  (FILE *source , Parse &params );
int cmd_pwd   (FILE *source , Parse &params );
int cmd_option(FILE *source, Parse &params );
int cmd_subject(FILE *source, Parse &params );
int cmd_comment(FILE *source, Parse &params );
int cmd_mkdir(FILE *source, Parse & );
int cmd_rmdir(FILE *source, Parse & );
int cmd_history(FILE *source, Parse & );

/* "open.cc" */
int cmd_open(FILE *source,Parse &);
int cmd_which( FILE *source , Parse &params );
int cmd_chcp( FILE *source , Parse &params );
int cmd_fg(FILE *source , Parse &argv );
int cmd_bg(FILE *source , Parse &argv );
int cmd_jobs( FILE *source , Parse & );
int cmd_console( FILE *source , Parse & );

/* "chdirs.cc" */
int chdir_with_cdpath(const char *cwd);
int cmd_chdir (FILE *srcfil, Parse &params );
int cmd_pushd( FILE *srcfil , Parse &params);
int cmd_popd( FILE *srcfil, Parse &params);
int cmd_dirs( FILE *srcfil , Parse &params );

/* "alias.cc" */
int cmd_alias( FILE * , Parse &params );
int cmd_unalias( FILE * , Parse &params );

/* "script.cc" */
int cmd_rehash(FILE *source, Parse &args);
int cmd_cache(FILE *source,Parse &args);

/* "command2.cc" */
int cmd_hotkey(FILE *source,Parse &param );
int cmd_bind(FILE *source, Parse &param );
int cmd_bindkey(FILE *source,Parse &param);
int cmd_set( FILE *srcfil, Parse &params );
int cmd_cursor( FILE *fp, Parse &params);
int cmd_lecho(FILE *source, Parse &params );
int cmd_echo(FILE *srcfil, Parse &params );

/* その他：１ソース＝１コマンド */
int eadir(int argc, char **argv,FILE *fout,Parse &);	/* "eadir.cc" */
int cmd_foreach(FILE *source, Parse &params );		/* "foreach2.cc" */
int cmd_source( FILE *srcfil, Parse &params );		/* "source.cc" */
int cmd_let( FILE *,Parse & );				/* "let.cc" */

/* その他：１コマンド＝１ライン関数 */
static int cmd_exit( FILE * , Parse & ){ return RC_QUIT; }
static int cmd_rem ( FILE * , Parse & ){ return 0;}

static int cmd_ls( FILE *srcfil, Parse &params )
{  return params.call_as_main(eadir);  }

/* Ctrl-C が押された際の、シグナルハンドラ */
volatile int ctrl_c=0;
void ctrl_c_signal(int sig)
{
  ctrl_c = 1;
  signal(sig,SIG_ACK);
}


/* 逆クォート処理をするパス */
char *backquote_replace(const char *sp )
{
  StrBuffer buf;
  int quote=0;

  while( *sp != '\0' ){
    if( *sp == '"' ){
      /* 引用符の中か外かを一応チェックしておく */
      if( quote & 2 ){
	buf << "\\\"";
	++sp;
      }else{
	quote ^= 1;
	buf << *sp++;
      }
    }else if( *sp == '\'' && (quote & 1)==0 && option_single_quote ){
      quote ^= 2;
      buf << '"'; ++sp;
    }else if( is_kanji(*sp) ){
      buf << sp[0] << sp[1]; sp+=2;
    }else if( *sp != '`' ){
      /* 逆クォート以外の文字は、そのままコピーする */
      buf << *sp++;
    }else if( *(sp+1) == '`' ){
      /* 連続する二つの逆クォートは、一つの逆クォートに置換するだけ */
      buf << '`';
      sp += 2;
    }else if( quote & 2 ){
      /* シングルクォート内の逆クォートは無視 */
      buf << *sp++;
    }else{
      /* 逆クォート内の命令を複写する。*/
      StrBuffer qbuf;
      int qquote=0;
      while( *++sp != '\0' && (*sp != '`' || *++sp == '`') ){
	if( *sp == '\''  &&  (qquote & 1)==0  ){
	  qbuf << '"';
	  qquote ^= 2;
	}else if( *sp == '"' ){
	  if( qquote & 2 ){
	    qbuf << '\\' << '"' ;
	  }else{
	    qbuf << *sp;
	    qquote ^= 1;
	  }
	}else{
	  qbuf << *sp;
	}
      }
      if( option_debug_echo )
	printf("--> `%s`\n",(const char*)qbuf );

      char *pass1 = replace_alias( (const char *)qbuf );
      if( option_debug_echo )
	printf( "--> `%s'\n",pass1);
      char *pass2 = replace_script( pass1 );
      if( option_debug_echo )
	printf( "--> `%s'\n",pass2);
      free( pass1 );
      FILE *pp=popen( pass2 , "rt" );
      free( pass2 );

      if( pp != NULL ){
	int ch;
	while( (ch=fgetc(pp)) != EOF  ){
	  if( !quote  && isspace(ch & 255) ){
	    buf << ' ';
	    do{
	      ch=fgetc(pp);
	      if( ch==EOF )
		goto pclose;
	    }while( isspace(ch & 255) );
	  }
	    
	  if( !quote && strchr("<>&|^",ch) != NULL ){
	    buf << '^' << (char)ch;
	  }else{
	    buf << (char)ch;
	    if( is_kanji(ch) )
	      buf << (char)fgetc(pp);
	  }
	  if( buf.getLength() > 4000 ){
	    while( fgetc(pp) != EOF )
	      ;
	    break;
	  }
	}
      pclose:
	pclose(pp);
      }
    }
  }
  return buf.finish();
}

Command jumptable[]={
  {"alias",  cmd_alias   },
//  {"bg",     cmd_bg      },
  {"cache",  cmd_cache   },
  {"chcp",   cmd_chcp    },
  {"bind",   cmd_bind    },
  {"bindkey",cmd_bindkey },
  {"call",   cmd_source  },
  {"cd",     cmd_chdir   },
  {"cds",    cmd_chdir   },
  {"chdir",  cmd_chdir   },
//  {"comment",cmd_comment },
  {"dirs",   cmd_dirs    },
  {"echo",   cmd_echo    },
  {"exec",   cmd_exec    },
  {"exit",   cmd_exit    },
//  {"fg",     cmd_fg      },
  {"foreach",cmd_foreach },
  {"history",cmd_history },
  {"hotkey", cmd_hotkey  },
//  {"jobs",   cmd_jobs    },
  {"lecho",  cmd_lecho   },
  {"let" ,   cmd_let     },
  {"ls",     cmd_ls      },
  {"md",     cmd_mkdir   },
  {"mode",   cmd_mode    },
  {"mkdir",  cmd_mkdir   },
  {"open",   cmd_open    },
  {"option", cmd_option  },
  {"pwd",    cmd_pwd     },
  {"popd",   cmd_popd    },
  {"pushd",  cmd_pushd   },
  {"rem",    cmd_rem     },
  {"rd",     cmd_rmdir   },
  {"rehash", cmd_rehash  },
  {"rmdir",  cmd_rmdir   },
  {"set",    cmd_set     },
  {"source", cmd_source  },
  {"unalias",cmd_unalias },
  {"ver",    cmd_ver     },
  {"which"  ,cmd_which   },
  { NULL    ,NULL        },
};

int option_ignore_cases=1;

Hash <Command> command_hash(512);

int execute( FILE *srcfil, const char *cmdline , int fastmode=0 )
     throw( Noclobber )
{
  static RemoteNyaos nyaosPipe;
  
  ctrl_c = 0;
  signal(SIGINT,ctrl_c_signal);
  int wh[2];
  get_scrsize( wh );
  screen_width = wh[0];
  screen_height = wh[1];

  /* 初めて実行するときは、ハッシュテーブルを初期化する */
  static int made_table=0;
  if( made_table == 0 ){
    made_table = 1;
    
    /* コマンドのハッシュテーブルを初期化 */
    for(int i=0; jumptable[i].name != NULL ;i++){
      command_hash.insert( jumptable[i].name , &jumptable[i] );
    }

    for(;;){
      if( *cmdline == '\0' )
	return 0;
      if( !is_space(*cmdline ) )
	break;
      if( is_kanji(*cmdline ) )
	++cmdline;
      ++cmdline;
    }
    /* 名前付きパイプの作成 */
    int rc=nyaosPipe.create( "NYAOS" );
    if( rc ){
      printf("Could not create named pipe(%d).",rc);
    }else{
      static char setPipeName[256];
      sprintf(setPipeName,"PIPE2NYAOS=%s",nyaosPipe.getName());
      putenv( setPipeName );
    }
  }

  if( cmdline[0] == '#' )
    return 0;

  /* カレントドライブの変更 */
  if(   is_alpha(cmdline[0]) && cmdline[1]==':' 
     && (cmdline[2]=='\0' || is_space(cmdline[2])) ) {
    char wd[FILENAME_MAX],c;

    DosError( FERR_DISABLEHARDERR );
    getcwd_case(wd);
    _chdrive( c=cmdline[0] );

    /* _chdrive( ) は、いつも0を返してくるので実行結果を把握できない (+_;)
     * 期待通りにカレントドライブが変わったかどうか疑ってみる。
     */
    if(_getdrive()==toupper(c)){/* うまく変わってたら  */
      strcpy(prevdir,wd);	/* prevdirを覚えておく */
      getcwd_case(wd);		/* カレントディレクトリをシェル変数へ反映 */
      setShellEnv("CWD",wd);
    }else{
      ErrMsg::say(ErrMsg::ChangeDriveError,cmdline,0);
    }
    DosError( FERR_ENABLEHARDERR );
    return 0;
  }

  if( cmdline[0]=='\0' )	return 0;
  if( option_debug_echo )	printf("PASS-0:{%s}\n",cmdline);

  try{
    /* ヒストリの置換処理 */
    heapchar_t pass1( replace_history(cmdline) );
    if( pass1 == NULL )		return 0;
    if( option_debug_echo )	printf("PASS-1:{%s}\n",(char*)pass1 );
    
    /* 一般的プリプロセス(環境変数など) */
    heapchar_t pass2(preprocess(pass1));
    if( pass2 == NULL )		return 0;
    if( option_debug_echo )	printf("PASS-2:{%s}\n",(char*)pass2 );
    
    /* エイリアスの置換処理 */
    heapchar_t pass3(replace_alias( pass2 ));
    if( pass3 == NULL )		return 0;
    if( option_debug_echo )	printf("PASS-3:{%s}\n",(char*)pass3 );
    
    /* 逆クォートの置換処理 */
    heapchar_t pass4;
    if( option_backquote ){
      pass4 = backquote_replace( pass3 );
      if( pass4 == NULL )	return 0;
    }else{
      pass4 = pass3;
    }
    if( option_debug_echo )	printf("PASS-4:{%s}\n" , (char*)pass4 );

    /* スクリプトの置換処理 */
    heapchar_t pass5(replace_script( pass4 ));
    if( pass5 == NULL )		return 0;
    if( option_debug_echo )	printf("PASS-5:{%s}\n", (char*)pass5 );
  
    /* 内臓コマンド実行 */
    for(const char *pointer=pass5;;){
      Parse params(pointer);
      
      /* ヒストリ変換などで文字列が０になることもあるので、
       * ここでチェックする。 */
      if( params.get_argc() <= 0 ){
	pointer = params.get_nextcmds();
	if( *pointer == '\0' )
	  return 0;
	else
	  continue;
      }
      
      Command *cmd = (  option_ignore_cases
		      ? command_hash.lookup_tolower( params[0] )
		      : command_hash[ params[0] ]
		      );
      
      if( cmd == NULL )
	break;
      
      if( params==NULL ){
	ErrMsg::say(ErrMsg::TooNearTerminateChar,0);
	return 1;
      }
      int rc=(*cmd->func)(srcfil,params);
      
      switch( rc ){
      case RC_HOOK:
	goto spawn;
	
      case RC_ABORT: /* Ctrl-C で終了していたら、続くコマンドは実行しない */   
	if( ctrl_c != 0 )
	  fputs("\n^C\n",stderr);
	return RC_ABORT;
	
      default:
	Parse::Terminal term=params.get_terminal();
	if(   term==Parse::SEMI_TERMINAL 
	   || term==Parse::AMP_TERMINAL
	 || term==(rc ? Parse::OR_TERMINAL: Parse::AND_TERMINAL) )
	  {
	    pointer = params.get_nextcmds();
	    continue;
	  }
	else
	  return rc;
      }
    }
    
    if( echoflag )
      puts( pass5 );
    
  spawn:
    int rc=0;
    if( nyaosPipe.isConnected() ){
      rc=spawnl(P_WAIT,cmdexe_path,cmdexe_path,"/C",(char*)pass5,NULL);
    }else{
      nyaosPipe.connect();
      rc=spawnl(P_WAIT,cmdexe_path,cmdexe_path,"/C",(char*)pass5,NULL);
      StrBuffer str;
      while( (nyaosPipe.readline(str)) >= 0 ){
	if( execute( NULL , (const char *)str ) == RC_QUIT )
	  return RC_QUIT;
	str.drop();
      }
      nyaosPipe.disconnect();
    }
    return rc;
  }catch(Noclobber e){
    ErrMsg::say(ErrMsg::FileExists,0);
    return -1;
  }catch(...){
    ErrMsg::say( ErrMsg::InternalError , 0 );
    return -1;
  }
}
