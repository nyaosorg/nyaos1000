#define INCL_WINWORKPLACE
#define INCL_DOSNLS
#include <os2.h>

#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "macros.h"
#include "parse.h"

int cmd_open( FILE *source , Parse &params )
{
  int argc = params.get_argc();
  int number = 0; /* OPEN する種類 */
  BOOL flag=TRUE; /* すでに open しているウインドウを利用するのか？*/

  FILE *fout=params.open_stdout();

  for(int i=1;i<argc;i++){
    const char *arg=params.get_argv(i);
    
    if( arg[0] == '-' ){
      switch( arg[1] ){
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
	number = atoi(arg+1);
	break;
	
      default:
	fprintf(fout,"open: bad option `%s'\n",arg);
	break;

      case 'p': /* プロパティーオプション */
	number = 2;
	break;

      case 'n': /* 新規ウインドウ */
	flag = FALSE;
	break;
      }
    }else{
      char *fname=(char*)alloca(params.get_length(i)+1);
      char absfname[512];
      char *p=absfname;

      params.copy(i,fname);
      _abspath( absfname , fname , sizeof(absfname) );
      int lastchar;
      while( *p != '\0' ){
	if( *p== '/' )
	  *p = '\\';
	lastchar = *p;
	if( is_kanji(*p) )
	  p++;
	p++;
      }
      if( lastchar == '\\' )
	*p++ = '.';
      
      fprintf(fout,"open %s\n", absfname );

      HOBJECT hObject=WinQueryObject( (PSZ)absfname );
      WinOpenObject( hObject , number , flag );
    }
  }
  return 0;
}

char dbcstable[256]; 

int dbcs_table_init()
{
  ULONG length;
  COUNTRYCODE country;
  char buffer[12];
  
  country.country = 0;
  country.codepage = 0;

  int rc=(int)DosQueryDBCSEnv((ULONG)numof(buffer)
			      ,&country
			      ,buffer);

  memset(dbcstable,0,256);
  
  char *p=buffer;
  while( (p[0]!=0 || p[1] !=0) && p < buffer+sizeof(buffer) ){
    /* printf("DBCS %02x--%02x\n",p[0] & 255 ,p[1] & 255); */
    memset(dbcstable+(p[0] & 255), 1 , (p[1] & 255)-(p[0] & 255)+1 );
    p += 2;
  }

  return rc;
}
