#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ea.h>
#include <fnmatch.h>
#include <conio.h>
#include <io.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include "nyaos.h"
#include "complete.h"

extern volatile int ctrl_c;
extern int screen_width;
extern int screen_height;

enum{
  LS_MODE    = 0,
  DIR_MODE   = 1,
  EADIR_MODE = 2,
  INDEX_MODE = 3,

  PRINT_MASK = 3,

  MORE_MODE  = 4,
  COLOR_MODE = 8,
  HIDDEN_MODE= 16, /* HIDDEN属性も表示する。*/
};
int column=0;

int nprintlines=0;

void kill_filelist(struct filelist *p)
{
  while( p != NULL ){
    struct filelist *nxt = p->next;
    free(p);
    p = nxt;
  }
}

void more(int flag,FILE *fout)
{
  putc('\n',fout);
  if(   isatty(fileno(fout)) && (flag & MORE_MODE) 
     && ++nprintlines >= screen_height-1 ){

    fputs("\x1B[0;30;1;47m[more]\x1b[0;1;37m",fout);
    fflush(fout);
    (void)getch();
    fputs("\r      \r",fout);
    nprintlines=0;
  }
}


void dir1(struct filelist *flist,int max_length,int flag,FILE *fout)
{
  int tailchar = ' ';
  const char *headstr;

  char attrstr[]="-rw--";
  /*              drwxa 
   *              01234 */
  if( flist->name[0] == '.'  &&  (HIDDEN_MODE & flag)==0 )
    return;

  if( flist->attr & A_DIR ){
    headstr  = "\x1B[1;32m";
    attrstr[0] = 'd';
    tailchar = Complete::directory_split_char;
  }else if( flist->attr & A_HIDDEN ){
    if( (flag & HIDDEN_MODE)==0 )
      return;
    headstr  = "\x1B[1;31m";
  }else if( flist->attr & A_SYSTEM ){
    headstr  = "\x1B[1;31m";
  }else if( flist->attr & A_RONLY ){
    headstr  = "\x1B[1;33m";
    attrstr[2] = '-';
  }else if( flist->attr & A_LABEL ){
    headstr  = "\x1B[1;34m";
  }else if( fnmatch("*.EXE",flist->name,_FNM_IGNORECASE |_FNM_OS2 )==0
	   || fnmatch("*.COM",flist->name,_FNM_IGNORECASE|_FNM_OS2 )==0
	   || fnmatch("*.CMD",flist->name,_FNM_IGNORECASE|_FNM_OS2 )==0
	   || fnmatch("*.BAT",flist->name,_FNM_IGNORECASE|_FNM_OS2 )==0 ){
    headstr  = "\x1B[1;35m";
    tailchar = '*';
    attrstr[3] = 'x';
  }else{
    headstr  = "\x1B[1;37m";	
  }
  
  if( flist->attr & A_ARCHIVE )
    attrstr[4] = 'a';
  
  if( flag & COLOR_MODE )
    fputs("\x1B[0m",fout);

  int ncolumns=0;

  /* lsモードの時は、このブロックだけで return する */
  if( (flag & PRINT_MASK) == LS_MODE ){
    if( flag & COLOR_MODE )
      fputs(headstr,fout);
    
    int i=fprintf(fout,"%s%c ",
		 flist->name,
		 tailchar
		 );
    while( i < max_length+2 ){
      ++i;
      putc(' ',fout);
    }

    column += i;
#if 0  /* 縦型 ls にしてからは不要になった。*/
    if( column + max_length >= screen_width ){
      more(flag,fout);
      column = 0;
    }
#endif
    return;
  }

  if( (flag & PRINT_MASK) != INDEX_MODE ){
    ncolumns += fprintf(fout,"%s %10d %4d-%02d-%02d %02d:%02d:%02d ",
		       attrstr,
		       flist->size,
		       flist->d.year+1980,
		       flist->d.month,
		       flist->d.day,
		       flist->t.hour,
		       flist->t.minute,
		       flist->t.second
		       );
  }
  
  if( flag & COLOR_MODE )
    fputs( headstr , fout );

  ncolumns += fprintf(fout,"%s%c ",flist->name , tailchar );

  if( flag & COLOR_MODE )
    fputs("\x1B[0;37m",fout );

  if( (flag & PRINT_MASK)==DIR_MODE ){
    more(flag,fout);
    return;
  }

  struct _ea ea;
  union{
    void  *value;
    const char *byte;
    const unsigned short *word;
  }ptr;

  if( (flag & PRINT_MASK)==EADIR_MODE ){
    /* EAの LONGNAME を表示する */
    if( _ea_get( &ea , flist->name , 0 , ".LONGNAME" ) == 0
       && ea.size > 0  &&  ea.value != NULL  ){
      
      ptr.value = ea.value;
      int type = *ptr.word++;
      if( type == 0xFFFD ){
	int x,y;
	
	int size = *ptr.word++; /* 実際のサイズ */
	int n = 0;              /* ctrl-codeを ^N などと変形した後のサイズ*/
	char *s=(char*)alloca(size*2); /* 変形後の文字列が入る */

	for(int i=0 ; i<size ; i++ ){
	  if( *ptr.byte == '\r' ){
	    ptr.byte++;
	  }else if( *ptr.byte == '\n' ){
	    s[n++] = ' ';
	    ptr.byte++;
	  }else if( 0 <= *ptr.byte  && *ptr.byte < ' ' ){
	    if( flag & COLOR_MODE )
	      s[n++] = '^';
	    s[n++] = '@'+*ptr.byte++ ;
	  }else{
	    s[n++] = *ptr.byte++ ;
	  }
	}/* for(int i...) */
	s[n++] = '\0';
	
	int nspaces = screen_width - ncolumns - n ;
	while( nspaces-- > 0 )
	  putc( ' ' , fout );
	
	while( *s != '\0' )
	  putc( *s++ , fout );
      }
    }
    _ea_free( &ea );
    more(flag,fout);
  }
  
  /* EAのコメントを表示する */
  if(   (flag & PRINT_MASK)!=DIR_MODE
     && _ea_get( &ea , flist->name , 0 , ".COMMENTS" ) == 0
     && ea.size > 0  &&  ea.value != NULL ){
    
    ptr.value = ea.value;
    if( *ptr.word++ == 0xFFDF ){
      ptr.word++; /* code page は要らない */
      int nentries = *ptr.word++;
      while( nentries-- > 0 ){
	if( *ptr.word++ == 0xFFFD ){
	  int size = *ptr.word++;
	  if( (flag & PRINT_MASK)== INDEX_MODE &&  ncolumns < 8 )
	    putc('\t',fout);
	  putc('\t',fout);
	  while( size-- > 0 ){
	    putc( *ptr.byte++ , fout );
	  }
	  more(flag,fout);
	}else{
	  ptr.byte += (*ptr.word + 2);
	}
	if( (flag & PRINT_MASK) == INDEX_MODE ) break;
      }/* end while */
    }
  }else if( (flag & PRINT_MASK) == INDEX_MODE ){
    more(flag,fout);
  }
  _ea_free( &ea );
}

