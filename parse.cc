#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <sys/nls.h>

#include "parse.h"

void Pipe::open(const char *cmdl,const char *modestr)
{
  mode = modestr;
  cmdline = cmdl;

  if( _osmode == OS2_MODE ){
    fp=popen(cmdline,modestr);
    return;
  }

  tmpfname = tempnam("C:/","NYA");
  assert(tmpfname != NULL );

  if( mode[0] == 'r' ){
    char buffer[1024];
    sprintf(buffer , "%s >%s",cmdline,tmpfname);
    system(buffer);
  }
  fp=fopen(tmpfname,modestr);
}

Pipe::~Pipe()
{
  if( fp == NULL )
    return;

  if( _osmode == OS2_MODE ){
    pclose(fp);
    return;
  }
  
  fclose(fp);

  if( mode != NULL && mode[0] == 'w' ){
    char buffer[1024];
    sprintf(buffer , "%s <%s",cmdline,tmpfname);
    system(buffer);
  }
}

Parse::~Parse()
{
  /* パイプの後始末 */
  if( input_fp != NULL  &&  input_fp != stdin )
    fclose(input_fp);
  if( output_fp != NULL  &&  output_fp != stdout ){
    if( pipemode == REDIRECT )
      fclose(output_fp);
    else
      pclose(output_fp);
  }
  /* 引数の後始末 */
  if( args != argbase  &&  args != NULL )
    free(args);
}

