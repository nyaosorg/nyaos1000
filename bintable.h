/* -*- c++ -*- 
 * bsearch 関数をラッピングする為のクラス。
 * テーブルの要素となるオブジェクトのクラスは
 * 「name」という名前の char* / char[] 型メンバを持つことが必要。
 */

#ifndef BINTABLE_H

#include <string.h> /* for str(i)cmp */
#include <stdlib.h> /* for bsearch */

template <class T> class BinTable {
  static int compare(void *key,void *el){
    return stricmp(  static_cast <const char *>(key)
		   , (static_cast <T *>(el) )->name  );
  }
  T *table;
  int n;
public:
  T *find( const char *key ){
    return static_cast <T*> (bsearch(  name
				     , table
				     , n
				     , sizeof(T)
				     , BinTable<T>::compare ) );
  }
  
  BinTable(T *_table,int _n) : table(_table) , n(_n) { }
  ~BinTable();
};

#endif
