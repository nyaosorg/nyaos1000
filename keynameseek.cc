#include <ctype.h>
#include <stdlib.h>
#include "macros.h"
#include "keyname.h"
// #include "bintable.h"

static KeyName keytable[]={
#  include "keynames.cc"
};

// static BinTable <KeyName> keyTable(keytable,numof(keytable));

int KeyName::compareWithTop(const void *key,const void *el)
{
  /* この比較関数は、
   *	・構造体が POD構造体で
   *	・最初のメンバがキー文字列へのポインタ
   * でなくてはいけない。
   */
  const unsigned char *s1= static_cast <const unsigned char *>(key);
  const unsigned char *s2=*static_cast <const unsigned char **>(el);
  
  for(;;){
    int c1=tolower(*s1) , c2=tolower(*s2);
    if( c1 != c2 )
      return c1-c2;
    if( c1 == '\0' )
      return 0;
    ++s1,++s2;
  }
}

KeyName *KeyName::find( int n )
{
  for(unsigned i=0 ; i<numof(keytable) ; i++){
    if( keytable[i].code == n )
      return &keytable[i];
  }
  return 0;
}

KeyName *KeyName::find( const char *name )
{
#if 1
  return static_cast <KeyName *> (bsearch(  name
					  , keytable 
					  , numof(keytable)
					  , sizeof(keytable[0])
					  , KeyName::compareWithTop ) );
#else
  return keyTable.find(name);
#endif
}
