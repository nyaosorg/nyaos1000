#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <signal.h>
#include <io.h>

#include "hash.h"
#include "parse.h"
#include "nyaos.h"
#include "complete.h"

extern char *cmdexe_path; /* in nyaos.cc */

extern int echoflag;

int option_backquote=1;
int option_backquote_in_quote=0;
int option_debug_echo;

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
int cmd_set( FILE *srcfil, Parse &params );
int cmd_cursor( FILE *fp, Parse &params);
int cmd_lecho(FILE *source, Parse &params );
int cmd_echo(FILE *srcfil, Parse &params );
int cmd_drvalias(FILE *srcfil, Parse &params );

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
static int cmd_eadir( FILE *srcfil, Parse &params )
{  return params.call_as_main(eadir);  }
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
      quote ^= 1;
      *dp++ = *sp++;
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
    }else if( quote != 0  &&  option_backquote_in_quote == 0 ){
      /* option によっては、引用符の中の逆クォートは無視する。*/
      *dp++ = *sp++;
    }else{
      char *ddp=buffer[0];
      
      while( *++sp != '\0' && ddp < buffer[0]+max ){
	if( *sp=='`' && *++sp != '`')
	  break;
	*ddp++ = *sp;
      }
      *ddp = '\0';

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
  {"bindkey",cmd_bindkey },
  {"cd",     cmd_chdir   },
  {"cds",    cmd_chdir   },
  {"chdir",  cmd_chdir   },
  {"comment",cmd_comment },
  {"cursor", cmd_cursor  },
  {"dirs",   cmd_dirs    },
  {"eadir",  cmd_eadir   },
  {"echo",   cmd_echo    },
  {"exec",   cmd_exec    },
  {"exit",   cmd_exit    },
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
  {"which"  ,cmd_which   },
  { NULL    ,NULL        },
};

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
    _chdrive(cmdline[0]);
    _rfnlwr();
    return 0;
  }

  if( cmdline[0]=='\0' )
    return 0;

  if( option_debug_echo )
    printf("PASS-0:{%s}\n",cmdline);

  /* 環境変数の置換処理 */
  char buffer[2][4096];
  int curbuf=0;

  replace_envvar( cmdline , buffer[curbuf] , sizeof(buffer[0]) );
  
  if( option_debug_echo )
    printf("PASS-1:{%s}\n",buffer[curbuf] );

  /* エイリアスの置換処理 */
  replace_alias( buffer[curbuf] , buffer[curbuf^1] , sizeof(buffer[0]) );
  curbuf ^= 1;

  if( option_debug_echo )
    printf( "PASS-2:{%s}\n" , buffer[curbuf] );

  /* 逆クォートの置換処理 */
  if( option_backquote ){
    backquote_replace( buffer[curbuf] , buffer[curbuf^1] , sizeof(buffer[0]));
    curbuf ^= 1;
  }
  if( option_debug_echo )
    printf( "PASS-3:{%s}\n" , buffer[curbuf] );
  
  for(const char *pointer=buffer[curbuf];;){
    Parse params(pointer);
    
    Command *cmd = command_hash[ params[0] ];
    if( cmd == NULL )
      goto script;

    if( params==NULL ){
      fputs("Too near terminate charactor.\n",stderr);
      return 1;
    }
    int rc=(*cmd->func)(srcfil,params);
    switch( rc ){
    case RC_HOOK:
      goto script;

    case RC_ABORT: /* Ctrl-C で終了していたら、続くコマンドは実行しない */   
      return RC_ABORT;

    default:
      Parse::Terminal term=params.get_terminal();
      if(   term==Parse::SEMI_TERMINAL 
	 || term==(rc ? Parse::OR_TERMINAL: Parse::AND_TERMINAL) )
	{
	  pointer = params.get_nextcmds();
	  continue;
	}
      else
	return rc;
    }
  }
 script:
  
  replace_script(  buffer[curbuf] , buffer[curbuf^1] , sizeof(buffer[0]) );
  curbuf ^= 1;
  
  if( option_debug_echo )
    printf("PASS-4:{%s}\n", buffer[curbuf] );
  
  if( echoflag )
    puts( buffer[curbuf] );
  
  return spawnl(P_WAIT,cmdexe_path,"CMD","/C",buffer[curbuf],NULL);
}
