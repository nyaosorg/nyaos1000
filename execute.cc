#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <signal.h>
#include <io.h>
#include <sys/nls.h>

#include "parse.h"
#include "nyaos.h"
#include "complete.h"
#include "edlin.h"

#define ECHODEBUG(x)		/* 通常モード */
/* #define ECHODEBUG(x) (x)	/* デバッグモード*/

extern int echoflag;
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

/* "chdirs.cc" */

int chdir_with_cdpath(const char *cwd);
int cmd_chdir (FILE *srcfil, Parse &params );
int cmd_pushd( FILE *srcfil , Parse &params);
int cmd_popd( FILE *srcfil, Parse &params);
int cmd_dirs( FILE *srcfil , Parse &params );

int cmd_bind(FILE *source, Parse &param )
{
  struct{
    const char *name;
    void (*func)();
    const char *usage;
  } table2[]={
    { "emacs" , Shell::bindkey_tcshlike , "key-bindings like emacs" },
    { "tcsh"  , Shell::bindkey_tcshlike , "same as emacs" },
    { "ws"    , Shell::bindkey_wordstar , "key-bindings like wordstar" },
    { "vz"    , Shell::bindkey_wordstar , "same as ws" },
  };

  if( param.get_argc() < 2 ){
    FILE *fout=param.open_stdout();
    for(int i=0;i<numof(table2);i++)
      fprintf(fout,"%s\t: %s\n",table2[i].name,table2[i].usage);
  }else{
    char *buffer=(char*)alloca(param.get_length(1)+1);
    param.copy(1,buffer);
    for(int i=0;i<numof(table2);i++){
      if(   to_lower(buffer[0])==table2[i].name[0]
	 && stricmp(buffer,table2[i].name)==0 ){
	(*table2[i].func)();
	return 0;
      }
    }
    fprintf(stderr,"%s : no such bindings\n",buffer);
  }
  return 0;
}

int cmd_bindkey(FILE *source,Parse &param)
{
  if( param.get_argc() < 3 ){
    Shell::bindlist(param.open_stdout());
    return 0;
  }

  char *key  = (char*)alloca(param.get_length(1)+1);
  param.copy(1,key);
  char *func = (char*)alloca(param.get_length(2)+1);
  param.copy(2,func);

  switch( Shell::bindkey(key,func) ){
  case 1:
    fprintf(stderr,"bindkey: %s: invalid key name.\n",key);
    break;
  case 2:
    fprintf(stderr,"bindkey: %s: invalid function name.\n",func);
    break;
  }
  return 0;
}


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
    argv[i] = (char*)malloc( params.get_length(i)+1 );
    params.copy(i,argv[i]);
  }
  return (*routine)( source , params.get_parameter() , argc , argv );
}

int foreach( FILE *srcfil , const char *parameter, int argc, char **argv);

static int foreach(FILE *source, Parse &params )
{
  return compatible(source,params,foreach);
}


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
static int cmd_set( FILE *srcfil, Parse &params )
{
  if( params.get_argc() < 2 )
    return RC_HOOK;

  const char *sp = params.get_parameter();
  if( sp == NULL )
    return RC_HOOK;

  int ch;
  char envname[1024],*dp=envname;

  /* 変数名の前の空白のスキップ */
  while( *sp!='\0' && is_space(*sp) )
    sp++;

  /* 変数名のコピ－ */
  while( *sp != '=' && !is_space(*sp ) ){
    if(   *sp=='\0' || *sp=='&' 
       || *sp=='>'  || *sp=='|' ){
      /* 変数名がない ---> 画面表示のみ */
      return RC_HOOK;
    }else if( *sp=='<' ){
      fputs("You cannot input-redirect on command set.\n",stderr);
      return 1;
    }
    if( is_kanji(*sp) ){
      *dp++ = *sp++;
      *dp++ = *sp++;
    }else{
      *dp++ = to_upper(*sp) ;
      sp++;
    }
  }

  /* 変数名～「=」の空白のスキップ */
  while( *sp != '=' ){
    if( *sp=='&'  || *sp=='\0'  ||  *sp=='|' 
       || *sp == '>' ){

      return RC_HOOK;
    }else if( *sp == '<' ){
      fputs("You cannot input-redirect on command set.\n",stderr);
      return 1;
    }

    if( !is_space(*sp) ){
      fputs("Invalid Argument.\n",stderr);
      return -1;
    }
    sp++;
  }
  /* 「=」のスキップ */
  *dp++ = *sp++;
  
  /* 「=」～引数の直前の空白を削除 */
  while( *sp != '\0'  && is_space(*sp) )
    sp++;

  /*  右辺値のコピ－ */
  
  char *final_space=NULL;
  int quote=0;
  int compati=( *sp != '"' );
  
  while( *sp!='\0' && (quote!=0 || (*sp!='&' && *sp!='|'))){
    if( is_space(*sp) ){
      if( final_space==NULL && quote==0 )
	final_space = dp;
    }else{
      final_space = NULL;
      if( *sp == '^' &&  *++sp !='\0' ){
	if( is_kanji(*sp) )
	  *dp++ = *sp++;
	*dp++ = *sp++;
	continue;
      }else if( *sp == '"' ){
	if( ! compati ){
	  if( *(sp+1) == '"' ){
	    *dp++ = '"';
	    sp += 2;
	  }else{
	    sp++;
	    quote ^= 1;
	  }
	  continue;
	}
	quote ^= 1;
      }
    }
    if( is_kanji(*sp) )
      *dp++ = *sp++;
    *dp++ = *sp++;
  }
 exit:
  *dp = '\0';
  
  if( final_space != NULL )
    *final_space = '\0';
  
  putenv( strdup(envname) );
  
  return 0;
}
extern int cmd_source( FILE *srcfil, Parse &params );

