#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nls.h>
#include <ctype.h>
#include <fnmatch.h>
#include <stdarg.h>
#include "complete.h"

int Complete::directory_split_char='\\';
int Complete::complete_tail_tilda=0;
int Complete::complete_hidden_file=0;

int which_suffix(const char *path,...)
{
  /* まず、拡張子のドットを検索する。*/
  while( *path != '.' ){
    if( *path == '\0' )
      return 0;
    if( _nls_is_dbcs_lead(*path) )
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
      if( toupper(*p) != toupper(*q) )
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
    if( _nls_is_dbcs_lead( *p & 255 ) ){
      ++p;
    }else if( *p=='\\' || *p=='/' || *p==':' ){
      lastroot = p;
    }
  }

  const char *p=path;

  if( lastroot != NULL ){
    if( *p == '~' ){
      const char *home=getenv("HOME");
      while( *home != '\0' )
	*dir++ = *home++;

      if( *++p != '/' && *p != '\\' ){
	*dir++ = '\\';
	*dir++ = '.';
	*dir++ = '.';
	*dir++ = '\\';
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

int dircompare(struct filelist *d1,struct filelist *d2)
{
  static const char *dircmd=NULL;

  if( dircmd == NULL ){
    dircmd = getenv("DIRCMD");
    if( dircmd==NULL ){
      dircmd = "n";
    }else{
      for(;;){
	if( *dircmd == '\0' ){
	  dircmd = "n";
	  break;
	}else if( *dircmd++ =='/'  &&  (*dircmd=='O' || *dircmd=='o') ){
	  ++dircmd;
	  break;
	}
      }
    }
  }

  /* nedsg */
  const char *p=dircmd;
  int sign=+1;
  while( *p != '\0' && !isspace(*p) ){
    int diff;
    const char *q;
    const char *period1,*period2;

    if( *p == '-' ){
      sign = -1;
    }else{
      switch( *p ){

      case 'n':
      case 'N':
	diff = d1->name[0] - d2->name[0];
	if( diff != 0 ) return sign*diff;
	diff = strcmp(d1->name , d2->name );
	if( diff != 0 ) return sign*diff;
	break;

      case 'g':
      case 'G':
	diff = 0;
	if( d1->attr & A_DIR ) diff--;
	if( d2->attr & A_DIR ) diff++;
	if( diff != 0 ) return sign*diff;
	break;

      case 'S':
      case 's':
	if( d1->size < d2->size )
	  return -sign;
	else if( d1->size > d2->size )
	  return +sign;
	else
	  break;

      case 'D':
      case 'd':
	if( d1->date < d2->date ){
	  return -sign;
	}else if( d1->date > d2->date ){
	  return sign;
	}else if( d1->time < d2->time ){
	  return -sign;
	}else if( d1->time > d2->time ){
	  return sign;
	}
	break;

      case 'e':
      case 'E':
	period1 = period2 = NULL;
	q=d1->name;
	while( *q != '\0' ){
	  if( _nls_is_dbcs_lead(*q) ){
	    q++;
	  }else if( *q == '/' || *q=='\\' ){
	    period1 = NULL;
	  }else if( *q=='.' ){
	    period1 = q;
	  }
	  q++;
	}
	q=d2->name;
	while( *q != '\0' ){
	  if( _nls_is_dbcs_lead(*q) ){
	    q++;
	  }else if( *q == '/' || *q=='\\' ){
	    period2 = NULL;
	  }else if( *q=='.' ){
	    period2 = q;
	  }
	  q++;
	}
	if( period1 == NULL ){
	  if( period2 == NULL )
	    break;
	  else
	    return sign;
	}else if( period2 == NULL ){
	  return -sign;
	}
	diff=strcmp(period1,period2);
	if( diff != 0 ) return sign*diff;
	break;

      }/* end switch */
      sign = +1;
    }
    p++;
  }/* letter loop */
  return +1;
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
    if( _nls_is_dbcs_lead( *s1 ) ){
      if( *s1 != *s2 )
	return *s1-*s2;
      if( *++s1 != *++s2 )
	return *s1-*s2;
      n--;
    }else if( toupper(*s1 & 255) != toupper(*s2 & 255) ){
       return *s1-*s2;
    }
    s1++;
    s2++;
  }
  return 0;
}
struct filelist *fsort_and_insert(struct filelist *first,struct filelist *tmp)
{
  if( first == NULL || dircompare(tmp,first) < 0 ){
    tmp->next = first;
    return tmp;
  }else{
    struct filelist *prev=first,*cur=first->next;
    for(;;){
      if( cur == NULL ){
	prev->next = tmp;
	tmp->next  = NULL;
	break;
      }
      int diff=dircompare(tmp,cur);
      if( diff < 0 ){
	tmp ->next = cur;
	prev->next = tmp;
	break;
      }else if( diff==0  &&  strcmp(tmp->name,cur->name)==0 ){
	/* 同じファイル名の場合、何もしない。 */
	break;
      }
      prev = cur;
      cur = cur->next;
    }
    return first;
  }
}

int Complete::makelist_core(int command_complete)
{
  DIR *dirp=opendir(directory);
  if( dirp == NULL )
    return -1;
  
  common_length = strlen(fname);
  struct dirent *dirbuf;

  while( (dirbuf=readdir(dirp)) != NULL ){
    if( common_length == 0
       || ( dirbuf->d_namlen >= common_length
	   && instrcmp( fname , dirbuf->d_name , common_length ) == 0 
	   ) ){

      /* コマンド名補完の場合、拡張子が、EXE,CMD,BAT,COM以外は除く。
       * (スクリプト名は、コマンド名補完モ－ドで実行していない)
       */
      if( command_complete 
	   && which_suffix(dirbuf->d_name,"EXE","CMD","BAT","COM",NULL)==0 )
	continue;

      /* HIDDEN属性を除く */
      if( (dirbuf->d_attr & A_HIDDEN) != 0  &&  complete_hidden_file == 0 )
	continue;

      /* 名前の末尾がチルダのファイルを除く */
      if( dirbuf->d_name[dirbuf->d_namlen-1]=='~' && complete_tail_tilda==0 )
	continue;
      
      struct filelist *tmp =
	(struct filelist *)malloc(sizeof(struct filelist)+dirbuf->d_namlen );
      assert(tmp != NULL);
      if( tmp == NULL ){
	status = ERROR;
	closedir(dirp);
	return -1;
      }
      strcpy( tmp->name , dirbuf->d_name );

      tmp->length = dirbuf->d_namlen;
      tmp->date   = dirbuf->d_date;
      tmp->time   = dirbuf->d_time;
      tmp->attr   = dirbuf->d_attr;
      tmp->size   = dirbuf->d_size;
      
      if( dirbuf->d_namlen > max_length )
	max_length = dirbuf->d_namlen;

      list = fsort_and_insert(list,tmp);
      nlists++;
    }
  }
  closedir(dirp);
  return nlists;
}

int Complete::makelist(const char *path)
{
  status = FILENAME_COMPLETED;
  max_length=0;
  list = NULL;
  nlists = 0;

  pathsplit( path , directory , fname );
  return makelist_core(false);
}

int Complete::makelist_with_path(const char *path)
{
  status = COMMAND_COMPLETED;
  max_length=0;
  list = NULL;
  nlists = 0;

  pathsplit( path , directory , fname );
  int rc=makelist_core(true);

  const char *p=path;
  while( *p != '\0'){
    if( _nls_is_dbcs_lead(*p) ){
      p++;
    }else if( *p==':' || *p=='/' || *p=='\\'){
      /* フルパスで記述されている場合、
       * PATHを検索するのは無意味なので、打ちきる
       */
      return rc;
    }
    p++;
  }
  
  /* ASSERT : path には、ディレクトリ名が含まれていない。*/
  strcpy( fname , path );
  
  /* 環境変数 PATH をたどる */
  const char *envpath=getenv("PATH");
  if( envpath != NULL ){
    char *envpath2=(char*)alloca(strlen(envpath)+1);
    strcpy(envpath2,envpath);

    char *dir=strtok(envpath2,";");
    while( dir != NULL ){
      strcpy( directory , dir );

      makelist_core(true);
      dir=strtok(NULL,";");
    }
  }

  /* 環境変数 SCRIPTPATH をたどる */
  extern int scriptflag;
  if( scriptflag && (envpath=getenv("SCRIPTPATH")) != NULL ){
    char *envpath2=(char*)alloca(strlen(envpath)+1);
    strcpy(envpath2,envpath);

    char *dir=strtok(envpath2,";");
    while( dir != NULL ){
      strcpy( directory , dir );
      makelist_core(false);
      dir=strtok(NULL,";");
    }
  }
  status = SIMPLE_COMMAND_COMPLETED;
  return nlists;
}

int Complete::add_buildin_command(const char *name)
{
  int length=strlen(name);

  if( common_length == 0 || ( length >= common_length
			     && instrcmp(fname,name,common_length)==0 )){
    struct filelist *tmp=
      (struct filelist *)malloc(sizeof(struct filelist)+length);
    
    if( tmp != NULL ){
      strcpy( tmp->name , name );
      tmp->length = length;
      tmp->attr = 0;
      tmp->size = 0;
      if( length > max_length )
	max_length = length;
      list = fsort_and_insert(list,tmp);
      nlists++;
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

  strcpy( buffer , p->name+common_length );

  while( (p=p->next) != NULL ){
    const char *q = p->name+common_length ;
    char *r=buffer;

    while( *r != '\0' ){
      if( _nls_is_dbcs_lead( *r & 255 ) ){
	/****** 倍角文字 ******/
	if( q[0] != r[0]  ||  q[1] != r[1] ){
	  *r = '\0';
	  break;
	}
	q += 2;
	r += 2;
      }else{
	/****** 半角文字 ******/
	if( toupper(*q & 255) != toupper(*r & 255) ){
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

#if 0

int main(int argc,char **argv)
{
  char dir[FILENAME_MAX],fn[FILENAME_MAX];

  for(int i=1;i<argc;i++){
    Complete complete; 

    int n=complete.makelist( argv[i] );
    const char *p=complete.nextchar();
    printf("[%s][%s] *%d\n",argv[i], p ,n );

    struct filelist *ptr=complete.findfirst();
    while( ptr!=NULL ){
      printf("[%s]\n",ptr->name);
      ptr =complete.findnext();
    }
    complete.cleanup();
  }
  return 0;
}

#endif
