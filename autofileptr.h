/* -*- c++ -*- */
#ifndef AUTOFILEPTR_H
#define AUTOFILEPTR_H
#include <stdio.h>

/* 自動クローズする、ファイルポインタ。
 *
 * AutoFilePtr fp("hogehoge","r");
 * if( fp == NULL ) puts("error");
 *
 * だけで、あとは普通の FILE * と同じ。
 * ただし、fclose だけは勝手にしてはいけない。
 * (自動クローズするから)
 *
 * どうしてもクローズしたい時は、
 * メソッド AutoFilePtr::close を使うこと！！
 */

class AutoFilePtr {
  FILE *fp;
public:
  int isOk() const { return fp != 0; }
  operator FILE* () { return fp; }
  
  /* オープン：fopen の代わりに使うべきだが、
   * 普通は、引数付きコンストラクタの方でよいだろう */
  FILE *open(const char *name,const char *mode){
    if( fp != 0 ) fclose(fp);
    return fp = fopen(name,mode);
  }

  /* クローズ：fclose の代わりに使う */
  void close(){ 
    if( fp != 0 ) fclose(fp);
    fp = 0;
  }

  /* 代入演算子。一つのファイルポインタは
   * 複数の AutoFilePtr インスタンスで
   * 共有することはできない！
   */
  AutoFilePtr &operator = (AutoFilePtr &x){
    if( fp != 0 ) fclose(fp);
    fp = x.fp;
    x.fp = 0;
    return *this;
  }

  /* コンストラクタ(ファイルをオープンしない) */
  explicit AutoFilePtr(FILE *p) : fp(p) { }

  /* コンストラクタ(ファイルを同時にオープンする) */
  AutoFilePtr(const char *name,const char *mode){
    fp = fopen(name,mode);
  }

  /* デストラクタ(ファイルを自動クローズする) */
  ~AutoFilePtr() {
    if( fp != 0 ) fclose(fp); 
  }
};

#endif
