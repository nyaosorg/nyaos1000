#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "nyaos.h"
#include "parse.h"
#include "edlin.h"

int cmd_bind(FILE *source, Parse &param )
{
  int rc=0;

  struct{
    const char *name;
    void (*func)();
    const char *usage;
  } table2[]={
    { "emacs" , Shell::bindkey_tcshlike , "key-bindings like emacs" },
    { "tcsh"  , Shell::bindkey_tcshlike , "same as emacs" },
    { "ws"    , Shell::bindkey_wordstar , "key-bindings like wordstar" },
    { "vz"    , Shell::bindkey_wordstar , "same as ws" },
  };

  if( param.get_argc() < 2 ){
    FILE *fout=param.open_stdout();
    for(int i=0;i<numof(table2);i++)
      fprintf(fout,"%s\t: %s\n",table2[i].name,table2[i].usage);
  }else{
    char *buffer=(char*)alloca(param.get_length(1)+1);
    param.copy(1,buffer);
    for(int i=0;i<numof(table2);i++){
      if(   to_lower(buffer[0])==table2[i].name[0]
	 && stricmp(buffer,table2[i].name)==0 ){
	(*table2[i].func)();
	return 0;
      }
    }
    fprintf(stderr,"%s : no such bindings\n",buffer);
    rc = 1;
  }
  return rc;
}

int cmd_bindkey(FILE *source,Parse &param)
{
  if( param.get_argc() < 3 ){
    Shell::bindlist(param.open_stdout());
    return 0;
  }

  char *key  = (char*)alloca(param.get_length(1)+1);
  param.copy(1,key);
  char *func = (char*)alloca(param.get_length(2)+1);
  param.copy(2,func);

  switch( Shell::bindkey(key,func) ){
  case 1:
    fprintf(stderr,"bindkey: %s: invalid key name.\n",key);
    return 1;

  case 2:
    fprintf(stderr,"bindkey: %s: invalid function name.\n",func);
    return 2;

  }
  return 0;
}

int cmd_set( FILE *srcfil, Parse &params )
{
  if( params.get_argc() < 2 )
    return RC_HOOK;

  const char *sp = params.get_parameter();
  if( sp == NULL )
    return RC_HOOK;

  int ch;
  char envname[1024],*dp=envname;
  const char *tail=params.get_tail();

  /* 変数名の前の空白のスキップ */
  while( *sp!='\0' && is_space(*sp) )
    sp++;

  /* 変数名のコピ－ */
  while( *sp != '=' && !is_space(*sp ) ){
    if( sp >= tail || *sp == '>' ){
      /* 変数名がない ---> 画面表示のみ */
      return RC_HOOK;
    }else if( *sp=='<' ){
      fputs("You cannot input-redirect on command set.\n",stderr);
      return 1;
    }
    if( is_kanji(*sp) ){
      *dp++ = *sp++;
      *dp++ = *sp++;
    }else{
      *dp++ = to_upper(*sp) ;
      sp++;
    }
  }

  char *final_space=dp;

  /* 変数名～「=」の空白のスキップ */
  while( *sp != '=' ){
    if( sp >= tail || *sp == '>' ){
      return RC_HOOK;
    }else if( *sp == '<' ){
      fputs("You cannot input-redirect on command set.\n",stderr);
      return 1;
    }

    if( !is_space(*sp) ){
      fputs("Invalid Argument.\n",stderr);
      return 2;
    }
    sp++;
  }

  /* 「=」のスキップ */
  *dp++ = *sp++;
  
  /* 「=」～引数の直前の空白を削除 */
  for(;;){
    if( *sp == '\0' ){
      /*「set ahaha=」で環境変数 ahaha を削除する */
      *final_space = '\0';
      putenv( strdup(envname) );
      return 0;
    }
    if( !is_space(*sp) )
      break;
    ++sp;
  }

  /*  右辺値のコピ－ */
  
  final_space=NULL;
  int quote=0;
  int compati=( *sp != '"' );
  
  while( sp < tail ){
    if( is_space(*sp) ){
      if( final_space==NULL && quote==0 )
	final_space = dp;
    }else{
      final_space = NULL;
      if( *sp == '^' &&  *++sp !='\0' ){
	if( is_kanji(*sp) )
	  *dp++ = *sp++;
	*dp++ = *sp++;
	continue;
      }else if( *sp == '"' ){
	if( ! compati ){
	  if( *(sp+1) == '"' ){
	    *dp++ = '"';
	    sp += 2;
	  }else{
	    sp++;
	    quote ^= 1;
	  }
	  continue;
	}
	quote ^= 1;
      }
    }
    if( is_kanji(*sp) )
      *dp++ = *sp++;
    *dp++ = *sp++;
  }
 exit:
  *dp = '\0';
  
  if( final_space != NULL )
    *final_space = '\0';
  
  putenv( strdup(envname) );
  
  return 0;
}

