#include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include <ctype.h>
#include <time.h>
// #include <sys/ea.h>
#include <sys/video.h>

#define INCL_DOSMISC
// #include <os2.h>

#include "nyaos.h"
#include "finds.h"
#include "strtok.h"
#include "strbuffer.h"
#include "prompt.h"

extern int nhistories;
extern int execute_result;
extern char *get_asciitype_ea(const char *fname,const char *eatype,int *l=0);

char *strcpytail(char *dp,const char *sp)
{
  while( *sp != '\0' )
    *dp++ = *sp++;
  *dp = '\0';
  return dp;
}

/* パスが、ホームディレクトリ名を含んでいれば、「～」に変換する。
 *	sp ファイル名
 * return 書き変えたファイル名(NULLの時は「～」を含まなかった。
 */
static char *to_tilda_name(const char *sp)
{
  const char *home=getShellEnv("HOME");
  if( home == NULL || *home == '\0' )
    return NULL;
  
  /* 比較する */
  while( *home != '\0' ){
    int x=tolower(*home & 255); if( x == '\\' ) x='/';
    int y=tolower(*sp   & 255); if( y == '\\' ) y='/';
    if( x != y )
      return NULL;
    ++home ; ++sp;
  }

  StrBuffer sbuf;
  sbuf << '~' << sp;

  return sbuf.finish();
}

/* 真のファイル名を得る(ファイル名のみ、ディレクトリは含まず)
 *   return 書き込み後の dp
 * ファイル名が得られなかった時は NULL を返す。
 */
static char *paste_true_name(char *dp,const char *cwd)
{
  Dir dir;
  if( dir.dosfindfirst( cwd ) != 0 )
    return NULL;

  return strcpytail(dp,dir.get_name());
}

/* 大文字・小文字を区別した正確なファイル名を得るが、
 * 元の SRC を破壊してしまう。破壊したく無い場合は
 * correct_case() を使うべし。
 *    in	src オリジナルファイル名(破壊される)
 *    out	dst 大文字・小文字を正確にしたファイル名
 * return dst の末尾へのポインタ
 */
static char *_correct_case(char *src,char *dst)
{
  /* 元の文字列は
   *    x:\hoge\hoge
   *    x:\
   * のどちらかのケース。
   */
  
  char *dp=dst;

  /* ドライブ文字処理 */
  if( isalpha(*src & 255)  &&  *(src+1)==':' ){
    *dp++ = *src++;
    *dp++ = *src++;
  }
  /* ルートディレクトリ処理 */
  if( *src=='\\' || *src=='/' ){
    ++src;
    *dp++ = '\\';
  }
  /* サブディレクトリ名を切り出す。*/
  Strtok tzer(src);
  char *token=tzer.cut_with("\\/");
  if( token != NULL ){
    for(;;){
      /* まず、素のファイル名をコピーしておく。 */
      char *tail_at_normal=strcpytail(dp,token);

      /* 大文字・小文字の正確なファイル名が得られたら、
       * そちらを先にコピーした上に上書きする。 */

      if( (dp=paste_true_name(dp,dst) ) == NULL )
	dp = tail_at_normal;

      if( (token=tzer.cut_with("\\/")) == NULL )
	break;

      *dp++ = '\\';
    }
  }
  *dp = '\0';
  return dp;
}

/* SRC の大文字/小文字を正しく修正したパス名を dst へ得る。
 * SRC は破壊しない。(open.cc から使われる)
 *	src      元のパス名
 *	dst,size 変換先のバッファとそのサイズ
 */
void correct_case( char *dst , const char *src , int size )
{
  char *tmp=(char*)alloca(size);
  _fullpath( tmp , src , size );
  _correct_case( tmp , dst );
}

/* パス名の「￥」を「/」へ変換する
 */
void anti_convroot(char *dst)
{
  while( *dst != '\0' ){
    if( *dst == '\\' ){
      *dst++ = '/';
    }else{
      if( is_kanji(*dst) )
	++dst;
      ++dst;
    }
  }
}