/* ファイル情報をstatで自前で調べる場合 */
void dir1(const char *filename,int max_length,int flag,FILE *fout)
{
  struct stat stbuf;
  if( stat( filename , &stbuf ) != 0 ){
    return;
  }

  int length=strlen(filename);

  struct filelist *flist=
    (struct filelist*)alloca(sizeof(struct filelist)+length);

  strcpy( flist->name , filename );

  flist->attr = stbuf.st_attr;
  flist->length = length;
  flist->size   = stbuf.st_size;

  struct tm *t=localtime(&stbuf.st_mtime);
  flist->d.year   = t->tm_year-80;
  flist->d.month  = t->tm_mon;
  flist->d.day    = t->tm_mday;
  flist->t.hour   = t->tm_hour;
  flist->t.minute = t->tm_min;
  flist->t.second = t->tm_sec;

  dir1(flist,max_length,flag,fout);
}

int is_file_print(struct filelist *f,int flag)
{
  if( flag & HIDDEN_MODE )
    return 1;
  if( f->name[0] == '.' || (f->attr & A_HIDDEN) )
    return 0;
  return 1;
}

int print_filelist(struct filelist *cur, int nlists,
		   int max_length ,int flag, FILE *fout)
{
  if( cur == NULL )
    return 0;

  if( (flag & PRINT_MASK)==LS_MODE ){

    int files_per_line   = (screen_width-1)/(max_length+2);
    int files_per_column = (nlists+files_per_line-1)/files_per_line; /* >= 1 */
    
    struct filelist **ptr =
      (struct filelist**)alloca(files_per_line*sizeof(struct filelist *));
    for(int i=0 ; i<files_per_line; i++ ){
      ptr[i] = NULL;
    }
    
    assert( ptr != NULL );
    
    while( cur != NULL && !is_file_print(cur,flag) )
      cur=cur->next;
    
    for(int i=0; i<files_per_line-1 && cur != NULL ; i++ ){
      ptr[i] = cur;
      for(int j=0 ; cur != NULL && j<files_per_column ; j++){
	cur = cur->next;      
	while( cur !=NULL && !is_file_print(cur,flag) )
	  cur=cur->next;
      }
    }
    ptr[files_per_line-1] = cur;
    
    for(int j=0; j<files_per_column ; j++ ){
      for(int i=0; i<files_per_line  &&  ptr[i] != NULL ; i++ ){
	if( ctrl_c )
	  return nlists;
	
	dir1(ptr[i], max_length , flag , fout );
	ptr[i] = ptr[i]->next;
	
	while( ptr[i] != NULL && !is_file_print(ptr[i],flag) )
	  ptr[i] = ptr[i]->next;
      }
      more(flag,fout);
      column=0;
    }
  }else{
    while( cur != NULL ){
      dir1(cur , max_length , flag , fout );
      cur=cur->next;
      if( ctrl_c )
	return nlists;
    }
  }
  return 0;
}


