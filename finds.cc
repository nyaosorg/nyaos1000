#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <stdio.h> /* for sprintf */

#define INCL_DOSNLS
#include "macros.h"
#include "finds.h"

#define DEBUG(x) x

int dbcs_fnmatch (const char *mask, const char *name, int flags);
#define _fnmatch dbcs_fnmatch

int convroot(char *&dp,int &size,const char *sp) throw(size_t)
{
  assert( sp != NULL );
  assert( dp != NULL );

  int lastchar=0;
  while( *sp != '\0' ){
    lastchar = *sp;
    if( *sp == '/' ){
      lastchar = *dp++ = '\\';
      ++sp;
    }else{
      if( is_kanji(*sp) ){
	*dp++ = *sp++;
	if( --size <= 0 )
	  throw (size_t)-2;
      }
      *dp++ = *sp++;
    }
    if( --size <= 0 )
      throw (size_t)-1;
  }
  *dp = '\0';
  return lastchar;
}


int get_current_cp()
{
  ULONG CpList[8],CpSize;
  if( DosQueryCp(sizeof(CpList),CpList,&CpSize) == 0 ){
    return CpList[0];
  }else{
    return 0;
  }
}

Dir::Dir(): handle(0xFFFFFFFF), count(1)
{
  /* 英語モードで日本語のファイル名を取得すると、
   * コアしてしまうので、無理やり日本語モードにしている。
   */
  codepage = get_current_cp();
  DosSetProcessCp( 932 );
}

Dir::Dir(const char *path,int attr=ALL) : handle(0xFFFFFFFF),count(1)
{
  /* 英語モードで日本語のファイル名を取得すると、
   * コアしてしまうので、無理やり日本語モードにしている。
   */
  codepage = get_current_cp();
  DosSetProcessCp( 932 );
  this->findfirst(path,attr); 
}

Dir::~Dir()
{ 
  DosFindClose(handle);
  
  /* 変更したコードページを元に戻す */
  DosSetProcessCp( codepage );
}

// 引数はディレクトリ名のみ。純粋に opendir に対応する。
// ディレクトリ名は末尾に \ や / がついていてもよい。
int Dir::findfirst(const char *fname,int attr)
{
  int size=strlen(fname)+3;
  char *path=static_cast<char*>(alloca(size));
  char *p=path;
  try{
    int lastchar = convroot(p,size,fname);
    if( lastchar != '\\'  &&  lastchar != ':' )
      *p++ = '\\';
    *p++ = '*';
  }catch(...){
    ;
  }
  *p = '\0';
  
  return dosfindfirst(path,attr);
}

/* ワイルドカードファイル名を受け入れる findfirst。
 * パス文字を変換するのみ
 */
int Dir::findfirst_with_wildcard(const char *fname,int attr)
{
  int size=strlen(fname)+1;
  char *path=static_cast<char*>(alloca(size));
  char *p=path;
  try{
    (void)convroot(p,size,fname);
    *p = '\0';
  }catch( ... ){
    return -1;
  }

  assert( size > 0 );
  return dosfindfirst(path,attr);
}

void fnexplode2_free(char **buffer)
{
  if( buffer != NULL ){
    for(char **p=buffer ; *p != NULL ; p++ ){
      free(*p);
      *p = NULL;
    }
    free(buffer);
  }
}

static int strcmp2( const void *x , const void *y )
{
  return *(char**)x-*(char**)y ?: strcmp( *(char**)x , *(char**)y );
}


/* emx 関数の _fnexplode の独自版
 */
