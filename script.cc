#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/nls.h>
#include "macros.h"

int scriptflag=1;
int option_amp_start=1;
int option_sos=0;

static int _copyterm( const char *&sp , char *&dp )
{
  if( *sp=='&' ){
    while( is_space(*++sp) )
      ;
    if( *sp =='&' ){  /* && の処理 */
      sp++; /* まず、'&' を読みとばす */
      const char *s="& if not errorlevel 1 ";
      while( *s != '\0' )
	*dp++ = *s++;
    }else if( *sp=='\0' ){
      *dp = '\0';
      return 1;
    }else{
      *dp++ = '&';
    }
    *dp = '\0';
    return 0;
  }
  if( *sp=='|' ){
    while( is_space(*++sp) )
      ;
    if( *sp == '|' ){
      sp++; /* | を読み飛ばす。*/
      const char *s="& if errorlevel 1 ";
      while( *s != '\0' )
	*dp++ = *s++;
      
    }else{
      *dp++ = '|';
    }
    *dp = '\0';
    return 0;
  }
  *dp = '\0';
  return 1;
}  

static int _copyargs( const char *&sp , char *&dp )
{
  for(;;){
    if( *sp == '"' ){
      do{
	if( is_kanji(*sp) ){
	  *dp++ = *sp++;
	  assert( *sp != '\0' );
	}
	*dp++ = *sp++;
	if( *sp == '\0' ){
	  *dp = '\0';
	  return 1;
	}
      }while( *sp != '"' );
    }
    
    if( *sp=='&'  ||  *sp=='|' ){
      *dp = '\0';
      return 0;
    }
    if( *sp=='\0'){
      *dp='\0';
      return 1;
    }
    if( is_kanji(*sp) )
      *dp++ = *sp++;
    *dp++ = *sp++;
  }
}

inline int copyargs( const char *&sp , char *&dp )
{
  return _copyargs(sp,dp) , _copyterm(sp,dp);
}

static int sos(int firstletter , const char *&sp , char *&dp ,
		const char *path , const char *ssp , FILE *fp )
{
  int lf=0,ch;
  for(;;){
    ch=getc(fp);
    if( ch=='\n' ){
      if( (ch=getc(fp)) != firstletter)
	return 1;
      if( ++lf==1 ){
	for( const char *s="soshdr/Nide" ; *s != '\0' ; s++ ){
	  if( getc(fp) != *s )
	    return 1;
	}
      }else if( lf >= 14 ) /* SOS.HDR は 13行 */
	break;
    }else if( !isprint(ch) || ch==EOF ){
      return 1;
    }
  }
  char args[256],*argp=args;
  sp = ssp;
  _copyargs(sp,argp);
  /* ここで、ポインタは、コマンド名の直前にあるはず */
  while( (ch=getc(fp)) != EOF && ch != '\n' ){
    if( ch != '%' ){
      *dp++ = ch;
    }else{
      switch( ch=getc(fp) ){
      case '0':
	for(const char *p=path; *p != '\0' ; p++ )
	  *dp++ = *p;
	break;
      case '@':
	for(const char *p=args; *p != '\0' ; p++ )
	  *dp++ = *p;
	break;
      case '%':
	*dp++ = '%';
	break;
      default:
	*dp++ = '%';
	*dp++ = ch;
	break;
      }
    }
  }
  _copyterm(sp,dp);
  return 0;
}

