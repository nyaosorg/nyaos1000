#include <cctype>
#include <cstring>
#include <cstdlib>

#include "substr.h"
#include "hash.h"

inline HashB::Bullet::~Bullet()
{
  free(key);
}

/* HashB::get_index --- キー文字列から、ハッシュ値を計算する
 *	p   - キー文字列
 *	len - キー文字列の長さ('\0'を認識するので ∞ でもよい)
 * return
 *	ハッシュ値(ハッシュテーブルのサイズで除算済みの値)
 */

int HashB::get_index(const char *p,int len)
{
  int index=0;
  while( len-- > 0  &&  *p != '\0'){
    index += (*p++ & 255);
  }
  return index % size;
}

/* HashB::get_index_without_cases
 * --- 大文字・小文字を無視する get_index。
 */
int HashB::get_index_without_cases(const char *p,int len)
{
  int index=0;
  while( len-- > 0  &&  *p != '\0' ){
    index += tolower(*p & 255) ; p++;
  }
  return index % size;
}

/* HashB::add 
 * オブジェクト rep を key として登録する。
 * 本メソッドでは、重複を許す。
 *	flag == 0 : リストの先頭に挿入する。(insert)
 *	flag != 0 : リストの末尾に追加する。(append)
 * return
 *	0 : 成功 , 非0 : 失敗(メモリエラー)
 */
int HashB::add(const char *key, void *rep, int flag)
{
  if( table == NULL ){
    if( (table=new Bullet*[size]) == NULL )
      return 1;
    for(int i=0;i<size;i++)
      table[i] = NULL;
  }

  int index=get_index(key);
  Bullet *tmp=new Bullet;
  if( tmp == NULL )
    return 1;

  if( (tmp->key = strdup(key)) == NULL ){
    delete tmp;
    return 1;
  }

  tmp->rep = rep;

  if( table[index]==(Bullet*)NULL || flag==0 ){
    tmp->next = table[index];
    table[index] = tmp;
  }else{
    tmp->next = NULL;
    Bullet *pre=table[index];
    while( pre->next != NULL )
      pre = pre->next;
    pre->next = tmp;
  }
  return 0;
}

/* HashB の [] 演算子 --- キー値からオブジェクトを検索する。
 *	key キー値 
 * return
 *	非NULL … オブジェクトへのポインタ
 *	NULL   … マッチするオブジェクトは無かった。
 */
void *HashB::operator[](const char *key)
{
  if( table == NULL ) return NULL;
  int index=get_index(key);
  
  for(Bullet *ptr=table[index] ; ptr != NULL ; ptr=ptr->next ){
    if( ptr->key[0]==key[0]  &&  strcmp(ptr->key,key)==0  ){
      return ptr->rep;
    }
  }
  return NULL;
}

/* HashB の [] 演算子 (SubStr版…char版とやってることは同じ)
 */
void *HashB::operator[](const Substr &key)
{
  if( table==NULL  ||  key.len==0 ) return NULL;
  int index=get_index(key.ptr,key.len);
  
  for(Bullet *cur=table[index] ; cur != NULL ; cur=cur->next ){
    if( cur->key[0]==key.ptr[0]
       && memcmp(cur->key , key.ptr , key.len)==0 
       && cur->key[key.len] == '\0' ){
      return cur->rep;
    }
  }
  return NULL;
}

/* 大文字小文字を区別せずに検索するメソッド。
 * 大文字小文字の区別するしない以外は [] と同じ
 */
void *HashB::lookup_tolower(const char *key)
{
  if( table==NULL  ) return NULL;
  int index=get_index_without_cases(key);

  int firstletter=tolower(*key & 255 );
  for(Bullet *cur=table[index] ; cur != NULL ; cur=cur->next ){
    if(   cur->key[0]==firstletter  && stricmp(cur->key,key) == 0  ){
      return cur->rep;
    }
  }
  return NULL;
}

/* 大文字小文字を区別せずに検索するメソッド(SubStr版)。
 * 大文字小文字の区別するしない以外は [] と同じ
 */
