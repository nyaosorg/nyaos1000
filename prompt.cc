#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/ea.h>
#include <sys/video.h>

#define INCL_DOSMISC
#include <os2.h>

// #include "edlin.h"
#include "nyaos.h"
#include "finds.h"
#include "strtok.h"

extern int nhistories;
extern int execute_result;

extern char *get_ea_longname( const char *fname );

char *strcpytail(char *dp,const char *sp)
{
  while( *sp != '\0' )
    *dp++ = *sp++;
  *dp = '\0';
  return dp;
}

/* パスが、ホームディレクトリ名を含んでいれば、「～」に変換する。
 *
 * in/out p ファイル名。直接書き変えられる
 * return 書き変え後のファイル名の末尾
 */
static char *to_tilda_name(char *p,int &size)
{
  const char *home=getenv("HOME");
  if( home == NULL || *home == '\0' )
    return NULL;
  
  /* 比較する */
  char *sp=p;
  while( *home != '\0' ){
    int x=tolower(*home & 255); if( x == '\\' ) x='/';
    int y=tolower(*sp   & 255); if( y == '\\' ) y='/';
    if( x != y )
      return NULL;
    ++home ; ++sp;
  }
  
  *p++ = '~'; --size;
  while( *sp != '\0'  &&  --size > 1 )
    *p++ = *sp++;
  *p = '\0';
  return p;
}

/* 真のファイル名を得る(ファイル名のみ、ディレクトリは含まず)
 *   return 書き込み後の dp
 * ファイル名が得られなかった時は NULL を返す。
 */
