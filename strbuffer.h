/* This header file is for -*- c++ -*- 
 *
 * StrBuffer : 一文字ずつ成長する文字列を管理するクラス。
 *	StrBuffer buf;
 *	buf << 'a' << "hogehoge";
 * を実行すると、ヒープに確保された領域に最初のものから
 * 書き込んでゆく。途中の文字列は、
 *	(const char *)buf
 *	buf.getTop()
 * などで参照可能。ただし、これは const であり、オブジェクトbuf
 * の消失と共に失われる。だが、
 *	char *s=buf.finish();
 * を実行すると、ヒープの文字列を buf と独立に取り出すことができる。
 *
 * s は strdup 関数などによって作られた普通の ヒープ文字列と何ら
 * 変わるところはない。ただし、StrBuffer のインスタンスはヒープ
 * 文字列を完全に手放すので、(const char *)へのキャスト等で得られる
 * 文字列は長さ０に戻ってしまう(NULLではない)。s は使用後、明示的に
 * free してやる必要がある。
 *
 * 例：
 *	try{
 *	    StrBuffer buf;
 *	    for(int i=0 ; i<argc ; i++ ){
 *		buf << argv[i] << ' ';
 *		printf("buf==[%s]\n",(const char *)buf);
 *	    }
 *	    char *s=buf.finish();
 *	    printf("buf==[%s]\nbuf.finish()==[%s]\n",(const char *)buf,s);
 *	    free(s);
 *	}catch( MallocError ){
 *	    fputs("Memory allocation error\n",stderr);
 *	}
 *
 * 上の例では、最後の printf で 「buf==[]」と表示される。
 *
 * StrBuffer は malloc/realloc で文字列を管理しているので、常に
 * ヒープ確保失敗を起こす恐れがある。これは、例外 MallocError
 * を catch することで対応できる。MallocError はメンバーを持たない
 * クラス。
 *
 * （注意点）
 * StrBuffer は０文字の段階では、(const char *)キャスト、finishメソッド
 * ともに、NULL を返してしまう。isNullメソッドで一々チェックしなくては
 * いけない。
 */

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
  
  void grow(int x) throw(MallocError);
public:
  bool isNull() const { return length==0; }
  StrBuffer &operator << ( const char *s ) throw(MallocError);
  StrBuffer &operator << ( char c ) throw(MallocError){
    if( length+1 >= max )
      grow(length+1+inc);
    buffer[ length++ ] = c;
    buffer[ length ] = 0;
    return *this;
  }
  StrBuffer &operator << ( int n ) throw(MallocError);
  
  /* メモリ領域(先頭アドレス＋バイト数)を追加する。*/
  StrBuffer &paste( const void *s , int size ) throw(MallocError);

  /* 文字列をヒープ文字列として取り出す。
   * 代わりにインスタンスは空になる。*/
  char *finish() throw();

  /* n文字目以降を切り捨てる。
   */
  void back(int n) throw() {
    buffer[ length=n ] = '\0';
  }

  char &operator[](int x){ return buffer[x]; }
  int getLength() const { return length; }

  const char *getTop() const { return buffer; }
  operator const char *() const { return buffer; }

  StrBuffer() : length(0),buffer(0),max(0),inc(80){ }
  StrBuffer(int x) : length(0),buffer(0),max(0),inc(x){ }
  ~StrBuffer();
};
#endif
