/* -*- c++ -*-
 * 内蔵 ls を実行するための関数 eadir および 下請け関数。
 *
 */

#undef DEBUG

#include <process.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ea.h>
#include <io.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include "strbuffer.h"
// #include "SmartPtr.h"
#include "keyname.h"
#include "errmsg.h"

#define INCL_VIO
#define INCL_DOSNLS

#include "nyaos.h"
#include "complete.h"
#include "finds.h"

#ifdef DEBUG
#  define DEBUG1(x) (x)
#else
#  define DEBUG1(x)
#endif

extern volatile int ctrl_c;
extern int screen_width;
extern int screen_height;

enum{
  LS_LONG ,
  LS_MORE ,
  LS_NOCOLOR ,
  LS_ALL , 
  LS_RECURSIVE ,
  LS_IGNORE_UNDERBAR ,
  LS_IGNORE_BACKUP ,
  LS_LAST_COLUMN ,
  LS_MARK, /* !0: どんな時でも * , / を実行ファイル・ディレクトリにつける*/
  LS_COMMENT ,	/* 0:表示しない  1:inline  2:multi-line */
  LS_LONGNAME ,	/* 0:表示しない  1:左よせ  2:右よせ */
  LS_SORT ,
  LS_SORT_REVERSE ,
  LS_ViRGE ,
  LS_COMMA ,
  LS_SUBJECT ,
  NUM_LS ,
};

static char ls_flag[ NUM_LS ];

static char *ls_left_code="\033[";
static char *ls_right_code="m";
static char *ls_end_code="\033[0m";

static char *ls_normal_file="1";	/* 白 */
static char *ls_directory="32;1";	/* 緑 */
static char *ls_system_file="31;1";	/* 青 */
static char *ls_hidden_file="44;37;1";	/* 青地の白 */
static char *ls_executable_file="35;1"; /* 紫 */

static char *ls_read_only_file="33;1";	/* 背景が黄 */
static char *ls_comment="44;37;1";	/* 青地に白 */
static char *ls_longname="41;37;1";     /* 赤字に白 */
static char *ls_subject="42;37;1";	/* 緑字に白 */

/* struct tm のカバークラス。
 * 扱い方は DirDateTime と互換性があるが、
 * 仮想関数で同じように使えるわけではない。
 */
class TmCover {
  struct tm tmbody;
public:
  operator tm&() { return tmbody; }
  
  int getYear()   const { return tmbody.tm_year+1900; }
  int getMonth()  const { return tmbody.tm_mon+1; }
  int getDay()    const { return tmbody.tm_mday; }
  int getHour()   const { return tmbody.tm_hour; }

  void setLocalTime(time_t &time){
    memcpy( &tmbody , localtime(&time) , sizeof(struct tm) );
  }
};

TmCover tmNow;

static struct {
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
  { "sj",&ls_subject} ,
};

/* 拡張属性を読み取る為のポインタ型 */
union MultiPtr {
  void *value;
  const char *byte;
  const unsigned short *word;
};

/* 数値をカンマ付きで表示する。
 *	width	最大表示桁(満たない時は空白で埋められる。
 *	n	表示する数値
 *	fp	表示先
 * return 実表示文字数
 */
static int print_num_with_comma(int width,int n,FILE *fp)
{
  if( width < 1 )
    width = 1;
  
  if( n >= 1000 ){
    int len=print_num_with_comma(width-4,n/1000,fp);
    putc( ',' , fp );
    /* 左の桁を 0 で埋めた形式で表示させる */
    return len+fprintf(fp,"%03d",n % 1000 );
  }else{
    /* 左の桁を 空白で埋めた形式で表示させる */
    return fprintf(fp,"%*d",width,n);
  }
}

/* ASCII タイプのＥＡ(.LONGNAME / .SUBJECT)を取得する。
 *	fname … ファイル名
 *	eatype … EAの属性名(".LONGNAME" や ".SUBJECT")
 *	*pLen … 非NULL の場合、長さが入る。
 * return
 *	得られた EA (要free文字列)
 *	NULL の時は失敗
 */
