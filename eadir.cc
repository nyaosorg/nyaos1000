/* eadir.cc $Id: eadir.cc 1.3 1997/08/15 19:24:11 kaoru Exp kaoru $
 *   color-ls や .COMMENT,.LONGNAME 表示機能付dir(eadir)
 *   を実際に実行するモジュール。
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ea.h>
#include <sys/nls.h>
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

static char *ls_left_code="\033[";
static char *ls_right_code="m";
static char *ls_end_code="\033[0m";

static char *ls_normal_file="1";	/* 白 */
static char *ls_directory="32;1";	/* 緑 */
static char *ls_system_file="31;1";	/* 青 */
static char *ls_read_only_file="33;1";	/* 黄 */
static char *ls_hidden_file="44;37;1";	/* 青地の白 */
static char *ls_executable_file="35;1"; /* 紫 */

static char *ls_comment="44;37;1";	/* 青地に白 */
static char *ls_longname="41;37;1";     /* 赤字に白 */

struct {
  const char *xx;
  char **where_to_code;
} ls_color_table[]= {
  { "lc",&ls_left_code },
  { "rc",&ls_right_code },
  { "ec",&ls_end_code },
  { "fi",&ls_normal_file },
  { "di",&ls_directory },
  { "sy",&ls_system_file },
  { "ro",&ls_read_only_file },
  { "hi",&ls_hidden_file },
  { "ex",&ls_executable_file },
  { "cm",&ls_comment} ,
  { "ln",&ls_longname} ,
};

void set_ls_color_table(const char *s)
{
  if( s==NULL )
    return;

  while( is_alpha(s[0]) && is_alpha(s[1]) && s[2]=='=' ){
    int s0=tolower(s[0] & 255) , s1=tolower(s[1] & 255 );
    s += 3;    
    for(int i=0;i<numof(ls_color_table);i++){
      if( s0==ls_color_table[i].xx[0]  &&  s1==ls_color_table[i].xx[1] ){
	char buffer[1024],*p=buffer;
	while( *s != ':' ){
	  assert( p < buffer+sizeof(buffer) );

	  if( *s == '\0' || *s == '\n' ){
	    *p = '\0';
	    if( buffer[0] != '\0' )
	      *ls_color_table[ i ].where_to_code = strdup( buffer );
	    else
	      *ls_color_table[ i ].where_to_code = "";
	    return;
	    
	  }else if( *s == '\\' ){ 
	    if( *++s == 'e' || *s=='E' ){ /* "\e"形式 */
	      s++;
	      *p++ = '\x1b';
	    }else if( '0' <= *s && *s < '8' ){ /* "\033" : 8進形式 */
	      int n=0,j=1;
	      do{
		n = (n*8) + (*s-'0');
	      }while( '0' <= *++s && *s < '8' && ++j <= 3 );
	      *p++ = n;
	    }else if( *s=='x' ){  /* "\x1b": 16進形式 */
	      int n=0,j=0;
	      while( is_xdigit(*++s) && ++j <= 3 ){
		n *= 16;
		if( is_lower(*s) )
		  n += (*s-'a'+10);
		else if( is_upper(*s) )
		  n += (*s-'A'+10);
		else
		  n += (*s-'0');
	      }
	      *p++ = n;
	    }

	  }else{ /* 普通の文字コ－ド */
	    *p++ = *s++;
	  }
	}
	*p = '\0';
	s++;     /* skip ':' */
	if( buffer != '\0' )
	  *ls_color_table[ i ].where_to_code = strdup( buffer );
	else
	  *ls_color_table[ i ].where_to_code = "";

	goto next_colomn;
      }/* endif hit! */
    }/* 検索ル－プ */
    printf("LS_COLORS: %c%c: Bad code name\n",s0,s1);
    return;
  next_colomn:
    ;
  }/* : で区切られたル－プ */
}

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
  if(   (flag & COLOR_MODE)  &&  (flag & MORE_MODE) 
     && ++nprintlines >= screen_height-1 ){

    fprintf(fout,"%s[more]",ls_end_code);
    fflush(fout);
    (void)getch();
    fputs("\r      \r",fout);
    nprintlines=0;
  }
}

