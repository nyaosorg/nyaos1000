#include <ctype.h>
#include <stdlib.h>
#include <process.h>
#include <signal.h>
#define INCL_DOSMISC
#include <os2.h>

#include "hash.h"
#include "parse.h"
#include "nyaos.h"
#include "complete.h"

extern char *cmdexe_path; /* in nyaos.cc */
extern char drivealias[];
extern char prevdir[];
extern int echoflag;

int option_single_quote=0;
int option_backquote=1;
int option_backquote_in_quote=0;
int option_debug_echo;

int cmd_ver   (FILE *source , Parse &params );
int cmd_exec  (FILE *source , Parse &params );
int cmd_mode  (FILE *source , Parse &params );
int cmd_pwd   (FILE *source , Parse &params );
int cmd_option(FILE *source, Parse &params );
int cmd_comment(FILE *source, Parse &params );
int cmd_alias(FILE *source, Parse & );
int cmd_unalias(FILE *source, Parse & );
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
int cmd_unalias(FILE *fin, const char *parameter,int argc,char **argv);
int cmd_alias(FILE *fin, const char *sp,int argc,char **argv);

/* "script.cc" */
int cmd_rehash(FILE *source, Parse &args);
int cmd_cache(FILE *source,Parse &args);

/* "source.cc" */
int cmd_source( FILE *srcfil, Parse &params );

/* "command2.cc" */
int cmd_bind(FILE *source, Parse &param );
int cmd_bindkey(FILE *source,Parse &param);
/* int cmd_bindcomplete(FILE *source,Parse &param); */
int cmd_set( FILE *srcfil, Parse &params );
int cmd_cursor( FILE *fp, Parse &params);
int cmd_lecho(FILE *source, Parse &params );
int cmd_echo(FILE *srcfil, Parse &params );
int cmd_drvalias(FILE *srcfil, Parse &params );

/* "suffix.cc" */
int cmd_ext(FILE *source , Parse &argp );

/* "prepro.cc" */
int cmd_drivealias(FILE *source , Parse &arg );

volatile int ctrl_c=0;
void ctrl_c_signal(int sig)
{
  ctrl_c = 1;
  signal(sig,SIG_ACK);
}

static int compatible(FILE *source , Parse &params ,
		      int (*routine)(FILE *,const char*,int,char**) )
{
  int argc=params.get_argc();
  char **argv=(char**)malloc( (argc+1)*sizeof(char*) );

  for(int i=0 ; i<argc ; i++){
    argv[i] = params[i].dup();
  }
  return (*routine)( source , params.get_parameter() , argc , argv );
}

int foreach( FILE *srcfil , const char *parameter, int argc, char **argv);
static int foreach(FILE *source, Parse &params )
{ return compatible(source,params,foreach);  }

static int cmd_ls( FILE *srcfil, Parse &params )
{  return params.call_as_main(eadir);  }
static int cmd_dir( FILE *srcfil, Parse &params )
{  return params.call_as_main(eadir);  }
#if 0
   static int cmd_eadir( FILE *srcfil, Parse &params )
   {  return params.call_as_main(eadir);  }
#endif
static int cmd_exit( FILE *srcfil, Parse &params )
{
  return RC_QUIT;
}

