#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "nyaos.h"
#include "parse.h"

extern int wrdcmp(const char *tblstr,const char *cmdstr);
struct Alias *alias_hashtable[];

int alias_nesting=0;
void alias_replace(const char *sp , char *destinate  )
{
  char *dp=destinate;
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

    int key=0;
    {/* ハッシュキーを計算する */
      int size=params.get_length(0);
      const char *sp2=params.get_argv(0);
      while( size-- ){
	key += tolower(*sp2 & 255);
	sp2++;
      }
    }

    Alias *ptr=alias_hashtable[ key % numof(alias_hashtable) ];
    while( ptr != NULL ){
      int len;
      if(   ptr->name[0] == params.get_argv(0)[0]
	 && wrdcmp(ptr->name,params.get_argv(0) ) == 0 ){
	
	const char *spa=ptr->base;
	int percent_used=0;

	while( *spa != '\0' ){
	  if( *spa == '%' ){
	    switch( *++spa ){
	    default:
	      if( is_digit(*spa) ){
		percent_used = 1;
		int n=0;
		do{
		  n *= 10;
		  n += (*spa-'0');
		}while( is_digit(*++spa) );
		if( n < params.get_argc() )
		  dp = params.copy(n,dp);
		
		if( *spa == '*' ){
		  while( ++n < params.get_argc() ){
		    *dp++ = ' ';
		    dp = params.copy(n,dp);
		  }
		  ++spa;
		}else if( *spa == '@' ){
		  while( ++n < params.get_argc() ){
		    *dp++ = ' ';
		    dp = params.copy(n,dp,Parse::QUOTE_COPY);
		  }
		  ++spa;
		}
	      }
	      break;

	    case '*':
	      percent_used = 1;
	      spa++;
	      dp = params.copyall(1,dp);
	      break;

	    case '@':
	      percent_used = 1;
	      spa++;
	      dp = params.copyall(1,dp,Parse::REPLACE_SLASH);
	      break;

	    case '%':
	      spa++;
	      *dp++ = '%';
	      break;

	    case '\\':case '/':/* case ';': */
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
	break;
      }
      ptr = ptr->next;
    }/* alias search loop */

    if( ptr == NULL )
      dp = params.betacopy(dp);
    
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

int unalias(int key,const char *name)
{
  Alias *pre=alias_hashtable[ key ];

  if( pre == NULL )
    return -1;

  Alias *cur=pre->next;
  if( pre->name[0] == name[0]  &&  strcmp(pre->name, name )==0  ){
    alias_hashtable[ key ] = cur; /* curとあるが、示しているのは次のリスト*/
    free(pre);
    return 0;
  }
  while( cur != NULL ){
    if( cur->name[0] == name[0]  &&  strcmp(cur->name,name)==0 ){
      pre = cur->next;
      free(cur);
      return 0;
    }
    pre = cur;
    cur = cur->next;
  }
  return -1;
}

int cmd_unalias(FILE *fin, Parse &params)
{
  const char *parameter=params.get_argv(1);
  int argc=params.get_argc();

  if( argc < 2 )
    return 0;
  
  char *name=(char*)alloca(params.get_length(1)+1);
  params.copy(1,name);

  const char *sp=name;
  int key=0;
  while( *sp != '\0' )
    key += *sp++;

  if( unalias( key %= numof(alias_hashtable) , name ) != 0 ){
    fprintf(stderr,"unalias: no such alias %s\n",name );
    return 1;
  }
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
    for(int i=0 ; i<numof(alias_hashtable) ; i++ ){
      Alias *cur=alias_hashtable[i];
      while( cur != NULL ){
	fprintf(fout,"%s=%s\n",cur->name,cur->base);
	cur=cur->next;
      }
    }
  }else{
    int length=strlen(sp);
    struct Alias *tmp=(Alias*)malloc(sizeof(struct Alias)+length);
    assert( tmp != NULL );

    int key=0;
    char *dp=tmp->name;
    while( *sp != '\0' && !is_space(*sp) ){
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
      key += (*dp++ = *sp++);
    }
    *dp++ = '\0';
    
    /* 空白スキップ */
    while( is_space(*sp) )
      sp++;

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
	if( *sp == '\0' || (!quote && (*sp=='|' || *sp=='&') ) )
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

    /* 重複するエイリアスは廃棄 */
    unalias( key %= numof(alias_hashtable) , tmp->name );

    tmp->next = alias_hashtable[ key ];
    alias_hashtable[ key ] = tmp;
  }
  return 0;
}
