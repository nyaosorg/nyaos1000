/* -*- c++ -*- */
#ifndef HASH_H
#define HASH_H

/* Hash --- 検索キーをchar[]型文字列に限定したハッシュコンテナ(template)
 *
 * 【ハッシュの宣言】
 * Hash <Object> hashbuf; // 宣言する。
 * Object *object; // ポインタであるのに注意
 *
 * hasubuf.insert("キー",object);
 *	登録：同一キーがある場合、最後に登録したものを優先させる。重複許す。
 *
 * hashbuf.append("キー",object);
 *	登録：同一キーがある場合、最初に登録したものを優先させる。重複許す。
 *
 * hashbuf["キー"]
 *	検索：キーにマッチするオブジェクトを返す。無い場合は NULL
 *
 * hashbuf.remove("キー");
 *	キーにマッチする最初のオブジェクトをテーブルより除く。delete しない。
 *
 * hashbuf.destruct("キー");
 *	キーにマッチする最初のオブジェクトをテーブルから除き、delete する。
 *
 * hashbuf.remove_all();
 *	全てのオブジェクトをテーブルから除く。
 *
 * hashbuf.destruct_all();
 *	全てのオブジェクトをテーブルから除いて、delete する。
 *
 * insert/append で用いるキーについては、Hash側で strdup/free する(新版より)。
 *
 * HashPtr --- Hash の全ての キー×オブジェクトの対を列挙するためのクラス。
 *             Iterator というほどのものではない。
 */

class Substr;
class HashPtr;

class HashB{
friend class HashPtr;
  struct Bullet{
    Bullet *next;
    char *key;
    void *rep;

    Bullet() : next(0),key(0),rep(0){ }
    ~Bullet();
  }**table;
  int size;

  int get_index(const char *key,int len=1000);
  int get_index_without_cases(const char *key,int len=1000);
protected:
  virtual void delete_node(void *){ };
  int add(const char *key, void *rep,int flag);
  int insert(const char *key, void *rep){ return add(key,rep,0); }
  int append(const char *key, void *rep){ return add(key,rep,1); }
  void *operator[](const char *key);
  void *operator[](const Substr &s);
  void *lookup_tolower(const Substr &s);
  void *lookup_tolower(const char *s);
public:
  int remove(const char *key   ,bool do_delete=false );
  int remove(const Substr &key ,bool do_delete=false );
  void remove_all(bool do_delete=false);
  
  HashB(int s) : table((Bullet**)NULL) , size(s) { }
  virtual ~HashB(){ }
};

template <class T>
class Hash : public HashB{
  void delete_node(void *one){ delete (T*)one; }
public:
  int insert(const char *key,T *rep){ return add(key,rep,0); }
  int append(const char *key,T *rep){ return add(key,rep,1); }
  T *operator[](const char *key){ return (T*)HashB::operator[](key); }
  T *operator[](const Substr &key){ return (T*)HashB::operator[](key); }
  T *lookup_tolower(const Substr &key){ return (T*)HashB::lookup_tolower(key);}
  T *lookup_tolower(const char *key){ return (T*)HashB::lookup_tolower(key);}
  
  int  destruct(const char   *key){ return remove(key,true); }
  int  destruct(const Substr &key){ return remove(key,true); }
  void destruct_all(){ remove_all(true); }
  
  Hash(int i) : HashB(i) { }
};

/* Hash に登録されている全てのオブジェクトを参照するためのクラス。
 * 
 */

class HashPtr{
  static void *preptr;
  HashB &hash;
  HashB::Bullet *ptr;
  int index;
  HashPtr(void);
public:
  HashPtr(HashB &h);
  
  void *operator*(){ return ptr != NULL ? ptr->rep : NULL ; }
  HashPtr &operator++();
  void **operator++(int)
    { preptr=**this; ++*this; return &preptr; }
};

template <class T>
class HashIndex : public HashPtr{
public:
  HashIndex(Hash<T> &h) : HashPtr(h) {}
  T *operator*(){ return (T*)HashPtr::operator*(); }
  T *operator->(){ return (T*)HashPtr::operator*(); }
};

#endif
