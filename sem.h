/* -*- c++ -*- */
#ifndef SEM_H
#define SEM_H

class Sem {
  HMTX hmtx;
public:
  int create() throw()
    { return DosCreateMutexSem(NULL,&hmtx,0,FALSE); }
  int request() throw()
    { return DosRequestMutexSem(hmtx,(ULONG)SEM_INDEFINITE_WAIT); }
  int release() throw()
    { return DosReleaseMutexSem(hmtx); }
};

#endif
