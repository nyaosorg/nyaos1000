/* eadir.cc $Id: eadir.cc 1.3 1997/08/15 19:24:11 kaoru Exp kaoru $
 *   color-ls や .COMMENT,.LONGNAME 表示機能付dir(eadir)
 *   を実際に実行するモジュール。
 */

#include <process.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ea.h>
#include <conio.h>
#include <io.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include <sys/video.h>	/* 3行スクロールモード用 */

#include "SmartPtr.h"

#define INCL_DOSNLS
#include "nyaos.h"
#include "complete.h"
#include "finds.h"

// #define INCL_VIO
//   #include <os2.h>

extern volatile int ctrl_c;
extern int screen_width;
extern int screen_height;

enum{
  FLAG__LONG ,		/* 0:普通のls   1: ls -l */
  FLAG__MORE ,
  FLAG__COLOR ,
  FLAG__PRINT_HIDDEN ,
  FLAG__RECURSIVE ,
  FLAG__IGNORE_UNDERBAR ,
  FLAG__IGNORE_BACKUP ,
  FLAG__LAST_COLUMN ,
  FLAG__COMMENT ,	/* 0:表示しない  1:inline  2:multi-line */
  FLAG__LONGNAME ,	/* 0:表示しない  1:左よせ  2:右よせ */
  NUM_FLAGS ,
};
static char ls_flag[ NUM_FLAGS ];

enum{
  LS_MODE		= 0x0 ,
  DIR_MODE		= 0x1 ,
  EADIR_MODE		= 0x2 ,
  INDEX_MODE		= 0x3 ,
  INLINE_COMMENT_MODE	= 0x4 ,
  PRINT_MASK		= 0x7 ,

  MORE_MODE		= 0x8 ,
  COLOR_MODE		= 0x10 ,
  HIDDEN_MODE		= 0x20 ,	/* HIDDEN属性も表示する。*/
  HALF_MODE		= 0x40 ,
  IGNORE_BACKUP		= 0x80 ,
  RECURSIVE_MODE	= 0x100 ,
  IGNORE_UNDERBAR	= 0x200 ,	/* _で始まるファイルを表示しない */
  LAST_COLUMNS		= 0x400 ,

  SORT_MODES		= 16 ,		/* bit */
};

static char *ls_left_code="\033[";
static char *ls_right_code="m";
static char *ls_end_code="\033[0m";

static char *ls_normal_file="1";	/* 白 */
static char *ls_directory="32;1";	/* 緑 */
static char *ls_system_file="31;1";	/* 青 */
static char *ls_read_only_file="43;";	/* 背景が黄 */
static char *ls_hidden_file="44;37;1";	/* 青地の白 */
static char *ls_executable_file="35;1"; /* 紫 */

static char *ls_comment="44;37;1";	/* 青地に白 */
static char *ls_longname="41;37;1";     /* 赤字に白 */

static int thisyear=0;

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

union MultiPtr {
  void *value;
  const char *byte;
  const unsigned short *word;
};

void free_pointors(char **table)
{
  while( *table != NULL )
    free( *table++ );
}

char **get_ea_comments( const char *fname )
{
  struct _ea ea;
  union MultiPtr ptr;

  if(   _ea_get( &ea , fname , 0 , ".COMMENTS" ) != 0
     || ea.size <= 0 || ea.value == NULL )
    return NULL;

  ptr.value = ea.value;
  if( *ptr.word++ != 0xFFDF ){
    _ea_free( &ea );
    return NULL;
  }
  ptr.word++; /* コードページを読みとばす */
  
  int n=*ptr.word++;
  char **table=(char**)malloc( sizeof(char*) * (n+1) );
  if( table == NULL ){
    _ea_free( &ea );
    return NULL;
  }

  for(int i=0;i<n;i++){
    ++ptr.word; /* ASCII を表す 0xFFFD を読みとばす */
    int size=*ptr.word++;
    char *dp = table[i] = (char*)malloc( size+1 );
    while( size-- > 0 )
      *dp++ = *ptr.byte++;
    *dp = '\0';
  }
  table[n] = NULL;
  _ea_free( &ea );
  return table;
}