static int cmd_cursor( FILE *fp, Parse &params)
{
  if( cursor_on_color_str != NULL ){
    free( cursor_on_color_str );
    cursor_on_color_str = NULL;
  }
  if( cursor_off_color_str != NULL ){
    free( cursor_off_color_str );
    cursor_off_color_str = NULL;
  }
  if( params.get_argc() >= 3 ){
    cursor_on_color_str  = (char*)malloc(params.get_length(1)+1);
    assert( cursor_on_color_str != NULL );
    params.copy(1,cursor_on_color_str );

    cursor_off_color_str  = (char*)malloc(params.get_length(2)+1);
    assert( cursor_off_color_str != NULL );
    params.copy(2,cursor_off_color_str );

    FILE *fout=params.open_stdout();
    if( fout == NULL ){
      fputs("cursor : cannot make a pipe or file\n",stderr);
      return 1;
    }

    fprintf(fout,
	   "Cursor Color Attribute ... ESC[%sm\n"
	   "  Text Color Attribute ... ESC[%sm\n",
	   cursor_on_color_str  ,
	   cursor_off_color_str );
  }
  return 0;
}

static int cmd_echo(FILE *srcfil, Parse &params )
{
  FILE *fout=params.open_stdout();
  if( fout == NULL ){
    fputs("echo : cannot make a pipe or file\n",stderr);
    return 0;
  }
  bool quote=false;
  
  const char *sp=params.get_argv(1);
  if( sp != NULL ){
    while( *sp != '\0'  &&  sp < params.get_tail() ){
      if( is_kanji(*sp) ){
	putc(*sp++,fout);
	putc(*sp,fout);
      }else if( *sp=='^' ){
	switch( *++sp ){
	case '"':
	  quote = !quote;
	  putc('^',fout);
	  break;
	case 't':
	  putc('\t',fout); break;
	case 'n':
	  putc('\n',fout); break;
	case 'v':
	  putc('\v',fout); break;
	case 'r':
	  putc('\r',fout); break;
	case 'f':
	  putc('\v',fout); break;
	case 'e':
	  putc('\033',fout); break;
	case 'q':
	  putc('"',fout); break;
	case 'c':
	  return 0;
	default:
	  if( is_digit(*sp) ){
	    int n = (*sp++ - '0');
	    for(int i=0 ; i<3 && is_digit(*sp) ; i++ ){
	      n = n*8 + (*sp++ - '0');
	    }
	    putc( n , fout );
	    continue;
	  }else{
	    putc( *sp , fout);
	  }
	}
      }else if( !quote && (*sp == '<' || *sp == '>') ){
	break;
      }else if( *sp == '"' ){
	if( *(sp+1) == '"' ){
	  putc( '"' , fout );
	  sp += 2;
	  continue;
	}
	quote = !quote;
      }else{
	putc( *sp , fout );
      }
      sp++;
    }
  }
  putc( '\n' , fout );
  return 0;
}
static int cmd_lecho(FILE *source, Parse &params )
{
  for(int i=0 ; i<params.get_argc() ; i++ ){
    char argv[1024];
    params.copy(i,argv);
    printf("[%s] ",argv);
  }
  printf("\n");
  return 0;
}

