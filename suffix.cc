#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "hash.h"
#include "parse.h"

struct Suffix{
  char *interpretor;
  char suffix[1];
};

Hash <Suffix> exttable(128);

int cmd_ext(FILE *source , Parse &argp)
{
  for(int i=1 ; i<argp.get_argc() ; i++){
    const char *arg=argp[i].ptr;
    Suffix *tmp=(Suffix *)malloc(sizeof(Suffix)+argp[i].len);
    char *dp=tmp->suffix;
    for(int j=0;j<argp[i].len;j++){
      if( arg[j] != '"' ){
	*dp++ = arg[j];
      }
    }
    *dp = '\0';
    
    for(char *p=tmp->suffix; ; p++){
      if( *p == '=' ){
	if( *(p+1) == '\0' ){
	  exttable.destruct( tmp->suffix );
	  free(tmp);
	}else{
	  *p = '\0';
	  tmp->interpretor = p+1;
	  exttable.insert( tmp->suffix , tmp );
	}
	break;
      }
      if( *p == '\0' ){
	Suffix *s=exttable[ tmp->suffix ];
	if( s != NULL )
	  printf("%s=%s\n", s->suffix , s->interpretor );
	else
	  printf("%s undefined.\n",tmp->suffix);
	free(tmp);
	break;
      }
    }
  }
  return 0;
}

int suffix(const char *path, SmartPtr &dp)
{
  const char *ext=NULL;
  const char *p=path;
  for(; *p != '\0' && !isspace(*p & 255) ; ++p ){
    if( *p=='.' ){
      if( *(p+1) == '\0' )
	return 2;
      ext = p+1;
    }
  }
  if( ext == NULL )
    return 1;

  char *sfx=(char*)alloca(p-ext+1);
  memcpy( sfx , ext , p-ext );
  sfx[ p-ext ] = '\0';

  Suffix *s=exttable[ sfx ];
  if( s == NULL )
    return 3;

  for( const char *sp=s->interpretor; *sp != '\0' ; ++sp )
    *dp++ = *sp;
  
  *dp++ = ' ' ;
  *dp   = '\0';
  return 0;
}
