#include <stdlib.h>
#include <string.h>
#include "strbuffer.h"

char *StrBuffer::finish() throw()
{
  if( isNull() )
    return 0;

  char *rc=(char*)realloc(buffer,length+1);
  if( rc == 0 )
    rc = buffer;
  
  buffer = 0;
  length = 0;
  max = 0;
  return rc;
}

void StrBuffer::grow(int newSize) throw(MallocError)
{
  if( isNull() ){
    /* 新規取得 */
    buffer = (char*)malloc( newSize+1 );
    if( buffer == 0 )
      throw MallocError();
    max = newSize;
  }else{
    /* ２回目移行、つまり増加！*/
    char *newBuffer=(char*)realloc( buffer , newSize+1 );
    if( newBuffer == 0 )
      throw MallocError();
    max = newSize;
    buffer = newBuffer;
  }
}

StrBuffer &StrBuffer::operator << (const char *s) throw(MallocError)
{
  /* 引数が NULL の時は何もしない。呼び出し元の NULL チェックを省略する為 */
  if( s == NULL )
    return *this;

  int len=strlen(s);
  if( length+len >= max )
    grow( length+len+inc );
  strcpy( buffer+length , s );
  length += len;
  return *this;
}

StrBuffer &StrBuffer::paste(const void *s , int size) throw(MallocError)
{
  /* 引数が NULL の時は何もしない。呼び出し元の NULL チェックを省略する為 */
  if( s == NULL || size <= 0 )
    return *this;

  if( length+size >= max )
    grow( length+size+inc );
  memcpy( buffer+length , s , size );
  length += size;
  buffer[ length ] = '\0';
  return *this;
}


StrBuffer::~StrBuffer()
{
  if( buffer != 0 )
    free(buffer);
}

#if 0
#include <stdio.h>

int main(void)
{
  try{
    StrBuffer buf;

    for(int i=0; i<200;i++ ){
      buf << "ahaha ";
      buf << '@' << ' ';
    }
    char *s=buf.finish();
    printf("%s",s);
    free(s);
  }catch( void *e ){
    if( e == NULL ){
      fputs("Heap Error\n",stderr);
    }else{
      fputs("Unknown Error\n",stderr);
    }
  }
}
#endif