void set_ls_color_table(const char *s)
{
  if( s==NULL )
    return;

  while( is_alpha(s[0]) && is_alpha(s[1]) && s[2]=='=' ){
    int s0=tolower(s[0] & 255) , s1=tolower(s[1] & 255 );
    s += 3;    
    for(size_t i=0;i<numof(ls_color_table);i++){
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

extern "C" {
  unsigned short VioGetCurPos(unsigned short *pusRow ,
			      unsigned short *pusColumn ,
			      unsigned short hvio );
}

static int dbcs_fputs(const char *s,FILE *fout)
{
  int i=0;
#if 0
  ULONG CpList[8];
  ULONG CpSize;
  
  if( DosQueryCp(sizeof(CpList),CpList,&CpSize)==0 && CpList[0] == 932 ){
#endif
    while( *s != '\0' ){
      putc( *s++ , fout );
      i++;
    }
#if 0
  }else{
    while( *s != '\0' ){
      if( *s & ~127 ){
	putc( '?' , fout);
      }else{
	putc( *s , fout );
      }
      ++s;
      i++;
    }
  }
#endif
  return i;
}

static void more(int flag,FILE *fout)
{
  if( (flag & HALF_MODE)!=0 && (fout==stdout || fout==stderr) ){
    fflush(fout);
    USHORT X,Y;
    VioGetCurPos(&Y,&X ,0 );
    if( Y >= screen_height-1 ){
      v_scroll(0,0,screen_width-1,Y,3,V_SCROLL_UP);
      fputs("\x1B[3A",fout);
    }
  }
  putc('\n',fout);
  if(   (flag & COLOR_MODE)  &&  (flag & MORE_MODE) 
     && ++nprintlines >= screen_height-1 ){

    fprintf(fout,"%s[more]",ls_end_code);
    fflush(fout);
    raw_mode();
    if( getkey() == ('C' & 0x1F) )
      ctrl_c = 1;
    cocked_mode();
    fputs("\r      \r",fout);
    nprintlines=0;
  }
}

static void smart_copy( SmartPtr &dp , const char *sp )
{
  while( *sp != '\0' )
    *dp++ = *sp++;
  *dp = '\0';
}

void dir1(struct filelist *flist,int max_length,int flag,FILE *fout)
{
  int tailchar = ' ';

  char headstr[ 128 ];
  SmartPtr headstrp(headstr,sizeof(headstr));

  char attrstr[]="-rw---";
  /*              drwxae 
   *              012345 */

  const char *top=flist->name;
  for(const char *p=flist->name ; *p != '\0' ; p++ ){
    if( *p == '\\' || *p == '/' )
      top = p+1 ;
  }

  /* ドットで始まるもの */
  if( (HIDDEN_MODE & flag)==0  &&  *top=='.' )
    return;

  /* アンダーラインで始まるもの */
  if( (flag & IGNORE_UNDERBAR)!=0  &&  *top=='_' )
    return;

  /* チルダで終わるもの */
  if( (flag & IGNORE_BACKUP) != 0  && flist->name[flist->length-1] == '~' )
    return;

  if( flist->attr & A_RONLY ){
    smart_copy( headstrp , ls_read_only_file );
    attrstr[2] = '-';
  }

  if( flist->attr & A_DIR ){
    smart_copy( headstrp , ls_directory );
    attrstr[0] = 'd';
    tailchar = '/';
  }else if( flist->attr & A_HIDDEN ){
    if( (flag & HIDDEN_MODE)==0 )
      return;
    for(const char *sp=ls_hidden_file ; *sp != '\0' ; sp++ )
      *headstrp++ = *sp;
    smart_copy( headstrp , ls_hidden_file );
  }else if( flist->attr & A_SYSTEM ){
    smart_copy( headstrp , ls_system_file );
  }else if( flist->attr & A_LABEL ){
    smart_copy( headstrp , ls_system_file );
  }else if( which_suffix(flist->name,"EXE","COM","CMD","BAT",NULL) != 0 ){
    smart_copy( headstrp , ls_executable_file );
    tailchar = '*';
    attrstr[3] = 'x';
  }else{
    smart_copy( headstrp , ls_normal_file );
  }

  
  if( flist->attr & A_ARCHIVE )
    attrstr[4] = 'a';

  if( flist->easize > 4 ) /* EA がなくても、サイズ情報で最低 4bytes は要る */
    attrstr[5] = 'e';
  
  if( flag & COLOR_MODE )
    fputs(ls_end_code,fout);

  int ncolumns=0;

  /* ls モードの時は、このブロックだけで return する */
  if( (flag & PRINT_MASK) == LS_MODE ){
    if( flag & COLOR_MODE )
      fprintf(fout,"%s%s%s",ls_left_code,headstr,ls_right_code);
    
    dbcs_fputs(flist->name,fout);
    /* fputs(flist->name , fout ); */
    if( flag & COLOR_MODE )
      fputs(ls_end_code,fout);
    putc(tailchar,fout);

    int i=strlen(flist->name);
    if( (flag & LAST_COLUMNS)==0 ){
      while( i < max_length+1 ){
	++i;
	putc(' ',fout);
      }
    }
    column += i;
    return;
  }

  if( (flag & PRINT_MASK) != INDEX_MODE ){
    static const char *month[]={
      "Jan","Feb","Mar","Apr","May","Jun",
      "Jul","Aug","Sep","Oct","Nov","Dec",
    };
    
    ncolumns += fprintf(fout,"%s %8ld %3s %2d "
			, attrstr
			, flist->size
			, month[ flist->d.month-1 ]
			, flist->d.day
			);

    if( flist->d.year+1980 != thisyear ){
      ncolumns += fprintf(fout," %4d " ,flist->d.year+1980 );
    }else{
      ncolumns += fprintf(fout,"%02d:%02d " ,flist->t.hour ,flist->t.minute );
    }

#if 0
    ncolumns += fprintf(fout,"%s %8ld %4d-%02d-%02d %02d:%02d "
			, attrstr
			, flist->size
			/* , flist->easize */
			, flist->d.year+1980
			, flist->d.month
			, flist->d.day
			, flist->t.hour
			, flist->t.minute
			/* , flist->t.second*2 */
			);
#endif
  }
  
  if( flag & COLOR_MODE ){
    fprintf(fout,"%s%s%s",ls_left_code,headstr,ls_right_code);
  }
  ncolumns += dbcs_fputs(flist->name,fout);
  /* ncolumns += fprintf(fout,"%s",flist->name); */
  if( flag & COLOR_MODE )
    fputs(ls_end_code,fout);
  
  putc(tailchar,fout);
  ncolumns++;

  if( (flag & PRINT_MASK)==DIR_MODE ){
    more(flag,fout);
    return;
  }

  struct _ea ea;
  union MultiPtr ptr;

  if(  (flag & PRINT_MASK)==EADIR_MODE ){
    /* EAの LONGNAME を表示する */
    if(   flist->easize > 4 
       &&_ea_get( &ea , flist->name , 0 , ".LONGNAME" ) == 0 ){
      if( ea.size > 0  &&  ea.value != NULL  ){
      
	ptr.value = ea.value;
	int type = *ptr.word++;
	if( type == 0xFFFD ){
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
	    more(flag,fout);
	    nspaces = screen_width - n;
	  }
	  
	  while( nspaces-- > 0 )
	    putc( ' ' , fout );
	  
	  if( flag & COLOR_MODE ){
	    fputs( ls_left_code , fout );
	    fputs( ls_longname , fout );
	    fputs( ls_right_code , fout );
	    dbcs_fputs( s , fout );
	    fputs( ls_end_code , fout );
	  }else{
	    while( *s != '\0' )
	      putc( *s++ , fout );
	  }
	}
	_ea_free( &ea );
      }
    }
  }

  char **comments;

  /* EAのコメントを表示する */
  if( flist->easize > 4 && (flag & PRINT_MASK)!=DIR_MODE
     && (comments=get_ea_comments(flist->name)) != NULL ){

    switch( flag & PRINT_MASK ){
    case INLINE_COMMENT_MODE:
      if( flag & COLOR_MODE )
	fprintf(fout," .. %s%s%s%s%s"
		,ls_left_code,ls_comment,ls_right_code
		,comments[0],ls_end_code);
      else
	fprintf(fout," .. %s",comments[0]);
      break;
      
    case EADIR_MODE:
      for(char **pp=comments;*pp != NULL;pp++){
	more(flag,fout);
	if( flag & COLOR_MODE )
	  fprintf(fout,"\t%s%s%s%s%s"
		  ,ls_left_code,ls_comment,ls_right_code
		  ,*pp,ls_end_code);
	else
	  fprintf(fout,"\t%s",*pp);
      }
      break;

    case INDEX_MODE:
      if( flag & COLOR_MODE )
	fprintf(fout,"\t%s%s%s%s%s"
	       ,ls_left_code,ls_comment,ls_right_code
	       ,comments[0],ls_end_code);
      else
	fprintf(fout,"\t%s",comments[0]);
      break;
    }
    free_pointors( comments );
  }
  more(flag,fout);
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
  if( f->name[f->length-1] == '~' && (flag & IGNORE_BACKUP) )
    return 0;
  if( *top == '_' && (flag & IGNORE_UNDERBAR) )
    return 0;
  return 1;
}

int print_filelist(struct filelist *cur, int nlists,
		   int max_length ,int flag, FILE *fout)
{
  if( cur == NULL )
    return 0;

  /* 「-l」が無い場合の ls */
  if( (flag & PRINT_MASK)==LS_MODE ){
    int files_per_line;
    if( screen_width-1 < max_length+2 || !isatty(fileno(fout)) )
      files_per_line = 1;
    else
      files_per_line = (screen_width-1)/(max_length+2);
    
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
	
	dir1(ptr[i], max_length 
	     , (i+1)==files_per_line || ptr[i+1] == NULL
	     ? (flag | LAST_COLUMNS) : flag 
	     , fout );
	ptr[i] = ptr[i]->next;
	
	while( ptr[i] != NULL && !is_file_print(ptr[i],flag) )
	  ptr[i] = ptr[i]->next;
      }
      more(flag,fout);
      column=0;
    }
  }else{
    /* -l モード */
    while( cur != NULL ){
      dir1(cur , max_length , flag , fout );
      cur=cur->next;
      if( ctrl_c )
	return nlists;
    }
  }
  return 0;
}