/* カレントドライブのカレントディレクトリを大文字・小文字も正確に得る
 * in/out - dst ファイル名(上書きされる)
 * return ファイル名の末尾の位置
 */
char *getcwd_case(char *dst)
{
  char cwd[ FILENAME_MAX ];

  cwd[0] = _getdrive();
  cwd[1] = ':';

  DosError( FERR_DISABLEHARDERR );
  char *rc = _getcwd( cwd+2 , sizeof(cwd)-2 );
  DosError( FERR_ENABLEHARDERR );
  if( rc == NULL ){
    *dst++ = cwd[0];
    *dst++ = cwd[1];
    *dst   = '\0';
    return dst;
  }
  
  char *tail = _correct_case(cwd,dst); // パス名の大文字・小文字を修正
  anti_convroot(dst);	  // パス名の「￥」を「/」へ修正
  return tail;
}

/* ファイルシステムを調べる。
 *	drivenum : ドライブ番号
 * return
 *   0:FAT  , 1:HPFS , 2:CDFS
 */
int query_filesystem(int drivenum)
{
  static unsigned char buffer[ sizeof(FSQBUFFER2)+(3*CCHMAXPATH) ];
  static unsigned char devname[3]="?:";

  ULONG cbBuffer = sizeof(buffer);
  PFSQBUFFER2	pfsqBuffer=(PFSQBUFFER2) buffer;

  devname[0] = toupper(drivenum & 255);
  
  if( DosQueryFSAttach(  devname , 0 , FSAIL_QUERYNAME
		       , pfsqBuffer , &cbBuffer ) != 0 ){
    return -1;
  }else{
    const unsigned char *p = pfsqBuffer->szName + pfsqBuffer->cbName + 1;

    if( p[0]=='F' && p[1]=='A' && p[2]=='T' && p[3]=='\0' )
      return 0;
    else if( p[0]=='H' && p[1]=='P' && p[2]=='F' && p[3]=='S' && p[4]=='\0')
      return 1;
    else if( p[0]=='C' && p[2]=='D' && p[3]=='F' && p[3]=='S' && p[4]=='\0')
      return 2;
    else
      return 3;
  }
}


char *get_cwd_long_name(char *dp)
{
  int rc=0;
  char cwd[ FILENAME_MAX ];
  *dp++ = cwd[0] = _getdrive();
  *dp++ = cwd[1] = ':';
  DosError( FERR_DISABLEHARDERR );    
  rc = _getcwd1( cwd+2 , toupper(cwd[0]) ) ;
  DosError( FERR_ENABLEHARDERR );
  if( rc != 0 )
    return dp;

  try{
    char *p=cwd+2;
    int size=sizeof(cwd)-2;
    (void)convroot(p,size,p);
  }catch(...){
    ; /* これぢゃあ、例外処理にした意味ないなぁ… (^^;;) */
  }
  
  /* A:\
     0123 */
  
  /* ルートディレクトリの場合の例外処理 */
  if( (cwd[2] == '/' || cwd[2] == '\\' ) && cwd[3]=='\0' ){
    *dp++ = '/'; *dp = '\0';
    return dp;
  }

  int filesystem=query_filesystem(cwd[0]);

  for(char *sp=cwd+3 ; ; sp++ ){
    if( *sp == '/' || *sp == '\\' || *sp == '\0' ){
      int org=*sp;
      *sp = '\0'; /* 一時的にルートを 0 にする */
      *dp++ = '/';
      
      char *longname=0;
      if(    filesystem != 2
	 &&  (longname=get_asciitype_ea(cwd,".LONGNAME")) != NULL ){
	/* .LONGNAME が存在する場合は、そちらを使う */
	const char *sp=longname;
	while( *sp != '\0' ){
	  if( *sp == '\r' ){
	    ++sp;
	  }else if( *sp == '\n' ){
	    *dp++ = '\r';
	    ++sp;
	  }else if( '\0' <= *sp && *sp < ' ' ){
	    *dp++ = '^';
	    *dp++ = '@' + *sp++;
	  }else{
	    *dp++ = *sp++;
	  }
	}
	free(longname);
      }else{
	/* さもなければ、ファイル名を大文字・小文字を修正するだけ */
	char *save_dp=dp;
	if( (dp = paste_true_name( dp , cwd )) == NULL ){
	  /* 真のファイル名が得られなかった場合、
	   * 単純にパス名から切り出す。
	   */
	  dp = save_dp;
	  const char *sq=_getname(cwd);
	  while( *sq != '\0' )
	    *dp++ = *sq;
	}
      }
      if( (*sp=org) == '\0' )
	break;
    }
    if( is_kanji(*sp) )
      ++sp;
  }
  *dp = '\0';
  return dp;
}


