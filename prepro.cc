#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/nls.h>
#include "parse.h"
#include "Edlin.h"
#include "macros.h"

int option_tilda_is_home=0;
int option_replace_slash_to_backslash_after_tilda=1;
int option_tcshlike_history=0;

static struct PublicHistory {
  const char *string;
  PublicHistory *prev,*next;
} Oth={NULL,NULL,NULL} , *public_history=&Oth;

int nhistories = 0;

static const char *seek_hist_top(const char *str,int len)
{
  PublicHistory *cur=public_history;
  while( cur != NULL && cur != &Oth ){
    if( cur->string[0] == str[0]  &&  memcmp(cur->string,str,len)==0 )
      return cur->string;
    cur = cur->prev;
  }
  return NULL;
}

static const char *seek_hist_mid(const char *str)
{
  PublicHistory *cur=public_history;
  while( cur != NULL && cur != &Oth ){
    if( strstr( cur->string , str ) != NULL )
      return cur->string;
    cur = cur->prev;
  }
  return NULL;
}

static const char *get_hist_f(int n)
{
  PublicHistory *cur=Oth.next;
  if( cur==NULL )
    return NULL;
  for(int i=0; i<n ; i++ ){
    if( cur == NULL )
      return NULL;
    cur = cur->next;
  }
  return cur->string;
}

static const char *get_hist_r(int n)
{
  PublicHistory *cur=public_history;
  if( cur==NULL || cur==&Oth )
    return NULL;
  for(int i=0; i<n ; i++ ){
    if( cur == NULL || cur==&Oth )
      return NULL;
    cur = cur->prev;
  }
  return cur->string;
}

char *insert_env(const char *env,char *dp)
{
  const char *sp=getenv(env);
  if( sp != NULL ){
    while( *sp != '\0' )
      *dp++ = *sp++;
  }
  *dp = '\0';
  return dp;
}

char *replace_envvar(const char *sp, char *_dp );

static char *history_copy(const char *&sp, char *dp )
{
  /* 引数 sp は、「!」を指していると仮定 */
  const char *histring=0;
  
  switch( *++sp ){
  case '!':
    histring = get_hist_r(0);
    if( histring == NULL )
      fprintf(stderr,"! : Event not found.\n");
    sp++;
    break;

  default:
    int minus=0;
    if( *sp == '-' ){
      minus=1;
      ++sp;
    }
    if( is_digit(*sp) ){
      int n=0;
      do{
	n = n*10+(*sp-'0');
      }while( is_digit(*++sp) );
      
      if( minus ){
	histring = get_hist_r(n>0 ? n-1 : 0 );
	if( histring == NULL )
	  fprintf(stderr,"-%d : Event not found.\n",n);
      }else{
	histring = get_hist_f(n);
	if( histring == NULL )
	  fprintf(stderr,"%d : Event not found.\n",n);
      }
      

    }else if( *sp == '?' ){
      char buffer[1024] , *bp = buffer;
      ++sp; /* 最初の'?'のスキップ */
      while( *sp != '?' &&  *sp != '\0' )
	*bp++ = *sp++;
      ++sp; /* 最後の'?'のスキップ */
      *bp = '\0';

      histring = seek_hist_mid(buffer);
      if( histring == NULL )
	fprintf(stderr,"%s : Event not found.\n",buffer);
	
    }else{
      char buffer[1024] , *bp = buffer;
      int len=0;
      while( *sp != '\0' ){
	*bp++ = *sp++;
	len++;
      }
      *bp = '\0';
      histring = seek_hist_top(buffer,len);
      if( histring == NULL )
	fprintf(stderr,"%s : Event not found.\n",buffer);
    }
    break;
  }/* end of switch */

  return (histring != NULL ? replace_envvar(histring,dp) : dp) ;
}