char *get_asciitype_ea( const char *fname , const char *eatype , int *pLen=0 )
{
#ifdef S2NYAOS
  return NULL;
#else
  struct _ea ea;
  union MultiPtr ptr;
  
  DEBUG1( fputs("(get_asciitype_ea) enter\n",stderr) );

  if( _ea_get( &ea , fname , 0 , eatype ) != 0 
     || ea.size <= 0 || ea.value == NULL ){
    DEBUG1( fputs("(get_asciitype_ea) leave : case 1\n",stderr) );
    return NULL;
  }

  DEBUG1( fputs("(get_asciitype_ea) hogehoge\n",stderr) );

  ptr.value = ea.value;
  int type = *ptr.word++;
  if( type != 0xFFFD ){
    _ea_free(&ea);
    DEBUG1( fputs("(get_asciitype_ea) leave : case 2\n",stderr) );
    return NULL;
  }

  DEBUG1( fputs("(get_asciitype_ea) tochu-\n",stderr) );
  
  int size = *ptr.word++; /* 実際のサイズ */
  StrBuffer sbuf;
  
  for(int i=0 ; i<size ; i++ ){
    if( *ptr.byte == '\r' ){
      ptr.byte++;
    }else if( *ptr.byte == '\n' ){
      sbuf << ' ';
      ptr.byte++;
    }else if( 0 <= *ptr.byte  && *ptr.byte < ' ' ){
      sbuf << '^' << char('@'+*ptr.byte++) ;
    }else{
      sbuf << *ptr.byte++ ;
    }
  }
  _ea_free( &ea );
  if( pLen != 0 )
    *pLen = sbuf.getLength();

  DEBUG1( fputs("(get_asciitype_ea) leave : case 3\n",stderr) );
  if( sbuf.getLength() <= 0 )
    return NULL;
  else
    return sbuf.finish();
#endif
}

/* ポインタ配列と、その中のポインタの示すHeapを全て解放する。
 *	table	ポインタ配列の先頭。末尾は NULL で終わっている必要がある。
 */
void free_pointors(char **table)
{
  while( *table != NULL )
    free( *table++ );
}

/* 拡張属性 .COMMENT の内容を得る。
 *	fname 属性を持つファイルの名前
 * return
 *	属性値をポインタ配列で返す。
 *	使用後は、free_pointors関数で解放してやる必要がある。
 */
char **get_ea_comments( const char *fname )
{
#ifndef S2NYAOS
  struct _ea ea;
  union MultiPtr ptr;

  if(   _ea_get( &ea , fname , 0 , ".COMMENTS" ) != 0
     || ea.size <= 0 || ea.value == NULL )
    return NULL;

  { // tiny try block.

    ptr.value = ea.value;
    if( *ptr.word++ != 0xFFDF )
      goto errpt;
    
    ptr.word++; /* コードページを読みとばす */
  
    int n=*ptr.word++;
    if( n==0 )
      goto errpt;
      
    char **table=(char**)malloc( sizeof(char*) * (n+1) );
    if( table == NULL )
      goto errpt;
    
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

  }// tiny try block.

 errpt:
  _ea_free( &ea );
#endif
  return NULL;
}

/* 一行の文字列で与えられた ls カラーオプションを
   各属性ごとの変数へ代入する。 */

static void set_ls_color_table(const char *s)
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

// int column=0;

int nprintlines=0;

/* そのファイルは、フラグと照合して、表示してよいかを判定する 
 *	f  ファイル情報
 */
static int is_file_print(const FileListT *f)
{
  assert( f != NULL );
  const char *top=_getname(f->name);

  if( ls_flag[LS_ALL]==0  &&  ( *top == '.' || (f->attr & Dir::HIDDEN) ))
    return 0;
  if( f->name[f->length-1] == '~' &&  ls_flag[LS_IGNORE_BACKUP] )
    return 0;
  if( *top == '_' &&  ls_flag[LS_IGNORE_UNDERBAR] )
    return 0;
  return 1;
}

/* 英字モードの時に、DBCS 文字を「?」に変換して表示する。
 * ようにしていたが、日本語モードにもいろいろなコードページ
 * があるので、現在は fputs と同じようにしか動かない。
 */
static int dbcs_fputs(const char *s,FILE *fout)
{
  int i=0;
  while( *s != '\0' ){
    putc( *s++ , fout );
    i++;
  }
  return i;
}