void *HashB::lookup_tolower(const Substr &key)
{
  if( table==NULL  ||  key.len == 0 ) return NULL;
  int index=get_index_without_cases(key.ptr,key.len);

  int firstletter=tolower(key.ptr[0] & 255 );
  for(Bullet *cur=table[index] ; cur != NULL ; cur=cur->next ){
    if(   cur->key[0]==firstletter 
       && memicmp(cur->key , key.ptr , key.len )==0
       && cur->key[key.len] == '\0' ){
      return cur->rep;
    }
  }
  return NULL;
}

/* キー値にマッチするオブジェクトをHashから除くメソッド。
 * destruct_flag 
 *	0   オブジェクトを Hash から除くだけ。
 *	非0 オブジェクトを Hash から除いた上で、delete する。
 *	    (実際の delete は仮想関数の delete_node を呼び出す)
 * return
 *	0 削除できた！
 *	1 削除できなかった！(マッチするオブジェクトが無かった)
 */
int HashB::remove(const char *key, bool destruct_flag)
{
  if( table == NULL ) return 1;
  int index=get_index(key);
  
  if( table[index] == NULL ){
    return 1;
  }else if(   table[index]->key[0] == key[0] 
	   && strcmp(table[index]->key,key)==0 ){
    
    Bullet *tmp=table[index]->next;
    delete table[index];
    table[index] = tmp;
    return 0;
  }else{
    Bullet *cur;
    for(Bullet *pre=table[index] ; (cur=pre->next) != NULL ; pre=cur ){
      if( cur->key[0] == key[0] && strcmp(cur->key,key)==0 ){
	pre->next = cur->next;
	if( destruct_flag )
	  delete_node( cur->rep );
	delete cur;
	return 0;
      }
    }
    return 1;
  }
}

/* キー値にマッチするオブジェクトをHashから除くメソッド。
 * SubStr をキーに使う以外は、char[]版と同じ。
 */
int HashB::remove(const Substr &key,bool destruct_flag)
{
  if( table == NULL ) return 1;
  int index=get_index(key.ptr,key.len);
  
  if( table[index] == NULL ){
    return 1;
  }else if( table[index]->key[0] == key.ptr[0] 
	   && memcmp(table[index]->key,key.ptr , key.len )==0
	   && table[index]->key[key.len] == '\0' ){
    
    Bullet *tmp=table[index]->next;
    if( destruct_flag )
      delete_node( table[index]->rep );
    delete table[index];
    table[index] = tmp;
    return 0;
  }else{
    Bullet *cur;
    for(Bullet *pre=table[index] ; (cur=pre->next) != NULL ; pre=cur ){
      if(   cur->key[0] == key.ptr[0] 
	 && memcmp(cur->key,key.ptr,key.len)==0
	 && cur->key[key.len] == '\0' ){
	pre->next = cur->next;
	if( destruct_flag )
	  delete_node( cur->rep );
	delete cur;
	return 0;
      }
    }
    return 1;
  }
}

/* ハッシュから、全てのインスタンスを除外する。
 * destruct_flag
 *	0    除外するだけ
 *	非0  除外した上で、インスタンスに対し、delete を実行する。
 */
void HashB::remove_all(bool destruct_flag)
{
  if( table != NULL ){
    for(int i=0; i<size; i++){
      Bullet *tmp;
      for(Bullet *ptr=table[i]; ptr != NULL ; ptr=tmp ){
	tmp=ptr->next;
	if( destruct_flag )
	  delete_node(ptr->rep);
	delete ptr;
      }
    }
    delete table;
    table = NULL;
  }
}

void *HashPtr::preptr = NULL;

HashPtr::HashPtr(HashB &h) : hash(h)
{
  if( hash.table != NULL ){
    for(index = 0 ; index < hash.size ; index++){
      if( hash.table[index] != NULL ){
	ptr = hash.table[index];
	return;
      }
    }
  }
  ptr = NULL;
}

HashPtr &HashPtr::operator++()
{
  if( ptr == NULL )
    return *this;

  if( ptr->next != NULL ){
    ptr = ptr->next;
    return *this;
  }
  while( ++index < hash.size ){
    if( hash.table[index] != NULL ){
      ptr = hash.table[index];
      return *this;
    }
  }
  ptr = NULL;
  return *this;
}
