/* -*- c++ -*- */
#ifdef AUTOCHARPTR_H
#define AUTOCHARPTR_H

#include <stdlib.h>

class AutoCharPtr {
  char *ptr;
public:
  operator const char *() const { return ptr; }
  operator char *() { return ptr; }

  AutoCharPtr() : ptr(0) { }
  ~AutoCharPtr(){
    if( ptr != 0 ) free(ptr);
  }

  


#endif