int the_dir(const char *dir,int flag , FILE *fout )
{
  DIR *dirp=opendir(dir);
  struct dirent *dirbuf;
  int nlists=0;
  int max_length=0;

  if( dirp == NULL )
    return 1;

  struct filelist *first=NULL;
  
  while( (dirbuf=readdir(dirp)) != NULL ){
    struct filelist *tmp;
    tmp = (struct filelist *)alloca(sizeof(struct filelist)+dirbuf->d_namlen );
    assert( tmp != NULL );

    strcpy( tmp->name , dirbuf->d_name );
    tmp->length = dirbuf->d_namlen;
    tmp->date   = dirbuf->d_date;
    tmp->time   = dirbuf->d_time;
    tmp->attr   = dirbuf->d_attr;
    tmp->size   = dirbuf->d_size;

    if( tmp->length > max_length )
      max_length = tmp->length;

    first = fsort_and_insert(first,tmp);

    if( is_file_print(tmp,flag) )
      nlists++;
  }
  closedir(dirp);

  column=0;
  if( nlists == 0 )
    return 0;

  print_filelist(first,nlists,max_length,flag,fout);

  column=0;
  return nlists;
}

void eadir1(const char *cmdname,const char *arg,int max_length,
	    int flag,FILE *fout)
{
  if( arg[1] == ':'  && arg[2] == '\0' ){
    static char arg_[]="?:.";
    arg_[0] = arg[0];
    arg = arg_;
  }
  struct stat stat_buffer;

  if( stat( arg , &stat_buffer ) != 0 ){
    fprintf(stderr,"%s: %s: no such file or directory\n",cmdname,arg );
  }else{
    if( stat_buffer.st_attr & A_DIR ){ /**** ディレクトリ名 ****/
      putc('\n',fout);
      if( flag & COLOR_MODE )
	fputs("\x1B[0m",fout);
      fputs(arg,fout);
      fputs(":\n",fout);
      
      int curdrv=_getdrive();
      if( arg[1]==':' )
	_chdrive(arg[0]);
      
      char curdir[FILENAME_MAX];
      getcwd( curdir , sizeof(curdir) );
      _chdir2( arg );
      
      the_dir(".",flag,fout);
      
      chdir( curdir );
      _chdrive( curdrv );
    }else{ /**** ファイル名 ****/
      dir1( arg , max_length , flag , fout);
    }
  }
}