/* バッファに NYAOS ロゴ(画面最上段に表示させるもの) を書き込む */
static void set_logo_to_prompt(StrBuffer &prompt)
{
  int a=0x0F;
  if( option_vio_cursor_control )
    a = v_getattr();

  const char logo[]=
    " Nihongo Yet Another Os/2 Shell "VERSION
      " (c) 1996-99 HAYAMA,Kaoru ";
	
  prompt << "\x1B[s\x1B[1;44;37m\x1B[H";
  prompt << logo << "\x1b[K";
  prompt << "\x1B[u\x1B[0m";
  
  if( option_vio_cursor_control )
    v_attrib(a);
}

/* バッファに、指定されている全ドライブのカレントディレクトリを書き込む */
static void set_multi_curdir_to_prompt(  StrBuffer &prompt
				       , const char *&promptenv )
{
  int a=0x0F;
  int curdrv=_getdrive();
  if( option_vio_cursor_control )
    a = v_getattr();
  
  /* カーソル位置を記憶 ＆ 画面最上段へ移動 */
  prompt << "\x1b[s\x1B[H";

  /* ここで変数 length は、最上段での表示文字数をカウントする */
  int length=0;

  while(   *++promptenv != '}' 
	&& *  promptenv != '\0'
	&& length < screen_width-1 ){
    
    /* ${...} の中はドライブレターのみ、他は無視 */
    if( !isalpha(*promptenv) )
      continue;

    char curdir[ FILENAME_MAX ];
    
    int drv=toupper(*promptenv);

    /* カレントドライブならば赤、さもなければ青で表示する。*/
    prompt << "\x1B[1;" << (drv==curdrv ? "41" : "44")
      << ";37m" << (char) drv << ':';
    length += 3;
    
    DosError( FERR_DISABLEHARDERR );    
    _getcwd1(curdir,drv);
    DosError( FERR_ENABLEHARDERR );
    int len=strlen(curdir);
    
    if( length + len < screen_width-1 ){
      /* カレントディレクトリのサイズが十分、画面幅に収まる場合 */
      length += len;
      prompt << curdir;
    }else{
      /* カレントディレクトリのサイズが、画面幅に入らない
       * ⇒ クリッピング処理 */
      
      const char *p=curdir;
      for(int i=length ; i<screen_width-4 ; i++ ){
	if( is_kanji(*p) ){
	  prompt << *++p;
	  ++i;
	}
	prompt << *++p;
      }
      if( length < 74 )
	prompt << "...";
      prompt << "\x1b[0m ";
      while( *promptenv != '}' && *promptenv != '\0' )
	++promptenv;
      break;
    }
    prompt << "\x1B[0m ";
  }
  prompt << "\x1b[K\x1b[u";

  if( option_vio_cursor_control )
    v_attrib(a);
}


/* プロンプトを作成する。
 *     promptenv プロンプトの元文字列
 */
