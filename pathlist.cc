#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "macros.h"
#include "pathlist.h"

const char *getShellEnv(const char *);

PathList::~PathList()
{
  while( first != NULL ){
    OnePath *trash=first;
    sum_of_length -= first->len+1;
    first=first->next;
    free(trash);
  }
  assert( sum_of_length == 0 );
}

/* @return 0:追加
 *	   1:重複しているので追加しなかった。
 *	  -1:メモリを確保できなかった。
 */
int PathList::appendOne(const char *top,int len)
{
  OnePath *onepath=(OnePath *)malloc(sizeof(OnePath)+len);
  if( onepath == NULL )
    return -1;
  memcpy( onepath->name , top , len );
  onepath->name[ len ] = '\0';
  onepath->next = NULL;
  onepath->len  = len;

  if( first == NULL ){
    first = onepath;
    sum_of_length = len+1;
    return 0;
  }

  if( stricmp( first->name , onepath->name ) == 0 )
    return 1;

  OnePath *pre=first , *cur=first->next;
  while( cur != NULL ){
    if( stricmp( cur->name , onepath->name )==0 )
      return 1;
    pre = cur;
    cur = cur->next;
  }
  pre->next = onepath;
  sum_of_length += len+1;
  return 0;
}

void PathList::append(const char *paths,int dem)
{
  const char *top=paths , *sp=top;
  for(;;){
    if( *sp == '\0' ){
      if( top != sp )
	appendOne(top,sp-top);
      return;
    }else if( *sp == dem ){
      if( top != sp )
	appendOne(top,sp-top);
      top = ++sp;
      continue;
    }
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
}

void PathList::listing(char *dp,int dem)
{
  OnePath *cur=first;
  const char *home=getShellEnv("HOME");
  for(;;){
    const char *sp=cur->name;
    if( *sp == '~'  &&  home != NULL ){
      for( const char *p=home; *p != '\0'; p++ )
	*dp++ = *p;

      if( *++sp != '/'  &&  *sp != '\\' ){
	*dp++ = '.';
	*dp++ = '.';
	*dp++ = '\\';
      }
    }

    while( *sp != '\0' )
      *dp++ = *sp++;
    
    cur = cur->next;
    if( cur != NULL ){
      *dp++ = dem;
    }else{
      *dp++ = '\0';
      return;
    }
  }
}

#if 0
// #include <stdio.h>

int main(int argc,char **argv)
{
  dbcs_table_init();

  PathList pathlist;
  for(int i=1;i<argc;i++){
    pathlist.append(argv[i]);
  }
  char *buffer=(char*)alloca(pathlist.getSumOfLength());
  pathlist.listing(buffer);
  puts(buffer);
  return 0;
}
#endif