int the_dir(const char *dirname,int flag , FILE *fout )
{
  int nlists=0;
  int max_length=0;
  struct filelist *first=NULL;
  struct filelist *dirlist=NULL;
  
  for(Dir dir(dirname) ; dir != NULL ; ++dir ){
    struct filelist *tmp;
    int alcsiz=sizeof(struct filelist)+dir.get_name_length();

    tmp = (struct filelist *)alloca( alcsiz );
    assert( tmp != NULL );

    strcpy( tmp->name , dir.get_name() );
    tmp->length = dir.get_name_length();
    tmp->date   = dir.get_last_write_date_by_short();
    tmp->time   = dir.get_last_write_time_by_short();
    tmp->attr   = dir.get_attr();
    tmp->size   = dir.get_size();
    tmp->easize = dir.get_easize();

    if( tmp->length > max_length )
      max_length = tmp->length;
    
    if( is_file_print(tmp,flag) ){
      if( (flag & RECURSIVE_MODE)!=0  &&  (tmp->attr & A_DIR )!= 0 
	 && tmp->name[0] != '.' ){
	struct filelist *tmp2=(struct filelist*)alloca( alcsiz );
	memcpy( tmp2 , tmp , alcsiz );
	dirlist = fsort_and_insert(dirlist,tmp2,NULL,flag >> SORT_MODES );
      }

      first = fsort_and_insert(first,tmp,NULL,flag >> SORT_MODES );
      nlists++;
    }
  }

  column=0;
  if( nlists == 0 )
    goto next;

  if( flag & EADIR_MODE ){
    char cwd[FILENAME_MAX];

    _getcwd2(cwd,sizeof(cwd));
    _chdir2( dirname );
    _rfnlwr();
    print_filelist(first,nlists,max_length,flag,fout);
    _chdir2( cwd );
    _rfnlwr();

  }else{
    print_filelist(first,nlists,max_length,flag,fout);
  }

  column=0;

 next:
  while( dirlist != NULL  &&  ctrl_c == 0 ){
    char fullpathbuffer[512];
    char *fullpath;
    if( dirname[0] == '.' && dirname[1] == '\0' )
      fullpath = dirlist->name;
    else
      sprintf(fullpath=fullpathbuffer,"%s/%s",dirname,dirlist->name);

    if( flag & COLOR_MODE ){
      fprintf( fout, "\n%s%s:\n",ls_end_code , fullpath );
    }else{
      more(flag,fout);
      dbcs_fputs(fullpath,fout);
      putc(':',fout);
      more(flag,fout);
      /* fprintf( fout, "\n%s:\n", fullpath ); */
    }

    the_dir( fullpath , flag , fout );
    
    dirlist = dirlist->next;
  }
  return nlists;
}