void backquote_replace(const char *sp , char *dp , int max )
{
  int quote=0;
  char *border=dp+max-2;

  char *buffer[2];
  buffer[0] = (char*)alloca(max);
  buffer[1] = (char*)alloca(max);

  while( *sp != '\0'  &&  dp < border ){
    if( *sp == '"' ){
      /* 引用符の中か外かを一応チェックしておく */
      if( quote & 2 ){
	*dp++ = '\\';
	*dp++ = '"';
	++sp;
      }else{
	quote ^= 1;
	*dp++ = *sp++;
      }
    }else if( *sp == '\'' && (quote & 1)==0 && option_single_quote ){
      quote ^= 2;
      *dp++ = '"'; ++sp;
    }else if( is_kanji(*sp) ){
      *dp++ = *sp++;
      *dp++ = *sp++;
    }else if( *sp != '`' ){
      /* 逆クォート以外の文字は、そのままコピーする */
      *dp++ = *sp++;
    }else if( *(sp+1) == '`' ){
      /* 連続する二つの逆クォートは、一つの逆クォートに置換するだけ */
      *dp++ = '`';
      sp += 2;
    }else if( quote & 2 ){
      /* シングルクォート内の逆クォートは無視 */
      *dp++ = *sp++;
    }else{

      { /* 逆クォート内の命令を複写する。*/
	char *ddp=buffer[0];
	int qquote=0;
	while(  *++sp != '\0' && ddp < buffer[0]+max 
	      && (*sp != '`' || *++sp == '`') ){
	  
	  if( *sp == '\''  &&  (qquote & 1)==0  ){
	    *ddp++ = '"';
	    qquote ^= 2;
	  }else if( *sp == '"' ){
	    if( qquote & 2 ){
	      *ddp++ = '\\';
	      *ddp++ = '"';
	    }else{
	      *ddp++ = *sp;
	      qquote ^= 1;
	    }
	  }else{
	    *ddp++ = *sp;
	  }
	}
	*ddp = '\0';
      }

      if( option_debug_echo )
	printf("--> `%s`\n",buffer[0]);
      replace_alias(  buffer[0] , buffer[1] , max );
      if( option_debug_echo )
	printf("--> `%s`\n",buffer[1]);
      replace_script( buffer[1] , buffer[0] , max );
      if( option_debug_echo )
	printf("--> `%s`\n",buffer[0]);
      
      FILE *pp=popen( buffer[0] , "r" );
      
      if( pp != NULL ){
	int ch,size=0;
	while( (ch=fgetc(pp)) != EOF  ){
	  if( !quote  && isspace(ch & 255) ){
	    *dp++ = ' ';
	    do{
	      ch=fgetc(pp);
	      if( ch==EOF )
		goto pclose;
	    }while( isspace(ch & 255) );
	  }
	    
	  if( !quote && strchr("<>&|^",ch) != NULL ){
	    *dp++ = '^';
	    *dp++ = ch;
	  }else{
	    *dp++ = ch;
	    if( is_kanji(ch) )
	      *dp++ = fgetc(pp);
	  }
	  if( dp >= border-100 ){
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
  *dp = '\0';
}

Command jumptable[]={
  {"alias",  cmd_alias   },
  {"bg",     cmd_bg      },
  {"cache",  cmd_cache   },
  {"chcp",   cmd_chcp    },
  {"bind",   cmd_bind    },
  /*  {"bindcomplete",cmd_bindcomplete} , */
  {"bindkey",cmd_bindkey },
  {"cd",     cmd_chdir   },
  {"cds",    cmd_chdir   },
  {"chdir",  cmd_chdir   },
  {"comment",cmd_comment },
  //  {"cursor", cmd_cursor  },
  {"dirs",   cmd_dirs    },
  {"drvalias",cmd_drivealias },
//  {"eadir",  cmd_eadir   },
  {"echo",   cmd_echo    },
  {"exec",   cmd_exec    },
  {"exit",   cmd_exit    },
  {"ext",    cmd_ext     },
  {"fg",     cmd_fg      },
  {"foreach",foreach     },
  {"history",cmd_history },
  {"jobs",   cmd_jobs    },
  {"lecho",  cmd_lecho   },
  {"ls",     cmd_ls      },
  {"md",     cmd_mkdir   },
  {"mode",   cmd_mode    },
  {"mkdir",  cmd_mkdir   },
  {"open",   cmd_open    },
  {"option", cmd_option  },
  {"pwd",    cmd_pwd     },
  {"popd",   cmd_popd    },
  {"pushd",  cmd_pushd   },
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
{
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
  }

  if( cmdline[0] == '#' )
    return 0;

  /* カレントドライブの変更 */
  if(   is_alpha(cmdline[0]) && cmdline[1]==':' 
     && (cmdline[2]=='\0' || is_space(cmdline[2])) ) {
    char wd[FILENAME_MAX],c;

    DosError( FERR_DISABLEHARDERR );
    getcwd_case(wd);
    _chdrive( c = drivealias[ cmdline[0] & 0x1F ] );
    /*
     * _chdrive( ) は、いつも0を返してくるので実行結果を把握できない (+_;)
     * 期待通りにカレントドライブが変わったかどうか疑ってみる
     */
    if(_getdrive()==c)	  /* うまく変わってたら	 */
      strcpy(prevdir,wd); /* prevdirを覚えておく */
    else
      fputs("Cannot find the specified drive.\n",stderr);
    DosError( FERR_ENABLEHARDERR );
    return 0;
  }

  if( cmdline[0]=='\0' )
    return 0;
  
  if( option_debug_echo )
    printf("PASS-0:{%s}\n",cmdline);

  /* ヒストリの置換処理 */
  char buffer[2][4096];
  int curbuf=0;

  replace_history( cmdline , buffer[curbuf] , sizeof(buffer[0]) );
  
  if( option_debug_echo )
    printf("PASS-1:{%s}\n",buffer[curbuf] );

  // 一般的プリプロセス(環境変数など) 
  
  preprocess( buffer[curbuf] , buffer[curbuf^1] , sizeof(buffer[0]) );
  curbuf ^= 1;
  if( option_debug_echo )
    printf("PASS-2:{%s}\n",buffer[curbuf] );
  
  /* エイリアスの置換処理 */
  replace_alias( buffer[curbuf] , buffer[curbuf^1] , sizeof(buffer[0]) );
  curbuf ^= 1;

  if( option_debug_echo )
    printf( "PASS-3:{%s}\n" , buffer[curbuf] );

  /* 逆クォートの置換処理 */
  if( option_backquote ){
    backquote_replace( buffer[curbuf] , buffer[curbuf^1] , sizeof(buffer[0]));
    curbuf ^= 1;
  }
  if( option_debug_echo )
    printf( "PASS-4:{%s}\n" , buffer[curbuf] );

  replace_script(  buffer[curbuf] , buffer[curbuf^1] , sizeof(buffer[0]) );
  curbuf ^= 1;
  
  /* スクリプト置換 */
  if( option_debug_echo )
    printf("PASS-5:{%s}\n", buffer[curbuf] );
  
  /* 内臓コマンド実行 */
  for(const char *pointer=buffer[curbuf];;){
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
      fputs("Too near terminate charactor.\n",stderr);
      return 1;
    }
    int rc=(*cmd->func)(srcfil,params);

    switch( rc ){
    case RC_HOOK:
      goto spawn;

    case RC_ABORT: /* Ctrl-C で終了していたら、続くコマンドは実行しない */   
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
    puts( buffer[curbuf] );

 spawn:
  return spawnl(P_WAIT,cmdexe_path,cmdexe_path,"/C",buffer[curbuf],NULL);
}
