/* -*- c++ -*- */
#ifndef AUTOFREEPTR_H
#define AUTOFREEPTR_H

#include <stdlib.h>

template <class P> class AutoFreePtr {
  P *ptr;
public:
  operator P*() { return ptr; }
  
  explicit AutoFreePtr(P *p) : ptr(p) { }
  ~AutoFreePtr(){ free(ptr); }
};

#endif