static int exit_with_ctrl_c()
{
  fputs("\n^C\n",stderr);
  ctrl_c = 0;
  signal(SIGINT,ctrl_c_signal);
  return RC_ABORT;
}

int call_original_ls( char **argv,FILE *fout=stdout)
{
  int rc;
  if( fout != stdout ){
    int org_stdout=dup(1);
    dup2(fileno(fout),1);
    rc=spawnvp(P_WAIT,"ls.exe",argv);
    close(1);
    dup2(org_stdout,1);
    close(org_stdout);
  }else{
    rc=spawnvp(P_WAIT,"ls.exe",argv);
  }
  return rc;
}

/* DosQueryPathInfo のフィルター */

int dos_query_path_info(const char *name,int len,FILESTATUS4 &buffer )
{
  char *new_name = (char*)alloca( len + 2 );

  const char *sp=name;
  char *dp=new_name;

  int lastchar=0;

  while( *sp != '\0' ){
    if( *sp == '/' ){
      lastchar = *dp++ = '\\';
      ++sp;
    }else{
      lastchar = *dp++ = *sp++;
    }
  }
  if( lastchar == '\\' || lastchar == ':' )
    *dp++ = '.';
  *dp = '\0';

  return DosQueryPathInfo((PUCHAR)new_name,2,&buffer,sizeof(FILESTATUS4) );
}

