#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "macros.h"
#include "parse.h"

void Pipe::open(const char *cmdl,const char *modestr)
{
  mode = modestr;
  cmdline = cmdl;

  if( _osmode == OS2_MODE ){
    fp=popen(cmdline,modestr);
    return;
  }

  tmpfname = tempnam("C:/","NYA");
  assert(tmpfname != NULL );

  if( mode[0] == 'r' ){
    char buffer[1024];
    sprintf(buffer , "%s >%s",cmdline,tmpfname);
    system(buffer);
  }
  fp=fopen(tmpfname,modestr);
}

Pipe::~Pipe()
{
  if( fp == NULL )
    return;

  if( _osmode == OS2_MODE ){
    pclose(fp);
    return;
  }
  
  fclose(fp);

  if( mode != NULL && mode[0] == 'w' ){
    char buffer[1024];
    sprintf(buffer , "%s <%s",cmdline,tmpfname);
    system(buffer);
  }
}

Parse::~Parse()
{
  /* パイプの後始末 */
  if( input_fp != NULL  &&  input_fp != stdin )
    fclose(input_fp);
  if( output_fp != NULL  &&  output_fp != stdout ){
    if( pipemode == REDIRECT )
      fclose(output_fp);
    else
      pclose(output_fp);
  }
  /* 引数の後始末 */
  if( args != argbase  &&  args != NULL )
    free(args);
}

