#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nls.h>
#include <ctype.h>
#include <fnmatch.h>
#include <stdarg.h>

#include "complete.h"
#include "macros.h"
#include "finds.h"
#include "hash.h"
#include "nyaos.h"

extern Hash <Alias> alias_hash;

int Complete::directory_split_char='\\';
int Complete::complete_tail_tilda=0;
int Complete::complete_hidden_file=0;

struct filelist *new_filelist(Dir &dir)
{
  struct filelist *tmp=(struct filelist*)
    malloc(sizeof(struct filelist)+dir.get_name_length() );
  assert( tmp != NULL );
  
  strcpy( tmp->name , dir.get_name() );
  tmp->length = dir.get_name_length();
  tmp->date   = dir.get_last_write_date_by_short();
  tmp->time   = dir.get_last_write_time_by_short();
  tmp->attr   = dir.get_attr();
  tmp->size   = dir.get_size();
  tmp->next   = NULL;

  return tmp;
}

struct filelist *dup_filelist(struct filelist *org)
{
  struct filelist *tmp=(struct filelist *)
    malloc( sizeof(struct filelist) + org->length );
  assert( tmp != NULL );
  memcpy( tmp , org , sizeof(struct filelist)+org->length );
  tmp->next = NULL;
  return tmp;
}

const char *Complete::get_real_name1() const
{
  struct filelist *p=list;
  if( p != NULL ){
    struct filelist *q=list->next;
    while( q != NULL ){
      if(     q->length < p->length 
	 || ( q->length == p->length && (q->attr & A_DIR) != 0 ) ){
	p=q;
      }
      q = q->next;
    }
  }
  return p->name;
}

int which_suffix(const char *path,...)
{
  /* まず、拡張子のドットを検索する。*/
  while( *path != '.' ){
    if( *path == '\0' )
      return 0;
    if( is_kanji(*path) )
      path++;
    path++;
  }

  /* 拡張子を発見、以下比較 */
  int rc=0;
  const char *q;

  va_list varptr;
  va_start(varptr,path);
  
  while( (q=va_arg(varptr,const char *)) != NULL ){
    const char *p=path+1;
    rc++;

    while( *p != '\0' ){
      /* まさか、拡張子に漢字は入らないだろうと楽観 */
      if( to_upper(*p) != to_upper(*q) )
	goto next_arg;
      p++;q++;
    }
    if( *q == '\0' ){
      va_end(varptr);
      return rc;
    }
  next_arg:
    ;
  }
  va_end(varptr);
  return 0;
}

/* パスを (ドライブ＋ディレクトリ) と (ファイル名) に分ける */

int pathsplit( const char *path, char *dir, char *fname )
{
  const char *lastroot=NULL;
  if( path[0]=='~' ){
    lastroot = path;
  }
  for(const char *p=path ; *p != '\0' ; p++ ){
    if( is_kanji(*p) ){
      ++p;
    }else if( *p=='\\' || *p=='/' || *p==':' ){
      lastroot = p;
    }
  }
  const char *p=path;

  if( lastroot != NULL ){
    if( *p == '~' ){
      if( *(p+1) == ':' ){
	const char *system_ini=getenv("SYSTEM_INI");
	if( system_ini != NULL ){
	  *dir++ = *system_ini;
	}else{
	  *dir++ = '~';
	}
	++p;
      }else{
	const char *home=getenv("HOME");
	if( home != NULL ){
	  while( *home != '\0' )
	    *dir++ = *home++;

	  if( *++p != '/' && *p != '\\' ){
	    *dir++ = '\\';
	    *dir++ = '.';
	    *dir++ = '.';
	    *dir++ = '\\';
	  }
	}else{
	  *dir++ = '~';
	  ++p;
	}
      }
    }
    while( p <= lastroot )
      *dir++ = *p++;
  }
  /* '.'を付けることで 末尾が ':','/'でも有効に働く (^_^) */
  *dir++ = '.';
  *dir   = '\0';

  if( *p != '\0' ){
    do{
      *fname++ = *p++;
    }while( *p != '\0' );
  }
  *fname = '\0';

  return (lastroot != NULL ? *lastroot : '\0');
}

const char *Complete::errmsg[]={
  "no error(s)",
  "malloc()/new operator error",
};

void Complete::cleanup()
{
  while( list != NULL ){
    struct filelist *tmp=list;
    list = list->next;
    free(tmp);
  }
}

