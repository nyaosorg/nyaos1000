#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/nls.h>
#include "macros.h"

int scriptflag=1;
int option_amp_start=1;

static int copyargs( const char *&sp , char *&dp )
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
    
    if( *sp=='&' ){
      while( is_space(*++sp) )
	;
      if( *sp =='&' ){  /* && の処理 */
	sp++; /* まず、'&' を読みとばす */
	const char *s="& if not errorlevel 1 ";
	while( *s != '\0' )
	  *dp++ = *s++;
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

    if( *sp=='\0' ){
      *dp='\0';
      return 1;
    }
    if( is_kanji(*sp) )
      *dp++ = *sp++;
    *dp++ = *sp++;
  }
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
      if( is_kanji(*ssp) ){
	*ddp++ = *ssp++;
	assert(*ssp != '\0');
      }
      *ddp++ = *ssp++;
    }
    *ddp = '\0';

    char path[FILENAME_MAX];

    /* puts("4"); */
    if(   scriptflag != 0
       && ( _searchenv(fname,"SCRIPTPATH",path) , path[0] != '\0' )
       &&  (fp=fopen(path,"r")) != NULL ){
      
      /* puts("4then"); */
      if( getc(fp) == '#' && getc(fp) == '!' ){

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
	copyargs(ssp,dp);
	sp = ssp;
	continue; /* 次のコマンドへ */
      }
      fclose(fp);
    }
    /******** スクリプトではない場合 *******/

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
    /* puts("copyargs"); */
    if( copyargs( sp , dp ) == 1 ){
      *dp = '\0';
      return 0;
    }
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