static int ncolumns=0;
static void more( FILE *fout )
{
  if( ls_flag[LS_ViRGE]  &&  (fout==stdout || fout==stderr) ){
    fflush(fout);
    USHORT X,Y;
    VioGetCurPos(&Y,&X ,0 );
    if( Y >= screen_height-1 ){
      static BYTE cell[2]={' ',0x00};
      VioScrollUp(0,0,screen_height-1,screen_width-1,3,cell,0);
      fputs("\x1B[3A",fout);
    }
  }

  if( ls_flag[ LS_NOCOLOR] ){
    putc('\n',fout );
  }else{
    fprintf(fout,"%s\x1B[K\n",ls_end_code);
  }
  if( ls_flag[LS_NOCOLOR]==0  &&  ls_flag[LS_MORE]
     && (nprintlines+=(1 + ncolumns / screen_width ))>= screen_height-2 ){
    
    fputs("[more]",fout);
    fflush(fout);
    raw_mode();
    if( getkey() == ('C' & 0x1F) )
      ctrl_c = 1;
    cocked_mode();
    fputs("\r      \r",fout);
    nprintlines=0;
  }
  ncolumns = 0;
}

/* 「ls -l」形式で、一ファイルを表示する 
 *	flist … 対象ファイルの情報
 *	max_length … 最大ファイル名の流さ
 *	curdir … 基準ディレクトリ
 *	fout … 出力先
 */
