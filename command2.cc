#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "nyaos.h"
#include "parse.h"
#include "edlin.h"
#include "pathlist.h"
#include "errmsg.h"

int cmd_hotkey(FILE *source , Parse &parse )
{
  if( parse.get_argc() < 3 )
    return 2;

  char *keyName  = static_cast<char*>(alloca(parse.get_length(1)+1));
  char *progName = static_cast<char*>(alloca(parse.get_length(2)+1));
  
  parse.copy(1,keyName);
  parse.copy(2,progName);
  if( Shell::bind_hotkey(keyName,progName,0) != 0 ){
    ErrMsg::say( ErrMsg::InvalidKeyName , keyName , 0 );
    return 1;
  }
  return 0;
}

int cmd_bind(FILE *source, Parse &param )
{
  int rc=0;

  struct{
    const char *name;
    void (*func)();
    const char *usage;
  } table2[]={
    { "nyaos" , Shell::bindkey_nyaos    , "like tcsh but ^P,^N like Vz." },
    { "emacs" , Shell::bindkey_tcshlike , "key-bindings like emacs" },
    { "tcsh"  , Shell::bindkey_tcshlike , "same as emacs" },
    { "ws"    , Shell::bindkey_wordstar , "key-bindings like wordstar" },
    { "vz"    , Shell::bindkey_wordstar , "same as ws" },
  };

  if( param.get_argc() < 2 ){
    FILE *fout=param.open_stdout();
    for(unsigned i=0;i<numof(table2);i++)
      fprintf(fout,"%s\t: %s\n",table2[i].name,table2[i].usage);
  }else{
    char *buffer=(char*)alloca(param.get_length(1)+1);
    param.copy(1,buffer);
    for(unsigned i=0;i<numof(table2);i++){
      if(   to_lower(buffer[0])==table2[i].name[0]
	 && stricmp(buffer,table2[i].name)==0 ){
	(*table2[i].func)();
	return 0;
      }
    }
    ErrMsg::say( ErrMsg::InvalidKeySetName , buffer , 0 );
    rc = 1;
  }
  return rc;
}

int cmd_bindkey(FILE *source,Parse &param)
{
  if( param.get_argc() < 3 ){
    Shell::bindlist( param.open_stdout() );
    return 0;
  }

  char *key  = (char*)alloca(param.get_length(1)+1);
  param.copy(1,key);
  char *func = (char*)alloca(param.get_length(2)+1);
  param.copy(2,func);
  
  switch( Shell::bindkey(key,func) ){
  case 1:
    ErrMsg::say(ErrMsg::InvalidKeyName,key,0);
    return 1;

  case 2:
    /* 機能名が無い → complete モードの bindmap に bind */
    switch( Edlin::bindCompleteKey(key,func) ){
    case 1:
      ErrMsg::say(ErrMsg::InvalidKeyName,key,0);
      return 1;
    case 2:
      ErrMsg::say(ErrMsg::InvalidFuncName,func,0);
      return 2;
    }
  }
  return 0;
}

/* 環境変数を本当に消す (putenv の消し方では不十分)
 *	env 環境変数名(必大文字)
 * return
 *	 0 成功
 *	-1 そんな環境変数は存在しない
 */
static int unsetenv(const char *env)
{
  int len=strlen(env);
  
  if( environ == NULL )
    return -1;

  for( char **p=environ ; *p != NULL ; p++ ){
    if( strncmp(*p,env,len)==0 && *(*p+len)=='=' ){
      char **q=p+1;
      if( *q == NULL ){
	*p = NULL;
	return 0;
      }
      while( *(q+1) != NULL )
	++q;
      
      *p = *q;
      *q = NULL;
      return 0;
    }
  }
  return -1;
}


/* putenv は putenv("VAR=VALUE")という形式でないと受けつけない為に作った
 * フィルター関数。VAR は小文字でも大文字に変換してくれる。
 *	env    環境変数名
 *	value  値。NULL か "\0" で、その環境変数を消す。
 */
static void setenv(const char *env,const char *value)
{
  int env_len=strlen(env);
  int value_len=(value != NULL ? strlen(value) : 0 );
  
  char *buffer=(char*)malloc(env_len+value_len+2);
  char *dp = buffer;
  while( *env != '\0' ){
    if( islower(*env & 255) ){
      *dp++ = toupper(*env);
      ++env;
    }else{
      *dp++ = *env++;
    }
  }
 
  if( value == NULL  ||  *value == '\0' ){
    *dp = '\0';
    unsetenv( buffer );
    free(buffer);
  }else{
    *dp++ = '=';
    strcpy( dp , value );
    putenv( buffer );
  }
}

