#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "finds.h"

FileListT *new_filelist(Dir &dir)
{
  assert( dir != NULL );

  FileListT *node=
    (FileListT*)malloc( sizeof(FileListT) + dir.get_name_length() );
  
  if( node == NULL )
    return NULL;

  strcpy( node->name , dir.get_name() );
  node->attr   = dir.get_attr();
  node->length = dir.get_name_length();
  node->size   = dir.get_size();
  node->write.setTime(  dir.get_last_write_time()  );
  node->write.setDate(  dir.get_last_write_date()  );
  node->access.setTime( dir.get_last_access_time() );
  node->access.setDate( dir.get_last_access_date() );
  node->create.setTime( dir.get_create_time()      );
  node->create.setDate( dir.get_create_date()      );

  node->easize = dir.get_easize();
  node->next = NULL;
  node->prev = NULL;
  
  return node;
}

FileListT *new_filelist(const char *fname)
{
  assert(fname != NULL);
  
  char path[256],*p=path;
  int size=sizeof(path);
  try{
    int lastchar = convroot(p,size,fname);
    assert( size > 0 );
    if( lastchar=='\\' || lastchar == ':' )
      *p++ = '.';
    *p = '\0';
  }catch(...){
    return NULL;
  }

  FileListT *node;
  int length;
  Dir dir;
  FILESTATUS4 statbuf;

  /* FindFirst系 → FATドライブのルートディレクトリ自身の情報を取得できない。
   * PathInfo系  → 作成途中のファイルの情報を取得できない。
   *
   * という欠点がある。そこで、まず FindFirst系を試し、だめな場合、
   * PathInfo系を試みるようにしている。
   */
  if( dir.dosfindfirst( path ) == 0 ){
    /* FindFirst系成功 */
    
    length=strlen(fname);
    if( (node = (FileListT*)malloc( sizeof(FileListT) + length ))== NULL )
      return NULL;
    
    node->attr   = dir.get_attr();
    node->size   = dir.get_size();
    node->write.setTime(  dir.get_last_write_time()  );
    node->write.setDate(  dir.get_last_write_date()  );
    node->access.setTime( dir.get_last_access_time() );
    node->access.setDate( dir.get_last_access_date() );
    node->create.setTime( dir.get_create_time()      );
    node->create.setDate( dir.get_create_date()      );
    node->easize = dir.get_easize();
  }else if( DosQueryPathInfo((PUCHAR)path,2,&statbuf,sizeof(FILESTATUS4))==0){
    /* PathInfo系成功 */

    length=strlen(fname);
    if( (node=(FileListT*)malloc(sizeof(FileListT)+length))==NULL )
      return NULL;

    node->attr   = statbuf.attrFile;
    node->size   = statbuf.cbFile;
    node->write.setTime( statbuf.ftimeLastWrite );
    node->write.setDate( statbuf.fdateLastWrite );
    node->access.setTime( statbuf.ftimeLastAccess );
    node->access.setDate( statbuf.fdateLastAccess );
    node->create.setTime( statbuf.ftimeCreation );
    node->create.setDate( statbuf.fdateCreation );
    node->easize = statbuf.cbList;
  }else{
    /* 大失敗 */
    return NULL;
  }
  strcpy( node->name , fname );
  node->length = length;
  node->next = NULL;
  node->prev = NULL;

  return node;
}

FileListT *dup_filelist(FileListT *org)
{
  assert( org != NULL);

  FileListT *tmp=(FileListT *)
    malloc( sizeof(FileListT) + org->length );

  if( tmp == NULL )
    return NULL;

  memcpy( tmp , org , sizeof(FileListT)+org->length );
  tmp->next = NULL;
  tmp->prev = NULL;
  return tmp;
}

static int compare(filelist::DirDateTime &A , filelist::DirDateTime &B )
{
  int rc;
  if( (rc=A.getYear()  - B.getYear()   )!= 0 ) return rc; /* 年 */
  if( (rc=A.getMonth() - B.getMonth()  )!= 0 ) return rc; /* 月 */
  if( (rc=A.getDay()   - B.getDay()    )!= 0 ) return rc; /* 日 */
  if( (rc=A.getHour()  - B.getHour()   )!= 0 ) return rc; /* 時 */
  if( (rc=A.getMinute()- B.getMinute() )!= 0 ) return rc; /* 分 */
  return A.getSecond()- B.getSecond();
}