static int instrcmp(const char *s1,const char *s2,int n)
{
  while( n-- > 0 ){
    if( is_kanji( *s1 ) ){
      if( *s1 != *s2 )
	return *s1-*s2;
      if( *++s1 != *++s2 )
	return *s1-*s2;
      n--;
    }else if( to_upper(*s1) != to_upper(*s2) ){
       return *s1-*s2;
    }
    s1++;
    s2++;
  }
  return 0;
}

static int compare(struct filelist *X,struct filelist *Y,int method)
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
      break;
    }
  case SORT_BY_NAME_IGNORE:
    if( stricmp(X->name,Y->name) == 0 ){
      rc = 0;
      break;
    }
    /* case less */

  case SORT_BY_NAME:
    rc = strcmp(X->name,Y->name);
    break;

  case SORT_BY_SIZE:
    rc = X->size - Y->size;
    break;

  case SORT_BY_CHANGE_TIME:
  case SORT_BY_LAST_ACCESS_TIME:
  case SORT_BY_MODIFICATION_TIME:

    rc = X->date - Y->date;
    if( rc == 0 )
      rc = X->time - Y->time;
    if( rc == 0 )
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

struct filelist *fsort_and_insert(struct filelist *first,struct filelist *tmp,
				  int *nfiles,int method=0)
{
  int diff;
  if( first == NULL || (diff=compare(tmp,first,method)) < 0 ){
    if( nfiles != NULL )
      ++ *nfiles;
    tmp->next = first;
    return tmp;
  }
  if( diff == 0 )
    return first;

  struct filelist *prev=first,*cur=first->next;
  for(;;){
    if( cur == NULL ){
      prev->next = tmp;
      tmp->next  = NULL;
      break;
    }
    int diff=compare(tmp,cur,method);
    
    if( diff == 0 ){
      return first;
    }else if( diff < 0 ){
      prev->next = tmp;
      tmp ->next = cur;
      break;
    }
    prev = cur;
    cur = cur->next;
  }

  if( nfiles != NULL )
    ++*nfiles;
  return first;
}

int Complete::makelist_core(int command_complete, int is_with_dir)
{
  common_length = strlen(fname);

  for(Dir dir(directory) ; dir != NULL ; ++dir ){
    if( common_length == 0
       || ( dir.get_name_length() >= common_length
	   && instrcmp( fname , dir.get_name() , common_length ) == 0 
	   ) ){

      /* コマンド名補完の場合、拡張子が、EXE,CMD,BAT,COM以外は除く。
       * (スクリプト名は、コマンド名補完モ－ドで実行していない)
       *
       * is_with_dir が立っていない場合は、ディレクトリも除く。
       */
      if(    command_complete 
	 && which_suffix(dir.get_name(),"EXE","CMD","BAT","COM",NULL)==0
	 && !( is_with_dir && (dir.get_attr() & Dir::DIRECTORY)) )
	{
	  continue;
	}
      
      /* HIDDEN属性を除く */
      if( (dir.get_attr() & Dir::HIDDEN) != 0  &&  complete_hidden_file == 0 )
	continue;

      /* 名前の末尾がチルダのファイルを除く */
      if( dir[dir.get_name_length()-1]=='~' && complete_tail_tilda==0 )
	continue;
      
      if( dir.get_name_length() > max_length )
	max_length = dir.get_name_length();
      
      list = fsort_and_insert(list,new_filelist(dir),&nlists );
			      
    }
  }
  return nlists;
}

int Complete::makelist(const char *path)
{
  status = FILENAME_COMPLETED;
  max_length=0;
  list = NULL;
  nlists = 0;

  typed_split_char = pathsplit( path , directory , fname );
  if (typed_split_char == ':' )
     typed_split_char = 0;

  return makelist_core(false,true);
}

struct filelist *path_cache=NULL;