int cmd_set( FILE *srcfil, Parse &params )
{
  if( params.get_argc() < 2 )
    return RC_HOOK;

  const char *sp = params.get_parameter();
  if( sp == NULL )
    return RC_HOOK;

  const char *tail=params.get_tail();
  int appendmode=0;
  
  /* 変数名の前の空白のスキップ */
  while( *sp!='\0' && is_space(*sp) )
    sp++;

  // --------------- 変数名のコピ－ --------------
  char env_name[256];
  char *dp=env_name;
  for(;;){
    if( dp >= tailof(env_name)-2 ){
      // サイズオーバー
      ErrMsg::say( ErrMsg::TooLongVarName , "set" , 0 );
      return 1;
    }else if( sp >= tail || *sp == '>'  || *sp == '\0' ){
      // 変数名が無い → 画面表示のみ。→ オリジナル set に任せる。
      return RC_HOOK;
    }else if( *sp=='<' ){
      // 入力リダイレクトはできないのでエラー
      ErrMsg::say( ErrMsg::MustNotInputRedirect , "set",0);
      return 1;
    }else if( *sp=='+'  &&  *(sp+1) == '=' ){
      // 「+=」演算子
      appendmode = 1;
      sp += 2;
      break;
    }else if( *sp=='=' ){
      sp++;
      break;
    }else if( is_space(*sp) ){
      // スペースがあった場合は、それを読み飛ばした上で、
      // 「=」「+=」があるかチェック。
      // 無ければ、変数内容の表示だけなので、
      // オリジナル set に動作を任せる。
      do{
	++sp;
      }while( is_space(*sp) );
      if( *sp == '<' ){
	ErrMsg::say( ErrMsg::MustNotInputRedirect,"set",0);
	return 1;
      }else if( *sp == '>' ){
	return RC_HOOK;
      }else if( *sp=='+' && *(sp+1)=='=' ){
	appendmode = 1;
	sp+=2;
	break;
      }else if( *sp=='=' ){
	++sp;
	break;
      }else{
	// 代入演算子が無いので、変数の内容を表示させる.
	*dp = '\0';
	const char *env_value=getenv(env_name);
	FILE *fp=params.open_stdout();
	fprintf(fp,"%s=%s\n",env_name,(env_value ? env_value : "(null)" ) );
	return 0;
      }
    }
    if( is_kanji(*sp) ){
      *dp++ = *sp++;
      *dp++ = *sp++;
    }else if( is_lower(*sp & 255) ){
      *dp++ = toupper(*sp);
      ++sp;
    }else{
      *dp++ = *sp++;
    }
  }
  *dp = '\0';
  
  // 「=」以降の空白を削除(ここまでする必要ない？)
  for(;;){
    if( *sp == '\0' || sp >= tail ){
      //「set ahaha=」で環境変数 ahaha を削除する
      //「set ahaha+=」は何もしない。
      if( ! appendmode )
	setenv( env_name , NULL );
      return 0;
    }
    if( !is_space(*sp) )
      break;
    ++sp;
  }
  
  // 右辺値の取得
  
  char *final_space=NULL;
  int quote=0;
  int compati=( *sp != '"' );
  
  char env_value[1024];
  dp = env_value;
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
  *dp = '\0';
  
  if( final_space != NULL )
    *final_space = '\0';
  
  if( appendmode ){
    PathList pathlist;
    pathlist.append( env_value );
    const char *org=getShellEnv(env_name);
    if( org != NULL )
      pathlist.append( getShellEnv(env_name) );
    pathlist.listing( env_value );
  }
  setenv( env_name , env_value );
  return 0;
}

int cmd_echo(FILE *srcfil, Parse &params )
{
  FILE *fout=params.open_stdout();
  if( fout == NULL ){
    ErrMsg::say( ErrMsg::CantOutputRedirect , 0 );
    return 0;
  }
  bool quote=false;
  
  const char *sp=params.get_argv(1);

  if( sp != NULL ){
    /* echo off や echo on では何も表示させないようにする
     * バッチファイルの先頭などの「@echo off」対策
     */
    if( strnicmp(sp,"OFF",3) ==0 || strnicmp(sp,"ON",2)==0 )
      return 0;

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
	case 'a':
	  putc('\a',fout); break;
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
      }else if( !quote && Parse::isRedirectMark(sp) ){
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
  for(int i=0 ; i<params.get_argc() ; i++ )
    printf("[%.*s] " , params[i].len , params[i].ptr );
  putchar('\n');
  return 0;
}
