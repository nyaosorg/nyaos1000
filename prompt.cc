#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/ea.h>
#include <sys/video.h>

#include "edlin.h"
#include "nyaos.h"
#include "finds.h"

extern int nhistories;

/* パスが、ホームディレクトリ名を含んでいれば、「～」に変換する。*/
static char *to_tilda_name(char *p)
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
  
  *p++ = '~';
  while( *sp != '\0' )
    *p++ = *sp++;
  *p = '\0';
  return p;
}


/* ---- 大文字・小文字を区別した正確なファイル名を得る。
   ---- src は見事に破壊される。 ---- */
char *get_true_name(char *src,char *dst)
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
  char *token=strtok(src,"\\/");
  if( token != NULL ){
    for(;;){
      char *p=dp;
      while( *token != '\0' )
	*p++ = *token++;
      *p = '\0';

      Dir dir;
      if( dir._findfirst(dst) == 0 ){
	const char *q=dir.get_name();
	while( *q != '\0' )
	  *dp++ = *q++;
	*dp = '\0';
      }else{
	dp = p;
      }
      if( (token=strtok(NULL,"\\/")) == NULL )
	break;
      *dp++ = '\\';
    }
  }
  *dp = '\0';
  return dp;
}
void truepath( char *dst , const char *src , int size )
{
  char *tmp=(char*)alloca(size);
  _fullpath( tmp , src , size );
  get_true_name( tmp , dst );
}

/* ---- 現在のカレントディレクトリを大文字・小文字も正確に得る ---- */
char *getcwd_case(char *dst)
{
  char cwd[ FILENAME_MAX ];

  cwd[0] = _getdrive();
  cwd[1] = ':';

  if( _getcwd( cwd+2 , sizeof(cwd)-2 ) == NULL ){
    *dst++ = cwd[0];
    *dst++ = cwd[1];
    return dst;
  }
  
  get_true_name(cwd,dst);
  
  /* 最後にルートを「/」に戻す */
  while( *dst != '\0' ){
    if( *dst == '\\' )
      *dst = '/';
    else if( is_kanji(*dst) )
      ++dst;
    ++dst;
  }
  return dst;
}

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

static char *paste_true_name(char *dp,const char *cwd)
{
  Dir dir;
  dir._findfirst( cwd );
  for(const char *ssp=dir.get_name(); *ssp != '\0' ; ssp++ )
    *dp++ = *ssp;
  return dp;
}

char *get_cwd_long_name(char *dp)
{
  char cwd[ FILENAME_MAX ];
  *dp++ = cwd[0] = _getdrive();
  *dp++ = cwd[1] = ':';
  if( _getcwd1( cwd+2 , toupper(cwd[0]) ) != 0 )
    return dp;

  for(char *p=cwd+2;*p != '\0';p++){
    if( *p=='/' )
      *p = '\\';
  }

  /* A:\
     0123 */
  
  if( (cwd[2] == '/' || cwd[2] == '\\' ) && cwd[3]=='\0' ){
    *dp++ = '/'; *dp = '\0';
    return dp;
  }
  int filesystem=query_filesystem(cwd[0]);

  for(char *sp=cwd+3; ;sp++ ){
    if( *sp == '/' || *sp == '\\' || *sp == '\0' ){
      int org=*sp;
      *sp = '\0'; /* 一時的にルートを 0 にする */
      *dp++ = '/';

      struct _ea ea;

      if(   filesystem < 0 || filesystem > 1
	 || _ea_get( &ea , cwd , 0 , ".LONGNAME" ) != 0 ){
	dp = paste_true_name( dp , cwd );
      }else{
	if( ea.size == 0 || ea.value == NULL ){
	  dp = paste_true_name( dp , cwd );
	}else{
	  union{
	    void  *value;
	    const char *byte;
	    const unsigned short *word;
	  }ptr;
	  
	  ptr.value = ea.value;
	  if( *ptr.word++ == 0xFFFD ){
	    int size = *ptr.word++;
	    for(int i=0;i<size;i++){
	      if( *ptr.byte == '\r' ){
		ptr.byte++;
	      }else if( *ptr.byte == '\n' ){
		*dp++ = ' '; ptr.byte++;
	      }else if( 0 <= *ptr.byte && *ptr.byte < ' ' ){
		*dp++ = '^';
		*dp++ = '@' + *ptr.byte++;
	      }else{
		*dp++ = *ptr.byte++;
	      }
	    }
	  }else{
	    dp = paste_true_name( dp , cwd );
	  }
	}
	_ea_free( &ea );
      }
      if( (*sp=org) == '\0' )
	break;
    }
  }
  *dp = '\0';
  return dp;
}