void dir1(  const FileListT *flist , int max_length 
	  , const char *curdir , FILE *fout)
{
  int tailchar = ' ';

  StrBuffer headstr;

  enum{
    AS_DIR , AS_READ , AS_WRITE , AS_EXEC , AS_ARCHIVE ,
    AS_HIDDEN , AS_SYSTEM , AS_EA , NUM_AS ,
  };
  char attrstr[ NUM_AS+1 ];

  for(int i=0 ; i<NUM_AS ; i++ )
    attrstr[ i ] = '-';
  
  attrstr[ NUM_AS   ] = '\0';
  attrstr[ AS_READ  ] = 'r';
  attrstr[ AS_WRITE ] = 'w';
  
  const char *top=flist->name;
  for(const char *p=flist->name ; *p != '\0' ; p++ ){
    if( *p == '\\' || *p == '/' )
      top = p+1 ;
  }
  
  /* 隠しファイルは表示せず、終了 */
  if( is_file_print(flist)==0 )
    return;
  
  if( flist->attr & A_DIR ){
    headstr << ls_directory ;
    attrstr[ AS_DIR ] = 'd';
    attrstr[ AS_EXEC ] = 'x';
    tailchar = '/';
  }else if( flist->attr & A_HIDDEN ){
    if( ! ls_flag[ LS_ALL ] )
      return;
    for(const char *sp=ls_hidden_file ; *sp != '\0' ; sp++ )
      headstr << *sp;
    headstr << ls_hidden_file;
    attrstr[ AS_HIDDEN ] = 'h';
  }else if( flist->attr & A_SYSTEM ){
    headstr << ls_system_file;
    attrstr[ AS_SYSTEM ] = 's';
  }else if( flist->attr & A_LABEL ){
    headstr << ls_system_file ;
    attrstr[ AS_SYSTEM ] = 'L' ;
  }else if( which_suffix(flist->name,"EXE","COM","CMD","BAT",NULL) != 0 ){
    headstr << ls_executable_file;
    tailchar = '*';
    attrstr[ AS_EXEC ] = 'x';
  }else if( flist->attr & A_RONLY ){
    headstr << ls_read_only_file;
  }else{
    headstr << ls_normal_file;
  }
  
  if( flist->attr & A_RONLY )
    attrstr[ AS_WRITE ] = '-';
  
  if( flist->attr & A_ARCHIVE )
    attrstr[ AS_ARCHIVE ] = 'a';
  
  if( flist->easize > 4 ) /* EA がなくても、サイズ情報で最低 4bytes は要る */
    attrstr[ AS_EA ] = 'e';
  
  if( ! ls_flag[ LS_NOCOLOR] )
    fputs(ls_end_code,fout);
  
  ncolumns=0;
  
  /* ls モードの時は、このブロックだけで return する */
  if( ! ls_flag[ LS_LONG] ){
    if( ! ls_flag[ LS_NOCOLOR] )
      fprintf(fout,"%s%s%s",ls_left_code,(const char*)headstr,ls_right_code);
    
    dbcs_fputs(flist->name,fout);
    if( ! ls_flag[ LS_NOCOLOR ] )
      fputs(ls_end_code,fout);
    
    int i=strlen(flist->name);
    
    /* 実行ファイルに「＊」ディレクトリに「/」を付ける。 
     * ただし、パイプ、ファイル出力の際は付けない。
     */
    if( tailchar != ' '  &&  (isatty(fileno(fout)) || ls_flag[LS_MARK] )){
      putc(tailchar,fout);
      ++i;
    }
    
    if( ! ls_flag[ LS_LAST_COLUMN ] ){
      while( i < max_length+2 ){
	++i;
	putc(' ',fout);
      }
    }
    /* column += i; */
    return;
  }
  
  static const char *month[]={
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec",
  };
  
  const filelist::DirDateTime *datetime;
  switch( ls_flag[ LS_SORT ] ){
  case SORT_BY_LAST_ACCESS_TIME: // -u
    datetime = &flist->access;
    break;
    
  case SORT_BY_CHANGE_TIME:      // -c
    datetime = &flist->create;
    break;
    
  default:
  case SORT_BY_MODIFICATION_TIME: // -t
    datetime = &flist->write;
    break;
  }

  ncolumns += fprintf(fout,"%s" , attrstr );
  if( ls_flag[ LS_COMMA ] ){
    ncolumns += print_num_with_comma(11,flist->size,fout);
    putc(' ',fout); ncolumns++;
  }else{
    ncolumns += fprintf(fout,"%9ld ",flist->size);
  }

  if( datetime->getMonth() > 12  || datetime->getMonth() < 1   ){
    /* FAT では、最終アクセス時刻を取得することができない。
     * この場合、時刻は 1989/0/0 になってしまう。
     */
  }else{
    ncolumns += fprintf(fout,"%3s %2d "
			, month[ datetime->getMonth()-1 ]
			, datetime->getDay()
			);
    
    if(    flist->write.getYear() <  tmNow.getYear()-1 /* 一昨年 */
       || (flist->write.getYear() == tmNow.getYear()-1 /* 去年 */
	   && ( flist->write.getMonth() < tmNow.getMonth() /* 去年の先月以前 */
	       || (flist->write.getMonth() == tmNow.getMonth() /* 去年の今月 */
		   && flist->write.getDay() <= tmNow.getDay() 
		   /*去年の今日以前*/)
	       )
	   )
       || ( (flist->write.getYear() == tmNow.getYear() /* 今年 */
	     && ( flist->write.getMonth() > tmNow.getMonth() /* 来月以降 */
		 || (flist->write.getMonth() == tmNow.getMonth() /* 今月 */
		     && flist->write.getDay() > tmNow.getDay() /* 明日以降 */ )
		 )
	     )
	   )
       || flist->write.getYear() > tmNow.getYear() /* 来年 */ ){

      /* 年数表示 */
      ncolumns += fprintf(fout," %4d " ,datetime->getYear() );
    }else{
      /* 時刻表示 */
      ncolumns += fprintf( fout
			  ,"%02d:%02d " 
			  , datetime->getHour() 
			  , datetime->getMinute() );
    }
  }
  
  if( ! ls_flag[ LS_NOCOLOR ] )
    fprintf(fout,"%s%s%s",ls_left_code,(const char*)headstr,ls_right_code);
  
  ncolumns += dbcs_fputs(flist->name,fout);

  if( ! ls_flag[ LS_NOCOLOR ] )
    fputs(ls_end_code,fout);
  
  putc(tailchar,fout);
  ncolumns++;

  char _fullpath[ FILENAME_MAX ];
  const char *fullpath=_fullpath;
  if( curdir == NULL || curdir[0] == '\0' ){
    fullpath = flist->name;
  }else{
    sprintf(_fullpath,"%s/%s",curdir,flist->name);
  }

  /* ==== EA が存在すれば、EA の各属性を表示させる。==== */
  if( flist->easize > 4 ){
    char *subject=0;
    char *longname=0;
    int length;

    /* ---- EA のサブジェクトを表示する ---- */
    if(   ls_flag[LS_SUBJECT] 
       && (subject=get_asciitype_ea(fullpath,".SUBJECT",&length)) != NULL ){

      if( ncolumns + 2 + length >= screen_width ){
	more(fout);
	for(int i=screen_width-length-2-1 ; i>0 ; i-- )
	  fputc(' ',fout);
	ncolumns = screen_width-1;
      }else{
	ncolumns += 2+length;
      }
      fputs("> ",fout);

      if( ls_flag[ LS_NOCOLOR ] ){
	fputs( subject , fout );
      }else{
	fputs( ls_left_code  , fout );
	fputs( ls_subject    , fout );
	fputs( ls_right_code , fout );
	dbcs_fputs( subject , fout );
	fputs( ls_end_code , fout );
      }
    }else  if(   ls_flag[ LS_LONGNAME ] 
	      && (longname=get_asciitype_ea(fullpath,".LONGNAME",&length))
	      != NULL ){
	
      /* ⇒ EA のロングネームを表示する
       * ロングネームとサブジェクトは同時に表示できない */
      
      if( strcmp( flist->name , longname ) != 0 ){
	int nspaces = screen_width - ncolumns - length;
	if( ls_flag[ LS_LONGNAME] == 2 ){
	  if( nspaces < 0 ){
	    more(fout);
	    nspaces = screen_width - length;
	  }
	  while( nspaces-- > 0 )
	    putc( ' ' , fout );
	}
	
	putc(' ',fout);
	putc('(',fout);
	if( ls_flag[ LS_NOCOLOR] ){
	  fputs( longname , fout );
	}else{
	  fputs( ls_left_code , fout );
	  fputs( ls_longname , fout );
	  fputs( ls_right_code , fout );
	  dbcs_fputs( longname , fout );
	  fputs( ls_end_code , fout );
	}
	putc(')',fout);
	ncolumns += 3 + length;
      }
      free( longname );
    }


    /* ----- EAのコメントを表示する ------ */
    char **comments=0;
    if( ls_flag[LS_COMMENT] && (comments=get_ea_comments(fullpath) ) != 0 ){
      
      if( ls_flag[LS_COMMENT] == 1 ){
	if( subject == 0 && longname == 0 ){
	  /* ---- インラインコメント ----- */
	  if( ! ls_flag[LS_NOCOLOR] )
	    fprintf(fout,".. %s%s%s%s%s"
		    ,ls_left_code,ls_comment,ls_right_code
		    ,comments[0],ls_end_code);
	  else
	    fprintf(fout,".. %s",comments[0]);
	  
	  ncolumns += 3 + strlen(comments[0]);
	}
      }else{
	/* ---- マルチコメント ---- */
	for(char **pp=comments;*pp != NULL;pp++){
	  more(fout);
	  if( ! ls_flag[LS_NOCOLOR] )
	    fprintf(fout,"\t%s%s%s%s%s"
		  ,ls_left_code,ls_comment,ls_right_code
		    ,*pp,ls_end_code);
	  else
	    fprintf(fout,"\t%s",*pp);
	}
      }
      free_pointors( comments );
    }
  }/* ↑ EA関係の処理 */

  more(fout);
}


