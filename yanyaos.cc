#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSSESMGR
#include <os2.h>
#include "macros.h"
#include "errmsg.h"
#include "strbuffer.h"

// #define DEBUG1(x) (x)
#define DEBUG1(x) /**/

int option_vmax=0;

#if defined(VMAX) && !defined(S2NYAOS)

extern int SearchEnv(const char *fname,const char *env,char *path);

/* PMプログラムを別セッションにて実行する。
 *	program	プログラム名
 *	argv	引数(プログラム名を含まない、単一文字列)
 */
static void startPMsession( const char *progname , const char *argv)
     throw()
{
  char errlog[ 32 ];
  STARTDATA sd;

  DEBUG1( fputs("startPMsession\n",stderr) );

  sd.Length		= 32;
  sd.Related		= SSF_RELATED_INDEPENDENT;
  sd.FgBg		= SSF_FGBG_FORE;
  sd.TraceOpt		= SSF_TRACEOPT_NONE;
  sd.PgmTitle		= NULL;
  sd.PgmName		= (PUCHAR)progname;
  sd.PgmInputs		= (PUCHAR)argv;
  sd.TermQ		= 0;
  sd.Environment	= 0;
  sd.InheritOpt		= SSF_INHERTOPT_PARENT;
  sd.SessionType	= SSF_TYPE_PM;
  sd.IconFile		= 0;
  sd.PgmHandle		= 0;
  sd.PgmControl		= 0;
  sd.InitXPos		= sd.InitYPos	= 0;
  sd.InitXSize		= sd.InitYSize	= 0;
  sd.Reserved		= 0;
  sd.ObjectBuffer	= (PUCHAR)errlog;
  sd.ObjectBuffLen	= sizeof(errlog);
  
  ULONG sessID;
  PID   pid;

  DosStartSession( &sd , &sessID , &pid );
}

/* VIO プログラムを実行する。
 *	progname プログラム名
 *	argv	 引数(「プログラム名\0真引数\0\0」形式)
 */
static int runVIOprogram(  const char *progname , const char *argv
			 , int execFlag=EXEC_SYNC )
{
  DEBUG1( fputs("runVIOprogram\n",stderr) );

  RESULTCODES retval;
  char errlog[ 32 ];
  int rc=DosExecPgm(  errlog
		    , sizeof(errlog)
		    , execFlag
		    , (unsigned char*)argv
		    , NULL
		    , &retval
		    , (unsigned char*)progname );

  return rc ? rc : retval.codeResult;
}

/* 空白で区切られた単語を一つ切り出す
 */
char *readWord(const char *&sp) throw(MallocError)
{
  bool quote=false;
  while( *sp != '\0'  &&  isspace(*sp & 255) ) // skip space
    ++sp;
  StrBuffer buf;
  while( *sp != '\0' && ( !isspace(*sp & 255) || quote ) ){
    if( *sp == '"' ){
      quote = !quote;
    }else if( !quote  &&  (*sp=='|' || *sp=='&') ){
      break;
    }else{
      buf << *sp;
    }
    ++sp;
  }
  return buf.finish();
}

struct AmbiguousRedirect {
  int fd;
  AmbiguousRedirect(int n) : fd(n) { }
};

struct RedirectSyntaxError {
  int fd;
  RedirectSyntaxError(int n) : fd(n) { }
};

class StdioFd {
  int fd[3];
public:
  StdioFd(){ fd[0] = fd[1] = fd[2] = -1; }
  ~StdioFd(){
    if( fd[0] != -1 ){ dup2(fd[0],0); close(fd[0]); }
    if( fd[1] != -1 ){ dup2(fd[1],1); close(fd[1]); }
    if( fd[2] != -1 ){ dup2(fd[2],2); close(fd[2]); }
  }
  void copy(int org,int no){
    if( fd[no] == -1 )
      fd[no] = dup(no);
    dup2( org , no );
  }
  void move(int org,int no){
    copy(org,no);
    close(org);
  }
};

enum{
  APPEND_MODE	= 1,
  JOIN_MODE	= 4,
  
  APPEND_STDOUT		= 1 ,
  APPEND_STDERR		= 2 ,
  STDOUT_TO_STDERR	= 4 , /* 1>&2 */
  STDERR_TO_STDOUT	= 8 , /* 2>&1 */
};

/* 
 *	sp '>' の位置にあるとする。
 */

