#include <ctype.h>
#include <stdlib.h>
#include "Edlin.h"

int option_tilda_is_home=0;
int option_tcshlike_history=0;

char *insert_env(const char *env,char *dp)
{
  const char *sp=getenv(env);
  if( sp != NULL ){
    while( *sp != '\0' )
      *dp++ = *sp++;
  }
  return dp;
}

static char *history_copy(const char *&sp, char *dp , int offset )
{
  /* 引数 sp は、「!」を指していると仮定 */
  const char *histring=0;
  
  switch( *++sp ){
#if 0
  case '#':
    if( offset > 9 )
      return dp;
    histring = Shell::get_nth_history(0+offset);
    sp++;
    break;
#endif
  case '!':
    histring = Shell::get_nth_history(1+offset++);
    sp++;
    break;
  default:
    int minus=0;
    if( *sp == '-' ){
      minus=1;
      ++sp;
    }
    if( isdigit(*sp & 255) ){
      int n=0;
      do{
	n = n*10+(*sp-'0');
      }while( isdigit(*++sp & 255) );
      if( minus ){
	histring = Shell::get_nth_history(offset+=n);
      }else{
	histring = Shell::get_nth_history(Shell::get_history_number()-1-n);
	offset += Shell::get_history_number()-n;
      }
    }
    break;
  }/* end of switch */
  if( histring != 0 ){
    while( *histring != '\0' ){
      if( *histring == '!' ){
	dp = history_copy(histring,dp,offset);
      }else{
	*dp++ = *histring++;
      }
    }
  }
  return dp;
}

void replace_envvar(const char *sp, char *dp )
{
  int quote=0;
  int prevchar=' ';

  if( *sp=='!' )
    dp = history_copy(sp,dp,0);

  while( *sp != '\0' ){
    switch( *sp ){
    case '"':
      quote ^= 1;
      break;
      
    case '~':
      if( option_tilda_is_home  &&  !quote  &&  isspace(prevchar & 255) ){
	/* isspace で _nls_is_dbcs_lead も兼ねている。*/
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

    case '!':
      if( !quote  &&  option_tcshlike_history ){
	dp = history_copy(sp,dp,0);
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