int fnexplode2(struct filelist *&list   , int &count ,
	       struct filelist *&dirlist, int &dircount ,
	       int &max_length ,
	       const char *path )
{
  char dir[ FILENAME_MAX ]    , *dir_p    = dir;
  char rawdir[ FILENAME_MAX ] , *rawdir_p = rawdir;
  char fname[ FILENAME_MAX ]  , *fname_p  = fname;
  
  const char *lastroot=NULL;
  if( path[0]=='~' ){
    lastroot = path;
  }
  for(const char *sp=path ; *sp != '\0' ; sp++ ){
    if( is_kanji(*sp) ){
      ++sp;
    }else if( *sp=='\\' || *sp=='/' || *sp==':' ){
      lastroot = sp;
    }
  }
  
  const char *p=path;
  if( lastroot != NULL ){
    while( p <= lastroot ){
      *rawdir_p++ = *p;
      *dir_p++ = *p++;
    }
  }
  /* '.'を付けることで 末尾が ':','/'でも有効に働く (^_^) */
  *dir_p++ = '.';
  *dir_p   = '\0';
  *rawdir_p = '\0';

  while( *p != '\0' )
    *fname_p++ = *p++;
  *fname_p = '\0';

  if( fname[0] == '\0' ){
    struct filelist *tmp =
      (struct filelist *)malloc(sizeof(struct filelist)	+ (dir_p-dir) );
    const char *sp=dir;
    char *dp=tmp->name;
    while( sp < dir_p )
      *dp++ = *sp++;
    *dp = '\0';
    dirlist = fsort_and_insert(dirlist,tmp);
    dircount++;
    return 0;
  }

  DIR *dirp=opendir(dir);
  if( dirp == NULL )
    return -1;
  
  struct dirent *dirbuf;
  while( (dirbuf=readdir(dirp)) != NULL ){
    if( _fnmatch( fname , dirbuf->d_name , _FNM_OS2 | _FNM_IGNORECASE )!=0 )
      continue;

    struct filelist *tmp =
      (struct filelist *)malloc(sizeof(struct filelist)
				+ dirbuf->d_namlen
				+ (rawdir_p-rawdir)
				);
    char *dp=tmp->name;
    /** ディレクトリ部をコピ－ **/
    const char *sp=rawdir;
    while( sp < rawdir_p )
      *dp++ = *sp++;

    /** ファイル名部をコピ－ **/
    sp = dirbuf->d_name;
    for(int i=0; i<dirbuf->d_namlen ; i++ )
      *dp++ = *sp++;
    *dp = '\0';
    
    tmp->length = dirbuf->d_namlen + (rawdir_p-rawdir) ;

    tmp->date   = dirbuf->d_date;
    tmp->time   = dirbuf->d_time;
    tmp->attr   = dirbuf->d_attr;
    tmp->size   = dirbuf->d_size;

    if( tmp->attr & A_DIR ){
      dirlist = fsort_and_insert(dirlist,tmp);
      dircount++;
    }else{
      list = fsort_and_insert(list,tmp);
      count++;
      if( tmp->length > max_length )
	max_length = tmp->length;
    }
  }
  closedir(dirp);
  return 0;
}

void dir1(struct filelist *flist,int max_length,int flag,FILE *fout)
{
  int tailchar = ' ';
  const char *headstr;

  char attrstr[]="-rw--";
  /*              drwxa 
   *              01234 */

  const char *top=flist->name;
  for(const char *p=flist->name ; *p != '\0' ; p++ ){
    if( *p == '\\' || *p == '/' )
      top = p+1 ;
  }
  if( (HIDDEN_MODE & flag)==0  &&  *top=='.' )
    return;

  if( flist->attr & A_DIR ){
    headstr = ls_directory;
    attrstr[0] = 'd';
    tailchar = Complete::directory_split_char;
  }else if( flist->attr & A_HIDDEN ){
    if( (flag & HIDDEN_MODE)==0 )
      return;
    headstr = ls_hidden_file;
  }else if( flist->attr & A_SYSTEM ){
    headstr = ls_system_file;
  }else if( flist->attr & A_RONLY ){
    headstr = ls_read_only_file;
    attrstr[2] = '-';
  }else if( flist->attr & A_LABEL ){
    headstr = ls_system_file;
  }else if( which_suffix(flist->name,"EXE","COM","CMD","BAT",NULL) != 0 ){
    headstr = ls_executable_file;
    tailchar = '*';
    attrstr[3] = 'x';
  }else{
    headstr = ls_normal_file;
  }
  
  if( flist->attr & A_ARCHIVE )
    attrstr[4] = 'a';
  
  if( flag & COLOR_MODE )
    fputs(ls_end_code,fout);


  int ncolumns=0;

  /* lsモードの時は、このブロックだけで return する */
  if( (flag & PRINT_MASK) == LS_MODE ){
    if( flag & COLOR_MODE )
      fprintf(fout,"%s%s%s",ls_left_code,headstr,ls_right_code);
    
    fputs(flist->name , fout );
    if( flag & COLOR_MODE )
      fputs(ls_end_code,fout);
    putc(tailchar,fout);

    int i=strlen(flist->name);
    while( i < max_length+1 ){
      ++i;
      putc(' ',fout);
    }
    column += i;
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
		       flist->t.second*2
		       );
  }
  
  if( flag & COLOR_MODE ){
    fprintf(fout,"%s%s%s",ls_left_code,headstr,ls_right_code);
  }
  ncolumns += fprintf(fout,"%s",flist->name,fout);
  if( flag & COLOR_MODE )
    fputs(ls_end_code,fout);
  
  putc(tailchar,fout);
  ncolumns++;

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
	if( nspaces < 0 ){
	  putc( '\n' , fout );
	  nspaces = screen_width - n;
	}

	while( nspaces-- > 0 )
	  putc( ' ' , fout );
	
	if( flag & COLOR_MODE )
	  fprintf(fout,"%s%s%s%s%s",
		  ls_left_code,ls_longname,ls_right_code,s,ls_end_code);
	else
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
	  if( flag & COLOR_MODE )
	    fprintf(fout,"%s%s%s",ls_left_code,ls_comment,ls_right_code);
	  while( size-- > 0 ){
	    putc( *ptr.byte++ , fout );
	  }
	  if( flag & COLOR_MODE )
	    fputs(ls_end_code,fout);
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

