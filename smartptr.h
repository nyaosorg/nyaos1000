#ifndef SMARTPTR_H
#define SMARTPTR_H

/* SmartPtr : (char *)の互換クラス。
 *	char buffer[1000];
 *	SmartPtr dp(buffer,sizeof(buffer));
 * と宣言すると、いくら ++dp しても、
 * dp は buffer+10000 を絶対越えないことが保証される。
 *
 * (本体の smartptr.cc は無い。インライン関数のみ)
 */

class SmartPtr{
  char *ptr;
  char *border; /* ptr の上限 , *border には「\0」がおけるのみ。*/
public:
  SmartPtr(char *p,int max) : ptr(p) , border(p+max-1) { }

  SmartPtr &operator++()
    { if( ptr<border ) ++ptr; return *this; }
  char *operator++(int)
    { return ptr<border ? ptr++ : ptr ; }
  char &operator*()
    { return *ptr; }
  operator const char*() const
    { return ptr; }
  SmartPtr &operator += (int n)
    { if( n+ptr < border ) ptr+=n ; else ptr=border; return *this; }
  SmartPtr operator + (int n) const
    { SmartPtr tmp(*this); return tmp += n ; }
  char *rawptr()
    { return ptr; }
  int operator !() const
    { return ptr >= border; }
  int ok() const
    { return ptr < border; }
  int ng() const
    { return ptr >= border; }
  void set(char *p,int max)
    { ptr=p ; border = p+max-2; }
};


#endif