int cmd_cursor( FILE *fp, Parse &params)
{
  if( cursor_on_color_str != NULL ){
    free( cursor_on_color_str );
    cursor_on_color_str = NULL;
  }
  if( cursor_off_color_str != NULL ){
    free( cursor_off_color_str );
    cursor_off_color_str = NULL;
  }
  if( params.get_argc() >= 3 ){
    cursor_on_color_str  = (char*)malloc(params.get_length(1)+1);
    assert( cursor_on_color_str != NULL );
    params.copy(1,cursor_on_color_str );

    cursor_off_color_str  = (char*)malloc(params.get_length(2)+1);
    assert( cursor_off_color_str != NULL );
    params.copy(2,cursor_off_color_str );

    FILE *fout=params.open_stdout();
    if( fout == NULL ){
      fputs("cursor : cannot make a pipe or file\n",stderr);
      return 1;
    }
  }
  return 0;
}

int cmd_echo(FILE *srcfil, Parse &params )
{
  FILE *fout=params.open_stdout();
  if( fout == NULL ){
    fputs("echo : cannot make a pipe or file\n",stderr);
    return 0;
  }
  bool quote=false;
  
  const char *sp=params.get_argv(1);
  if( sp != NULL ){
    while( *sp != '\0'  &&  sp < params.get_tail() ){
      if( is_kanji(*sp) ){
	putc(*sp++,fout);
	putc(*sp,fout);
      }else if( *sp=='^' ){
	switch( *++sp ){
	case '"':
	  quote = !quote;
	  putc('^',fout);
	  break;
	case 't':
	  putc('\t',fout); break;
	case 'n':
	  putc('\n',fout); break;
	case 'v':
	  putc('\v',fout); break;
	case 'r':
	  putc('\r',fout); break;
	case 'f':
	  putc('\v',fout); break;
	case 'e':
	  putc('\033',fout); break;
	case 'q':
	  putc('"',fout); break;
	case 'c':
	  return 0;
	default:
	  if( is_digit(*sp) ){
	    int n = (*sp++ - '0');
	    for(int i=0 ; i<3 && is_digit(*sp) ; i++ ){
	      n = n*8 + (*sp++ - '0');
	    }
	    putc( n , fout );
	    continue;
	  }else{
	    putc( *sp , fout);
	  }
	}
      }else if( !quote && (*sp == '<' || *sp == '>') ){
	break;
      }else if( *sp == '"' ){
	if( *(sp+1) == '"' ){
	  putc( '"' , fout );
	  sp += 2;
	  continue;
	}
	quote = !quote;
      }else{
	putc( *sp , fout );
      }
      sp++;
    }
  }
  putc( '\n' , fout );
  return 0;
}

int cmd_lecho(FILE *source, Parse &params )
{
  for(int i=0 ; i<params.get_argc() ; i++ ){
    char argv[1024];
    params[i] >> argv;
    printf("[%s] ",argv);
  }
  printf("\n");
  return 0;
}