int eadir( int argc, char **argv,FILE *fout=stdout)
{
  time_t now;
  struct tm *date;

  time(&now);
  thisyear = localtime(&now)->tm_year + 1900;

  int rc=0;
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

  for( int i=1 ; i<argc ; i++ ){
    assert( argv[i] != NULL );
    
    /* オプション文字列 */
    if( argv[i][0] == '-' ){
      /* long option */
      if( argv[i][1] == '-' ){
	struct longoption_tg {
	  const char *name;
	  int andmask , ormask;
	} option_table[]={
	  { "inline-comment"	, ~PRINT_MASK	, INLINE_COMMENT_MODE },
	  { "multi-comment"	, ~PRINT_MASK	, EADIR_MODE },
	  { "index-comment"	, ~PRINT_MASK	, INDEX_MODE },
	  { "all"		, ~0	, HIDDEN_MODE },
	  { "ignore-backups"	, ~0	, IGNORE_BACKUP },
	  { "ignore-underbar"	, ~0	, IGNORE_UNDERBAR },
	  { "color"		, ~0	, COLOR_MODE },
	  { "no-color"		, ~COLOR_MODE , 0 },
	  { "reverse"		, ~0	, SORT_REVERSE<<SORT_MODES },
	  { "more"		, ~0	, MORE_MODE },
	  { "virge"		, ~0	, HALF_MODE },
	  { "numeric-sort"	, ~0	, SORT_BY_NUMERIC << SORT_MODES },
	  { "classify"		, ~0	, 0 },
	  { NULL , ~0 , 0 },
	};

	for(struct longoption_tg *op=option_table
	    ; op->name != NULL ; op++){

	  if( *op->name==argv[i][2]  &&  strcmp(op->name+1,&argv[i][3])==0 ){
	    flag = (flag & op->andmask) | op->ormask ;
	    break;
	  }
	}


      }else for( const char *p=argv[i]+1 ; *p != '\0' ; p++ ){
	switch( *p ){
	  /* ------- 互換オプション ------ */
	case 'a':
	  flag |= HIDDEN_MODE;
	  break;
	case 'l':
	  flag = ((flag & ~PRINT_MASK) | DIR_MODE );
	  break;
	case 'R':
	  flag |= RECURSIVE_MODE;
	  break;

	  /* ------- ソートオプション ------- */
	case '2':
	  flag |= (SORT_BY_NUMERIC << SORT_MODES );
	  break;
	case 'c':
	  flag |= (SORT_BY_CHANGE_TIME << SORT_MODES);
	  break;
	case 'S':
	  flag |= (SORT_BY_SIZE << SORT_MODES);
	  break;
	case 'u':
	  flag |= (SORT_BY_LAST_ACCESS_TIME << SORT_MODES);
	  break;
	case 'X':
	  flag |= (SORT_BY_SUFFIX << SORT_MODES);
	  break;
	case 'U':
	  flag |= (UNSORT << SORT_MODES);
	  break;
	case 'r':
	  flag |= (SORT_REVERSE << SORT_MODES);
	  break;
	case 't':
	  flag |= (SORT_BY_MODIFICATION_TIME << SORT_MODES);
	  break;

	case '_':
	  flag |= IGNORE_UNDERBAR;
	  break;
	  
	case 'B':
	  flag |= IGNORE_BACKUP;
	  break;
	case 'E':
	  flag = ((flag & ~PRINT_MASK) | EADIR_MODE );
	  break;
	case '0':
	  flag = ((flag & ~PRINT_MASK) | INDEX_MODE );
	  break;

	case 'P':
	  /* case 'p': */
	  flag |= MORE_MODE;
	  break;
	case 'O':
	  flag &= ~COLOR_MODE;
	  break;
	case 'o':
	  flag |= COLOR_MODE;
	  break;
	case '3':
	  flag |= HALF_MODE;
	  break;
	case 'H':
	  flag = ((flag & ~PRINT_MASK)|INLINE_COMMENT_MODE);
	  break;

	case 'F':
	  break;

	default:
	  call_original_ls( argv , fout );
	  return rc;
	}/* end switch */
      }/* end for */

    }else{
      /* オプションでない文字列 ... ファイル名 */
      char **list=fnexplode2(argv[i]);
      if( list != NULL ){
	for(char **ptr=list; *ptr != NULL ; ptr++ ){
	  
	  /* struct stat stbuf; */
	  int len=strlen(*ptr);
	  FILESTATUS4 buffer;
	  
	  if( dos_query_path_info(*ptr,len,buffer) == 0 ){
	    struct filelist *node=
	      (struct filelist*)alloca(sizeof(struct filelist)+len);
	    strcpy( node->name , *ptr );
	    node->attr   = buffer.attrFile;
	    node->length = len;
	    node->size   = buffer.cbFile;
	    node->time   = *(unsigned short *)&buffer.ftimeLastWrite;
	    node->date   = *(unsigned short *)&buffer.fdateLastWrite;
	    node->easize = buffer.cbList;

	    if( node->attr & A_DIR ){
	      dirs  = fsort_and_insert(dirs ,node,&dircount
				       , flag >> SORT_MODES );
	    }else{
	      files = fsort_and_insert(files,node,&filecount
				       , flag >> SORT_MODES );
	      if( len > max_length )
		max_length = len;
	    }

	  }else{
	    fprintf(stderr,"%s: no such file or directory.\n",argv[i]);
	    rc = 1;
	    filefault++;
	  }
	}
	fnexplode2_free(list);
      }else{ // _fnexplode で展開できない場合

	FILESTATUS4 buffer;
	int len=strlen(argv[i]);
	
	if( dos_query_path_info(argv[i],len,buffer) == 0 ){
	  struct filelist *node=
	    (struct filelist*)alloca(sizeof(struct filelist)+len);
	  strcpy( node->name , argv[i] );
	  node->attr   = buffer.attrFile;
	  node->length = len;
	  node->size   = buffer.cbFile;
	  node->time   = *(unsigned short *)&buffer.ftimeLastWrite;
	  node->date   = *(unsigned short *)&buffer.fdateLastWrite;
	  node->easize = buffer.cbList;

	  if( node->attr & A_DIR ){
	    dirs  = fsort_and_insert(dirs ,node,&dircount
				     , flag >> SORT_MODES );
	  }else{
	    files = fsort_and_insert(files,node,&filecount
				     , flag >> SORT_MODES );
	    if( len > max_length )
	      max_length = len;
	  }
	}else{
	  fprintf(stderr,"%s: no such file or directory\n",argv[i]);
	  rc = 1;
	  filefault++;
	}
      }// _fnexplode で展開できない場合の処理
    }/* if(argv[i][0]=='-' ){...}else{...} */
  }/* argv loop */

  if( ctrl_c )
    return exit_with_ctrl_c();
  
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
	more(flag,fout);
    }
    struct filelist *p=dirs;
    if( p != NULL ){
      for(;;){
	if( dircount+filecount > 1 ){
	  if( flag & COLOR_MODE ){
	    fputs( ls_end_code , fout);
	    dbcs_fputs(p->name,fout);
	    fputs( " : \n",fout);
	  }else{
	    dbcs_fputs(p->name,fout);
	    fputs(" : ",fout);
	    more(flag,fout);
	  }
	}
	
	the_dir( p->name , flag , fout );
	if( ctrl_c )
	  return exit_with_ctrl_c();

	if( (p=p->next) == NULL ) break;
	
	more(flag,fout);
      }
    }

    if( isatty(fileno(fout)) )
      fputs( ls_end_code , fout );

  }else if( filefault <= 0 ){
    /* ファイル名が指定されていない ---> カレントディレクトリ */

    the_dir( "." , flag , fout );
    if( ctrl_c )
      return exit_with_ctrl_c();

  }
  if( (flag & COLOR_MODE) && isatty(fileno(fout)) )
    fputs( ls_end_code ,fout);

  fflush(fout);
  return rc;
}
