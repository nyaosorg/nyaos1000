// #define INCL_DOSMEMMGR
#define INCL_RXSUBCOM
#define INCL_RXFUNC
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include "nyaos.h"
#include "edlin.h"
#include "parse.h"
#include "errmsg.h"

extern int convroot(char *&dp,int &size,const char *sp) throw(size_t);

static ULONG rexx_handler(PRXSTRING source, PUSHORT flags, PRXSTRING result)
{  
  if( execute(stdin,(const char*)RXSTRPTR(*source))==0 ){
    *flags = RXSUBCOM_OK;
  }else{
    *flags = RXSUBCOM_ERROR;
  }
  MAKERXSTRING(*result,"0",1);
  return 0;
}

static ULONG rexx_nyaos_input(PCSZ name,  ULONG argc,PRXSTRING argv,
			      PCSZ qname, PRXSTRING result )
{
  Shell shell;

  /* 次の行が無いとバグる。そのうち、なんとかしなくては... */
  shell.setcursor( cursor_on_color_str , cursor_off_color_str );
  
  const char *s;
  int rc=shell.line_input(  argc >= 1 ? (char*)RXSTRPTR(argv[0]) : ""
			  , "and.." , &s );

  if( rc > 0 ){
    strncpy( (char*)result->strptr , s , result->strlength );
    result->strlength = strlen( (char*)result->strptr );
  }else{
    result->strlength = 0;
    result->strptr    = (PUCHAR)"";
  }
  putchar('\n');
  
  return 0;
}

int do_rexx( const char *progname , LONG argc , RXSTRING *rx_argv )
{
  RexxRegisterSubcomExe((PUCHAR)"NYAOS",(PFN)&rexx_handler,(PUCHAR)NULL);
  RexxRegisterFunctionExe(  (PCSZ)"NyaosLinein"
			  , &rexx_nyaos_input );
  
  /* 帰り値の RX文字列 */
  SHORT rc;
  UCHAR return_buffer[256];
  RXSTRING rx_rc;

  MAKERXSTRING( rx_rc , return_buffer , sizeof(return_buffer) );
  
  int size=strlen(progname)+1;
  char *truename=(char*)alloca(size+1);
  char *dp=truename;
  
  /* / の ￥マークへの変換 */
  convroot(dp,size,progname);
  
  RexxStart(  argc		/* argc */
	    , rx_argv		/* argv */
	    , (PUCHAR)truename	/* program name */
	    , NULL		/* instore */
	    , (PUCHAR)"NYAOS" 	/* envname */
	    , RXCOMMAND		/* calltype */
	    , NULL		/* exit */
	    , (PSHORT)&rc	/* return code */
	    , &rx_rc );		/* result */
  
  if( RXSTRPTR(rx_rc) != return_buffer )
    DosFreeMem( RXSTRPTR(rx_rc) );
  return rc;
}

extern int source_history( const char *fname );

int cmd_source( FILE *srcfil, Parse &params )
{
  if( params.get_argc() < 2 ){
    ErrMsg::say(ErrMsg::TooFewArguments,"source",0);
    return 1;
  }

  static int limitter=0;
  if( limitter > 5 ){
    ErrMsg::say(ErrMsg::TooManyNesting,"source",0);
    return 0;
  }
  limitter++;

  char *fname=(char*)alloca(params.get_length(1)+1);
  params.copy(1,fname);

  if( fname[0] == '-' ){
    if( fname[1] != 'h' && fname[1] != 'e' ){
      ErrMsg::say(ErrMsg::UnknownOption,"source",0);
      return 1;
    }
    if( params.get_argc() < 3 ){
      ErrMsg::say(ErrMsg::TooFewArguments,"source -h",0);
      return 1;
    }
    
    char *arg=(char*)alloca(params.get_length(2)+1);
    params.copy(2,arg);
    
    if( fname[1] == 'h' ){
      /*
       *  ヒストリを読み込むモード 
       */
      
      if( source_history(arg) != 0 ){
	ErrMsg::say(ErrMsg::NoSuchFile,arg,0);
	return 1;
      }
    }else{
      /*
       * エラーメッセージを読み込むモード
       */

      int nmsgs = source_errmsg(arg);
      if( nmsgs < 0 ){
	ErrMsg::say(ErrMsg::NoSuchFile,arg,0);
	return 1;
      }
    }
    return 0;
  }
    
  char *cmdname=(char*)alloca(params.get_length(1)+5);
  sprintf(cmdname,"%s.cmd",fname);

  FILE *fp;
  char buffer[1024];
  const char *finalname;

  if(   (fp=fopen(finalname=fname,"r"))   == NULL
     && (fp=fopen(finalname=cmdname,"r")) == NULL 
     && (_path(buffer,fname),  fp=fopen(finalname=buffer,"r"))==NULL
     && (_path(buffer,cmdname),fp=fopen(buffer,"r"))==NULL ){
    
    ErrMsg::say(ErrMsg::NoSuchFile,fname,0);
    limitter--;
    return 1;
  }
  const char *rc=fgets_chop(buffer,sizeof(buffer),fp);
  if( buffer[0] == '/' && buffer[1]=='*' ){
    fclose(fp);

    /* 引数を RX文字列に変換する */
    LONG argc=params.get_argc()-2;
    RXSTRING *rx_argv = (RXSTRING *)alloca(sizeof(RXSTRING)*argc);
    for( int i=0 ; i<argc ; i++ ){
      int length=params.get_length(2+i);
      char *buf=(char*)alloca(length+1);
      params.copy(2+i,buf);
      MAKERXSTRING(rx_argv[i],buf,length);
    }

    int rc=do_rexx( finalname , argc , rx_argv );

    limitter--;
    return rc;
  }else{
    int nerrors=0;

    while( rc != NULL ){
      int rv=execute(fp,buffer);
      if( rv== RC_QUIT )
	break;
      if( rv != 0 )
	++nerrors;
      if( nerrors > 7 ){
	ErrMsg::say(ErrMsg::TooManyErrors,"source",0);
	break;
      }
      rc=fgets_chop(buffer,sizeof(buffer),fp);
    }
    fclose(fp);
  }
  limitter--;
  return 0;
}