static int read_output_redirect(const char *&sp , char *&out , int fd)
     throw( AmbiguousRedirect )
{
  int flag=0;
  if( out != NULL )
    throw AmbiguousRedirect(fd);
  
  if( *++sp == '>' ){
    flag |= APPEND_MODE;
    ++sp;
  }
  if( *sp == '&' ){ /* 1>&2 or 2>&1 */
    ++sp; // skip '&'
    if( fd==1  &&  *sp != '2' )
      throw RedirectSyntaxError(1);
    if( fd==2  &&  *sp != '1' )
      throw RedirectSyntaxError(2);
    ++sp; // skip ffd
    flag |= JOIN_MODE;
  }else{
    out = readWord( sp );
  }
  return flag;
}


/* 一命令をバスっと、本体とリダイレクトの記述に分離してしまう
 * in:	sp	パース前のコマンド文字列。
 *		\0 だけでなく、| や & で終わっていてもよい。
 * out:	argv	コマンド名'\0'引数"\0\0"
 *	in	入力リダイレクト先
 *	out	出力リダイレクト先(&で始まる場合は、ファイルハンドルのASCII)
 *	err	エラーリダイレクト先 (&で始まる場合は、ファイルハンドル)
 *	flag	リダイレクト関係の状況
 */
static void parseOne(  const char *&sp 
		     , char *&argv
		     , int   &off
		     , char *&in 
		     , char *&out 
		     , char *&err
		     , int   &flag
		     ) 
     throw( MallocError , AmbiguousRedirect , RedirectSyntaxError )
{
  argv = in = out = err = NULL;
  flag = 0;

  /* コマンド名の読み取り */
  char *name = readWord(sp);

  StrBuffer buf;
  buf << name << '\0';
  free(name);

  off = buf.getLength();
  
  bool quote=false;
  while( *sp != '\0' ){
    if( *sp == '"' ){
      quote = !quote;
      buf << *sp++;
    }else if( !quote && *sp == '<' ){
      if( in != NULL )
	throw AmbiguousRedirect(0);
      in = readWord(++sp);
    }else if( !quote && *sp == '>' ){
      flag |= read_output_redirect( sp , out , 1 );
    }else if( !quote && *sp == '0' && *(sp+1) == '<' ){
      if( in != NULL )
	throw AmbiguousRedirect(0);
      in  = readWord( sp+=2 );
    }else if( !quote && *sp == '1' && *(sp+1) == '>' ){
      flag |= read_output_redirect( ++sp , out , 1 );
    }else if( !quote && *sp == '2' && *(sp+1) == '>' ){
      flag |= read_output_redirect( ++sp , err , 2 ) << 1 ;
    }else if( !quote && *sp=='|' ){
      if( out != NULL )
	throw AmbiguousRedirect(1);
      break;
    }else if( !quote &&  *sp == '&' ){
      break;
    }else{
      buf << *sp++;
    }
  }
  buf << '\0';
  argv = buf.finish();
}


/* 単一のコマンドを実行する。
 *	sp0～sp0+len	… コマンドの範囲。\0 で終わってなくてもよい。
 * 	execFlag	… DosExecPgm に渡す引数
 * return
 *	≧ 0 : 実行したプログラムの結果コード
 *	-1 : コマンド名、または、ファイル名が違います。
 *	-2 : 標準出力の出力先があいまいです。
 *	-3 : 標準入力のリダイレクトすべきファイルが存在しない。
 *	-4 : 標準出力をリダイレクトできない。
 *	-5 : 標準エラー出力をリダイレクトできない。
 */