/* 複数のファイル名を表示する。
 *	files ... ファイル名のリスト
 *	fout ... 出力先
 * return
 *	0 ... 成功
 *	1 ... 失敗
 */
int print_filelist(const Files &files , FILE *fout)
{
  FileListT *cur=files.get_top();

  if( cur == NULL )
    return 0;

  /* --- 表示可能なファイルの数と、ファイル名の最大長を求める。--- */
  int nlists=0;
  int max_length=2;
  for(const FileListT *p=files.get_top() ; p != NULL ; p=p->next ){
    if( is_file_print(p) ){
      nlists++;
      if( p->length > max_length )
	max_length = p->length;
    }
  }

  /* 「-l」が無い場合の ls */
  if( ! ls_flag[ LS_LONG] ){
    
    /* --- 一行あたりに表示するファイルの数 ---- */
    int files_per_line;
    if( screen_width-1 < max_length+2 || !isatty(fileno(fout)) )
      files_per_line = 1;
    else
      files_per_line = (screen_width-1)/(max_length+2);

    
    /* --- 一列あたりに表示するファイルの数  ----
     *     nlists ≧ files_per_line × files_per_column
     */
    int files_per_column = (nlists+files_per_line-1)/files_per_line;
    
    FileListT **row = (FileListT **)alloca(files_per_line*sizeof(FileListT *));
    
    for(int i=0 ; i<files_per_line; i++ )
      row[i] = NULL;
    
    assert( row != NULL );

    while( cur != NULL && !is_file_print(cur) )
      cur=cur->next;
    
    for(int i=0; i<files_per_line-1 && cur != NULL ; i++ ){
      row[i] = cur;
      for(int j=0 ; cur != NULL && j<files_per_column ; j++){
	do{
	  cur = cur->next;      
	}while( cur != NULL && !is_file_print(cur) );
      }
    }
    row[files_per_line-1] = cur;
    
    for(int j=0; j<files_per_column ; j++ ){
      for(int i=0; i<files_per_line  &&  row[i] != NULL ; i++ ){
	if( ctrl_c )
	  return nlists;
	
	int saveflag = ls_flag[ LS_LAST_COLUMN ];
	if( (i+1)==files_per_line || row[i+1] == NULL )
	  ls_flag[ LS_LAST_COLUMN ] = 1;
	else
	  ls_flag[ LS_LAST_COLUMN ] = 0;
	
	dir1(row[i] , max_length , files.getDirName() , fout );
	ls_flag[ LS_LAST_COLUMN ] = saveflag;

	row[i] = row[i]->next;
	
	while( row[i] != NULL && !is_file_print(row[i]) )
	  row[i] = row[i]->next;
      }
      more(fout);
      /* column=0; */
    }
  }else{
    /* -l モード */
    while( cur != NULL ){
      dir1( cur , max_length , files.getDirName() , fout );
      cur=cur->next;
      if( ctrl_c )
	return 1;
    }
  }
  return 0;
}

