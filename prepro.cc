#include <ctype.h>
#include <stdlib.h>
#include "parse.h"
#include "Edlin.h"
#include "macros.h"

int option_tilda_is_home=1;
int option_replace_slash_to_backslash_after_tilda=1;
int option_tcshlike_history=0;
int option_dots=1;
int option_history_in_doublequote=0;

static struct PublicHistory {
  const char *string;
  PublicHistory *prev,*next;
} Oth={NULL,NULL,NULL} , *public_history=&Oth;

int nhistories = 0;

char drivealias[]="@ABCDEFGHIJKLMNOPQRSTUVWXYZ";

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

int cmd_drivealias( FILE *source , Parse &params )
{
  for(int i=1; i<params.get_argc() ; i++ ){
    Substr arg=params[i];

    if( ! isalpha(arg[0] & 255) ){
      fprintf(stderr,"drvalias: syntax error\n");
      return 0;
    }
    
    if( arg[1]=='\0' || isspace(arg[1] & 255) ){
      printf("%c: = %c:\n" , arg[0] , drivealias[ arg[0] & 0x1F ] );
    }else if( arg[1]=='=' ){
      if( isalpha(arg[2] & 255 ) )
	drivealias[ arg[0] & 0x1F ] = toupper( arg[2] & 255 );
      else
	drivealias[ arg[0] & 0x1F ] = toupper( arg[0] & 255 );
    }else{
      fprintf(stderr,"drvalias: syntax error\n");
      return 0;
    }
  }
  return 0;
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

SmartPtr insert_env(const char *env,SmartPtr dp)
{
  const char *sp=getenv(env);
  if( sp != NULL ){
    while( *sp != '\0' )
      *dp++ = *sp++;
  }
  *dp = '\0';
  return dp;
}

static SmartPtr word_designator(const char *&sp , const char *histring ,
				SmartPtr dp )
{
  /* sp は、':' の後にあるとする */

  Parse argv(histring);
  int argc=argv.get_argc();

  if( *sp=='$' ){
    ++sp;

    argv[ argc-1 ] >> dp;
    return dp + argv[ argc-1 ].len;

  }else if( *sp=='^' ){
    ++sp;

    if( 1 < argc ){
      argv[ 1 ] >> dp;
      return dp + argv[ 1 ].len;
    }else{
      return dp;
    }

  }else if( *sp=='*' ){
    ++sp;
    
    for( int i=1 ; i<argc ; i++ ){
      argv[ i ] >> dp;
      dp += argv[ i ].len;
      *dp++ = ' ';
    }
    return dp;

  }else if( *sp=='-' && isdigit(sp[1] & 255) ){
    ++sp;
    
    int n=0;
    do{
      n = n*10 + (*sp-'0');
    }while( isdigit( *++sp & 255 ) );
    
    if( n >= argc )
      n = argv.get_argc()-1;

    for(int i=0 ; i<=n ; i++ ){
      argv[ i ] >> dp;
      dp += argv[ i ].len;
      *dp++ = ' ';
    }
    return dp;

  }else if( isdigit(*sp) ){

    int n=0;
    do{
      n = n*10 + (*sp-'0');
    }while( isdigit(*++sp & 255) );
      
    if( n < argc ){
      argv[ n ] >> dp;
      dp += argv[ n ].len ;
    }

    if( *sp == '-' ){
      int end=0;
      if( isdigit( *++sp & 255 ) ){
	do{
	  end = end*10 + (*sp-'0');
	}while( isdigit( *++sp & 255) );
	if( end >= argc-1 )
	  end = argc-1;
      }else{
	end = argc-1;
      }
      while( ++n <= end ){
	*dp++ = ' ';
	argv[ n ] >> dp;
	dp += argv[ n ].len;
      }
    }
    return dp;
  }
  while( *histring != '\0' )
    *dp++ = *histring++;
  *dp = '\0';
}

static SmartPtr history_copy(const char *&sp, SmartPtr dp )
{
  /* 引数 sp は、「!」を指していると仮定 */
  const char *histring=0;
  
  switch( *++sp ){

  case '!':
    sp++;
  case '*':
  case ':':
  case '$':
  case '^':

    histring = get_hist_r(0);
    if( histring == NULL )
      fprintf(stderr,"! : Event not found.\n");
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
      while( *sp != '\0' && !isspace(*sp) ){
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

  if( histring == NULL )
    return dp;

  switch( *sp ){
  case ':':
    ++sp;
  case '^':
  case '$':
  case '*':
    return word_designator( sp , histring , dp );
    
  default:
    while( *histring != '\0' )
      *dp++ = *histring++;
    *dp = '\0';
    return dp;
  }
}

void replace_envvar(const char *sp, char *_dp , int max )
{
  SmartPtr dp(_dp,max);
  
  int is_history_refered=0;
  int quote=0;
  int prevchar=' ';

  if( *sp=='!' ){
    dp = history_copy(sp,dp);
    is_history_refered = 1 ;
  }

  // ---------------- CD と DIR に対する例外処理 -------------
  
  //「cd/usr/local/bin」などという入力に対応するための処理
  // この場合、cd と「/」の間に空白を挿入する。
  
  if(   (sp[0]=='c' || sp[0]=='C')
     && (sp[1]=='d' || sp[1]=='D')
     && (sp[2]=='.' || sp[2]=='\\' || sp[2]=='/' ) ){
    *dp++ = *sp++; // c
    *dp++ = *sp++; // d
    *dp++ = ' ';
    *dp++ = *sp++; // 「.」「/」or「\」
  }else if(   (sp[0]=='d' || sp[0]=='D')
	   && (sp[1]=='i' || sp[1]=='I')
	   && (sp[2]=='r' || sp[2]=='R')
	   && (sp[3]=='.' || sp[3]=='\\' || sp[3]=='/' ) ){
    *dp++ = *sp++; // d
    *dp++ = *sp++; // i
    *dp++ = *sp++; // r
    *dp++ = ' ';
    *dp++ = *sp++; // 「.」「/」or「\」
  }
  
  // ------------------- 本来のプリプロセス業務 ----------------

  while( *sp != '\0' ){
    switch( *sp ){
    case '\'':
      if( (quote & 1)==0 )
	quote ^= 2;
      break;
      
    case '"':
      if( (quote & 2)==0 )
	quote ^= 1;
      break;

    case ';': /* 空白＋「；」を「&;」に変換する */
      ++sp;
      if( Parse::option_semicolon_terminate && !quote && is_space(prevchar) ){
	*dp++ = '&';
      }
      *dp++ = ';';
      continue;

    case '.': /* 空白＋「...」を「..\..」に変換する */
      if(   option_dots 
	 && !quote 
	 && is_space(prevchar) && sp[1]=='.' && sp[2]=='.' ){
	
	++sp;
	/* sp は二つ目の . を差している。*/
	for(;;){
	  *dp++ = '.';
	  *dp++ = '.';
	  if( *++sp != '.' )
	    break;
	  *dp++ = '\\';
	}
	continue;
      }
      break;

    case '~':
      if(    option_tilda_is_home  
	 &&  quote==0
	 &&  is_space(prevchar) ){
	if( *(sp+1) == ':' ){ /* `~:' をブートドライブに置換する */
	  ++sp;
	  const char *system_ini = getenv("SYSTEM_INI");
	  if( system_ini == NULL ){
	    *dp++ = '?';
	  }else{
	    *dp++ = *system_ini;
	  }
	}else{ /* 普通の UNIX 的チルダの変換 */
	  dp = insert_env("HOME",dp);
	  if( isalnum(*++sp&255) || is_kanji(*sp&255) ){
	    *dp++ = Edlin::complete_tail_char;
	    *dp++ = '.';
	    *dp++ = '.';
	    prevchar = *dp++ = Edlin::complete_tail_char;;
	  }else{
	    prevchar = '~';
	  }
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
      if(  option_tcshlike_history
	 && (   option_history_in_doublequote
	     ?  (quote & 2)==0  :  quote == 0 ) ) {
	/* history_in_doublequote が有効(not 0)ならば、
	 *    "～!～"はヒストリ変換する。
	 */

	dp = history_copy(sp,dp);
	/* is_history_refered = 1; */
      }
      break;

    case '%':
      if( (quote & 2)==0  &&  isalpha(sp[1] & 255) ){
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
      if( quote==0  && isalpha(sp[0] & 255) && sp[1]==':' ){
	if( islower(sp[0] & 255) )
	  *dp++ = drivealias[ sp[0] & 0x1F ] + ('a'-'A');
	else
	  *dp++ = drivealias[ sp[0] & 0x1F ];
	prevchar = *dp++ = ':';
	sp += 2;
      }else{
	prevchar = *dp++ = *sp++;
      }
    }
  }
 exit:
  *dp = '\0';

  /* ヒストリが参照されていない場合だけ、入力した文字列を
   * 公式ヒストリに残す。
   */

  if( is_history_refered ){
    puts( _dp );
  }

  PublicHistory *tmp=new PublicHistory;
  if( tmp != NULL  &&  (tmp->string = strdup(_dp))!=NULL ){
    tmp->prev = public_history ;
    tmp->next = public_history->next ;
    public_history = public_history->next = tmp ;
    nhistories++;
  }
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
