#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <sys/video.h>

#include "finds.h"
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
	  if( *sp != '\0'  &&  (is_alpha(*sp) || *sp=='_' ) ){
	    do{
	      *wp++ = *sp++;
	    }while( *sp != '\0' && ( is_alnum(*sp) || *sp=='_' ) );
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
    fputs("foreach is not available in REXX Script!\n",stderr);
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
    const char *promptenv=getenv("NYAOSPROMPT2");
    char prompt[1024];
    if( promptenv == NULL ){
      prompt[0] = '?';
      prompt[1] = ' ';
      prompt[2] = '\0';
    }

    int rc;
    for(;;){
      if( promptenv != NULL )
	setprompt(promptenv,prompt,&edlin);
      edlin.setcursor( cursor_on_color_str , cursor_off_color_str );

      if (!(   (rc=shell.line_input(prompt,32767)) >= 0 
	    && (   (buffer[0] != 'e' && buffer[0] != 'E' )
		|| (buffer[1] != 'n' && buffer[1] != 'N' )
		|| (buffer[2] != 'd' && buffer[2] != 'D' )
		||  buffer[3] !='\0'
		)
	    ))
	break;
      
      putchar('\n');
      if( buffer[0] != '\0' ){
	cur = cur->next = 
	  (struct Line*)alloca(sizeof(struct Line)+strlen(buffer));
	strcpy( cur->buffer  , buffer );
      }
    }
    if( rc==Shell::ABORT ){
      puts("^C");
      return 0;
    }
    if( rc==Shell::FATAL ){
      fputs("Unknown error occured.\n"
	    "Please mail kaoru@ferrari6.cheme.kyoto-u.ac.jp!\n"
	    ,stderr );
      return 0;
    }
    putchar('\n');
  }else{
    /* ファイル入力 */
    for(;;){
      if( fgets_chop(buffer,sizeof(buffer),srcfil) == NULL )
	break;
      
      char *sp=buffer;

      while( *sp != '\0' && is_space(*sp) )
	sp++;
      
      if (   (sp[0] == 'e' || sp[0] == 'E' )
	  && (sp[1] == 'n' || sp[1] == 'N' )
	  && (sp[2] == 'd' || sp[2] == 'D' )
	  && (sp[3] =='\0' || is_space(sp[3]) ) )
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
#if 0
    Dir dir(argv[i]);
    if( dir == NULL ){
      eachcmd(srcfil,argv[1],argv[i],dummyfirst.next);
    }else{
      do{
	int rc=eachcmd(srcfil , argv[1] , dir.get_name() , dummyfirst.next);
	if( rc != 0 )
	  return rc;
      }while( ++dir != NULL );
    }
#else
    char **list = fnexplode2(argv[i]);
    if( list==NULL ){
      eachcmd(srcfil,argv[1],argv[i],dummyfirst.next);
    }else{
      for(char **listptr=list ; *listptr != NULL ; listptr++ ){
	int rc=eachcmd(srcfil,argv[1],*listptr,dummyfirst.next);
	if( rc != 0 ){
	  fnexplode2_free(list);
	  return rc;
	}
      }
      fnexplode2_free(list);
    }/* 展開後の名前ループ */
#endif
  }/* パラメータループ */
  return 0;
}
