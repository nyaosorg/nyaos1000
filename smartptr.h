#ifndef SMARTPTR_H
#define SMARTPTR_H

/* SmartPtr : (char *)の互換クラス。
 *	char buffer[1000];
 *	SmartPtr dp(buffer,sizeof(buffer));
 * と宣言すると、いくら ++dp しても、
 * dp は buffer+10000 を絶対越えない。
 * 越えた場合、例外 SmartPtr::BorderOut が発生する。
 *
 * (本体の smartptr.cc は無い。インライン関数のみ)
 */

class SmartPtr{
  char *ptr;
  char *border; /* ptr の上限 , *border には「\0」がおけるのみ。*/
public:
  class BorderOut{}; /* ptr が border を越えた時に発生する例外 */
  
  SmartPtr(char *p,int max) : ptr(p) , border(p+max-1) { }

  SmartPtr &operator++() throw(BorderOut) {
    if( ++ptr > border ){
      throw BorderOut();
    }
    return *this;
  }
  char *operator++(int) throw(BorderOut) {
    if( ptr+1 > border ) throw BorderOut();
    return ptr++;
  }
  SmartPtr &operator << (char c){
    *ptr++ = c;
    if( ptr+1 > border ) throw BorderOut();
    return *this;
  }
  SmartPtr &operator << (const char *s){
    while( *s != '\0' )
      *this << *s++;
    return *this;
  }

  char &operator*() throw()
    { return *ptr; }
  operator const char*() const throw()
    { return ptr; }
  SmartPtr &operator += (int n) throw(BorderOut) {
    if( (ptr+=n) > border ) throw BorderOut();
    return *this;
  }
  SmartPtr operator + (int n) const
    { SmartPtr tmp(*this); return tmp += n ; }
  char *rawptr() throw()
    { return ptr; }
  int operator !() const throw()
    { return ptr > border; }
  int ok() const throw()
    { return ptr <= border; }
  int ng() const throw()
    { return ptr >= border; }
  void set(char *p,int max)
    { ptr=p ; border = p+max-1; }
  char &operator[](int n) throw(BorderOut){
    if( ptr+n > border ) throw BorderOut();
    return ptr[n];
  }
  void terminate() { *(ptr=border) = '\0'; }
};


#endif