void setprompt(const char *promptenv,char *dp,ShellEdlin *edlin=NULL)
{
  const char *sp;
  time_t now;
  time( &now );
  struct tm *thetime = localtime( &now );
  if( edlin != NULL )
    edlin->using_i_mark=0;
  int a;
  
  while( *promptenv != '\0' ){
    if( *promptenv == '$' ){
      switch( promptenv++ , to_upper(*promptenv) ){
	
      case '!': /* ヒストリ番号 */
	dp += sprintf(dp,"%d",nhistories );
	break;
      case '@': /* ボリュームラベル */
	sp = _getvol(0);
	if( sp != NULL ){
	  while( *sp != '\0' )
	    *dp++ = *sp++;
	}
	break;

      case '$': *dp++ = '$';	  break;
      case '_': *dp++ = '\n';	  break;
      case 'A': *dp++ = '&';	  break;
      case 'B': *dp++ = '|';	  break;
      case 'C': *dp++ = '(';	  break;
	
      case 'D':/* 現在の日付 */
	dp += sprintf(dp,"%4d-%02d-%02d" ,
		      thetime->tm_year+1900 ,
		      thetime->tm_mon+1 ,
		      thetime->tm_mday );
	break;
	
      case 'E': *dp++ = '\x1b'; break;
      case 'F': *dp++ = ')';	  break;
      case 'G': *dp++ = '>';	  break;
      case 'H': *dp++ = '\b';	  break;
	
      case 'I':
	if( option_vio_cursor_control )
	  a = v_getattr();
	
	dp += sprintf(dp,"\x1B[s\x1B[1;44;37m\x1B[H%-*s\x1B[m\x1B[u"
		      , screen_width ,
		      " Nihongo Yet Another Os/2 Shell "VERSION
		      " (c) 1996-98 HAYAMA,Kaoru "
		      );
	if( edlin != NULL )
	  edlin->using_i_mark = 1;
	if( option_vio_cursor_control )
	  v_attrib(a);
	break;

      case '{':
	{
	  int curdrv=_getdrive();
	  if( option_vio_cursor_control )
	    a = v_getattr();
	  
	  dp += sprintf(dp,"\x1b[s\x1B[H" );
	  for(int length=0; *++promptenv != '}' && *promptenv != '\0'
	      && length < screen_width-1 ;){
	    if( isalpha(*promptenv) ){
	      int drv=toupper(*promptenv);
	      dp += sprintf(dp,"\x1B[1;%s;37m%c:"
			    ,(drv==curdrv ? "41" : "44")
			    ,drv);
	      length += 3;
	      
	      _getcwd1(dp,drv);
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
		  *dp++ = '.';
		  *dp++ = '.';
		  *dp++ = '.';
		}
		dp += sprintf(dp,"\x1b[0m ");
		while( *promptenv != '}' && *promptenv != '\0' )
		  ++promptenv;
		goto driveloop;
	      }
	      dp += sprintf(dp,"\x1b[0m ");
	    }
	  }
	driveloop:
	  dp += sprintf(dp,"\x1b[K\x1b[u");
	  if( edlin != NULL )
	    edlin->using_i_mark = 1;
	  if( option_vio_cursor_control )
	    v_attrib(a);
	  
	  if( *promptenv == '\0' )
	    goto promptend;
	}
	break;
	
      case 'L': *dp++ = '<';	  break;
	
      case 'N':/* カレントドライブ */
	*dp++ = _getdrive();
	break;
	
      case 'P':/* カレントディレクトリ */
	dp = getcwd_case(dp);
	break;
	
      case 'Q': *dp++ = '=';	  break;
      case 'S': *dp++ = ' ';	  break;
	
      case 'T':/* 現在の時刻 */
	dp += sprintf(dp,"%02d:%02d:%02d",
		      thetime->tm_hour ,
		      thetime->tm_min ,
		      thetime->tm_sec );
	break;
      case 'V':/* OS/2のバージョン */
	if( _osmode == OS2_MODE )
	  dp += sprintf(dp,"The Operating System/2 Version is %d.%d"
			, _osmajor/10 , _osminor );
	else
	  dp += sprintf(dp,"PC DOS Version is %d.%d"
			, _osmajor , _osminor );
	break;

      case 'W':/* カレントディレクトリ:ホームディレクトリを「~」に変換する */
	{
	  char *tail=getcwd_case(dp);
	  if( (dp=to_tilda_name(dp))==NULL )
	    dp = tail;
	}
	break;

      case 'Z':
	switch( ++promptenv , to_upper(*promptenv) ){
	case 'H': /* ヒストリ番号 */
	  dp += sprintf(dp,"%d",nhistories);
	  break;

	case 'V': /* ボリュームラベル */
	  sp = _getvol(0);
	  if( sp != NULL ){
	    while( *sp != '\0' )
	      *dp++ = *sp++;
	  }
	  break;
	case 'P': /* LONGNAME */
	  dp = get_cwd_long_name(dp);
	  break;
	case '\0':
	  goto promptend;
	}
	break;
      }
      promptenv++;
    }else{
      *dp++ = *promptenv++;
    }
  }
 promptend:    
    *dp = '\0';
}