int eadir( int argc, char **argv,FILE *fout=stdout)
{
  int flag=0;
  nprintlines=0;

  if( argv[0][0] == 'l' ){
    flag = LS_MODE;
  }else if( argv[0][0] == 'e' ){
    flag = EADIR_MODE;
  }else{
    flag = DIR_MODE;
  }
  if( isatty(fileno(fout) ) )
    flag |= COLOR_MODE;

  column=0;

  int filefault=0;
  int filecount=0;
  int dircount=0;
  int max_length=0;

  struct filelist *files=NULL;
  struct filelist *dirs =NULL;

  if( argc > 1 ){
    column=0;
    for( int i=1 ; i<argc ; i++ ){
      assert( argv[i] != NULL );

      /* オプション文字列 */
      if( argv[i][0] == '-' ){
	for( const char *p=&argv[i][1] ;  *p != '\0' ; p++ ){
	  switch( *p ){
	  case 'e':
	    flag = ((flag & ~PRINT_MASK) | EADIR_MODE );
	    break;
	  case 'l':
	    flag = ((flag & ~PRINT_MASK) | DIR_MODE );
	    break;
	  case '0':
	    flag = ((flag & ~PRINT_MASK) | INDEX_MODE );
	    break;
	  case 'p':
	    flag |= MORE_MODE;
	    break;
	  case 'a':
	    flag |= HIDDEN_MODE;
	    break;
	  case 'o':
	    flag &= ~COLOR_MODE;
	    break;
	  }/* end switch */
	}/* end for */

      }else{
	/* オプションでない文字列 ... ファイル名 */

	char **list=_fnexplode(argv[i]);
	if( list != NULL ){
	  for(char **ptr=list; *ptr != NULL ; ptr++ ){
	    struct stat stbuf;
	    int len=strlen(*ptr);
	    
	    if( stat( *ptr , &stbuf ) == 0 ){
	      struct filelist *node=
		(struct filelist*)alloca(sizeof(struct filelist)+len);
	      strcpy( node->name , *ptr );
	      node->attr   = stbuf.st_attr;
	      node->length = len;
	      node->size   = stbuf.st_size;
	      
	      if( stbuf.st_attr & A_DIR ){
		dirs  = fsort_and_insert(dirs ,node);
		dircount++;
	      }else{
		files = fsort_and_insert(files,node);
		if( len > max_length )
		  max_length = len;
		filecount++;
	      }
	    }else{
	      fprintf(stderr,"%s: no such file or directory.\n",argv[i]);
	      filefault++;
	    }
	  }
	  _fnexplodefree(list);
	}else{
	  struct stat stbuf;
	  int len=strlen(argv[i]);
	  char *fn=argv[i];

	  /*「ls A:」にも対応させるため、ドットを末尾に追加する。*/
	  if( argv[i][1]==':' && argv[i][2]=='\0' ){
	    static char drv[]="@:.";
	    drv[0]=argv[i][0];
	    fn = drv;
	  }

	  if( stat( fn , &stbuf ) == 0 ){
	    struct filelist *node=
	      (struct filelist*)alloca(sizeof(struct filelist)+len);
	    strcpy( node->name , argv[i] );
	    node->attr   = stbuf.st_attr;
	    node->length = len;
	    node->size   = stbuf.st_size;
	    
	    if( stbuf.st_attr & A_DIR ){
	      dirs  = fsort_and_insert(dirs ,node);
	      dircount++;
	    }else{
	      files = fsort_and_insert(files,node);
	      if( len > max_length )
		max_length = len;
	      filecount++;
	    }
	  }else{
	    fprintf(stderr,"%s: no such file or directory\n",argv[i]);
	    filefault++;
	  }
	}
      }/* argv loop */
    }
    if( ctrl_c ){
      fputs("\nCtrl-C Hit.\n",fout);
      ctrl_c = 0;
      signal(SIGINT,ctrl_c_signal);
      return 0;
    }
  }

  if( filecount > 0 || dircount > 0  ){
    /* ファイル名が指定された */
    if( filecount > 0 ){
      print_filelist( files , filecount , max_length , flag , fout );
      if( dircount > 0 )
	putc('\n',fout);
    }
    struct filelist *p=dirs;
    if( p != NULL ){
      for(;;){
	if( dircount+filecount > 1 )
	  fprintf(fout,"\x1b[0m%s : \n",p->name);
	
	the_dir( p->name , flag , fout );
	
	if( (p=p->next) == NULL ) break;
	
	putc('\n',fout);
      }
    }
    fprintf(fout,"\x1b[0m" );

  }else if( filefault <= 0 ){
    /* ファイル名が指定されていない ---> カレントディレクトリ */

    the_dir( "." , flag , fout );
    if( ctrl_c ){
      fputs("\nCtrl-C Hit.\n",fout);
      ctrl_c = 0;
      signal(SIGINT,ctrl_c_signal);
      return 0;
    }
  }
  if( flag & COLOR_MODE )
    fputs("\x1B[0m",fout);

  fflush(fout);
  return 0;
}
