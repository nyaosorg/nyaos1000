#define INCL_DOSMEMMGR
#define INCL_RXSUBCOM
#define INCL_RXFUNC
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include "nyaos.h"
#include "edlin.h"
#include "parse.h"

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
  ShellEdlin edlin( ">" , (char*)result->strptr , (int)result->strlength );

  /* 次の行が無いとバグる。そのうち、なんとかしなくては... */
  edlin.setcursor( cursor_on_color_str , cursor_off_color_str );

  Shell shell(edlin);
  
  (void)shell.line_input( argc >= 1 ? (char*)RXSTRPTR(argv[0]) : "" , 32767 );
  result->strlength = strlen( (char*)result->strptr );
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
  
  RexxStart(  argc		/* argc */
	    , rx_argv		/* argv */
	    , (PUCHAR)progname	/* program name */
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

int cmd_source( FILE *srcfil, Parse &params )
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
  const char *finalname;

  if(   (fp=fopen(finalname=fname,"r"))   == NULL
     && (fp=fopen(finalname=cmdname,"r")) == NULL 
     && (_path(buffer,fname),  fp=fopen(finalname=buffer,"r"))==NULL
     && (_path(buffer,cmdname),fp=fopen(buffer,"r"))==NULL ){
    
    fprintf(stderr,"source: %s: no such file \n",fname);
    limitter--;
    return 0;
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
    while( rc != NULL ){
      if( execute(fp,buffer) == RC_QUIT )
	break;
      rc=fgets_chop(buffer,sizeof(buffer),fp);
    }
    fclose(fp);
  }
  limitter--;
  return 0;
}
