/* -*- c++ -*- */

#ifndef STRBUFFER_H
#define STRBUFFER_H

#ifndef MALLOC_ERROR
#define MALLOC_ERROR
class MallocError {};
#endif

class StrBuffer {
  int length;
  char *buffer;
  int max;	/* この max は length の max なので、サイズは +1 必要 */
  int inc;
  
  int isNull() const { return length==0; }
  void grow(int x) throw(MallocError);
public:
  StrBuffer &operator << ( const char *s ) throw(MallocError);
  StrBuffer &operator << ( char c ) throw(MallocError){
    if( length+1 >= max )
      grow(length+1+inc);
    buffer[ length++ ] = c;
    buffer[ length ] = 0;
    return *this;
  }
  
  /* メモリ領域(先頭アドレス＋バイト数)を追加する。*/
  StrBuffer &paste( const void *s , int size ) throw(MallocError);

  /* 文字列をヒープ文字列として取り出す。
   * 代わりにインスタンスは空になる。*/
  char *finish() throw();

  char &operator[](int x){ return buffer[x]; }
  int getLength() const { return length; }

  const char *getTop() const { return buffer; }
  operator const char *() const { return buffer; }

  StrBuffer() : length(0),buffer(0),max(0),inc(80){ }
  StrBuffer(int x) : length(0),buffer(0),max(0),inc(x){ }
  ~StrBuffer();
};
#endif