int is_file_print(struct filelist *f,int flag)
{
  if( flag & HIDDEN_MODE )
    return 1;
  const char *top=f->name;
  for(const char *p=f->name ; *p != '\0' ; p++ ){
    if( *p=='/' || *p=='\\' )
      top=p+1;
  }
  if( *top == '.' || (f->attr & A_HIDDEN) )
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

  if( flag & EADIR_MODE ){
    char cwd[FILENAME_MAX];

    _getcwd2(cwd,sizeof(cwd));
    _chdir2( dir );
    print_filelist(first,nlists,max_length,flag,fout);
    _chdir2( cwd );

  }else{
    print_filelist(first,nlists,max_length,flag,fout);
  }

  column=0;
  return nlists;
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

  set_ls_color_table( getenv("LS_COLORS") );

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
#if 1
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
	      
	      struct tm *tmbuf=localtime(&stbuf.st_mtime);
	      node->t.second = tmbuf->tm_sec/2;   /* 0..59 --> 0..29  */
	      node->t.minute = tmbuf->tm_min;     /* 0..59  */
	      node->t.hour   = tmbuf->tm_hour;    /* 0..23  */
	      node->d.day    = tmbuf->tm_mday;    /* 1..31  */
	      node->d.month  = tmbuf->tm_mon+1;   /* 0..11 --> 1..12   */
	      node->d.year   = tmbuf->tm_year-80; /* 0:1900 --> 0:1980 */
	      
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
	      
	    struct tm *tmbuf=localtime(&stbuf.st_mtime);
	    node->t.second = tmbuf->tm_sec/2;   /* 0..59  --> 0..29 */
	    node->t.minute = tmbuf->tm_min;     /* 0..59  */
	    node->t.hour   = tmbuf->tm_hour;    /* 0..23  */
	    node->d.day    = tmbuf->tm_mday;    /* 1..31  */
	    node->d.month  = tmbuf->tm_mon+1;   /* 0..11  --> 1..12  */
	    node->d.year   = tmbuf->tm_year-80; /* 0:1900 --> 0:1980 */
	    
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
#else
      }else if(fnexplode2(files, filecount, dirs, dircount,
			  max_length,argv[i]) !=0 ){
	/* オプションでない文字列 ... ファイル名 */
	fprintf(stderr,"%s: no such file or directory.\n",argv[i]);
	filefault++;
      }/* argv loop */
#endif
    }/* if( argc > 1 ) */
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
      assert( files != NULL );

      /* dotfile や Hidden属性があっても、直接コマンドラインで指定しているの
       * だから、表示させる 
       */
      print_filelist( files , filecount , max_length 
		     , flag | HIDDEN_MODE , fout );

      if( dircount > 0 )
	putc('\n',fout);
    }
    struct filelist *p=dirs;
    if( p != NULL ){
      for(;;){
	if( dircount+filecount > 1 ){
	  if( flag & COLOR_MODE )
	    fprintf(fout,"%s%s : \n",ls_end_code,p->name);
	  else
	    fprintf(fout,"%s : \n",p->name);
	}
	
	the_dir( p->name , flag , fout );
	
	if( (p=p->next) == NULL ) break;
	
	putc('\n',fout);
      }
    }

    if( isatty(fileno(fout)) )
      fputs( ls_end_code , fout );

  }else if( filefault <= 0 ){
    /* ファイル名が指定されていない ---> カレントディレクトリ */

    the_dir( "." , flag , fout );
    if( ctrl_c ){
      fputs("\nCtrl-C Hit.\n",stderr);
      ctrl_c = 0;
      signal(SIGINT,ctrl_c_signal);
      return 0;
    }
  }
  if( (flag & COLOR_MODE) && isatty(fileno(fout)) )
    fputs( ls_end_code ,fout);

  fflush(fout);
  return 0;
}
