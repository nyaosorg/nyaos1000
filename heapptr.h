/* -*- c++ -*- */
#ifndef HEAPPTR_H
#define HEAPPTR_H

#include <stdlib.h>

/* auto_ptr の普通の malloc/free 版。
 * ヒープにとられたオブジェクトの領域の自動解放を司る。
 */

template <class C> class heapptr_t {
  C *pointor;
public:
  C *operator -> ()	{ return pointor; }
  operator C* ()	{ return pointor; }
  C &operator[](int n)	{ return pointor[n]; }
  
  /* コンストラクタ・デストラクタ */
  heapptr_t() : pointor(NULL) { }
  explicit heapptr_t(heapptr_t &f){
    pointor = f.pointor;
    f.pointor = NULL;
  }
  explicit heapptr_t(C *p) : pointor(p) { }
  ~heapptr_t(){ free(pointor); }

  /* 代入演算子 */
  heapptr_t &operator = (heapptr_t &f){
    if( &f != this ){
      free(pointor);
      pointor = f.pointor;
      f.pointor = NULL;
    }
    return *this;
  }
  heapptr_t &operator = (C *p){
    if( p != pointor ){
      free(pointor);
      pointor = p;
    }
    return *this;
  }
};

typedef heapptr_t <char> heapchar_t;

#endif
