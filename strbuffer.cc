#include <assert.h>
#include <stdlib.h>
// #include <string.h>
#include "strbuffer.h"

char StrBuffer::zero[1]={ '\0' };

void StrBuffer::drop() throw()
{
  free( buffer );
  buffer = zero;
  length = max = 0;
}

char *StrBuffer::finish() throw()
{
  if( isZero() )
    return NULL;

  char *rc=(char*)realloc(buffer,length+1);
  if( rc == NULL )
    rc = buffer;
  
  buffer = zero;
  length = 0;
  max = 0;
  return rc;
}

char *StrBuffer::finish2()
     throw(StrBuffer::MallocError)
{
  if( isZero() ){
    char *rc=(char*)malloc(1);
    if( rc==NULL )
      throw MallocError();
    rc[0] = '\0';
    return rc;
  }
  return this->finish();
}

void StrBuffer::grow(int newSize)
     throw(StrBuffer::MallocError)
{
  char *newBuffer = (char*)(  isZero() 
			    ? malloc( newSize+1 ) 
			    : realloc( buffer , newSize+1 ) );
  assert( newBuffer != NULL );
  if( newBuffer == NULL )
    throw MallocError();

  max = newSize;
  buffer = newBuffer;
}

StrBuffer &StrBuffer::operator << (const char *s)
     throw(StrBuffer::MallocError)
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

/* 数字を右詰めで出力する 
 *	num	 出力すべき数値
 *	width	 表示桁数
 *	fillchar 表示桁数に満たなかった時に、埋めるべき文字。
 *	sign	 数値の直前に置くべき文字。'\0' か '-'
 */
StrBuffer &StrBuffer::putNumber(int num,int width,char fillchar,char sign)
{
  if( num < 0 ){ /* マイナス */
    return putNumber( -num , width , fillchar , '-' );
  }else if( num >= 10 ){
    putNumber( num / 10 , width-1 , fillchar , sign );
    return *this << "0123456789"[ num % 10 ];
  }else{
    if( sign != '\0' )
      width -= 2;
    else
      --width;
    
    while( width-- > 0 )
      *this << (char)fillchar;

    if( sign != '\0' )
      *this << sign;

    return *this << "0123456789"[ num ];
  }
}

StrBuffer &StrBuffer::paste(const void *s , int size)
     throw(StrBuffer::MallocError)
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
  if( ! isZero() )
    free(buffer);
}

StrBuffer &StrBuffer::operator << (int n)
     throw(StrBuffer::MallocError)
{
  if( n < 0 ){
    return *this << '-' << -n;
  }else if( n < 10 ){
    return *this << "0123456789"[ n ];
  }else{
    return *this << (n / 10) << "0123456789"[ n % 10 ];
  }
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
