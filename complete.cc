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

const char *Complete::get_real_name1() const
{
  FileListT *p=get_top();
  if( p != NULL ){
    FileListT *q=p->next;
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
    }else if( *p=='.' && *(p+1)=='.' && *(p+2)=='.' ){
      *dir++ = *p++;
      *dir++ = *p++;
      while( *p == '.' ){
	*dir++ = '\\';
	*dir++ = '.';
	*dir++ = '.';
	p++;
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

int Complete::makelist_core(int command_complete, int is_with_dir)
{
  common_length = strlen(fname);

  for(Dir dir(directory) ; dir != NULL ; ++dir ){

    /* 「.」と「..」を除く */
    if( dir[0]=='.' && ( dir[1]=='.' || dir[1]=='\0' ) )
      continue;
    
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
      
      insert( new_filelist(dir) );
    }
  }
  return get_num();
}

int Complete::makelist(const char *path)
{
  status = FILENAME_COMPLETED;
  max_length=0;
  clear();
  
  typed_split_char = pathsplit( path , directory , fname );
  if (typed_split_char == ':' )
     typed_split_char = 0;

  return makelist_core(false,true);
}

Files path_cache;

void make_command_cache()
{
  path_cache.clear();

  const char *envpath=getenv("PATH");
  if( envpath != NULL ){
    char *env=(char*)alloca(strlen(envpath)+1);
    strcpy(env,envpath);
    
    for(  const char *dirname=strtok(env,";")
	; dirname != NULL
	; dirname = strtok(NULL,";") ){
      
      for(Dir dir(dirname); dir ; dir++ ){
	if(   which_suffix(dir.get_name(),"EXE","CMD","COM",NULL) != 0
	   && dir[dir.get_name_length()-1] != '~'
	   && (dir.get_attr() & (Dir::DIRECTORY|Dir::HIDDEN))==0  )

	  path_cache.insert( new_filelist(dir) , SORT_BY_NAME_IGNORE );
      }
    }
  }
  envpath=getenv("SCRIPTPATH");
  if( envpath != NULL ){
    char *env=(char*)alloca(strlen(envpath)+1);
    strcpy(env,envpath);
    
    for(  const char *dirname=strtok(env,";")
	; dirname != NULL
	; dirname = strtok(NULL,";") ){
      
      for(Dir dir(dirname); dir ; dir++ ){
	if(   dir[dir.get_name_length()-1] != '~'
	   && (dir.get_attr() & (Dir::DIRECTORY|Dir::HIDDEN))==0  )
	
	  path_cache.insert( new_filelist(dir) ,SORT_BY_NAME_IGNORE );
      }
    }
  }
}

int Complete::makelist_with_path(const char *path)
{
  status = COMMAND_COMPLETED;
  max_length=0;
  this->clear();

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

  if( path_cache.get_top() == NULL )
    make_command_cache();
  
  common_length=strlen(fname);
  
  for(FileListT *cur=path_cache.get_top() ; cur != NULL ; cur=cur->next ){
    if(   cur->length > common_length
       && instrcmp(fname,cur->name,common_length ) == 0 ){
      
      insert( dup_filelist(cur) );
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
    
      insert( new_filelist(dir) );
      if( dir.get_name_length() > max_length )
	max_length = dir.get_name_length();
    }
  }
  status = SIMPLE_COMMAND_COMPLETED;
  return get_num();
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

      insert( tmp );
      return 0;
    }
  }
  return -1;
}

char *Complete::nextchar()
{
  static char buffer[FILENAME_MAX];

  if( get_num() <= 0 )
    return "\0";

  FileListT *p=get_top();
  
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
