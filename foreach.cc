#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <sys/video.h>
#include "edlin.h"
#include "nyaos.h"

extern volatile int ctrl_c;

struct Line{
  Line *next;
  char buffer[1];
};

enum{
  OPTION_I = 1,
  OPTION_N = 2,
  OPTION_V = 4,
};
static unsigned option=0;

static int eachcmd(FILE *srcfil, const char *var, const char *str, Line *line )
{
  /* 各命令毎にループ */
  for( ; line != NULL ; line=line->next ){
    char buffer[1024];
    char *dp=buffer;
    const char *sp=line->buffer;
	
    while( *sp != '\0' ){
      if( *sp != '$' ){
	*dp++ = *sp++;
      }else if( *(sp+1) == '$' ){
	*dp++ = '$';
	sp += 2;
      }else{  /* 環境変数、あるいは、パラメータへの置換 */
	/* 変数名を word[] にコピー */
	char word[256],*wp=word;
	if( *++sp == '{' ){
	  ++sp; /*  '{'を読み飛ばし */
	  while( *sp != '}' ){
	    if( *sp == '\0' ){
	      fputs("foreach : '${' without '}'\n",stderr);
	      return -1;
	    }
	    *wp++ = *sp++;
	  }
	  ++sp; /* '}'を読み飛ばし */
	}else if( *sp == '(' ){
	  ++sp; /*  '('を読み飛ばし */
	  while( *sp != ')' ){
	    if( *sp == '\0' ){
	      fputs("foreach : '$(' without ')'\n",stderr);
	      return -1;
	    }

	    *wp++ = *sp++;
	  }
	  ++sp; /* ')'を読み飛ばし */
	}else{
	  if( *sp != '\0'  &&  (isalpha(*sp) || *sp=='_' ) ){
	    do{
	      *wp++ = *sp++;
	    }while( *sp != '\0' && ( isalnum(*sp) || *sp=='_' ) );
	  }
	}
	*wp = '\0';
	
	const char *sp2;
	if( strcmp(word,var)==0 ){
	  sp2=str;
	  while( *sp2 != '\0' )
	    *dp++ = *sp2++;
	}else if( (sp2=getenv(word)) != NULL ){
	  while( *sp2 != '\0' )
	    *dp++ = *sp2++;
	}else{
	  fprintf(stderr,"foreach : no environment variable $%s\n",word);
	  return -1;
	}
      }
    }
    *dp = '\0';
    
    if( ctrl_c ){
      puts( "\nCtrl-C Hit." );
      ctrl_c = 0;
      return -1;
    }
    
    if( option & OPTION_N ){
      puts(buffer);
    }else{
      if( option & OPTION_V ){
	fputs(buffer,stderr);
	if( isatty(fileno(srcfil)) )
	  fputc('\n',stderr);
      }
      int err=execute(srcfil,buffer,1);
      
      if( err != 0  &&  (option & OPTION_I)==0 ){
	fprintf(stderr,"foreach : error level %d",err);
	return -1;
      }
    }
  }/* 命令ループ */
  return 0;
}

int foreach(FILE *srcfil,const char *parameter, int argc, char **argv)
{
  if( srcfil == NULL ){
    fputs("foreach needs end commands!",stderr);
    return 0;
  }
  
  /* _wildcard( &argc , &argv ); */
  
  if( argc < 3 ){
    fputs("foreach [-ivn] var param1 param2 ... paramN\n",stderr);
    return 0;
  }

  option = 0;

  while( argv[1][0] == '-' ){
    const char *p=argv[1]+1;
    while( *p != '\0' ){
      switch(*p){
      case 'i':
      case 'I':
	option |= OPTION_I;
	break;
      case 'n':
      case 'N':
	option |= OPTION_N;
	break;
      case 'v':
      case 'V':
	option |= OPTION_V;
	break;
      }
      p++;
    }
    argv++;
    argc--;
  }

  struct Line dummyfirst , *cur=&dummyfirst;

  dummyfirst.next = NULL;

  char buffer[1024];
  /** 繰り返す命令群を全て入力させる。 **/
  if( isatty(fileno(srcfil)) ){
    /* キーボード入力 */
    ShellEdlin edlin("? ",buffer,sizeof(buffer));
    Shell shell(edlin);

    while(   shell.line_input("? ",32767) >= 0 
	  && (   (buffer[0] != 'e' && buffer[0] != 'E' )
	      || (buffer[1] != 'n' && buffer[1] != 'N' )
	      || (buffer[2] != 'd' && buffer[2] != 'D' )
	      ||  buffer[3] !='\0'
	      )
	  ){
      putchar('\n');
      if( buffer[0] != '\0' ){
	cur = cur->next = 
	  (struct Line*)alloca(sizeof(struct Line)+strlen(buffer));
	strcpy( cur->buffer  , buffer );
      }
    }
    putchar('\n');
  }else{
    /* ファイル入力 */
    for(;;){
      if( fgets_chop(buffer,sizeof(buffer),srcfil) == NULL )
	break;
      
      char *sp=buffer;

      while( *sp != '\0' && isspace(*sp) )
	sp++;
      
      if (   (sp[0] == 'e' || sp[0] == 'E' )
	  && (sp[1] == 'n' || sp[1] == 'N' )
	  && (sp[2] == 'd' || sp[2] == 'D' )
	  && (sp[3] =='\0' || isspace(sp[3]) ) )
	break;

      if( *sp != '\0' ){
	cur = cur->next =
	  (struct Line*)alloca(sizeof(struct Line)+strlen(buffer));
	strcpy( cur->buffer  , buffer );
      }/* 空行は無視する */
    }
  }
  cur->next   = NULL;

  /* 各引数毎にループ */
  for(int i=2;i<argc;i++){
    /* 展開したファイル名ごとのループ */
    char **list=_fnexplode(argv[i]);
    if( list==NULL ){
      eachcmd(srcfil,argv[1],argv[i],dummyfirst.next);
    }else{
      for(char **listptr=list ; *listptr != NULL ; listptr++ ){
	int rc=eachcmd(srcfil,argv[1],*listptr,dummyfirst.next);
	if( rc != 0 ){
	  _fnexplodefree(list);
	  return rc;
	}
      }
      _fnexplodefree(list);
    }/* 展開後の名前ループ */
  }/* パラメータループ */
  return 0;
}
