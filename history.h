

class History {
  struct Line {
    Line *prev,*next;

    Line() : prev(0) , next(0){ }
    ~Line(){
      if( prev ) prev->next = next;
      if( next ) next->prev = prev;
    }
    void connect(Line *another){
      if( next ){ next->prev = 0; }
      if( another->prev ){ another->prev->next = 0; }
      next = another;
      another->prev = this;
    }
    void *operator new(size_t basesize,size_t more)
      {  return malloc(basesize+more);  }
    void operator delete(void *it)
      {  free(it);  }
    char buffer[1];
  };


};
