/* -*- c++ -*- */
#ifndef EACLASS_H
#define EACLASS_H

#include <sys/ea.h>

union UniPtr {
  void *value;
  const char *byte;
  const unsigned short *word;
};

class ExtAttr{
  int rc;
  struct _ea ea;
  UniPtr ptr;
public:
  int open(const char *fname,const char *eaname);
  UniPtr *operator->(){ return &ptr; }

  ExtAttr() : rc(-1)
    { }
  ~ExtAttr()
    { if( rc != -1 ) _ea_free( &ea ); }
};  

int ExtAttr::open(const char *fname , const char *eaname )
{
  if(   _ea_get( &ea , fname , 0 , eaname ) == 0
     && ea.size > 0 && ea.value != NULL ){
    
    ptr.value = ea.value;
    return 0;
  }
  return 1;
}

#endif