/* 一ディレクトリー下のファイルを表示する。*/

int the_dir(const char *dirname, FILE *fout )
{
  assert( dirname != NULL);
  assert( fout != NULL );

  Files files , dirs;
  int max_length=0;

  files.setDirName(dirname);
  dirs.setDirName(dirname);
  
  for(Dir dir(dirname) ; dir != NULL ; ++dir ){
    FileListT *tmp=new_filelist(dir);
    assert(tmp != NULL );
    
    if( tmp->length > max_length )
      max_length = tmp->length;
    
    if( is_file_print(tmp) ){
      if(   ls_flag[ LS_RECURSIVE ]
	 && (tmp->attr & A_DIR )!= 0 
	 && tmp->name[0] != '.' ){

	dirs.insert( dup_filelist(tmp) , ls_flag[LS_SORT] );
      }
      files.insert( tmp , ls_flag[LS_SORT] );
    }
  }

  files.sort( ls_flag[LS_SORT] );

  /* カレントディレクトリのファイルを表示する。*/
  if( files.get_num() >= 0 ){
    print_filelist( files , fout);
  }

  /* カレントディレクトリ以下のサブディレクトリを表示する。*/
  for(  FileListT *dirlist=dirs.get_top()
      ; dirlist != NULL  &&  ctrl_c==0 
      ; dirlist = dirlist->next ){

    char fullpathbuffer[512];
    char *fullpath;
    if( dirname[0] == '.' && dirname[1] == '\0' )
      fullpath = dirlist->name;
    else
      sprintf(fullpath=fullpathbuffer,"%s/%s",dirname,dirlist->name);

    if( ! ls_flag[ LS_NOCOLOR ] ){
      fprintf( fout, "\n%s%s:\n",ls_end_code , fullpath );
    }else{
      more(fout);
      dbcs_fputs(fullpath,fout);
      putc(':',fout);
      more(fout);
    }
    the_dir( fullpath , fout );
  }
  return files.get_num();
}