static int runSimpleCommand(  const char *sp , int len 
			    , unsigned long execFlag=EXEC_SYNC )
     throw(MallocError,AmbiguousRedirect,RedirectSyntaxError)
{
  int rc=0 , off , append = 0;
  char *argv=0 , *red[3]={0,0,0}; /* パラメータとリダイレクト先ファイル名 */

  DEBUG1( fputs("runSimpleCommand: enter\n",stderr) );

  try{
    parseOne(sp , argv , off , red[0],red[1],red[2] , append);
  }catch(...){
    free(red[0]),free(red[1]),free(red[2]);
    throw;
  }

  DEBUG1( fputs("runSimpleCommand: parseOne finish\n",stderr) );

  StdioFd stdioFd;
  char fullpath[ FILENAME_MAX ];
  
  try{
    int type=SearchEnv( argv , "PATH" , fullpath );
    if( type == NO_FILE ){
      ErrMsg::say( ErrMsg::BadCommandOrFileName , argv , 0);
      rc = -1 ; 
      goto exit;
    }else if( type == CMD_FILE ){
      /* バッチファイルの場合は、CMD.EXE に実行させるために
       * 頭に「CMD /C」を挿入する */
      StrBuffer cmdline;
      cmdline << "CMD.EXE" << '\0';
      int off2 = cmdline.getLength();
      cmdline << "/C " << fullpath << ' ' << argv+off << '\0';
      
      free(argv);
      argv = cmdline.finish();
      off  = off2;
      
      strcpy( fullpath , "CMD.EXE" );
    }

    /* リダイレクト処理 */
    if( red[0] ){ /* 標準入力のリダイレクト */
      int fd=open(red[0],O_RDONLY,S_IREAD|S_IWRITE);
      if( fd == -1 ){
	rc = -3; goto exit;
      }
      stdioFd.move(fd,0);
    }
    if( append & STDOUT_TO_STDERR ){ /* 1>&2 */
      fputs("append & STDOUT_TO_STDERR\n",stderr);
      stdioFd.copy(2,1);
    }
    if( append & STDERR_TO_STDOUT ){ /* 2>&1 */
      fputs("append & STDERR_TO_STDOUT\n",stderr);
      stdioFd.copy(1,2);
    }
    for(int i=1;i<=2;i++){
      if( red[i] ){ /* 標準出力のリダイレクト */
	int fd=open(  red[i]
		    , O_WRONLY | O_BINARY 
		    | ((append & i) ? O_APPEND : (O_CREAT | O_TRUNC))
		    , S_IREAD | S_IWRITE );
	if( fd == -1 ){
	  rc = -4; goto exit;
	}
	stdioFd.move(fd,i);
      }
    }

    /* プログラムを実際に呼び出す */
    ULONG apptype;
    DosQueryAppType( (PSZ)argv , &apptype );
    if( (apptype & 3) == 3 ){
      startPMsession( fullpath , argv+off );
    }else{
      rc=runVIOprogram( fullpath , argv , execFlag );
    }
  }catch(...){
    free( argv ) , free(red[0]) , free(red[1]) , free(red[2]);
    throw;
  }
 exit:
  /* リダイレクトで使ったファイルハンドルの後始末 */
  free( argv ) , free(red[0]) , free(red[1]) , free(red[2]);
  DEBUG1( fputs("leave runSimpleCommand\n",stderr) );
  return rc;
}

static int runPipeLines( const char *&sp , char *demilitor )
{
  DEBUG1( fputs("Enter runPipeLines()\n",stderr) );
  int save_stdin=-1;

  bool quote=false;
  const char *top=sp;
  while( *sp != '\0' ){
    if( *sp == '"' ){
      quote = !quote;
    }else if( !quote && strchr(demilitor,*sp) != NULL ){
      break;
    }else if( !quote  &&  *sp == '|' ){
      int handle[2];

      if( pipe( handle ) != 0 )
	return -1;
      DEBUG1( fputs("make pipe success\n",stderr) );

      if( fork()==0 ){ /* 「A | B」 の A 側 */
	dup2( handle[1] , 1 );
	close( handle[1] );
	close( handle[0] );
	runSimpleCommand( top , sp-top );
	close( 1 );
	exit(0);
      }
      top = sp+1;
      
      if( save_stdin == -1 )
	save_stdin = dup(0);
      
      dup2( handle[0] , 0 );
      close( handle[0] );
      close( handle[1] );
    }
    ++sp;
  }
  int rc=runSimpleCommand(top,sp-top);
  if( rc != 0 ){
    fprintf(stderr,"errcode = %d\n",rc);
    perror("???");
  }
  if( save_stdin != -1 ){
    dup2( save_stdin , 0 );
    close( save_stdin );
  }
  DEBUG1( fputs("Leave runPipeLines.\n",stderr) );

  PID pid;
  RESULTCODES retval;
  DosWaitChild(DCWA_PROCESS,DCWW_WAIT,&retval,&pid,0);
  return 0;
}
#endif

void cocked_mode(void);

int yanyaos( const char *s )
{
#if defined(S2NYAOS) || !defined(VMAX)
  return 0;
#else
  return runPipeLines( s , "&\0" );
#endif
}
