#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "macros.h"
#include "parse.h"

int Parse::option_semicolon_terminate=1;

char *Substr::dup() const
{
  if( len > 0 && ptr != NULL ){
    char *s=(char*)malloc(len+1);
    
    char *p=s;
    for(int i=0;i<len;i++)
      *p++ = ptr[i];
    *p = '\0';
    return s;
  }
  return NULL;
}

void Substr::operator >> (SmartPtr dp) const
{
  for(int i=0;i<len;i++)
    *dp++ = ptr[i];
  *dp = '\0';
}

char *Substr::quote(char *dp) const
{
  const char *tail=ptr+len;
  const char *sp=ptr;
  
  while( sp < tail ){
    /* 単一の「"」は無視するが、連続する「""」は「"」に変換する */
    if( *sp == '"' ){
      if( *++sp == '"' ){
	*dp++ = '"';
	sp++;
      }
    }else{
      if( is_kanji(*sp) )
	*dp++ = *sp++;
      *dp++ = *sp++;
    }
  }
  *dp = '\0';
  return dp;
}

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
    system( buffer );
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
    if( pipemode == REDIRECT ){
      fclose(output_fp);
    }else{
      pclose(output_fp);
    }
  }
  /* 引数の後始末 */
  if( args != argbase  &&  args != NULL )
    delete args;
}

int Parse::check_redirect()
{
  int ionum = 1;

  if( *sp=='0' || *sp=='1' || *sp=='2' )
    ionum = *sp++ - '0';

  if( *sp == '<' ){
    ionum = 0;
    ++sp;
  }else if( *sp == '>' ){
    if( *++sp == '>' ){
      appendflag[ ionum ] = 1;
      ++sp;
    }
  }else{
    fprintf(stderr,"nyaos internal error occurs ( redirect ? )\n");
    return 1;
  }
  
  if( *sp == '&' )
    ++sp;

  while( isspace(*sp & 255) )
    ++sp;

  redirect[ ionum ].ptr = sp;
  
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
  redirect[ ionum ].len = sp - redirect[ ionum ].ptr ;
  return 0;
}


/* 末尾でないときは、値 \0 を返す。
 * 末尾の時は、「&」「|」「0」(0は\0ではない)を返す。
 *
 * terminal は 「&」「|」「\0」が設定される。
 */
int Parse::tailcheck ()
{
  if( *sp=='&' ){
    tail = sp++;
    if( *sp == '&' ){
      nextcmds = ++sp;
      return terminal = AND_TERMINAL;
    }else if( *sp == ';' ){
      nextcmds = ++sp;
      return terminal = SEMI_TERMINAL;
    }else{
      nextcmds = sp;
      return terminal = AMP_TERMINAL;
    }
  }

  if( *sp=='|' ){
    tail = sp++;
    if( *sp=='|' ){
      nextcmds = ++sp;
      return terminal=OR_TERMINAL;
    }else if( *sp=='&' ){
      nextcmds = ++sp;
      return terminal=PIPEALL_TERMINAL;
    }else{
      nextcmds = sp;
      return terminal=PIPE_TERMINAL;
    }
  }

  if( *sp=='\0' ){
    tail=nextcmds=sp;
    return terminal=NULL_TERMINAL;
  }
  return terminal=NOT_TERMINAL;
}