int Parse::check_redirect()
{
  int mark=*sp;
  if( *++sp == '>' ){
    isappend = true;
    ++sp;
  }
  while( isspace(*sp) )
    ++sp;

  const char *top=sp;

  if( *sp == '&' || *sp == '|'  ||  *sp == '\0' )
    return -1;

  do{
    if( _nls_is_dbcs_lead(*sp & 255) )
      ++sp;
    ++sp;
    if( *sp=='"' ){
      do{
	++sp;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
      sp++;
    }
  }while( *sp != '\0'  &&  !isspace(*sp & 255) );
 exit:
  switch( mark ){
  case '>':
    output_redirect = top;
    output_redirect_length = sp-top;
    break;
  case '<':
    input_redirect = top;
    input_redirect_length = sp-top;
    break;
  }
  return 0;
}

int Parse::check()
{
  argc = 0;
  output_redirect = NULL;
  input_redirect = NULL;
  isappend = false;
  terminal = -1;

  for(;;){
    /* 空白を読み飛ばす */
    while( *sp != '\0'  &&  isspace(*sp) )
      ++sp;

    if( *sp == '&' || *sp == '|' || *sp == '\0' )
      return terminal = *sp;

    if( *sp == '<' || *sp == '>' ){
      int rc=check_redirect();
      if( rc != 0 )
	return -1;
      continue;
    }
    
    if( argc+1 >= limit ){
      if( args != argbase ){
	args = (Array*)malloc(sizeof(Array)*(limit += 30) );
	assert( args != NULL );
	memcpy( args , argbase , sizeof(argbase) );
      }else{
	args = (Array*)realloc( args , sizeof(Array)*(limit += 30) );
	assert( args != NULL );
      }
    }
    args[ argc ].pointor = sp;

    while( !isspace( *sp & 255 ) ){
      
      if( *sp == '\0' || *sp=='|' || *sp=='&' ){
	goto exit;
      }else if( *sp == '^' && *(sp+1) != '\0' ){
	/* キャレットはヌル以外の次の機能文字を無効化する。*/
	if( _nls_is_dbcs_lead(*++sp & 255) )
	  sp++;
	sp++;
      }else if( *sp == '"' ){
	/* クォ－トは次のクォ－トが来るまで、
	 * ヌルとクォ－ト以外の全ての機能文字を無効化する。
	 * キャレットも無効化される。
	 */
	do{
	  if( _nls_is_dbcs_lead(*sp & 255) )
	    ++sp;
	  ++sp;
	  if( *sp=='\0' )
	    goto exit;
	}while( *sp != '"' );
      }

      /* ANKなら2byte,漢字なら1byteずらす */
      if( _nls_is_dbcs_lead(*sp & 255) )
	++sp;
      ++sp;
    }

    args[ argc ].length = sp - args[argc].pointor;
    ++argc;
  }
 exit:
  args[ argc ].length = sp - args[argc].pointor;
  ++argc;
  return terminal = *sp;
}

FILE *Parse::open_stdout()
{
  if( output_redirect != NULL ){
    if( *sp == '|' )
      return NULL;
    
    char *fname = (char*)alloca( output_redirect_length+1 );
    memcpy(fname,output_redirect , output_redirect_length );
    fname[ output_redirect_length ] = '\0';
    pipemode = REDIRECT;
    return output_fp = fopen( fname , isappend ? "a" : "w" );
  }else if( *sp == '|' ){
    pipemode = PIPE;
    return output_fp = popen( sp+1 , "w" );
  }else
    return stdout;
}

FILE *Parse::open_stdin()
{
  if( input_redirect != NULL ){
    char *fname = (char*)alloca( input_redirect_length+1 );
    memcpy(fname,input_redirect, input_redirect_length );
    fname[ input_redirect_length ] = '\0';

    return input_fp = fopen( fname , "r" );
  }else 
    return stdin;
}

int Parse::call_as_main(int (*routine)(int argc,char **argv) )
{
  int i;
  char **argv=(char**)alloca(sizeof(char*)*(argc+1));
  for(i=0;i<argc;i++){
    argv[i]=(char *)alloca(args[i].length+1);
    copy(i,argv[i]);
  }
  argv[i] = NULL;
  return (*routine)(argc,argv);
}

int Parse::call_as_main(int (*routine)(int argc,char **argv,FILE *fout))
{
  int i;
  char **argv=(char**)alloca(sizeof(char*)*(argc+1));
  for(i=0;i<argc;i++){
    argv[i]=(char *)alloca(args[i].length+1);
    copy(i,argv[i]);
  }

  argv[i] = NULL;
  if( _osmode != OS2_MODE )
    return (*routine)(argc,argv,stdout);

  FILE *fout=open_stdout();
  if( fout==NULL ){
    fputs("nyaos : cannot make file or pipe.\n",stderr);
    return -1;
  }
  
  return (*routine)(argc,argv,fout);
}

char *Parse::copy(int n, char *dp, bool quote_copy_flag )
{
  if( n < argc ){
    const char *sp   = args[n].pointor ;
    const char *tail = sp + args[n].length ;

    /* 基本的に引用符とキャレットはコピ－しない。
     * 二重キャレット「^^」は「^」としてコピ－する。
     * ただし、引用符に囲まれたキャレットはそのままコピ－する。
     */

    bool quote=false;

    while( sp < tail ){
      
      if( *sp == '"' ){
	/* 引用符の場合は、フラグを反転させて、ポインタを進めるだけ。*/
	quote = !quote;
	++sp;
	if( quote_copy_flag )
	  *dp++ = '"';

      }else if( *sp == '^' && *(sp+1) != '\0' && !quote ){
	/* 引用符の中ではない、キャレットは、ポインタを進めるだけ。
	 * 二重キャレットは「^」としてコピーする。*/
	if( *++sp == '^' )
	  *dp++ = *sp++;
      }else{
	/* それ以外はコピ－ */
	if( _nls_is_dbcs_lead(*sp & 255 ) ){
	  *dp++ = *sp++;
	}
	*dp++ = *sp++;
      }
    }
    *dp = '\0';
  }
  return dp;
}

char *Parse::copyall(int n, char *dp, bool quote_copy_flag )
{
  if( n < argc ){
    
    /* 基本的に引用符は普通の文字と同様にコピ－する。
     * ただし、キャレットは引用符の中にない限りコピ－しない。
     * 二重キャレット「^^」は「^」としてコピ－する。
     */
    
    const char *ssp=args[n].pointor;
    bool quote=false;

    while( ssp < sp ){
      if( *ssp == '"' ){
	/* 引用符は、フラグを反転させる。*/
	quote = !quote;
	++ssp;
	if( quote_copy_flag )
	  *dp++ = '"';

      }else if( *ssp=='^' && !quote ){
	/* 引用符の中にないキャレットはポインタを進めるだけ。
	 * ただし、二重キャレットは「^」としてコピーする。
	 */
	if( *++ssp == '^' )
	  *dp++ = *ssp++;
	
      }else{
	/* それ以外はコピ－ */
	if( _nls_is_dbcs_lead(*ssp & 256) ){
	  *dp++ = *ssp++;
	}
	*dp++ = *ssp++;
      }
    }
    *dp = '\0';
  }
  return dp;
}

#if 0

int main(void)
{
  char buffer[256];

  while( fgets(buffer,sizeof(buffer),stdin) != NULL ){
    Parse params(buffer);
    int argc=params.get_argc();
    printf( "argc == %d\n",argc);
    for(int i=0 ; i<argc ; i++ ){
      char args[128];
      params.copy(i,args);
      printf("argv[%d]==\"%s\"\n",i,args);
    }
    putchar('\n');
  }
  return 0;
}
#endif