static int compare(FileListT *X,FileListT *Y,int method)
{
  assert( X != NULL );
  assert( Y != NULL );

  int rc=0;
  switch( method & ~SORT_REVERSE ){
  case SORT_BY_SUFFIX:
    {
      const char *x_sfx=NULL , *y_sfx=NULL;
      const char *xp=X->name , *yp=Y->name;
      while( *xp != '\0' ){
	if( *xp == '.' ){
	  x_sfx = xp+1;
	}else if( *xp == '/' || *xp == '\\' ){
	  x_sfx = NULL;
	}
	++xp;
      }
      while( *yp != '\0' ){
	if( *yp == '.' ){
	  y_sfx = yp+1;
	}else if( *yp == '/' || *yp == '\\' ){
	  y_sfx = NULL;
	}
	++yp;
      }
      if( x_sfx == NULL ){
	if( y_sfx == NULL )
	  rc = strcmp(X->name,Y->name);
	else
	  rc = -1;
      }else{
	if( y_sfx == NULL ){
	  rc = +1;
	}else{
	  rc = strcmp(x_sfx,y_sfx);
	  if( rc == 0 )
	    rc = strcmp(X->name,Y->name);
	}
      }
    }
    break;

  case SORT_BY_NUMERIC:
    rc = strnumcmp(X->name,Y->name);
    if( rc == 0 )
      rc = strcmp(X->name,Y->name);
    break;

  case SORT_BY_NAME_IGNORE:
    if( stricmp(X->name,Y->name) == 0 ){
      rc = 0;
      break;
    }
    /* continue to next case */

  case SORT_BY_NAME:
    rc = strcmp(X->name,Y->name);
    break;

  case SORT_BY_SIZE:
    rc = Y->size - X->size;
    if( rc == 0 )
      rc = strcmp(X->name,Y->name);
    break;

  case SORT_BY_CHANGE_TIME:
    if( (rc=compare( Y->create , X->create )) == 0 )
      rc = strcmp( Y->name , X->name );
    break;

  case SORT_BY_LAST_ACCESS_TIME:
    if( (rc=compare( Y->access , X->access )) == 0 )
      rc = strcmp(X->name,Y->name);
    break;

  case SORT_BY_MODIFICATION_TIME:
    if( (rc=compare( Y->write , X->write )) == 0 )
      rc = strcmp(X->name,Y->name);
    break;
    
  default: // unsort
    rc = +1;
    break;
  }

  if( method & SORT_REVERSE )
    return -rc;
  else
    return rc;
}

FileListT *fsort_and_insert(FileListT *first , FileListT *tmp ,
			    int *nfiles , int method=0)
{
  assert( tmp != NULL );

  int diff;
  if(   method == UNSORT 
     || first == NULL 
     || (diff=compare(tmp,first,method)) < 0 ){
    if( nfiles != NULL )
      ++ *nfiles;
    tmp->next = first;
    if( first != NULL )
      first->prev = tmp;
    tmp->prev = NULL;
    return tmp;
  }
  if( diff == 0 )
    return first;

  FileListT *prev=first,*cur=first->next;
  for(;;){
    if( cur == NULL ){
      prev->next = tmp;
      tmp->next  = NULL;
      tmp->prev  = prev;
      break;
    }
    int diff=compare(tmp,cur,method);
    
    if( diff == 0 ){
      return first;
    }else if( diff < 0 ){
      prev->next = tmp;
      tmp ->prev = prev;
      tmp ->next = cur;
      cur ->prev = tmp;
      break;
    }
    prev = cur;
    cur = cur->next;
  }

  if( nfiles != NULL )
    ++*nfiles;
  return first;
}

FileListT *merge_sort(FileListT *list,int method=0) throw()
{
  if( list == NULL  || list->next == NULL )
    return list;

  /* まずは、リストを二分割 */
  FileListT *tail=list , *half=list;
  while( tail != NULL ){
    tail = tail->next;
    if( tail == NULL )
      break;
    half = half->next;
    tail = tail->next;
  }
  FileListT *list2=half;
  half->prev->next = NULL;
  half->prev = NULL;

  /* ソート */
  list  = merge_sort( list  , method );
  list2 = merge_sort( list2 , method );
  if( list == NULL )
    return list2;
  if( list2 == NULL )
    return list;
  
  /* マージ */
  FileListT *merge;
  if( compare(list,list2,method) < 0 ){
    merge = list;
    list = list->next;
  }else {
    merge = list2;
    list2 = list2->next;
  }
  merge->prev = NULL;

  FileListT *result=merge;
  for(;;){
    if( list == NULL ){
      merge->next = list2;
      if( list2 != NULL )
	list2->prev = merge;
      return result;
    }else if( list2 == NULL ){
      merge->next = list;
      if( list != NULL )
	list->prev = merge;
      return result;
    }else if( compare(list,list2,method) < 0 ){
      list->prev = merge;
      merge = merge->next = list;
      list = list->next;
    }else{
      list2->prev = merge;
      merge = merge->next = list2;
      list2 = list2->next;
    }
  }
}



/** ------ Files class ------ **/

void Files::insert( FileListT *newone , int sort )
{
  assert( newone != NULL );
  top = fsort_and_insert( top , newone , &n , UNSORT );
}

void Files::sort( int method )
{
  top = merge_sort( top , method );
}

void Files::setDirName( const char *name )
{
  if( dirname != NULL )
    free(dirname);

  dirname = strdup(name);
}

void Files::clear()
{
  while( top != NULL ){
    FileListT *tmp=top->next;
    free(top);
    top=tmp;
    --n;
  }
  if( dirname != NULL ){
    free(dirname);
    dirname = NULL;
  }
}

/* 最も最後尾のファイルを返す。
 * ファイルが一つも無い時は NULL を返す。
 */
FileListT *Files::get_tail() const
{
  FileListT *ptr=top;
  if( ptr == NULL )
    return NULL;

  while( ptr->next != NULL ){
    ptr = ptr->next;
  }
  return ptr;
}