char **fnexplode2(const char *path)
{
  assert( path != NULL );
  
  const char *lastroot=NULL;
  int finalchar = 0;
  bool have_wildcard = false;
  
  /* パスのディレクトリ名部分とファイル名部分の分離
   * ワイルドカードを使っているか否かの調査
   */
  for(const char *p=path; *p != '\0' ; p++ ){
    finalchar = *p;
    if( *p=='\\' || *p=='/' || *p==':' ){
      lastroot = p;
    }else if( *p=='?' || *p=='*' ){
      have_wildcard = true;
    }
    if( is_kanji(*p) ){
      ++p;
      assert(*p != '\0');
    }
  }
  
  /* ワイルドカードを使っていない場合は終了 */
  if(   have_wildcard==false || finalchar=='\\'
     || finalchar == '/' || finalchar == ':')
    return NULL;
  
  bool dotprint=false;

  int lendir = 0;
  char *dirname = "";
  if( lastroot != NULL ){
    assert( lastroot >= path );
    lendir=lastroot-path+1;

    // alloca には複雑な引数を渡してはいけない。
    int memsiz=lendir+1;
    dirname = (char*)alloca(memsiz);
    
    memcpy( dirname , path , lendir );
    dirname[lendir] = '\0';
    if( lastroot[1]=='.' )
      dotprint = true;
  }else{
    if( path[0] == '.' )
      dotprint = true;
  }
  Dir dir;

  dir.dosfindfirst(path,Dir::ALL);
  if( dir == NULL )
    return NULL;

  int nfiles=0;
  char **result = (char**)malloc(sizeof(char**));
  if( result == NULL )
    return NULL;
  
  do{
    /* 「.」で始まるファイルは基本的に表示しない。
     *	- ファイル名自体の指定で「.」で始まる場合ば別
     *	- 「.」自体には展開しない
     */
    if(  dir.get_name()[0] == '.'
       && ( dotprint == false || dir.get_name()[1]=='\0' ) )
      continue;

    /* OS/2 のワイルドカード展開では、「hoge.＊」で「hoge」もマッチしてしまう。
     * そこで、emx の関数で、そういうケースを除いてやる。
     */
    if(  lastroot != NULL  
       ? _fnmatch(lastroot+1,dir.get_name(),_FNM_POSIX | _FNM_IGNORECASE)!=0 
       : _fnmatch(path,dir.get_name()      ,_FNM_POSIX | _FNM_IGNORECASE)!=0)
      continue;
	
    result[ nfiles ] = 
      (char*)malloc( lendir + dir.get_name_length()+1 );
    sprintf( result[ nfiles ] , "%s%s" , dirname , dir.get_name() );
    result = (char**)realloc( result , (++nfiles+1) * sizeof(char*) );
  }while( ++dir != NULL );
  result[ nfiles ] = NULL;

  if( nfiles <= 0 ){
    free(result);
    return NULL;
  }

  qsort(result,nfiles,sizeof(char*),strcmp2);

  return result;
}

void numeric_sort(char **array)
{
  if( *array == NULL )
    return;

  /* 最も添字の小さいものから確定させてゆく、選択ソート法 */
  while( *(array+1) != NULL ){
    if( isdigit(**array & 255) ){
      /* 数値ソートをする場合がある時 */
      for( char **cur=array+1 ; *cur != NULL ; ++cur ){
	if(  isdigit(**cur & 255)
	   ? strnumcmp(*array,*cur) >= 0
	   : (**array >= **cur && strcmp(*array,*cur) >= 0 ) ){
	  char *tmp = *array;
	  *array = *cur;
	  *cur   = tmp;
	}
      }
    }else{
      /* 数値ソートはありえない場合 */
      for( char **cur=array+1 ; *cur != NULL ; ++cur ){
	if( **array >= **cur  &&  strcmp(*array,*cur) >= 0 ){
	  char *tmp = *array;
	  *array = *cur;
	  *cur   = tmp;
	}
      }
    }
    ++array;
  }
}

int strnumcmp(const char *s1,const char *s2)
{
  const unsigned char *p1=(const unsigned char *)s1;
  const unsigned char *p2=(const unsigned char *)s2;

  for(;;){
    if( isdigit(*p1) && isdigit(*p2) ){
      int n1=0,len1=0;
      do{
	n1 = n1*10+(*p1-'0');
	len1++;
      }while( isdigit(*++p1) );

      int n2=0,len2=0;
      do{
	n2 = n2*10+(*p2-'0');
	len2++;
      }while( isdigit(*++p2) );

      if( n1 != n2 ){
	return n1-n2;
      }else if( len1 != len2 ){
	/* 文字数が長い方が先頭の 0 の数が多いと考えられるので、
	 * 逆順扱いする。*/
	return len1-len2;
      }else if( *p1 == '\0' || *p2 == '\0' )
	return 0;
    }else{
      if( *p1 != *p2 )
	return *p1 - *p2;
      if( *p1 == '\0' )
	return 0;
    }
    ++p1;++p2;
  }
}
