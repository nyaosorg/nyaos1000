#include <process.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>

#define INCL_DOSSESMGR
#define INCL_DOSPROCESS
#include <os2.h>

static int my_spawn(const char *sp,int mode)
{
  int limit=256;
  int argc=0;

  int len=strlen(sp);
  char *cmdline=(char*)alloca(len+10);
  char *dp=cmdline;
  
  for(;;){ /* 空白スキップ */
    if( *sp == '\0' ) return -1;
    if( !isspace(*sp & 255) ) break;
    ++sp;
  }
  for(;;){ /* プログラム名のコピー */
    if( *sp == '\0' || isspace(*sp & 255) ) break;
    *dp++ = *sp++;
  }
  *dp++ = '\0';

  /* その他の引数のコピー */
  while( *sp !='\0' )
    *dp++ = *sp++;
  *dp++ = '\0';
  *dp++ = '\0';

  char error_message[ 128 ]="\0";

  RESULTCODES rv;

  int rc;
  ULONG proctype;
  
  if( DosQueryAppType( (PUCHAR)cmdline , &proctype )==0 && (proctype & 3)==3 )
    mode = 4;
  
  rc=DosExecPgm(  error_message , sizeof( error_message )
		, mode , (PUCHAR)cmdline , (PUCHAR)NULL 
		, &rv , (PUCHAR)cmdline );
  return 0;
}

static int dup_and_call(  const char *p,int len
			, int (*func)(char *s,int mode),int mode)
{
  char *cmdline=(char*)alloca(len+1);
  memcpy(cmdline , p , len );
  cmdline[ len ] = '\0';
  return (*func)( cmdline , mode );
}

static int system_1(const char *s , int mode ) /* A | B | C | D */
{
  int rc=0;
  int org_stdin=-1;
  const char *p=s , *top=s;
  
  while( *p != '\0' ){
    if( *p == '|' ){
      int handle[2];
      int org_stdout=dup(1);

      pipe(handle);
      dup2(handle[1],1);
      close(handle[1]);

      rc = dup_and_call( top , p-top , my_spawn , 4 );
      dup2(org_stdout,1);
      close(org_stdout);

      if( org_stdin == -1 )
	org_stdin = dup(0);
      dup2(handle[0],0);
      close(handle[0]);
      top = p+1;
    }else if( *p == '&' ){
      rc = dup_and_call( top , p-top , my_spawn, 4 );
      top = p+1;
    }else if( *p == ';' ){
      rc = dup_and_call( top , p-top , my_spawn , mode );
      top = p+1;
    }
    p++;
  }
  rc = dup_and_call( top , p-top , my_spawn , mode );
  
  if( org_stdin != -1 ){
    dup2( org_stdin , 0 );
    close( org_stdin );
  }
  return 0;
}

int my_command(const char *s)
{
  return system_1(s,0);
}

int main( int argc, char **argv )
{
  for(int i=1;i<argc;i++){
    my_command( argv[i] );
  }
  return 0;
}