void make_command_cache()
{
  if( path_cache != NULL ){
    struct filelist *tmp;
    for( struct filelist *q=path_cache ; q != NULL ; q=tmp ){
      tmp = q->next;
      free( q );
    }
    path_cache=NULL;
  }

  int n=0;

  const char *envpath=getenv("PATH");
  if( envpath != NULL ){
    char *env=(char*)alloca(strlen(envpath)+1);
    strcpy(env,envpath);
    
    for(const char *dirname=strtok(env,";");
	dirname != NULL ;
	dirname = strtok(NULL,";") ){
      
      for(Dir dir(dirname); dir ; dir++ ){
	if(   which_suffix(dir.get_name(),"EXE","CMD","COM",NULL) != 0
	   && dir[dir.get_name_length()-1] != '~'
	   && (dir.get_attr() & (Dir::DIRECTORY|Dir::HIDDEN))==0  )

	  path_cache = fsort_and_insert( path_cache , new_filelist(dir) 
					, &n , SORT_BY_NAME_IGNORE );
      }
    }
  }
  envpath=getenv("SCRIPTPATH");
  if( envpath != NULL ){
    char *env=(char*)alloca(strlen(envpath)+1);
    strcpy(env,envpath);
    
    for(const char *dirname=strtok(env,";");
	dirname != NULL ;
	dirname = strtok(NULL,";") ){
      
      for(Dir dir(dirname); dir ; dir++ ){
	if(   dir[dir.get_name_length()-1] != '~'
	   && (dir.get_attr() & (Dir::DIRECTORY|Dir::HIDDEN))==0  )
	  
	  path_cache = fsort_and_insert( path_cache , new_filelist(dir) 
					, &n , SORT_BY_NAME_IGNORE );
      }
    }
  }
}

int Complete::makelist_with_path(const char *path)
{
  status = COMMAND_COMPLETED;
  max_length=0;
  list = NULL;
  nlists = 0;

  typed_split_char = pathsplit( path , directory , fname );
  if( typed_split_char == ':' )
    typed_split_char = 0;

  const char *p=path;
  while( *p != '\0'){
    if( is_kanji(*p) ){
      p++;
    }else if( *p==':' || *p=='/' || *p=='\\'){
      /* フルパスで記述されている場合、
       * PATHを検索するのは無意味なので、打ちきる
       */
      return makelist_core(true,true);
    }
    p++;
  }
  
  /* ASSERT : path には、ディレクトリ名が含まれていない。*/
  strcpy( fname , path );

  if( path_cache == NULL )
    make_command_cache();
  
  struct filelist **tail=&list;
  common_length=strlen(fname);
  
  for(struct filelist *cur=path_cache ; cur != NULL ; cur=cur->next ){
    if(   cur->length > common_length
       && instrcmp(fname,cur->name,common_length ) == 0 ){
      
      struct filelist *tmp=dup_filelist(cur);
      *tail = tmp;
      tail = &tmp->next;
      nlists++;

      if( cur->length > max_length )
	max_length = cur->length;
    }
  }
  for(Dir dir(".") ; dir != NULL ; ++dir ){
    if(   dir.get_name_length() >= common_length
       && instrcmp( fname , dir.get_name() ,common_length )==0
       && which_suffix(dir.get_name(),"EXE","CMD","COM",NULL) != 0
       && dir[dir.get_name_length()-1] != '~'
       && (dir.get_attr() & (Dir::DIRECTORY|Dir::HIDDEN))==0  ){
      
      struct filelist *tmp=new_filelist(dir);
      list = fsort_and_insert( list , tmp , &nlists );
      if( tmp->length > max_length )
	max_length = tmp->length;
    }
  }
  status = SIMPLE_COMMAND_COMPLETED;
  return nlists;
}

int Complete::add_buildin_command(const char *name)
{
  int length=strlen(name);

  if(   length >= common_length
     && instrcmp( fname , name ,common_length )==0 ){
    
    struct filelist *tmp=
      (struct filelist *)malloc(sizeof(struct filelist)+length);
    
    if( tmp != NULL ){
      strcpy( tmp->name , name );
      tmp->length = length;
      tmp->attr = 0;
      tmp->size = 0;
      if( length > max_length )
	max_length = length;
      list = fsort_and_insert(list,tmp,&nlists);
      return 0;
    }
  }
  return -1;
}

char *Complete::nextchar()
{
  static char buffer[FILENAME_MAX];

  if( nlists <= 0 )
    return "\0";

  struct filelist *p=list;
  
  if( p == NULL )
    return "\0";

  strcpy( buffer , get_real_name1() + common_length );

  /* ここの、get_real_name1() は、p でもよいのだが、
     表示時の大文字・小文字の統一をうんたらかんたら...*/
  
  for( ; p != NULL ; p=p->next ){
    const char *q = p->name+common_length ;
    char *r=buffer;

    while( *r != '\0' ){
      if( is_kanji(*r) ){
	/****** 倍角文字 ******/
	if( q[0] != r[0]  ||  q[1] != r[1] ){
	  *r = '\0';
	  break;
	}
	q += 2;
	r += 2;
      }else{
	/****** 半角文字 ******/
	if( to_upper(*q) != to_upper(*r) ){
	  *r = '\0';
	  break;
	}
	q++;
	r++;
      }
    }
  }
  return buffer;
}
