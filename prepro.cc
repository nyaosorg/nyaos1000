#include <ctype.h>
#include <stdlib.h>
#include "Edlin.h"

int option_tilda_is_home=0;

char *insert_env(const char *env,char *dp)
{
  const char *sp=getenv(env);
  if( sp != NULL ){
    while( *sp != '\0' )
      *dp++ = *sp++;
  }
  return dp;
}

void replace_envvar(const char *sp, char *dp )
{
  int quote=0;
  int prevchar=' ';

  while( *sp != '\0' ){
    switch( *sp ){
    case '"':
      quote ^= 1;
      break;
      
    case '~':
      if( option_tilda_is_home  &&  !quote  &&  isspace(prevchar & 255) ){
	/* isspace ‚Å _nls_is_dbcs_lead ‚àŒ“‚Ë‚Ä‚¢‚éB*/
	dp = insert_env("HOME",dp);
	if( *++sp != '/' && *sp != '\\' && *sp != '\0' && !isspace(*sp) ){
	  *dp++ = Edlin::complete_tail_char;
	  *dp++ = '.';
	  *dp++ = '.';
	  prevchar = *dp++ = Edlin::complete_tail_char;;
	}else{
	  prevchar = '~';
	}
	continue;
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
    prevchar = *dp++ = *sp++;
  }
  *dp = '\0';
}
