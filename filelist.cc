#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "finds.h"

void kill_filelist(FileListT *p)
{
  while( p != NULL ){
    FileListT *nxt = p->next;
    free(p);
    p = nxt;
  }
}

static FileListT *new_filelist_sub(FILESTATUS4 &buffer,const char *fname)
{
  int len=strlen(fname);  
  
  FileListT *node=
    (FileListT*)malloc(sizeof(FileListT)+len);

  strcpy( node->name , fname );
  node->attr   = buffer.attrFile;
  node->length = len;
  node->size   = buffer.cbFile;
  node->write.time =  *(unsigned short *)&buffer.ftimeLastWrite;
  node->write.date =  *(unsigned short *)&buffer.fdateLastWrite;
  node->access.time = *(unsigned short *)&buffer.ftimeLastAccess;
  node->access.date = *(unsigned short *)&buffer.fdateLastAccess;
  node->create.time = *(unsigned short *)&buffer.ftimeCreation;
  node->create.date = *(unsigned short *)&buffer.fdateCreation;
  
  node->easize = buffer.cbList;
  node->next = NULL;
  node->prev = NULL;
  
  return node;
}

/* DosQueryPathInfo のフィルター */

static int dos_query_path_info(const char *name,int len,FILESTATUS4 &buffer )
{
  char *new_name = (char*)alloca( len + 2 );
  const char *sp=name;
  char *dp=new_name;

  try{
    int lastchar=convroot(dp,len+=2,sp);
    if( lastchar == '\\' || lastchar == ':' )
      *dp++ = '.';
  }catch(...){
    ;
  }
  *dp = '\0';

  return DosQueryPathInfo((PUCHAR)new_name,2,&buffer,sizeof(FILESTATUS4) );
}

FileListT *new_filelist(const char *fname)
{
  FILESTATUS4 buffer;

  if( dos_query_path_info(fname,strlen(fname),buffer) != 0 )
    return NULL;

  return new_filelist_sub(buffer,fname);
}

FileListT *new_filelist(Dir &dir)
{
  FileListT *node=
    (FileListT*)malloc( sizeof(FileListT)+dir.get_name_length() );

  if( node == NULL )
    return NULL;

  strcpy( node->name , dir.get_name() );
  node->attr   = dir.get_attr();
  node->length = dir.get_name_length();
  node->size   = dir.get_size();
  node->write.time =  *(unsigned short *)&dir.get_last_write_time();
  node->write.date =  *(unsigned short *)&dir.get_last_write_date();
  node->access.time = *(unsigned short *)&dir.get_last_access_time();
  node->access.date = *(unsigned short *)&dir.get_last_access_date();
  node->create.time = *(unsigned short *)&dir.get_create_time();
  node->create.date = *(unsigned short *)&dir.get_create_date();
  
  node->easize = dir.get_easize();
  node->next = NULL;
  node->prev = NULL;
  
  return node;
}

FileListT *dup_filelist(FileListT *org)
{
  FileListT *tmp=(FileListT *)
    malloc( sizeof(FileListT) + org->length );
  assert( tmp != NULL );

  memcpy( tmp , org , sizeof(FileListT)+org->length );
  tmp->next = NULL;
  tmp->prev = NULL;
  return tmp;
}

static int compare(filelist::DirDateTime &A , filelist::DirDateTime &B )
{
  int rc;
  // 年
  rc=(int)A.d.year - (int)B.d.year;
  if( rc != 0 ) return rc;
  // 月
  rc=(int)A.d.month - (int)B.d.month;
  if( rc != 0 ) return rc;
  // 日
  rc=(int)A.d.day - (int)B.d.day;
  if( rc != 0 ) return rc;
  // 時
  rc=(int)A.t.hour - (int)B.t.hour;
  if( rc != 0 ) return rc;
  // 分
  rc=(int)A.t.minute - (int)B.t.minute;
  if( rc != 0 ) return rc;
  // 秒
  return (int)A.t.second - (int)B.t.second;
}

static int compare(FileListT *X,FileListT *Y,int method)
{
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
    
  default:
    rc = -1;
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
  int diff;
  if( first == NULL || (diff=compare(tmp,first,method)) < 0 ){
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

/** ------ Files class ------ **/

void Files::insert( FileListT *newone , int sort )
{
  top = fsort_and_insert( top , newone , &n , sort );
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
  if( dirname != NULL )
    free(dirname);
}
