// -*- c++ -*-
#ifndef SHARED_H
#define SHARED_H

/* 共有メモリの構造
 *	4 bytes : シェアードメモリ全体のサイズ[bytes]
 *	4 bytes : 全体の使用サイズ[bytes]
 *	4 bytes : ファイルの数
 *
 *   12th byte より 以下、繰り返し:
 *	2 bytes : 文字列の長さ ( 0 なら、終了)
 *	n bytes : 文字列
 * 
 */

class PathCacheShared{
  enum { header_size = 12 , default_size = 65536 };
public:  
  struct Node {
    unsigned short length;
    char name[1];
  };
private:
  union SharedPtr {
    void *set;
    long *dword;
    unsigned short *word;
    char *byte;
    Node *node;
  };

  SharedPtr sharedmem ;
  int allsize , nfiles , nbytes;
  unsigned long hmtx;
  bool isInit;
public:
  // error object
  struct CantMake{};
  void lock();
  void unlock();

  PathCacheShared() : isInit(false) {}
  
  int insert( const char *name );
  int init();
  void clear();
  Node *get_top(){ 
    return isInit &&  nfiles > 0
      ? (Node*)(sharedmem.byte+header_size) : 0 ;
  }

  class Cursor {
    Node *cur;
    PathCacheShared &cache;
  public:
    Cursor(PathCacheShared &pc) : cur(pc.get_top()),cache(pc) { pc.lock(); }
    ~Cursor(){ cache.unlock(); }
    Node *operator->(){ return cur; }
    Node *operator*(){ return cur; }
    void operator++(){ if( cur ) cur = (Node*)( cur->length+3+(char*)cur ); }
    operator const void*() const {
      return cur != 0  &&  cur->length != 0 ? this : 0; 
    }
    bool operator ! () const {
      return cur == 0 || cur->length == 0 ;
    }
  };
  unsigned queryBytes() const { return nbytes; }
  unsigned queryFiles() const { return nfiles; }
};

#endif