static int exit_with_ctrl_c()
{
  fputs("\n^C\n",stderr);
  fflush(stderr);

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

static void on_inline_comment()
{  ls_flag[ LS_COMMENT ] = 1 ; ls_flag[ LS_LONG ] = 1; }
static void on_multi_comment()
{  ls_flag[ LS_COMMENT ] = 2 ; ls_flag[ LS_LONG ] = 1; }
static void on_longname()
{  ls_flag[ LS_LONGNAME ] = 1 ; ls_flag[ LS_LONG ] = 1; }

static void on_all()
{  ls_flag[ LS_ALL ] = 1; }
static void on_ignore_backup()
{  ls_flag[ LS_IGNORE_BACKUP ] = 1; }
static void on_ignore_underbar()
{  ls_flag[ LS_IGNORE_UNDERBAR ] = 1; }
static void on_color()
{  ls_flag[ LS_NOCOLOR ] = 0; }
static void on_nocolor()
{  ls_flag[ LS_NOCOLOR ] = 1; }
static void on_more()
{  ls_flag[ LS_MORE ] = 1; }
static void on_virge()
{  ls_flag[ LS_ViRGE ] = 1; }
static void on_subject()
{  ls_flag[ LS_SUBJECT ] = 1; }

static void on_numeric_sort()
{
  ls_flag[ LS_SORT ] 
    = (ls_flag[ LS_SORT] & SORT_REVERSE) | SORT_BY_NUMERIC ;
}

static void on_sort_reverse()
{  ls_flag[ LS_SORT ] |= SORT_REVERSE;  }

class Parse;

int eadir( int argc, char **argv,FILE *fout,Parse &parser)
{
  /* --- フラグを全て初期化する --- */
  memset( ls_flag , 0 , sizeof(ls_flag) );

  /* --- 西暦の年数を前もって取得 --- */
  time_t now;
  time(&now);
  
  tmNow.setLocalTime(now);

  /* --------------------------------------------------------- */

  int rc=0;
  nprintlines=0;
  
  if( ! isatty(fileno(fout) ) )
    ls_flag[ LS_NOCOLOR ] = 1;
  
  int filefault=0;
  Files files,dirs;

  set_ls_color_table( getShellEnv("LS_COLORS") );

  for( int i=1 ; i<argc ; i++ ){
    assert( argv[i] != NULL );
    
    /* オプション文字列 */
    if( argv[i][0] == '-' ){
      /* long option */
      if( argv[i][1] == '-' ){
	struct longoption_tg {
	  const char *name;
	  void (*func)();
	} option_table[]={
#include "eadirop.cc"
	} , *longopt = (struct longoption_tg *)
	  bsearch(  &argv[i][2]
		  , option_table
		  , numof(option_table)
		  , sizeof(struct longoption_tg)
		  , &KeyName::compareWithTop );
	
	if( longopt == NULL ){
	  ErrMsg::say( ErrMsg::UnknownOption , "ls" , argv[i] , 0 );
	  return 0;
	}
	(*longopt->func)();

      }else for( const char *p=argv[i]+1 ; *p != '\0' ; p++ ){
	switch( *p ){
	  /* ------- 互換オプション ------ */
	case 'a':
	  ls_flag[ LS_ALL ] = 1; break;
	case 'l':
	  ls_flag[ LS_LONG ] = 1 ; break;
	case 'R':
	  ls_flag[ LS_RECURSIVE ] = 1; break;

	  /* ------- ソートオプション ------- */
	case '2':
	  ls_flag[ LS_SORT ] = SORT_BY_NUMERIC ; break;
	case 'c':
	  ls_flag[ LS_SORT ] = SORT_BY_CHANGE_TIME; break;
	case 'S':
	  ls_flag[ LS_SORT ] = SORT_BY_SIZE; break;
	case 'u':
	  ls_flag[ LS_SORT ] = SORT_BY_LAST_ACCESS_TIME; break;
	case 'X':
	  ls_flag[ LS_SORT ] = SORT_BY_SUFFIX ; break;
	case 'U':
	  ls_flag[ LS_SORT ] = UNSORT; break;
	case 'r':
	  ls_flag[ LS_SORT ] = SORT_REVERSE ; break;
	case 't':
	  ls_flag[ LS_SORT ] = SORT_BY_MODIFICATION_TIME ; break;
	case '_':
	  ls_flag[ LS_IGNORE_UNDERBAR ] = 1; break;
	case ',':
	  ls_flag[ LS_COMMA ] =  ls_flag[ LS_LONG  ] = 1;
	  break;
	case 'B':
	  ls_flag[ LS_IGNORE_BACKUP ] = 1; break;
	case 'E':
	  ls_flag[ LS_LONG ] = 1;
	  ls_flag[ LS_LONGNAME ] = 1;
	  ls_flag[ LS_COMMENT ] = 1;
	  ls_flag[ LS_SUBJECT ] = 1;
	  break;
	case 'P':
	  ls_flag[ LS_MORE ] = 1; break;
	case 'O':
	  ls_flag[ LS_NOCOLOR ] = 1;  break;
	case 'o':
	  ls_flag[ LS_NOCOLOR ] = 0; break;
	case '3':
	  ls_flag[ LS_ViRGE ] = 1 ; break ;
	case 'H':
	  ls_flag[ LS_LONG ] = 1;
	  ls_flag[ LS_COMMENT ] = 1;
	  break;

	case 'j':
	  ls_flag[ LS_SUBJECT ] = 1;
	  break;

	case 'F':
	  ls_flag[ LS_MARK ] = 1; break;

	default:
	  call_original_ls( argv , fout );
	  return rc;
	}/* end switch */
      }/* end for */

    }else{
      /* オプションでない文字列 ... ファイル名を登録する。
       * ここでは、全てのファイル・ディレクトリを一度、
       * オブジェクト files , dirs に登録している。
       */
      char **list=fnexplode2(argv[i]);
      if( list != NULL ){
	/* ワイルドカード展開が出来た場合 */
	for(char **ptr=list; *ptr != NULL ; ptr++ ){
	  FileListT *node=new_filelist( *ptr );
	  if( node != NULL ){
	    if( node->attr & A_DIR ){
	      dirs.insert( node , ls_flag[ LS_SORT ] );
	    }else{
	      files.insert( node , ls_flag[ LS_SORT ] );
	    }
	  }else{
	    ErrMsg::say( ErrMsg::NoSuchFileOrDir , argv[i] , 0 );
	    rc = 1;
	    filefault++;
	  }
	}
	fnexplode2_free(list);
      }else{
	/* ワイルドカード展開が出来なかった場合
	 * それ自身ファイル名だから、直接、ファイル情報を得る。
	 */
	FileListT *node = new_filelist(argv[i]);
	if( node != NULL ){
	  if( node->attr & A_DIR ){
	    dirs.insert( node , ls_flag[ LS_SORT] );
	  }else{
	    files.insert( node , ls_flag[ LS_SORT] );
	  }
	}else{
	  ErrMsg::say( ErrMsg::NoSuchFileOrDir , argv[i] , 0 );
	  rc = 1;
	  filefault++;
	}
      }// _fnexplode で展開できない場合の処理
    }/* if(argv[i][0]=='-' ){...}else{...} */
  }/* argv loop */

  if( ctrl_c )
    return exit_with_ctrl_c();
  
  /* files , dirs の登録されたファイル・ディレクトリを
   * 表示してゆく 
   */
  if( files.get_num() > 0 || dirs.get_num() > 0  ){
    /* 少なくとも一つ以上のファイル名、あるいはディレクトリ名が
     * 指定されている。*/
    if( files.get_num() > 0 ){
      /* dotfile や Hidden属性があっても、直接コマンドラインで指定しているの
       * だから、表示させる 
       */
      files.sort( ls_flag[ LS_SORT ] );
      
      int flagsave=ls_flag[ LS_ALL ];
      ls_flag[ LS_ALL ] = 1;
      print_filelist( files , fout );
      ls_flag[ LS_ALL ] = flagsave;

      if( dirs.get_num() > 0 ){
	dirs.sort( ls_flag[ LS_SORT ] );
	more(fout);
      }
    }

    FileListT *p=dirs.get_top();
    if( p != NULL ){
      for(;;){
	/* ファイルとディレクトリ名が複数登録されている場合、
	 * ディレクトリの中身を表示する際に
	 * 「ディレクトリ名 :」という行を入れる。
	 * 逆にそのディレクトリしか、登録されていない場合は省略する。
	 */
	if( dirs.get_num()+files.get_num() > 1 ){
	  if( ! ls_flag[ LS_NOCOLOR ] ){
	    fputs( ls_end_code , fout);
	    dbcs_fputs(p->name,fout);
	    fputs( " : \n",fout);
	  }else{
	    dbcs_fputs(p->name,fout);
	    fputs(" : ",fout);
	    more(fout);
	  }
	}
	/* ディレクトリ名の中身を表示 */
	the_dir( p->name , fout );
	if( ctrl_c )
	  return exit_with_ctrl_c();

	if( (p=p->next) == NULL ) break;
	more(fout);
      }
    }

    if( isatty(fileno(fout)) )
      fputs( ls_end_code , fout );

  }else if( filefault <= 0 ){
    /* ファイル名/ディレクトリ名が一つも指定されていない
     * カレントディレクトリを表示する。 
     */
    the_dir( "." ,  fout );
    if( ctrl_c )
      return exit_with_ctrl_c();
  }
  if( ! ls_flag[ LS_NOCOLOR ]  &&  isatty(fileno(fout)) )
    fputs( ls_end_code ,fout);
  
  fflush(fout);
  return rc;
}
