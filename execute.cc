#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <signal.h>
#include <io.h>
#include <sys/nls.h>

#include "params.h"
#include "nyaos.h"

extern int echoflag;
int cmd_pwd   (FILE *source , Params &params );
int cmd_chdir (FILE *srcfil, Params &params);
int cmd_option(FILE *source, Params &params);
int cmd_comment(FILE *source, Params &params);
int cmd_alias(FILE *source, Params &);
int cmd_unalias(FILE *source, Params &);
void alias_replace(const char *sp,char *dp);

volatile int ctrl_c=0;
void ctrl_c_signal(int sig)
{
  ctrl_c = 1;
  signal(sig,SIG_ACK);
}

static int compatible(FILE *source , Params &params ,
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

static int foreach(FILE *source, Params &params )
{
  return compatible(source,params,foreach);
}

struct Dirstack{
  Dirstack *prev;
  char buffer[1];
} *dirstack=NULL;

static int cmd_dirs( FILE *srcfil , Params &params )
{
  char cwd[FILENAME_MAX];
  _getcwd2(cwd,sizeof(cwd));
  
  FILE *fout=params.open_stdout();
  if( fout == NULL ){
    fputs("nyaos : cannot make a pipe or file\n",stderr);
    return 1;
  }
  fputs(cwd,fout);

  Dirstack *tmp=dirstack;
  while( tmp != NULL ){
    putc(' ',fout);
    fputs(tmp->buffer,fout);

    tmp = tmp->prev;
  }
  putc('\n',fout);

  return 0;
}

static int cmd_pushd( FILE *srcfil , Params &params)
{
  char cwd[FILENAME_MAX];
  _getcwd2(cwd,sizeof(cwd));

  Dirstack *tmp=(Dirstack*)malloc(sizeof(dirstack)+strlen(cwd));
  tmp->prev = dirstack;
  strcpy( tmp->buffer , cwd );
  dirstack = tmp;
  
  if( params.get_argc() > 1 ){
    char dir[FILENAME_MAX];
    params.copy(1,dir);
    _chdir2( dir );
  }

  return cmd_dirs(srcfil,params);
}

static int cmd_popd( FILE *srcfil, Params &params)
{
  if( dirstack != NULL ){
    _chdir2( dirstack->buffer );
    Dirstack *tmp=dirstack;
    dirstack = dirstack->prev;
    free(tmp);

    return cmd_dirs( srcfil , params );
  }else{
    printf("dirs : directory stack is empty!\n");
    return 0;
  }
}
static int cmd_ls( FILE *srcfil, Params &params )
{  return params.call_as_main(eadir);  }
static int cmd_dir( FILE *srcfil, Params &params )
{  return params.call_as_main(eadir);  }
static int cmd_eadir( FILE *srcfil, Params &params )
{  return params.call_as_main(eadir);  }
static int cmd_exit( FILE *srcfil, Params &params )
{
  return RC_QUIT;
}
static int cmd_set( FILE *srcfil, Params &params )
{
  if( params.get_argc() < 2 )
    return RC_HOOK;

  const char *parameter = params.get_parameter();
  if( parameter == NULL )
    return RC_HOOK;

  int ch;
  char envname[1024],*dp=envname;

  /* 変数名の前の空白のスキップ */
  while( *parameter!='\0' && isspace(*parameter) )
    parameter++;

  /* 変数名のコピ－ */
  while( *parameter != '=' && !isspace(*parameter & 255 ) ){
    if(   *parameter=='\0' || *parameter=='&' 
       || *parameter=='>'  || *parameter=='|' ){
      /* 変数名がない ---> 画面表示のみ */
      return RC_HOOK;
    }else if( *parameter=='<' ){
      fputs("You cannot input-redirect on command set.\n",stderr);
      return 1;
    }
    *dp++ = toupper(*parameter) , parameter++;
  }

  /* 変数名～「=」の空白のスキップ */
  while( *parameter != '=' ){
    if( *parameter=='&'  || *parameter=='\0'  ||  *parameter=='|' 
       || *parameter == '>' ){

      return RC_HOOK;
    }else if( *parameter == '<' ){
      fputs("You cannot input-redirect on command set.\n",stderr);
      return 1;
    }

    if( !isspace(*parameter & 255) ){
      fputs("Invalid Argument.\n",stderr);
      return -1;
    }
    parameter++;
  }
  /* 「=」のスキップ */
  *dp++ = *parameter++;
  
  /* 「=」～引数の空白を削除 */
  while( *parameter != '\0'  && isspace(*parameter & 255 ) )
    parameter++;

  /*  右辺値のコピ－ */
  while( *parameter != '\0' ){
    if( *parameter == '%' ){
      char refenv[256] , *dp2=refenv;
      for(;;){
	++parameter;
	if( *parameter == '%' ){
	  ++parameter;
	  break;
	}else if( *parameter == '\0' ){
	  break;
	}else{
	  *dp2++ = toupper(*parameter);
	}
	if( dp2 >= refenv+sizeof(refenv)-2 )
	  break;
      }
      *dp2 = '\0';
      const char *sp2=getenv(refenv);
      if( sp2 != NULL ){
	while( *sp2 != '\0' )
	  *dp++ = *sp2++;
      }
    }else{
      *dp++ = *parameter++;
    }
  }

  *dp = '\0';
  
  /* 末尾の空白を除いておく */
  while( isspace( *(dp-1) & 255 ) ){
    *--dp = '\0';
  }
  putenv( strdup(envname) );
  
  return 0;
}

static int cmd_source( FILE *srcfil, Params &params )
{
  if( params.get_argc() < 2 )
    return 0;

  static int limitter=0;
  if( limitter > 5 ){
    fputs( "Too many source command nesting.\n" , stderr);
    return 0;
  }
  limitter++;

  char *fname=(char*)alloca(params.get_length(1)+1);
  params.copy(1,fname);

  char *cmdname=(char*)alloca(params.get_length(1)+5);
  sprintf(cmdname,"%s.cmd",fname);

  FILE *fp;
  char buffer[1024];

  if(   (fp=fopen(fname,"r"))   == NULL
     && (fp=fopen(cmdname,"r")) == NULL 
     && (_path(buffer,fname),  fp=fopen(buffer,"r"))==NULL
     && (_path(buffer,cmdname),fp=fopen(buffer,"r"))==NULL ){

    fprintf(stderr,"source: %s: no such file \n",fname);
    limitter--;
    return 0;
  }
  while( fgets_chop(buffer,sizeof(buffer),fp) != NULL ){
    if( execute(fp,buffer) == RC_QUIT )
      break;
  }
  fclose(fp);
  limitter--;
  return 0;
}

static int cmd_cursor( FILE *fp, Params &params)
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

static int cmd_echo(FILE *srcfil, Params &params )
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
      if( _nls_is_dbcs_lead(*sp) ){
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
	  if( isdigit(*sp) ){
	    int n = (*sp++ - '0');
	    for(int i=0 ; i<3 && isdigit(*sp) ; i++ ){
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
static int cmd_lecho(FILE *source, Params &params )
{
  for(int i=0 ; i<params.get_argc() ; i++ ){
    char argv[256];
    params.copy(i,argv);
    printf("[%s] ",argv);
  }
  printf("\n");
  return 0;
}

static struct {
  const char *name;
  int (*func)( FILE *srcfil, Params &params );
} jumptable[]={
  {"alias",  cmd_alias   },
  {"cd",     cmd_chdir   },
  {"chdir",  cmd_chdir   },
  {"comment",cmd_comment },
  {"cursor", cmd_cursor  },
  {"dirs",   cmd_dirs    },
  {"eadir",  cmd_eadir   },
  {"echo",   cmd_echo    },
  {"exit",   cmd_exit    },
  {"foreach",foreach     },
  {"ls",     cmd_ls      },
  {"option", cmd_option  },
  {"pwd",    cmd_pwd     },
  {"popd",   cmd_popd    },
  {"pushd",  cmd_pushd   },
  {"set",    cmd_set     },
  {"source", cmd_source  },
  {"lecho",  cmd_lecho   },
  {"unalias",cmd_unalias },
}, *hashtable[512];

int wrdcmp(const char *s1,const char *s2)
{
  /* s1は、NULで終わっていなければいけないが、
   * s2は、アルファベット文字以外ならば問題ない
   * また、大文字小文字の区別をしない
   */
  
  while( *s1 != 0 ){
    if( toupper(*s1) != toupper(*s2) )
      return *s1-*s2;

    /* 漢字ならば、2byte目を toupper越しに比較してはいけない */
    if( _nls_is_dbcs_lead( *s1 ) ){
      if( *++s1 != *++s2 )
	return *s1-*s2;
    }
    ++s1;++s2;
  }

  /* s2に続きがあるとき、つまり、
   * setemx とかいうふうに続いている場合、
   * 同じと判定させない
   */

  if( *s2 != 0  &&  !isspace(*s2 & 255) )
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

    for(int i=0;i<numof(jumptable);i++){
      int key=0;
      const char *p=jumptable[i].name;
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
    if( !isspace(*cmdline & 255) )
      break;
    if( _nls_is_dbcs_lead(*cmdline & 255) )
      ++cmdline;
    ++cmdline;
  }

  if( cmdline[0] == '-' )
    return system(cmdline+1);
  else if( cmdline[0] == '#' )
    return 0;

  /* カレントドライブの変更 */
  if( isalpha(cmdline[0]) && cmdline[1]==':' && cmdline[2]=='\0' ){
    _chdrive(cmdline[0]);
    return 0;
  }

  /* エイリアスの置換処理 */
  char alias_replaced_buffer[1024];
  alias_replace( cmdline , alias_replaced_buffer );
  cmdline = alias_replaced_buffer;
  
  /* 環境変数の置換処理 */
  char env_replaced_buffer[1024];
  replace_envvar( cmdline , env_replaced_buffer );
  cmdline = env_replaced_buffer;
  
  Params params(cmdline);

  int key=0;
  {/* ハッシュキーを計算する */
    int size=params.get_length(0);
    const char *sp=params.get_argv(0);
    while( size-- ){
      key += tolower(*sp);
      sp++;
    }
  }

  key %= numof(hashtable);
  /* 内蔵コマンド --> Open hash */
  while( hashtable[key] != NULL ){
    if(   hashtable[key]->name[0] == params.get_argv(0)[0]
       && wrdcmp(hashtable[key]->name, params.get_argv(0) )==0 ){	

      int rc=(*hashtable[key]->func)(srcfil,params);
      if( rc == RC_HOOK ){
	break;
      }else{
	if( rc == 0  &&  *params.get_tail() == '&' )
	  return execute( srcfil , params.get_tail()+1 );
	return rc;
      }
    }
    if( ++key > numof(hashtable) )
      key = 0;
  }

  replace_script( cmdline , alias_replaced_buffer );
  cmdline = alias_replaced_buffer;

  if( echoflag )
    puts( cmdline );
    
  return system( cmdline );
}