const struct commandtable_tag jumptable[]={
  {"alias",  cmd_alias   },
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
  {"foreach",foreach     },
  {"history",cmd_history },
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
  {"rmdir",  cmd_rmdir   },
  {"set",    cmd_set     },
  {"source", cmd_source  },
  {"unalias",cmd_unalias },
  {"which"  ,cmd_which   },
  { NULL    ,NULL        },
}, *hashtable[512];

int wrdcmp(const char *s1,const char *s2)
{
  /* s1は、NULで終わっていなければいけないが、
   * s2は、アルファベット文字以外ならば問題ない
   * また、大文字小文字の区別をしない
   */
  
  while( *s1 != 0 ){
    if( to_upper(*s1) != to_upper(*s2) )
      return *s1-*s2;

    /* 漢字ならば、2byte目を toupper越しに比較してはいけない */
    if( is_kanji( *s1 ) ){
      if( *++s1 != *++s2 )
	return *s1-*s2;
    }
    ++s1;++s2;
  }

  /* s2に続きがあるとき、つまり、
   * setemx とかいうふうに続いている場合、
   * 同じと判定させない
   */

  if( *s2 != 0  &&  !is_space(*s2) )
    return *s2;
  
  return 0;
}

int execute( FILE *srcfil, const char *cmdline , int use_spawn =0 )
{
  ctrl_c = 0;
  signal(SIGINT,ctrl_c_signal);
  int wh[2];
  _scrsize( wh );
  screen_width = wh[0];
  screen_height = wh[1];

  /* 初めて実行するときは、ハッシュテーブルを初期化する */
  static int made_table=0;
  if( made_table == 0 ){
    made_table = 1;
    
    /* エイリアスのハッシュテーブルも初期化 */
    for(int i=0 ; i<numof(alias_hashtable) ; i++)
      alias_hashtable[ i ] = NULL;

    for(int i=0 ; i<numof(hashtable) ; i++)
      hashtable[i] = NULL;

    for(int i=0; jumptable[i].name != NULL ;i++){
      int key=0;
      const char *p=jumptable[i].name;
      if( p==NULL )
	break;
      while( *p != '\0' ){
	key += *p++;
      }
      key %= numof(hashtable);
      while( hashtable[key] != NULL ){
	if( ++key >= numof(hashtable) )
	  key=0;
      }
      hashtable[key] = &jumptable[i];
    }
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

  if( cmdline[0] == '-' )
    return system(cmdline+1);
  else if( cmdline[0] == '#' )
    return 0;

  /* カレントドライブの変更 */
  if(   is_alpha(cmdline[0]) && cmdline[1]==':' 
     && (cmdline[2]=='\0' || is_space(cmdline[2])) ) {
    _chdrive(cmdline[0]);
    _rfnlwr();
    return 0;
  }

  ECHODEBUG( printf("org:{%s}\n",cmdline) );

  /* 環境変数の置換処理 */
  char env_replaced_buffer[1024];
  replace_envvar( cmdline , env_replaced_buffer );
  cmdline = env_replaced_buffer;
  if( cmdline[0]=='\0' )
    return 0;
  
  ECHODEBUG( printf("pre:{%s}\n",cmdline) );
  
  /* エイリアスの置換処理 */
  char alias_replaced_buffer[1024];
  alias_replace( cmdline , alias_replaced_buffer );
  cmdline = alias_replaced_buffer;

  ECHODEBUG( printf("ali:{%s}\n",cmdline) );
  
  Parse params(cmdline);

  int key=0;
  {/* ハッシュキーを計算する */
    int size=params.get_length(0);
    const char *sp=params.get_argv(0);
    while( size-- )
      key += to_lower(*sp++);
  }

  key %= numof(hashtable);
  /* 内蔵コマンド --> Open hash */
  while( hashtable[key] != NULL ){
    if(   hashtable[key]->name[0] == params.get_argv(0)[0]
       && wrdcmp(hashtable[key]->name, params.get_argv(0) )==0 ){

      if( params==NULL ){
	fputs("Too near terminate charactor.\n",stderr);
	return 0;
      }
      int rc=(*hashtable[key]->func)(srcfil,params);
      if( rc == RC_HOOK ){
	break;
      }else{
	if( rc == 0  &&  *params.get_tail() == '&' )
	  return execute( srcfil , params.get_nextcmds() );
	return rc;
      }
    }
    if( ++key > numof(hashtable) )
      key = 0;
  }
  ECHODEBUG( printf("tmp:{%s}\n",cmdline ) );
  replace_script( cmdline , env_replaced_buffer );
  cmdline = env_replaced_buffer;
  
  ECHODEBUG( printf("scr:{%s}\n",cmdline) );

  if( echoflag )
    puts( cmdline );
  
  return system( cmdline );
}
