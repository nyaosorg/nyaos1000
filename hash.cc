#include <stddef.h>
#include <ctype.h>

#include "parse.h"
#include "hash.h"

int HashB::get_index(const char *p,int len)
{
  int index=0;
  while( len-- > 0  &&  *p != '\0'){
    index += (*p++ & 255);
  }
  return index % size;
}

int HashB::get_index_without_cases(const char *p,int len)
{
  int index=0;
  while( len-- > 0  &&  *p != '\0' ){
    index += tolower(*p & 255) ; p++;
  }
  return index % size;
}

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

  tmp->key = key;
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

void *HashB::operator[](const Substr &key)
{
  if( table == NULL ) return NULL;
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

void *HashB::lookup_tolower(const Substr &key)
{
  if( table == NULL ) return NULL;
  int index=get_index_without_cases(key.ptr,key.len);

  int firstletter=tolower(key[0]);
  for(Bullet *cur=table[index] ; cur != NULL ; cur=cur->next ){
    if(   cur->key[0]==firstletter 
       && memicmp(cur->key , key.ptr , key.len )==0
       && cur->key[key.len] == '\0' ){
      return cur->rep;
    }
  }
  return NULL;
}

int HashB::remove(const char *key, int destruct_flag)
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

int HashB::remove(const Substr &key,int destruct_flag)
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

void HashB::remove_all(int destruct_flag)
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