int Parse::check_redirect()
{
  int mark=*sp;
  if( is_kanji(*sp) )
    ++sp;

  /*  >> , >& , >>& を許容 */
  if( *++sp == '>' ){
    isappend = true;
    ++sp;
  }
  if( *sp == '&' )
    ++sp;

  while( isspace(*sp & 255) )
    ++sp;

  const char *top=sp;

  if( tailcheck() )
    return err=-1;

  do{
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
    if( *sp=='"' ){
      do{
	if( is_kanji(*sp) )
	   ++sp;
	++sp;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
      sp++;
    }
  }while( *sp != '\0'  &&  !isspace(*sp & 255) );
 exit:
  switch( mark ){
  case '>':
    output_redirect = top;
    output_redirect_length = sp-top;
    break;
  case '<':
    input_redirect = top;
    input_redirect_length = sp-top;
    break;
  }
  return 0;
}


/* 末尾でないときは、値 \0 を返す。
 * 末尾の時は、「&」「|」「0」(0は\0ではない)を返す。
 *
 * terminal は 「&」「|」「\0」が設定される。
 */
int Parse::tailcheck()
{
  if( *sp=='&' ){
    tail = sp++;
    nextcmds = (*sp=='&' ? ++sp : sp );
    return terminal='&';
  }

  if( *sp=='|' ){
    tail = sp++;
    if( *sp=='|' ){
      nextcmds = ++sp;
      return terminal='&';
    }else if( *sp=='&'){
      nextcmds = ++sp;
      return terminal='|';
    }else{
      nextcmds = sp;
      return terminal='|';
    }
  }
  if( *sp=='\0' ){
    tail=nextcmds=sp;
    terminal = '\0';
    return '0';
  }
  return 0;
}

int Parse::check()
{
  argc = 0;
  output_redirect = NULL;
  input_redirect = NULL;
  isappend = false;
  terminal = -1;

  for(;;){
    /* 空白を読み飛ばす */
    while( *sp != '\0' &&  is_space(*sp) )
      ++sp;

    if( tailcheck() != 0 )
      return terminal;

    if( *sp == '<' || *sp == '>' ){
      /* リダイレクトマークがあるのに、直後に「&」などがあると、
       * エラーを返すべく終了する。
       */
      if( check_redirect() != 0 )
	return err=-1;
      continue;
    }
    
    if( argc+1 >= limit ){
      if( args != argbase ){
	args = (Array*)malloc(sizeof(Array)*(limit += 30) );
	assert( args != NULL );
	memcpy( args , argbase , sizeof(argbase) );
      }else{
	args = (Array*)realloc( args , sizeof(Array)*(limit += 30) );
	assert( args != NULL );
      }
    }
    args[ argc ].pointor = sp;

    while( !is_space(*sp) && tailcheck() == 0 ){
      if( *sp == '^' && *(sp+1) != '\0' ){
	/* キャレットはヌル以外の次の機能文字を無効化する。*/
	if( is_kanji(*++sp) ){
	  sp++;
	  assert(*sp != '\0');
	}
	sp++;
      }else if( *sp == '"' ){
	/* クォ－トは次のクォ－トが来るまで、
	 * ヌルとクォ－ト以外の全ての機能文字を無効化する。
	 * キャレットも無効化される。
	 */
	do{
	  if( is_kanji(*sp) ){
	    ++sp;
	    assert(*sp != '\0' );
	  }
	  ++sp;
	  if( *sp=='\0' ){
	    terminal = 0;
	    tail=nextcmds=sp;
	    goto exit;
	  }
	}while( *sp != '"' );
	++sp;
      }else{
	/* ANKなら2byte,漢字なら1byteずらす */
	if( is_kanji(*sp) )
	  ++sp;
	++sp;
      }
    }
    args[ argc ].length = sp - args[argc].pointor;
    ++argc;
  }
 exit:
  args[ argc ].length = sp - args[argc].pointor;
  ++argc;
  return terminal;
}

FILE *Parse::open_stdout()
{
  if( output_redirect != NULL ){
    /* リダイレクト先が、ファイルに指定されている場合
     * 末尾が '|' では、おかしい
     */

    if( terminal == '|' )
      return NULL;
    
    char *fname = (char*)alloca( output_redirect_length+1 ); /* ! */
    memcpy(fname,output_redirect , output_redirect_length );
    fname[ output_redirect_length ] = '\0';
    pipemode = REDIRECT;
    return output_fp = fopen( fname , isappend ? "a" : "w" );
  }else if( terminal == '|' ){
    pipemode = PIPE;
    return output_fp = popen( nextcmds , "w" );
  }else
    return stdout;
}

FILE *Parse::open_stdin()
{
  if( input_redirect != NULL ){
    char *fname = (char*)alloca( input_redirect_length+1 ); /* ! */
    memcpy(fname,input_redirect, input_redirect_length );
    fname[ input_redirect_length ] = '\0';
    
    return input_fp = fopen( fname , "r" );
  }else 
    return stdin;
}

int Parse::call_as_main(int (*routine)(int argc,char **argv) )
{
  int i;
  char **argv=(char**)alloca(sizeof(char*)*(argc+3));
  for(i=0;i<argc;i++){
    argv[i]=(char *)alloca(args[i].length+5);
    copy(i,argv[i]);
  }
  argv[i] = NULL;
  return (*routine)(argc,argv);
}

int Parse::call_as_main(int (*routine)(int argc,char **argv,FILE *fout))
{
  int i;
  char **argv=(char**)alloca(sizeof(char*)*(argc+5));
  for(i=0;i<argc;i++){
    argv[i]=(char *)alloca(args[i].length+5);
    copy(i,argv[i]);
  }

  argv[i] = NULL;
  if( _osmode != OS2_MODE )
    return (*routine)(argc,argv,stdout);

  FILE *fout=open_stdout();
  if( fout==NULL ){
    fputs("nyaos : cannot make file or pipe.\n",stderr);
    return -1;
  }
  
  return (*routine)(argc,argv,fout);
}

char *Parse::copy(int n, char *dp, int flag )
{
  if( n < argc ){
    const char *sp   = args[n].pointor ;
    const char *tail = sp + args[n].length ;

    /* 基本的に引用符とキャレットはコピ－しない。
     * 二重キャレット「^^」は「^」としてコピ－する。
     * ただし、引用符に囲まれたキャレットはそのままコピ－する。
     */

    bool quote=false;
    
    /* UNIXライクなパス/オプション指定法を OS/2 ライクに変換する処理
     * (1) s|^-|/|;
     */
       
    if( (flag & REPLACE_SLASH)  &&  *sp == '-' ){
      *dp++ = '/';
      ++sp;
    }

    int lastchar = -1;

    while( sp < tail ){
      
      if( *sp == '"' ){
	/* 引用符の場合は、フラグを反転させて、ポインタを進めるだけ。*/

	if( (flag & QUOTE_COPY)==0 && *(sp+1) == '"' ){
	  /* 連続する二つの引用符は、単一の引用符に変換する。*/
	  *dp++ = '"';
	  sp += 2;
	}else{
	  quote = !quote;
	  ++sp;
	  if( flag & QUOTE_COPY )
	    *dp++ = '"';
	}

      }else if( *sp == '^' && !quote ){
	/* キャレットの次の文字を無条件に put する。 */
	if( *++sp != '\0' ){
	  if( is_kanji(lastchar=*sp) )
	    *dp++ = *sp++;
	  *dp++ = *sp++;

	}else{
	  *dp = '\0';
	  return dp;
	}
      }else if( (flag & REPLACE_SLASH) && !quote && *sp == '/' ){
	/* UNIXライクなパス/オプション指定法を OS/2 ライクに変換する処理
	 * (2) s|/|\|g; (ただし引用符に囲まれていないもの)
	 */

	lastchar = *dp++ = '\\';
	sp++;
      }else{
	/* それ以外はコピ－ */
	if( is_kanji(lastchar=*sp) ){
	  *dp++ = *sp++;
	  assert(*sp != '\0' );
	}
	*dp++ = *sp++;
      }
    }
    if(   (flag & REPLACE_SLASH) != 0  
       && (lastchar=='/' || lastchar=='\\' ) )
      *dp++ = '.';

    *dp = '\0';
  }
  return dp;
}
char *Parse::betacopy(char *dp,int n=0)
{
  const char *ssp=args[n].pointor;

  while( ssp < tail ){
    *dp++ = *ssp++;
  }
  *dp = '\0';
  return dp;
}

char *Parse::copyall(int n, char *dp, int flag)
{
  if( n < argc ){
    
    /* 基本的に引用符は普通の文字と同様にコピ－する。
     * ただし、キャレットの次の機能文字を無効化する。
     * キャレット自身はコピーしない(「^^」は別)
     */
    
    const char *ssp=args[n].pointor;
    bool quote=false;

    /* UNIXライクなパス/オプション指定法を OS/2 ライクに変換する処理
     * (1) s|^-|/|;
     */
    if( (flag & REPLACE_SLASH) &&  *ssp == '-' ){
      *dp++ = '/';
      ++ssp;
    }

    int lastchar = -1;

    while( ssp < tail ){
      if( *ssp == '"' ){
	/* 引用符は、フラグを反転させる。*/

	if( (flag & QUOTE_COPY)==0 && *(ssp+1) == '"' ){
	  /* 連続する二つの引用符は、単一の引用符に変換する。*/
	  *dp++ = '"';
	  ssp += 2;
	}else{
	  quote = !quote;
	  ++ssp;
	  if( flag & QUOTE_COPY )
	    *dp++ = '"';
	}
	continue;
      }

      if( *ssp=='^' && !quote ){
	/* 引用符の中にないキャレットは次の特殊文字の機能を
	 * 無効化する。
	 */
	if( *++ssp != '\0' ){
	  if( is_kanji(lastchar=*ssp) )
	    *dp++ = *ssp++;
	  *dp++ = *ssp++;
	}else{
	  *dp++ = '\0';
	  return dp;
	}
	continue;
      }
      
      if( (flag & REPLACE_SLASH) && !quote ){
	/* UNIXライクなパス/オプション指定法を OS/2 ライクに変換する処理 */
	
	if( *ssp == '/' ){
	  /* (2) s|/|\|g; (ただし引用符に囲まれていないもの) */
	  lastchar = *dp++ = '\\';
	  ssp++;
	  if( *ssp == '\0' || is_space(*ssp) )
	    *dp++ = '.';
	  continue;
	}else if( *ssp == '-' && is_space(lastchar) ){
	  lastchar = *dp++ = '/';
	  ssp++;
	  continue;
	}else if( *ssp=='\\' && (ssp[1]=='\0' || is_space(ssp[1]) )){
	  *dp++ = *ssp++;
	  lastchar = *dp++ = '.';
	  continue;
	}
      }
      /* それ以外はコピ－ */
      if( is_kanji(lastchar=*ssp) )
	*dp++ = *ssp++;
      *dp++ = *ssp++;
    }
    *dp = '\0';
  }
  return dp;
}

#if 0

int main(void)
{
  char buffer[256];

  while( fgets(buffer,sizeof(buffer),stdin) != NULL ){
    Parse params(buffer);
    int argc=params.get_argc();
    printf( "argc == %d\n",argc);
    for(int i=0 ; i<argc ; i++ ){
      char args[128];
      params.copy(i,args);
      printf("argv[%d]==\"%s\"\n",i,args);
    }
    putchar('\n');
  }
  return 0;
}
#endif
