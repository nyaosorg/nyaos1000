#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/nls.h>

int scriptflag=1;
int option_amp_start=1;

int replace_script( const char *sp , char *dp )
{
  for(;;){ /* パイプで区切られた各コマンドに関するループ */
    while( isspace(*sp) )
      *dp++ = *sp++;

    char path[FILENAME_MAX];
    char fname[FILENAME_MAX];
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
	  *dp++ = 's';
	  *dp++ = 't';
	  *dp++ = 'a';
	  *dp++ = 'r';
	  *dp++ = 't';
	  *dp++ = ' ';
#if 0
	  *dp++ = '/';
	  *dp++ = 'C';
	  *dp++ = ' ';
#endif
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
	  if( _nls_is_dbcs_lead(*p) ){
	    p+=2;
	  }else{
	    ++p;
	  }
	}
      }
    }

  check_script:

    const char *ssp = sp;
    char *ddp = fname;
    for(;;){
      if( *ssp == '"' ){
	do{
	  *ddp++ = *ssp++;
	}while( *ssp != '\0' && *ssp != '"' );
      }
      if( *ssp == '\0' || isspace(*ssp) )
	break;
      *ddp++ = *ssp++;
    }
    *ddp = '\0';

    if(   scriptflag != 0
       && ( _searchenv(fname,"SCRIPTPATH",path) , path[0] != '\0' )
       &&  (fp=fopen(path,"r")) != NULL ){
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
	do{
	  if( *ssp=='"' ){
	    do{
	      *dp++ = *ssp++;
	    }while( *ssp != '\0' && *ssp != '"' );
	  }
	  if( (ch=*dp++ = *ssp++)=='\0' )
	    return 0;
	}while( ch != '|'  &&  ch != '&' );
	sp = ssp;
	continue; /* 次のコマンドへ */
      }
      fclose(fp);
    }
    /******** スクリプトではない場合 *******/

    while( *sp != '\0' && *sp != '|' && *sp != '&' && !isspace(*sp) ){
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
	*dp++ = *sp++;
      }
    }
    do{ /* 引数 */
      if( *sp == '"' ){
	do{
	  *dp++ = *sp++;
	  if( *sp == '\0' ){
	    *dp = '\0';
	    return 0;
	  }
	}while( *sp != '"' );
      }
      if( (ch = *dp++ = *sp++)=='\0' )
	return 0;
    }while( ch != '|' && ch != '&' );
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