int Prompt::parse( const char *promptenv )
{
  try{
    StrBuffer prompt;
    used_topline=false;
    
    time_t now;
    time( &now );
    struct tm *thetime = localtime( &now );
    
    while( *promptenv != '\0' ){
      if( *promptenv == '$' ){
	switch( promptenv++ , to_upper(*promptenv) ){
	case '!':
	  prompt << nhistories+1;	break;
	case '@': prompt << _getvol(0);		break; /* ボリュームラベル */
	case '$': prompt << '$';			break;
	case '_': prompt << '\n';			break;
	case 'A': prompt << '&';			break;
	case 'B': prompt << '|';			break;
	case 'C': prompt << '(';			break;
	case 'E': prompt << '\x1b'; 		break;
	case 'F': prompt << ')';			break;
	case 'G': prompt << '>';			break;
	case 'H': prompt << '\b';			break;
	case 'L': prompt << '<';			break;
	case 'Q': prompt << '=';			break;
	case 'S': prompt << ' ';			break;
	case 'N': prompt << (char)_getdrive();	break;
	case 'R': prompt << execute_result;	break;
	case 'D':/* 現在の日付 */
	  prompt.putNumber( thetime->tm_year+1900 , 4 , '0' ) << '-';
	  prompt.putNumber( thetime->tm_mon +1    , 2 , '0' ) << '-';
	  prompt.putNumber( thetime->tm_mday      , 2 , '0' );
	  break;
	case 'T':/* 現在の時刻 */
	  prompt.putNumber( thetime->tm_hour	, 2 , '0' ) << ':';
	  prompt.putNumber( thetime->tm_min	, 2 , '0' ) << ':';
	  prompt.putNumber( thetime->tm_sec	, 2 , '0' );
	  break;
	case 'I':/* ロゴ */
	  set_logo_to_prompt( prompt );
	  used_topline = true;
	  break;
	case '{':/* 各ドライブのカレントディレクトリ */
	  set_multi_curdir_to_prompt( prompt , promptenv );
	  used_topline = true;
	  break;
	case 'V':/* OS/2のバージョン */
	  prompt << "The Operating System/2 Version is "
	    << _osmajor/10 << '.' << _osminor;
	  break;
	case 'P':/* カレントディレクトリ */
	  {
	    char curdir[ FILENAME_MAX ];
	    getcwd_case( curdir );
	    prompt << curdir;
	  }
	  break;
	  
	case 'W':/* カレントディレクトリ:ホームディレクトリを「~」に変換する */
	  {
	    char curdir[ FILENAME_MAX ];
	    getcwd_case( curdir );
	    char *tilda_name=to_tilda_name(curdir);
	    if( tilda_name != NULL ){
	      prompt << tilda_name;
	      free(tilda_name);
	    }else{
	      prompt << curdir;
	    }
	  }
	  break;
	  
	case 'Z':
	  switch( ++promptenv , to_upper(*promptenv) ){
	  case 'A': 
	    prompt << '\a'; break;
	  case 'H': /* ヒストリ番号 */
	    prompt << nhistories+1;
	    break;
	  case 'V': /* ボリュームラベル */
	    prompt << _getvol(0);
	    break;
	  case 'P': /* LONGNAME */
	    {
	      char curdir[FILENAME_MAX];
	      get_cwd_long_name( curdir );
	      prompt << curdir;
	    }
	    break;
	  case '\0':
	    goto promptend;
	  }
	  break;
	}
	promptenv++;
      }else{
	if( is_kanji(*promptenv) )
	  prompt << *promptenv++;
	prompt << *promptenv++;
      }
    }
  promptend:    
    free( promptstr );
    promptstr = prompt.finish();
    if( used_topline ){
      prompt << "\x1B[1A\x1B[1B" << promptstr;
      promptstr = prompt.finish();
    }
    return 0;
  }catch( StrBuffer::MallocError ){
    free( promptstr );
    promptstr = NULL;
    return -1;
  }
}
