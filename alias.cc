#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "hash.h"
#include "nyaos.h"
#include "parse.h"
#include "finds.h"

Hash <Alias> alias_hash(1024);

static void translate_copy( const Substr &arg , SmartPtr &dp )
{
  if( arg[0] == '-' )
    *dp++ = '/';
  else
    *dp++ = arg[0];

  int quote=(arg[0] == '"' ? 1 : 0);
  for(int i=1;i<arg.len;i++){
    if( arg[i] == '"' )
      quote ^= 1;

    if( quote && arg[i] == '/' )
      *dp++ = '\\';
    else
      *dp++ = arg[i];
  }

  if( arg[arg.len-1] == '/' || arg[arg.len-1] == '\\' )
    *dp++ = '.';
  
  *dp = '\0';
}
static void wildcard_expand_copy( const Substr &arg , SmartPtr &dp 
				 ,int translate_flag )
{
  int quote=0;
  int wildcard=0;
  
  for(int i=0;i<arg.len;i++){
    if( arg[i] == '"' )
      quote ^= 1;
    
    if( quote==0  && (arg[i] == '*' || arg[i] == '?') ){
      wildcard = 1;
      break;
    }
  }
  if( wildcard == 0 ){
    /* ワイルドカード展開無しの場合 */
    if( translate_flag ){
      translate_copy( arg , dp );
    }else{
      for(int i=0;i<arg.len ; i++)
	*dp++ = arg[i];
      *dp = '\0';
    }
    return;
  }
  char *buffer=(char*)alloca(arg.len+1);
  {
    char *q=buffer;
    for(int i=0;i<arg.len;i++){
      if( arg[i] != '"' )
	*q++ = arg[i];
    }
    *q = '\0';
  }
  char **filelist=fnexplode2(buffer);
  if( filelist == NULL ){
    if( translate_flag ){
      translate_copy( arg , dp );
    }else{
      for(int i=0 ; i<arg.len ; i++)
	*dp++ = arg[i];
      *dp = '\0'; 
    }
    return;
  }
  numeric_sort(filelist);
  for(char **ptr=filelist ; *ptr != NULL ; ++ptr ){
    int need_quote=0;
    for(const char *sp=*ptr ; *sp != '\0' ; ++sp ){
      if( isspace(*sp & 255) ){
	need_quote = 1;
	*dp++ = '"';
	break;
      }
    }
    for(const char *sp=*ptr ; *sp != '\0' ; ++sp ){
      if( translate_flag  &&  *sp == '/' )
	*dp++ = '\\';
      else
	*dp++ = *sp;
    }
    if( need_quote )
      *dp++ = '"';
    *dp++ = ' ';
  }
  *dp = '\0';
  fnexplode2_free(filelist);
  return;
}

void replace_alias(const char *sp , char *destinate , int max )
{
  SmartPtr dp(destinate,max);

  for(;;){ /* 各コマンド単位 */
    Parse params(sp);

    /* 命令が空の場合、ただちにやり直し。
     * 「&&」や startに変換する「&」などでは、これが必要らしい 
     */
    if( params.get_argc() == 0 ){
      sp = params.get_tail();
      if( *sp == '\0' )
	break;
      while( sp < params.get_nextcmds() )
	*dp++ = *sp++;
      if( *sp == '\0' )
	break;

      continue;
    }

    Alias *ptr = alias_hash[ params[0] ];
    if( ptr == NULL ){
      dp = params.betacopy(dp);
    }else{
      const char *spa=ptr->base;
      int percent_used=0;
      
      while( *spa != '\0' ){
	if( *spa == '%' ){
	  int wildcard_flag = 0;
	  if( *++spa == '+' ){
	    wildcard_flag = 1;
	    ++spa;
	  }

	  switch( *spa ){
	  default:
	    if( is_digit(*spa) ){
	      percent_used = 1;
	      int n=0;
	      do{
		n *= 10;
		n += (*spa-'0');
	      }while( is_digit(*++spa) );

	      if( n < params.get_argc() ){
		if( wildcard_flag )
		  wildcard_expand_copy( params[n] , dp , *spa=='@' ? 1 : 0 );
		else
		  dp = params.copy(n,dp);
	      }
	      
	      if( *spa == '*' ){
		while( ++n < params.get_argc() ){
		  *dp++ = ' ';
		  if( wildcard_flag )
		    wildcard_expand_copy( params[n] , dp , 0 );
		  else
		    dp = params.copy(n,dp);
		}
		++spa;
	      }else if( *spa == '@' ){
		while( ++n < params.get_argc() ){
		  *dp++ = ' ';
		  if( wildcard_flag )
		    wildcard_expand_copy( params[n] , dp , 1 );
		  else
		    dp = params.copy(n,dp,Parse::QUOTE_COPY);
		}
		++spa;
	      }
	    }
	    break;
	    
	  case '*':
	    percent_used = 1;
	    spa++;
	    if( wildcard_flag ){
	      for(int i=1;i<params.get_argc();i++)
		wildcard_expand_copy( params[i] , dp , 0 );
	    }else{
	      dp = params.copyall(1,dp);
	    }
	    break;
	    
	  case '@':
	    percent_used = 1;
	    spa++;
	    if( wildcard_flag ){
	      for(int i=1;i<params.get_argc() ; i++)
		wildcard_expand_copy( params[i] , dp , 1 );
	    }else{
	      dp = params.copyall(1,dp,Parse::REPLACE_SLASH);
	    }
	    break;
	    
	  case '%':
	    spa++;
	    *dp++ = '%';
	    break;

	  case '\\':case '/':
	    if( dp==destinate || (dp[-1] != '\\' && dp[-1] != '/') )
	      *dp++ = *spa;
	    spa++;
	  }
	}else{
	  *dp++ = *spa++;
	}
      }
      if( percent_used == 0 ){
	*dp++ = ' ';
	dp = params.copyall(1,dp);
      }
      
      /* リダイレクト文字列の再現 */
      const Substr *redirect=params.get_redirect();
      if( redirect[0] != NULL ){
	*dp++ = ' ';
	*dp++ = '<';
	redirect[0] >> dp;
	dp += redirect[0].len;
      }
      if( redirect[1] != NULL ){
	*dp++ = ' ';
	*dp++ = '>';
	if( params.is_append_redirect(1) )
	  *dp++ = '>';
	redirect[1] >> dp;
	dp += redirect[1].len;
      }
      if( redirect[2] != NULL ){
	*dp++ = ' ';
	*dp++ = '2';
	*dp++ = '>';
	if( params.is_append_redirect(2) )
	  *dp++ = '>';
	redirect[2] >> dp;
	dp += redirect[2].len;
      }
    }
    
    sp = params.get_tail();
    if( *sp == '\0' )
      break;

    while( sp < params.get_nextcmds() )
      *dp++ = *sp++;

    if( *sp == '\0' )
      break;
  }/* for(;;) */

  *dp = '\0';
}