int replace_script( const char *sp , char *dp )
{
  for(;;){ /* パイプで区切られた各コマンドに関するループ */
    /* puts("1"); */
    while( is_space(*sp) )
      *dp++ = *sp++;

    FILE *fp;
    int ch;

    if( option_amp_start ){
      /* 先行して、末尾が & かどうかしらべる。
       * もし、そうならば先頭に「start」を追加する。
       */
      const char *p=sp;
      for(;;){
	switch( *p ){
	case '&':
	  while( is_space(*++p) )
	    ;
	  if( *p != '&' ){ /* 「&&」でない「&」なら start を挿入 */
	    char *s = "start ";
	    while( *s != '\0' )
	      *dp++ = *s++;
	  }
	  goto check_script;

	case '|':
	case '\0':
	  goto check_script;

	case '"':
	  do{
	    ++p;
	    if( *p == '\0' )
	      goto check_script;
	  }while( *p != '"' );
	  ++p;
	  break;

	default:
	  if( is_kanji(*p) ){
	    p+=2;
	  }else{
	    ++p;
	  }
	  break;
	}/* switch() */
      }/* for(;;) */
    } /* if option_amp... */

    /* スクリプトかどうかを調べるため、
     * まず、最初の単語を切り出している。
     */
    /* puts("3"); */

  check_script:
    char fname[FILENAME_MAX];
    const char *suffix=NULL;

    const char *ssp = sp;
    char *ddp = fname;
    for(;;){
      if( *ssp == '"' ){
	do{
	  *ddp++ = *ssp++;
	}while( *ssp != '\0' && *ssp != '"' );
      }
      if( *ssp == '\0' || is_space(*ssp) )
	break;
      if( *ssp=='.' && *(ssp+1) !='\0' ){
	suffix = ssp+1;
      }else if( *ssp=='\\' || *ssp=='/' ){
	suffix = NULL;
      }

      if( is_kanji(*ssp) ){
	*ddp++ = *ssp++;
	assert(*ssp != '\0');
      }
      *ddp++ = *ssp++;
    }
    *ddp = '\0';

    if(   scriptflag != 0 ){
      /****** スクリプト実行支援機能 ******/
	 
      char path[FILENAME_MAX];

      /* 普通の「#!」型 スクリプトファイル */
      if(   ( _searchenv(fname,"SCRIPTPATH",path) , path[0] != '\0' )
	 && (fp=fopen(path,"r")) != NULL ){
	
	int firstletter,secondletter;
      
	/* 普通のスクリプトの場合 */
	if(   (firstletter=getc(fp))  == '#' 
	   && (secondletter=getc(fp)) == '!' ){
	  
	  /* 環境変数 USRDRIVE の最初の一文字を複写 */
	  const char *usp;
	  if( (ch=getc(fp))=='/'  &&  (usp=getenv("SCRIPTDRIVE")) != NULL ){
	    while( *usp != '\0' && *usp != ':' )
	      *dp++ = *usp++;
	    *dp++ = ':';
	  }
	  
	  /* perlやawkなどの実行ファイル名の複写 */
	  while( ch != EOF  &&  ch != '\n' ){
	    if( ch == '/' )
	      *dp++ = '\\';
	    else
	      *dp++ = ch;
	    ch=getc(fp);
	  }
	  fclose(fp);
	  
	  *dp++ = ' ';
	  
	  /* スクリプト名の複写 */
	  for(const char *sp3=path ; *sp3 != '\0' ; sp3++ ){
	    *dp++ = *sp3;
	  }
	  *dp++ = ' ';
	  
	  /* 引数の複写 */
	  if( copyargs(ssp,dp) ==  1 ){
	    *dp = '\0';
	    return 0;
	  }
	  sp = ssp;
	  continue; /* 次のコマンドへ */

	  
	  /***** 拡張子 COM の付いた SOS スクリプトの場合 ****/

	}else if( option_sos && suffix != NULL
		 && (suffix[0]=='c' || suffix[0]=='C')
		 && (suffix[1]=='o' || suffix[1]=='O')
		 && (suffix[2]=='m' || suffix[2]=='M')
		 && (suffix[3]=='\0' || is_space(suffix[3])) ){
	  if( sos(firstletter,sp,dp,path,ssp,fp)==0 ){
	    fclose(fp);
	    continue;
	  }
	}
	fclose(fp);
      }/* end : _serchenv(そのまま,"SCRIPTPATH",path) */



      /* 拡張子 COM の付かない SOS スクリプトの場合 */
      if( option_sos && suffix == NULL ){
	FILE *_fp;
	strcat(fname,".COM");
	_searchenv(fname,"SCRIPTPATH",path);
	
	if( path[0] != '\0'  &&  (_fp=fopen(fname,"rt")) != NULL ){
	  int rc=sos(getc(_fp),sp,dp,path,ssp,_fp);
	  fclose(_fp);
	  if( rc==0 )
	    continue;
	}
      }
    }/* end : if( scriptflag != 0 ) */


    /******** スクリプトではない場合 *******/

#if 0
    /*** DOSVPATH ***/
    if( dosvpathflag != 0 ){
      
       && ( _searchenv(fname,"DOSVPATH",path) , path[0] != '\0' ) ){
      
      const char *ssp=getenv("STARTDOS");
      while( *ssp != '\0' )
	*dp++ = *ssp++;
      *dp++ = ' ';
      ssp = path;
      while( *ssp != '\0' ){
	if( *ssp == '/' ){
	  *dp++ = '\\';
	  ++ssp;
	}
	if( is_kanji(*ssp) )
	  *dp++ = *ssp++;
	*dp++ = *ssp++;
      }
      *dp++ = ' ';
    }
#endif

    /* puts("4else"); */
    while( *sp != '\0' && *sp != '|' && *sp != '&' && !is_space(*sp) ){
      /* コマンド名 : "/"-->"\\"に置換 */
      if( *sp == '"' ){
	do{
	  *dp++ = *sp++;
	  if( *sp == '\0' ){
	    *dp = '\0';
	    return 0;
	  }
	}while( *sp != '"' );
      }
      if( *sp == '/' ){
	*dp++ = '\\';
	sp++;
      }else{
	if( is_kanji(*sp) ){
	  *dp++ = *sp++;
	  assert(*sp != '\0');
	}
	*dp++ = *sp++;
      }
    }
    if( copyargs(sp,dp) == 1 ){
      *dp = '\0';
      return 0;
    }
 nextcmds: ;
  }/* パイプで区切られた各コマンド毎のループ */
}

#if 0
#include <stdio.h>
#include "edlin.h"

int main()
{
  char buffer[1024];
  char replace[1024];
  
  ShellEdlin edlin("> ",buffer,sizeof(buffer));
  edlin.simple_input("> ");
  putchar('\n');
  replace_script(buffer,replace);
  puts(replace);

  return 0;
}
#endif
