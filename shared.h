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

class SharedMem{
  enum { default_size = 4096 };

  struct Header{
    Header *next;
    unsigned long allsize;
    unsigned long usedsize;
  } *first,*last;

  unsigned long hmtx;
  int nlocked;
  bool isvirgin;
  int static_area_size;
public:
  struct Fail{};
  struct SemError{
    int errcode;
    SemError(int n) : errcode(n){}
  }; // セマフォのタイムアウト等 

  SharedMem(  const char *semname , const char *memname
	    , int staticAreaSize=0 ) throw(Fail,SemError);
  ~SharedMem();
  void *getStaticArea(){ return first+1; }
  void *alloc(int s) throw(SemError);
  void clear() throw(SemError);
  
  void lock() throw(SemError);
  void unlock();
  bool isVirgin() const { return isvirgin; }
};

class PathCacheShared : public SharedMem {
public:  
  struct Node {
    Node *next;
    unsigned short length;
    char name[1];
  };
  struct Common{
    int nfiles;
    int nbytes;
    Node *first,*last;
  } *common;

  PathCacheShared() throw(SemError);
  void clear() throw(SemError);
  int insert( const char *name ) throw(SemError);
  
  class Cursor {
    PathCacheShared &cache;
    Node *cur;
  public:
    Cursor(PathCacheShared &pc) throw(SemError): cache(pc) {
      pc.lock(); 
      cur=pc.common->first;
    }
    ~Cursor(){ cache.unlock(); }
    Node *operator->(){ return cur; }
    Node *operator*(){ return cur; }
    void operator++(){ if( cur ) cur = cur->next; }
    operator const void*() const {  return (const void*)cur; }
    bool operator ! () const { return cur == 0; }
  };
  unsigned queryBytes() throw(SemError){
    lock(); int rc=common->nbytes; unlock(); return rc;
  }
  unsigned queryFiles() throw(SemError){ 
    lock(); int rc=common->nfiles; unlock(); return rc;
  }
};

#endif
