/* -*- c++ -*- */
#ifndef HASH_H
#define HASH_H

class Substr;
class HashPtr;

class HashB{
friend class HashPtr;
  struct Bullet{
    Bullet *next;
    const char *key;
    void *rep;
  }**table;
  int size;

  int get_index(const char *key);
  int get_index(const Substr &s);
protected:
  virtual void delete_node(void *){ };
  int add(const char *key, void *rep,int flag);
public:
  int insert(const char *key, void *rep){ return add(key,rep,0); }
  int append(const char *key, void *rep){ return add(key,rep,1); }

  int remove(const char *key,int destruct_flag=0 );
  int remove(const Substr &key,int destruct_flag=0 );
  int destruct(const char *key){ return remove(key,1); }
  int destruct(const Substr &key){ return remove(key,1); }

  void remove_all(int destruct_flag=0);
  void destruct_all(void){ remove_all(1); }

  void *operator[](const char *key);
  void *operator[](const Substr &s);
  
  HashB(int s) : size(s) , table((Bullet**)NULL) { }
  ~HashB(){ }
};

template <class T>
class Hash : public HashB{
  void delete_node(void *one){ delete (T*)one; }
public:
  int insert(const char *key,T *rep){ return add(key,rep,0); }
  int append(const char *key,T *rep){ return add(key,rep,1); }
  T *operator[](const char *key){ return (T*)HashB::operator[](key); }
  T *operator[](const Substr &key){ return (T*)HashB::operator[](key); }

  void clean_and_delete();
  Hash(int i) : HashB(i) { }
};

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