static char *paste_true_name(char *dp,const char *cwd)
{
  Dir dir;
  if( dir._findfirst( cwd ) != 0 )
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
      if( filesystem != 2  &&  (longname=get_ea_longname(cwd)) != NULL ){
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

/* プロンプトを作成する。
 *     promptenv プロンプトの元文字列
 *     dp        プロンプトの変換後文字列の入れるバッファ
 *     size      バッファサイズ
 * return
 *     false: 画面最上段を使用しなかった。
 *     true:  画面最上段を使用した。
 */
bool set_prompt(const char *promptenv , char *dp , int size)
{
  bool used_topline=false;

  const char *sp;
  time_t now;
  time( &now );
  struct tm *thetime = localtime( &now );

  while( *promptenv != '\0'  &&  size >= 3 ){
    if( *promptenv == '$' ){
      int n;
      switch( promptenv++ , to_upper(*promptenv) ){
	
      case '!': /* ヒストリ番号 */
	n = snprintf(dp,size,"%d",nhistories+1 );
	dp += n;
	break;

      case '@': /* ボリュームラベル */
	sp = _getvol(0);
	if( sp != NULL ){
	  while( *sp != '\0' &&  --size > 0 )
	    *dp++ = *sp++;
	}
	break;
	
      case '$': *dp++ = '$';  --size;  break;
      case '_': *dp++ = '\n'; --size;  break;
      case 'A': *dp++ = '&';  --size;  break;
      case 'B': *dp++ = '|';  --size;  break;
      case 'C': *dp++ = '(';  --size;  break;
	
      case 'D':/* 現在の日付 */
	n = sprintf(dp,"%4d-%02d-%02d" ,
		    thetime->tm_year+1900 ,
		    thetime->tm_mon+1 ,
		    thetime->tm_mday );
	dp += n;
	size -= n;
	break;
	
      case 'E': *dp++ = '\x1b'; --size; break;
      case 'F': *dp++ = ')';	--size; break;
      case 'G': *dp++ = '>';	--size; break;
      case 'H': *dp++ = '\b';	--size; break;
	
      case 'I':
	{
	  int a=0x0F;
	  if( option_vio_cursor_control )
	    a = v_getattr();
	
	  n = snprintf(dp,size
		       ,"\x1B[s\x1B[1;44;37m\x1B[H%-*s\x1B[m\x1B[u"
		       , screen_width ,
		       " Nihongo Yet Another Os/2 Shell "VERSION
		       " (c) 1996-99 HAYAMA,Kaoru "
		       );
	  dp += n;
	  size -= n;
	  used_topline = true;
	  
	  if( option_vio_cursor_control )
	    v_attrib(a);
	}
	break;

      case '{':
	{
	  int a=0x0F;
	  int curdrv=_getdrive();
	  if( option_vio_cursor_control )
	    a = v_getattr();
	  
	  dp += sprintf(dp,"\x1b[s\x1B[H" );
	  for(int length=0; *++promptenv != '}' && *promptenv != '\0'
	      && length < screen_width-1 ;){
	    if( isalpha(*promptenv) ){
	      int drv=toupper(*promptenv);
	      n = sprintf(dp,"\x1B[1;%s;37m%c:"
			  ,(drv==curdrv ? "41" : "44")
			  ,drv);
	      dp += n;
	      size -= n;
	      length += 3;
	      
	      DosError( FERR_DISABLEHARDERR );    
	      _getcwd1(dp,drv);
	      DosError( FERR_ENABLEHARDERR );
	      int len=strlen(dp);
	      if( length + len < screen_width-1 ){
		length += len;
		dp += len;
	      }else{
		for(int i=length ; i<screen_width-4 ; i++ ){
		  if( is_kanji(*dp) ){
		    ++dp;
		    ++i;
		  }
		  ++dp;
		}
		if( length < 74 ){
		  if( size <= 3 )
		    goto promptend;
		  *dp++ = '.';
		  *dp++ = '.';
		  *dp++ = '.';
		  size -= 3;
		}
		dp += sprintf(dp,"\x1b[0m ");
		while( *promptenv != '}' && *promptenv != '\0' )
		  ++promptenv;
		goto driveloop;
	      }
	      if( size > 0 ){
		n = snprintf(dp,size,"\x1b[0m ");
		dp += n; size -= n;
	      }
	    }
	  }
	driveloop:
	  n = snprintf(dp,size,"\x1b[K\x1b[u");
	  dp += n;
	  size -= n;
	  used_topline = true;
	  if( option_vio_cursor_control )
	    v_attrib(a);
	  
	  if( *promptenv == '\0' )
	    goto promptend;
	}
	break;
	
      case 'L': *dp++ = '<';	--size;  break;
	
      case 'N':/* カレントドライブ */
	*dp++ = _getdrive();
	--size;
	break;
	
      case 'P':/* カレントディレクトリ */
	/* !!!! サイズチェック !!!!! */
	dp = getcwd_case(dp);
	break;
	
      case 'Q': *dp++ = '='; --size;  break;

      case 'R':
	n = snprintf(dp,size,"%d",execute_result);
	dp += n;
	size -= n;
	break;

      case 'S': *dp++ = ' ';	  break;
	
      case 'T':/* 現在の時刻 */
	n = sprintf(dp,"%02d:%02d:%02d",
		    thetime->tm_hour ,
		    thetime->tm_min ,
		    thetime->tm_sec );
	dp += n;
	size -= n;
	break;
      case 'V':/* OS/2のバージョン */
	if( _osmode == OS2_MODE )
	  n = snprintf(dp,size,"The Operating System/2 Version is %d.%d"
		       , _osmajor/10 , _osminor );
	else
	  n = snprintf(dp,size,"PC DOS Version is %d.%d"
		       , _osmajor , _osminor );
	dp += n;
	size -= n;
	break;

      case 'W':/* カレントディレクトリ:ホームディレクトリを「~」に変換する */
	{
	  char *tail=getcwd_case(dp);
	  if( (dp=to_tilda_name(dp,size))==NULL )
	    dp = tail;
	}
	break;

      case 'Z':
	switch( ++promptenv , to_upper(*promptenv) ){
	case 'A':
	  *dp++ = '\a';	  --size;  break;

	case 'H': /* ヒストリ番号 */
	  n = snprintf(dp,size,"%d",nhistories+1);
	  dp += n; size -= n;
	  break;

	case 'V': /* ボリュームラベル */
	  sp = _getvol(0);
	  if( sp != NULL ){
	    while( *sp != '\0' &&  --size > 1 )
	      *dp++ = *sp++;
	  }
	  break;
	case 'P': /* LONGNAME */
	  /* !!!!! 容量チェック !!!!! */
	  dp = get_cwd_long_name(dp);
	  break;
	case '\0':
	  goto promptend;
	}
	break;
      }
      promptenv++;
    }else{
      if( is_kanji(*promptenv) ){
	*dp++ = *promptenv++;
	--size;
      }
      *dp++ = *promptenv++;
      --size;
    }
  }
 promptend:    
  *dp = '\0';
  return used_topline;
}
