#include <stddef.h>
#include "macros.h"
#include "strtok.h"

/* p[0] (漢字の場合はp[0],p[1])にある文字が、dem の中に含まれている
 * ならば 1 、さもなければ 0 を返す
 */
static int has_a_char(const char *p,const char *dem)
{
  while( *dem != '\0' ){
    if( *p == *dem  &&  ( ! is_kanji(*p) || p[1] == dem[1] ) )
      return 1;
    
    if( is_kanji(*dem) )
      ++dem;
    ++dem;
  }
  return 0;
}

/* strtok といっしょ。ただし、strtokの第一引数にあたる文字列は
 * インスタンスが記憶している。
 *  in	dem デミリタ
 */
char *Strtok::cut_with(const char *dem)
{
  if( p==NULL )
    return NULL;
  
  // 先頭のデミリタ文字列を読みとばす。
  for(;;){
    if( *p=='\0' )
      return p=NULL;

    if( ! has_a_char(p,dem) )
      break;
    
    ++p;
  }
  // 次のデミリタ文字を探しだす。
  char *top=p;
  for(;;){
    if( *p == '\0' ){
      p = NULL;
      return top;
    }
    if( has_a_char(p,dem) ){
      *p++ = '\0';
      return top;
    }
    if( is_kanji(*p) )
      ++p;
    ++p;
  }
}
