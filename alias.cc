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

    unsigned int key=0;
    {/* ハッシュキーを計算する */
      int size=params.get_length(0);
      const char *sp2=params.get_argv(0);
      while( size-- ){
	key += tolower(*sp2 & 255);
	sp2++;
      }
    }
    Alias *ptr=alias_hashtable[ key % numof(alias_hashtable) ];
    for( ; ptr != NULL ; ptr = ptr->next ){

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
	  redirect[1] >> dp;
	  dp += redirect[1].len;
	}
	if( redirect[2] != NULL ){
	  *dp++ = ' ';
	  *dp++ = '2';
	  *dp++ = '>';
	  redirect[2] >> dp;
	  dp += redirect[2].len;
	}
	break;
      }

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
  int firstchar=tolower(name[0]);
  while( cur != NULL ){
    if( tolower(cur->name[0]) == firstchar  &&  stricmp(cur->name,name)==0 ){
      pre->next = cur->next;
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
  
  char *name=(char*)alloca(params.get_length(1)+3);
  params.copy(1,name);

  const char *sp=name;
  unsigned int key=0;
  while( *sp != '\0' ){
    key += (tolower( *sp & 0xFF ) );
    ++sp;
  }

  if( unalias( key %= numof(alias_hashtable) , name ) != 0 ){
    fprintf(stderr,"unalias: no such alias %s\n",name );
    return 1;
  }
  return 0;
}

static int print_one_alias(int key,const char *name,FILE *fout=stdout)
{
  Alias *ptr=alias_hashtable[ key % numof(alias_hashtable) ];
  while( ptr != NULL ){
    if( stricmp(ptr->name,name)==0 ){
      fprintf(fout,"%s=%s\n",ptr->name,ptr->base);
      return 0;
    }
    ptr = ptr->next;
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
    while( !is_space(*sp) ){
      if( sp >= params.get_tail() ){
	*dp = '\0';
	FILE *fout=params.open_stdout();
	print_one_alias(key,tmp->name,fout);
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
      key += (*dp++ = tolower( *sp & 255 ));
      sp++;
    }
    *dp++ = '\0';
    
    /* 空白スキップ */
    while( is_space(*sp) )
      sp++;

    if( sp >= params.get_tail() ){
      FILE *fout=params.open_stdout();
      print_one_alias(key,tmp->name,fout);
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

    /* 重複するエイリアスは廃棄 */
    unalias( key %= numof(alias_hashtable) , tmp->name );

    tmp->next = alias_hashtable[ key ];
    alias_hashtable[ key ] = tmp;
  }
  return 0;
}