char *replace_envvar(const char *sp, char *_dp )
{
  char *dp=_dp;
  int is_history_refered=0;
  int quote=0;
  int prevchar=' ';

  if( *sp=='!' ){
    dp = history_copy(sp,dp);
    is_history_refered = 1 ;
  }

  if( is_alpha(*sp) ){
    /* 「cd/usr/local/bin」などという入力に対応するための処理
     * この場合、cd と「/」の間に空白を挿入する。
     */

    /* 直後に空白を挿入しなければならないキーワードのリスト */
    const static char *keyword[]={
      "cd",
      "dir",
    };

    char buffer[16],*p=buffer;

    /* dp : 返り値用バッファ と
     * p  : 比較用一時バッファ に英字以外の文字が来るまで、
     * まず、コピーする。
     */

    do{
      *p++ = *dp++ = *sp++;
      if( ! is_alpha(*sp) ){
	*p = '\0';
	for(int i=0; i<numof(keyword); i++){
	  const char *q=keyword[i];
	  p=buffer;
	  while( to_lower(*p) == *q ){
	    if( *p == '\0' ){
	      *dp++ = ' ';
	      goto nextstep;
	    }
	    p++;q++;
	  }
	}
	goto nextstep; /* 本来のルーチンへ飛べ！ */
      }
    } while( p < buffer+sizeof(buffer)-2 );
  }

 nextstep:  /* ここから、本来のプリプロセス業務を行うってか？ */
  while( *sp != '\0' ){
    switch( *sp ){
    case '"':
      quote ^= 1;
      break;
      
    case '~':
      if( option_tilda_is_home  &&  !quote  &&  is_space(prevchar) ){
	/* is_space で _nls_is_dbcs_lead も兼ねている。*/
	dp = insert_env("HOME",dp);
	if( *++sp != '/' && *sp != '\\' && *sp != '\0' && !is_space(*sp) ){
	  *dp++ = Edlin::complete_tail_char;
	  *dp++ = '.';
	  *dp++ = '.';
	  prevchar = *dp++ = Edlin::complete_tail_char;;
	}else{
	  prevchar = '~';
	}
	if( option_replace_slash_to_backslash_after_tilda ){
	  /* チルダの後の「/」を全て「\」に変換する。 */
	  for(;;){
	    if( *sp == '\0' )
	      goto exit;
	    if( is_space(*sp) )
	      break;
	    if( is_kanji(*sp) ){
	      prevchar = *dp++ = *sp++;
	      *dp++ = *sp++;
	    }else if( *sp=='/' ){
	      ++sp;
	      prevchar = *dp++ = '\\';
	    }else{
	      prevchar = *dp++ = *sp++;
	    }
	  }
	}
	continue;
      }
      break;

    case '!':
      if( !quote  &&  option_tcshlike_history ){
	dp = history_copy(sp,dp);
	is_history_refered = 1;
      }
      break;

    case '%':
      if( !quote  &&  isalpha(sp[1] & 255) ){
	char envname[128];
	
	++sp;
	char *ddp=envname;
	for(;;){
	  if( *sp=='\0' ){
	    break;
	  }else if( *sp=='%' ){
	    prevchar = *sp++;
	    break;
	  }else if( ddp >= envname+sizeof(envname)-2 ){
	    break;
	  }
	  prevchar = *ddp++ = toupper(*sp & 255);
	  ++sp;
	}
	*ddp = '\0';
	dp = insert_env(envname,dp);
	continue;
      }
      break;
    }

    if( is_kanji(*sp) ){
      prevchar = *dp++ = *sp++;
      *dp++ = *sp++;
    }else{
      prevchar = *dp++ = *sp++;
    }
  }
 exit:
  *dp = '\0';

  /* ヒストリが参照されていない場合だけ、入力した文字列を
   * 公式ヒストリに残す。
   */

  if( is_history_refered == 0 ){
    PublicHistory *tmp=new PublicHistory;
    if( tmp != NULL  &&  (tmp->string = Shell::get_nth_history(0))!=NULL ){
      tmp->prev = public_history ;
      tmp->next = public_history->next ;
      public_history = public_history->next = tmp ;
      nhistories++;
    }
  }else{
    puts( _dp );
  }
  return dp;
}

int cmd_history(FILE *source,Parse &param)
{
  int n=10;
  if( param.get_argc() >= 2 ){
    char *arg1=(char*)alloca(param.get_length(1)+1);
    param.copy(1,arg1);
    if( (n=atoi(arg1)) < 1 )
      n = 10;
  }
  FILE *fout=param.open_stdout();

  PublicHistory *cur=public_history;
  if( cur != NULL ){
    int i;
    for( i=0 ; i<n  && cur != NULL && cur != &Oth ; i++ )
      cur = cur->prev;

    while( i > 0  && cur !=NULL ){
      cur = cur->next;
      fprintf( fout , "%4d : %s\n"
	      , nhistories-(i--) , cur->string );
    }
  }
  return 0;
}
