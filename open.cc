#define INCL_WINWORKPLACE

#include <os2.h>

/* for stat() */
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h> /* for A_DIR */

#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "macros.h"
#include "nyaos.h"
#include "parse.h"

#if 0
enum{
  HAVE_ROOT = 1,
  HAVE_DOT  = 2,
};

int how_filename(const char *p)
{
  int rc=0;
  for(; *p != '\0' ; p++){
    if( *p == ':' || *p=='\\' || *p=='/' ){
      rc = HAVE_ROOT;
    }else if( *p == '.' ){
      rc |= HAVE_DOT;
    }
  }
  return rc;
}
#endif

int eadir( int argc, char **argv,FILE *fout=stdout);
int wrdcmp(const char *s1,const char *s2);

static int print_file(char *s)
{
  char *eadir_argv[]={
    "eadir",
    s,
    NULL,
  };

  struct stat statbuf;
  stat(s ,&statbuf);
  
  if( statbuf.st_attr & A_DIR ){
    return 1;
  }else{
    eadir(2,eadir_argv);
    return 0;
  }
}

int cmd_which( FILE *source , Parse &params )
{
  int argc = params.get_argc();

  if( argc < 2 ){
    fprintf(stderr,"nyaos: which: usage ...which command-name\n\n");
    return 0;
  }

  char buffer[FILENAME_MAX];
  
  for(int i=1;i<argc;i++){
    int len=params.get_length(i);
    // params.get_length() はどうも、怪しい。そのうち、要チェックである。

    char *arg =(char *)alloca(len+5);
    char *arg2=(char *)alloca(len+10);
    params.copy(i,arg);

    char replace_buffer1[FILENAME_MAX];
    char replace_buffer2[FILENAME_MAX];
    
    alias_replace( arg , replace_buffer1 );
    replace_script( replace_buffer1 , replace_buffer2 );

    char *sp=replace_buffer2; /* 置換後のコマンドライン全体が入っている   */
    char *dp=replace_buffer1; /* 置換後のコマンド名のみを入れる(これから) */
    while( *sp != '\0' && !is_space(*sp) )
      *dp++ = *sp++;
    *dp = '\0';

    if( strcmp(replace_buffer1,arg) != 0 ){
      printf("replaced to `%s'\n",replace_buffer2 );
      continue;
    }

    /* 内部コマンドの検索 */
    for(const struct commandtable_tag *p=jumptable ; p->name != NULL ; p++ ){
      if(   tolower(arg[0])==p->name[0]
	 && wrdcmp(p->name,arg)==0 ){
	printf( "%s: nyaos built-in command\n",arg);
	goto next;
      }
    }
    
    /* 実行ファイルの検索 */
    if( _path(buffer,arg)!=0  ||  print_file(buffer)!=0 ){

      /* そのままで見付からない場合は、拡張子を付けてみる。*/
      const static char *suffix_list[]={
	"CMD","EXE","COM",NULL,
      };
      
      for(const char **p=suffix_list ; *p != NULL ; p++ ){
	sprintf(arg2,"%s.%s",arg,*p);
	if( _path(buffer,arg2)==0  &&  print_file(buffer)==0 )
	  goto next;
      }
      printf( "%s :not found %s.\n"
	     , arg 
	     , scriptflag ? "in PATH and SCRIPTPATH" : "in PATH" );
    }
  next:
    ;

    /* このみっともない、ラベル、どーにか、ならんかなぁー。
     * C++ が、ループに対するラベルを認めて、
     * 「continue ラベル」させてくれたら、万事解決なんだが...
     */
  }
  return 0;
}

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
      char *fname=(char*)alloca(params.get_length(i)+3);
      char absfname[512];
      char *p=absfname;

      params.copy(i,fname);
      _abspath( absfname , fname , sizeof(absfname) );

      char *lastp=NULL , *last2p=NULL;
      while( *p != '\0' ){
	last2p = lastp;
	lastp  = p;

	if( *p == '/' )
	  *p = '\\';
	
	if( is_kanji(*p) )
	  p++;
	p++;
      }
      *p = '\0';

      if(   lastp != NULL  &&  *lastp  == '\\' 
	 && last2p != NULL &&  *last2p != ':'  ){
	*lastp = '\0' ;
      }
      
      fprintf(fout,"open %s\n", absfname );

      HOBJECT hObject=WinQueryObject( (PSZ)absfname );
      WinOpenObject( hObject , number , flag );
    }
  }
  return 0;
}
