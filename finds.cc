#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#define INCL_DOSNLS
#include "macros.h"
#include "finds.h"

/* #define is_kanji(x) 0 */
#define DEBUG(x) x

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
  codepage = get_current_cp();
  DosSetProcessCp( 932 );
}

Dir::Dir(const char *path,int attr=ALL) : handle(0xFFFFFFFF),count(1)
{
  codepage = get_current_cp();
  
  DosSetProcessCp( 932 );
  this->findfirst(path,attr); 
}

Dir::~Dir()
{ 
  DosFindClose(handle);
  DosSetProcessCp( codepage );
}

// strcpy_tail 
// : 帰り値がコピーした文字列の末尾である以外は、strcpy と同じ

char *strcpy_tail(char *dp,const char *sp)
{
  while( *sp != '\0' )
    *dp++ = *sp++;
  *dp = '\0';
  return dp;
}

// 引数はディレクトリ名のみ。純粋に opendir に対応する。
// ディレクトリ名は末尾に \ や / がついていてもよい。
int Dir::findfirst(const char *fname,int attr)
{
  char *path=(char*)alloca(strlen(fname));
  char *p=path;
  
  int lastchar = 0;
  while( *fname != '\0' ){
    if( *fname == '/' ){
      fname++;
      lastchar = *p++ = '\\';
      continue;
    }
    if( is_kanji(lastchar=*fname) )
      *p++ = *fname++;
    *p++ = *fname++;
  }
  if( lastchar != '\\'  &&  lastchar != ':' )
    *p++ = '\\';
  *p++ = '*';
  *p   = '\0';
  
  return _findfirst(path,attr);
}

/* ワイルドカードファイル名を受け入れる findfirst。
 * パス文字を変換するのみ
 */
int Dir::findfirst_with_wildcard(const char *fname,int attr)
{
  char *path=(char*)alloca(strlen(fname));
  char *p=path;
  
  int lastchar = 0;
  while( *fname != '\0' ){
    if( *fname == '/' ){
      fname++;
      *p++ = '\\';
      continue;
    }
    if( is_kanji(lastchar=*fname) )
      *p++ = *fname++;
    *p++ = *fname++;
  }
  *p = '\0';
  return _findfirst(path,attr);
}

void fnexplode2_free(char **buffer)
{
  if( buffer != NULL ){
    for(char **p=buffer ; *p != NULL ; p++ )
      free(*p);
    free(buffer);
  }
}

char **fnexplode2(const char *path)
{
  const char *lastroot=NULL;
  int finalchar = 0;
  int have_wildcard = 0;
  
  for(const char *p=path; *p != '\0' ; p++ ){
    finalchar = *p;
    if( *p=='\\' || *p=='/' || *p==':' ){
      lastroot = p;
    }else if( *p=='?' || *p=='*' ){
      have_wildcard = 1;
    }
    if( is_kanji(*p) ){
      ++p;
      assert(*p != '\0');
    }
  }
  
  if(   have_wildcard==0 || finalchar=='\\'
     || finalchar == '/' || finalchar == ':')
    return NULL;
  
  int dotprint=0;

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
      dotprint = 1;
  }else{
    if( path[0] == '.' )
      dotprint = 1;
  }
  Dir dir;

  dir._findfirst(path,Dir::ALL);
  if( dir == NULL )
    return NULL;

  int nfiles=0;
  char **result = (char**)malloc(sizeof(char**));
  if( result == NULL )
    return NULL;
  
  do{
    //  o「.」で始まるファイルは基本的に表示しない。
    //    - ファイル名自体の指定で「.」で始まる場合ば別
    //    - 「.」自体には展開しない
    if(  dir.get_name()[0] == '.'
       && ( dotprint == 0 || dir.get_name()[1]=='\0' ) )
      continue;
	
    result[ nfiles ] = 
      (char*)malloc( lendir + dir.get_name_length()+1 );
    strcpy_tail( strcpy_tail( result[ nfiles ] , dirname )
		, dir.get_name() );
    result = (char**)realloc( result , (++nfiles+1) * sizeof(char*) );
  }while( ++dir != NULL );
  result[ nfiles ] = NULL;

  if( nfiles <= 0 ){
    free(result);
    return NULL;
  }
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
