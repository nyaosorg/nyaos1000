#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "nyaos.h"
#include "params.h"

extern int wrdcmp(const char *tblstr,const char *cmdstr);
struct Alias *alias_hashtable[];

int alias_nesting=0;

void alias_replace(const char *sp , char *dp  )
{
  for(;;){ /* 各コマンド単位 */
    Params params(sp);

    int key=0;
    {/* ハッシュキーを計算する */
      int size=params.get_length(0);
      const char *sp2=params.get_argv(0);
      while( size-- ){
	key += tolower(*sp2);
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
	    if( isdigit(*++spa) ){
	      percent_used = 1;
	      int n=0;
	      do{
		n *= 10;
		n += (*spa-'0');
	      }while( isdigit(*++spa) );
	      if( n < params.get_argc() )
		dp = params.copy(n,dp);
	      
	      if( *spa == '*' ){
		while( ++n < params.get_argc() ){
		  *dp++ = ' ';
		  dp = params.copy(n,dp);
		}
		++spa;
	      }
	    }else if( *spa == '*' ){
	      percent_used = 1;
	      spa++;
	      dp = params.copyall(1,dp);
	    }else if( *spa == '%' ){
	      spa++;
	      *dp++ = '%';
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
      dp = params.copyall(0,dp);
    
    sp = params.get_tail();
    if( *sp == '\0' )
      break;
    
    *dp++ = ' ';
    *dp++ = *sp++;
    *dp++ = ' ';

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

int cmd_unalias(FILE *fin, Params &params)
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

int cmd_alias(FILE *fp, Params &params)
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
    while( *sp != '\0' && !isspace(*sp) ){
      if( *sp == '=' ){
	++sp;
	break;
      }
      if( isspace(*sp) ){
	while( isspace(*sp) )
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
    while( isspace(*sp) )
      sp++;
    
    tmp->base = dp;
    while( *sp != '\0' )
      *dp++ = *sp++;
    *dp = '\0';

    /* 重複するエイリアスは廃棄 */
    unalias( key %= numof(alias_hashtable) , tmp->name );

    tmp->next = alias_hashtable[ key ];
    alias_hashtable[ key ] = tmp;
  }
  return 0;
}