int Parse::check ()
{
  argc = 0;

  redirect[0].clean(); appendflag[0] = 0;
  redirect[1].clean(); appendflag[1] = 0;
  redirect[2].clean(); appendflag[2] = 0;

  terminal = NOT_TERMINAL;

  for(;;){
    if( argc+1 >= limit ){
      if( args == argbase ){
	args = new Substr[limit+=30];
	assert( args != NULL );
	for(int i=0;i<limit;i++)
	  args[i] = argbase[i];
      }else{
	Substr *tmp=new Substr[limit + 30];
	for(int i=0;i<limit;i++)
	  tmp[i] = args[i];
	delete args;
	args = tmp;
	limit += 30;
	assert( args != NULL );
      }
    }

    /* 空白を読み飛ばす */
    while( *sp != '\0' &&  is_space(*sp) )
      ++sp;

    if( tailcheck() != 0 ){
      args[ argc ].ptr = NULL;
      args[ argc ].len = 0;
      return terminal;
    }

    if(   *sp == '<' || *sp == '>'
       || ((*sp=='1' || *sp=='2' ) && sp[1] == '>') ){
      
      if( check_redirect() != 0 )
	return err=-1;
      continue;
    }
    
    args[ argc ].ptr = sp;
    while( !is_space(*sp) ){
      if( tailcheck() ){
	args[ argc ].len = tail - args[argc].ptr;
	++argc;
	return terminal;
      }
      if( *sp=='<' || *sp=='>' 
	 || ((*sp=='1' || *sp=='2') && sp[1] == '>' ) )
	break;

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
	    terminal = NULL_TERMINAL;
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
    args[ argc ].len = sp - args[argc].ptr;
    ++argc;
  }
 exit:
  args[ argc ].len = sp - args[argc].ptr;
  ++argc;
  return terminal;
}

FILE *Parse::open_stdout()
{
  if( redirect[1] != NULL ){
    /* リダイレクト先が、ファイルに指定されている場合
     * 末尾が '|' では、おかしい
     */
    
    if( terminal==PIPE_TERMINAL ){
      fprintf(stderr,"to which redirects?\n");
      return NULL;
    }
    
    char *fname = (char*)alloca( redirect[1].len+1 ); /* ! */
    redirect[1].quote(fname);
    pipemode = REDIRECT;
    return output_fp = fopen( fname , appendflag[1] ? "a" : "w" );

  }else if( terminal==PIPE_TERMINAL || terminal==PIPEALL_TERMINAL ){
    
    pipemode = PIPE;
    return output_fp = popen( nextcmds , "w" );

  }else
    return stdout;
}

FILE *Parse::open_stdin()
{
  if( redirect[0] != NULL ){
    char *fname = (char*)alloca( redirect[0].len+1 ); /* ! */
    redirect[0].quote(fname);
    return input_fp = fopen( fname , "r" );
  }else 
    return stdin;
}

int Parse::call_as_main(int (*routine)(int argc,char **argv) )
{
  /* 一般の引数 */
  int i;
  char **argv=(char**)alloca(sizeof(char*)*(argc+3));
  for(i=0;i<argc;i++){
    argv[i]=(char *)alloca(args[i].len+5);
    copy(i,SmartPtr(argv[i],args[i].len+5) );
  }
  argv[i] = NULL;
  
  return (*routine)(argc,argv);
}

int Parse::call_as_main(int (*routine)(int argc,char **argv,FILE *fout))
{
  int i;
  char **argv=(char**)alloca(sizeof(char*)*(argc+5));
  for(i=0;i<argc;i++){
    argv[i]=(char *)alloca(args[i].len+5);
    copy(i,SmartPtr(argv[i],args[i].len+5));
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

SmartPtr Parse::copy(int n, SmartPtr dp, int flag )
{
  if( n < argc ){
    const char *sp   = args[n].ptr ;
    const char *tail = sp + args[n].len ;

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

SmartPtr Parse::betacopy(SmartPtr dp,int n)
{
  const char *ssp=args[n].ptr;

  while( ssp < tail )
    *dp++ = *ssp++;

  *dp = '\0';
  return dp;
}

SmartPtr Parse::copyall(int n, SmartPtr dp, int flag)
{
  if( n < argc ){
    
    /* 基本的に引用符は普通の文字と同様にコピ－する。
     * ただし、キャレットの次の機能文字を無効化する。
     * キャレット自身はコピーしない(「^^」は別)
     */
    
    const char *ssp=args[n].ptr;
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
