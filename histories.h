/* -*- c++ -*- */
#ifndef HISTORIES_H
#define HISTORIES_H

class InterfaceHistory{
  virtual void goHome()=0;		/* ホームへ戻る(Enterを押した時等) */
  virtual void operator++()=0;		/* 過去(古い方)へ移動 */
  virtual void operator--()=0;		/* 未来(新しい方)へ移動 */
  virtual const char *operator*()=0;	/* ヒストリの内容を参照する */
  virtual bool operator !() const=0;	/* これ以上過去が無いかどうか */
  virtual operator void *() { return !*this ? NULL : this; }
};

class Histories {
  struct Node {
    Node *prev; /* 過去方向 */
    Node *next;	/* 未来方向 */
    char buffer[1];
  } pole; /* first でもあり、last でもあるダミーオブジェクト */
  int n;
public:
  Node *getPole(){ return &pole; }
  int append(const char *s); /* last の末尾に追加する。*/

  class Cursor{
    Histories &histories;
    Node *ptr;
    int n;
  public:
    Cursor(Histories &h) : histories(h) , ptr( h.getPole() ){ }
    void operator++(){ 
      if( (ptr=ptr->next) != histories.getPole() )
	++n;
      else
	n=0;
    }
    void operator--(){
      if( (ptr=ptr->prev) != histories.getPole() )
	--n;
      else
	n=0;
    }
    char *operator *() { return ptr->buffer; }

    const char *getNth(int n);
    
    operator const void*() const { return ptr != histories.getPole() 
				     ? this : NULL ; }
    bool operator !() const { return ptr != histories.getPole(); }
    int replace(const char *s);
    int getN();
  };

  Histories() : n(0) { pole.prev=pole.next=NULL; pole.buffer[0]='\0'; }
  ~Histories();
};

#endif