int cmd_unalias(FILE *fin, Parse &params)
{
  const char *parameter=params.get_argv(1);
  int argc=params.get_argc();

  if( argc < 2 )
    return 0;
  
  if( alias_hash.remove( params[1] ) != 0 ){
    fputs("unalias: no such alias ",stderr);
    for(int i=0;i<params[1].len;i++)
      putc(params[1].ptr[i],stderr);
    putc('\n',stderr);
    
    return 1;
  }
  return 0;
}

static int print_one_alias(const char *name,FILE *fout=stdout)
{
  Alias *ptr=alias_hash[ name ];
  if( ptr != NULL ){
    fprintf(fout,"%s=\"%s\"\n",ptr->name,ptr->base);
    return 0;
  }
  return 1;
}

int cmd_alias(FILE *fp, Parse &params)
{
  const char *sp=params.get_argv(1);
  int argc=params.get_argc();

  if( argc < 2  ||  sp==NULL ){
    FILE *fout=params.open_stdout();
    if( fout == NULL ){
      fputs("alias : cannot make a pipe or file\n",stderr);
      return 1;
    }
    for( HashPtr hp(alias_hash) ; *hp != NULL ; hp++ ){
      Alias *cur = (Alias*)*hp;
      fprintf(fout,"%s=\"%s\"\n",cur->name,cur->base);
    }
  }else{
    int length=strlen(sp);
    struct Alias *tmp=(Alias*)malloc(sizeof(struct Alias)+length);
    assert( tmp != NULL );

    char *dp=tmp->name;
    while( !is_space(*sp) ){
      if( sp >= params.get_tail() ){
	*dp = '\0';
	FILE *fout=params.open_stdout();
	print_one_alias(tmp->name,fout);
	free(tmp);
	return 0;
      }
      if( *sp == '=' ){
	++sp;
	break;
      }
      if( is_space(*sp) ){
	while( is_space(*sp) )
	  sp++;
	if( *sp == '=' ){
	  ++sp;
	  break;
	}
      }
      *dp++ = *sp++;
    }
    *dp++ = '\0';
    
    /* 空白スキップ */
    while( is_space(*sp) )
      sp++;

    if( sp >= params.get_tail() ){
      FILE *fout=params.open_stdout();
      print_one_alias(tmp->name,fout);
      free(tmp);
      return 0;
    }
    
    tmp->base = dp;

    if( *sp == '"' ){
      /* alias ahaha="ufufuf ""ohoho""" の場合。
       * 引用符一個は空文字に、連続する引用符二個は引用符一個に置換される。
       */
      int quote = 1;
      if( *++sp == '"' ){
	/* 余り考えられない状況だが「alias ufufu=""ahaha""」などの場合の為 */
	*dp++ = '"';
	++sp;
      }
      
      for(;;){
	if( *sp == '"' ){
	  if( *++sp == '"' ){
	    *dp++ = '"';
	    ++sp;
	    continue;
	  }else{
	    quote ^= 1;
	    /* continue せずに直後のコピーへ移行する */
	  }
	}
	if( sp >= params.get_tail() )
	  break;
	*dp++ = *sp++;
      }
      *dp = '\0';

    }else{
      /* 従来と互換性のある alias。引用符一個は引用符一個にしか置換されない。
       * 引用符で囲まれていない「&」や「|」以降もエイリアスに含まれてしまう等
       * のバグがあるが、互換性のため修正はしていない。
       */
      
      while( *sp != '\0' )
	*dp++ = *sp++;
      *dp = '\0';
    }
    alias_hash.destruct( tmp->name );
    alias_hash.insert( tmp->name , tmp );
  }
  return 0;
}
