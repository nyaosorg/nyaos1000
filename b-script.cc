#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/nls.h>
#include "macros.h"
#include "finds.h"
#include "hash.h"
#include "Parse.h"

int scriptflag=1;
int option_amp_start=1;
int option_sos=0;
int option_script_cache=1;

#define JAVA_SUPPORT 0

// ファイル名を、'/' <--> '\\' 変換しながら、コピーする
// 空白や、ヌルをファイル名末尾とみなす。

enum{
  SPACE_TERMINATE	= 1,
  SLASH_DEMILITOR	= 2,
  BACKSLASH_DEMILITOR	= 4,
};

struct ScriptCache{
  char *name;
  char interpreter[1];
  void *operator new(int s,int n){ return malloc(s+n-1); }
  void operator delete(void *p){ free(p); }
};
Hash<ScriptCache> script_hash(1024);

extern int option_debug_echo;

SmartPtr strcpy_tail(SmartPtr dp,const char *sp)
{
  while( *sp != '\0' )
    *dp++ = *sp++;
  *dp = '\0';
  return dp;
}

int cmd_cache(FILE *source, Parse &args )
{
  for(HashIndex<ScriptCache> hi(script_hash) ; *hi != NULL ; hi++ ){
    printf("%s = %s\n",hi->name,hi->interpreter);
  }
}

int cmd_rehash(FILE *source , Parse &args )
{
  extern void make_command_cache(void);

  make_command_cache();
  script_hash.destruct_all();
}

#if 0
int java(const char *name , char *dp )
{
  const char *period=NULL;
  const char *sp=name;
  while( *sp != '\0'  &&  !isspace(*sp & 255) ){
    if( *sp == '.' ){
      period = sp;
    }else if( *sp == '/' || *sp=='\\' ){
      period = NULL;
    }
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
  int len=sp-name;
  char *classfn=(char*)alloca( len + 6 );

  if( period == NULL ){
    for(int i=0; i<len ; i++ )
      classfn[ i ]=name[ i ];
    strcpy( name+len , ".class" );
    
    sprintf(classfn,"%s.class",name);
  }else{
    if( strnicmp(period,".class",6) != 0
  }

}
#endif

static void copy_filename(  const char *sp , SmartPtr dp
			  , const char **sp_tail=NULL 
			  , SmartPtr *dp_tail=NULL
			  , int flag=SPACE_TERMINATE )
{
  while( ! Parse::is_terminal_char(*sp)
	&& ! ((flag & SPACE_TERMINATE)!=0 && is_space(*sp)) && dp.ok() ){

    /* コマンド名 : "/"-->"\\"に置換 */
    if( *sp == '"' ){
      do{
	*dp++ = *sp++;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
    }
    if( *sp == '/' && (flag & BACKSLASH_DEMILITOR) !=0 ){
      *dp++ = '\\';
      sp++;
    }else if( *sp == '\\' && (flag & SLASH_DEMILITOR) !=0 ){
      *dp++ = '/';
      sp++;
    }else{
      if( is_kanji(*sp) )
	*dp++ = *sp++;
      *dp++ = *sp++;
    }
  }
 exit:
  *dp = '\0';
  if( sp_tail != NULL ) *sp_tail = sp;
  if( dp_tail != NULL ) *dp_tail = dp;
}


// copyargs :
//	全ての引数をコピーする
//      この関数の実行後、*sp_tail は \0 , & , | のどれかを差している。

static void copyargs(  const char *sp       , SmartPtr dp
		     , const char **sp_tail , SmartPtr *dp_tail )
{
  while( ! Parse::is_terminal_char(*sp) && dp.ok() ){
    if( *sp == '"' ){
      do{
	if( is_kanji(*sp) )
	  *dp++ = *sp++;
	*dp++ = *sp++;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
    }
    if( is_kanji(*sp) )
      *dp++ = *sp++;
    *dp++ = *sp++;
  }
 exit:
  *dp = '\0';
  if( sp_tail != NULL ) *sp_tail = sp;
  if( dp_tail != NULL ) *dp_tail = dp;
}

// skipargs:
//	実際にコピーしない、copyargs

static void skipargs(  const char *&sp )
{
  while( ! Parse::is_terminal_char(*sp) ){
    if( *sp == '"' ){
      do{
	if( is_kanji(*sp) )
	  ++sp;
	++sp;
	if( *sp == '\0' )
	  return;
      }while( *sp != '"' );
    }
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
}


// SOS 処理
//   sp は スクリプト名の後、パラメーターの前
//   dp は スクリプト名を書く直前
//   path は スクリプトの絶対パス
//
// 帰り値 : 1 ... 変換しなかった。
//

static int sos(const char *&sp , SmartPtr &dp ,  const char *path )
{
  FILE *fp=fopen(path,"r");
  if( fp==NULL )
    return 1;
  
  int ch,nlines=0;
  for(;;){
    ch=getc(fp);
    if( ch=='\n' ){
      int ch=getc(fp); /* 先頭の # を読みとばす */
      if( ch !='#' && ch !=';' && ch !='%' && ch !=':' && ch !='\'' ){
	fclose(fp);
	return 1;
      }
      if( ++nlines==1 ){
	for( const char *s="soshdr/Nide" ; *s != '\0' ; s++ ){
	  if( getc(fp) != *s ){
	    fclose(fp);
	    return 1;
	  }
	}
      }else if( nlines >= 14 ) /* SOS.HDR は 13行 */
	break;
    }else if( !isprint(ch) || ch==EOF ){
      fclose(fp);
      return 1;
    }
  }
  
  /* ここで、ポインタは、コマンド名の直前にあるはず */
  while( (ch=getc(fp)) != EOF && ch != '\n' ){
    if( ch != '%' ){
      *dp++ = ch;
    }else{
      switch( ch=getc(fp) ){
      case '0':
	copy_filename( path , dp , NULL , &dp , SLASH_DEMILITOR);
	break;
      case '@':
	copyargs(sp,dp,NULL,&dp);
	break;
      case '%':
	*dp++ = '%';
	break;
      default:
	*dp++ = '%';
	*dp++ = ch;
	break;
      }
    }
  }
  fclose(fp);

  skipargs(sp);
  
  return 0;
}

/* インタープリタ名を挿入する */
static int insert_interpretor(const char *cache,const char *fname,SmartPtr &dp)
{
  FILE *fp=fopen(fname,"r");
  if( fp==NULL )
    return -1;
  
  if( getc(fp) != '#' || getc(fp) != '!' ){
    fclose(fp);
    return -2;
  }

  const char *interpreter=dp.rawptr();
    
  /* 環境変数 USRDRIVE の最初の一文字を複写 */
  const char *usp;
  int ch;
  if( (ch=getc(fp))=='/' && (usp=getenv("SCRIPTDRIVE")) != NULL ){
    while( *usp != '\0' && *usp != ':' )
      *dp++ = *usp++;
    *dp++ = ':';
  }
  
  /* perlやawkなどの実行ファイル名の複写 */
  while( ch != EOF  &&  ch != '\n' ){
    if( ch == '/' )
      *dp++ = '\\';
    else
      *dp++ = ch;
    ch=getc(fp);
  }
  fclose(fp);

  if( option_script_cache ){
    ScriptCache *sc=new(dp.rawptr()-interpreter+1) ScriptCache;
    assert( sc != NULL );
    sc->name = strdup(cache);
    assert( sc->name != NULL );
    char *p=sc->interpreter;
    while(interpreter < dp.rawptr() )
      *p++ = *interpreter++;
    *p = '\0';

    script_hash.destruct( sc->name );
    script_hash.insert( sc->name , sc );
  }
  *dp++ = ' ';
  return 0;
}

extern int suffix( const char *path , SmartPtr &dp );

enum{
  NOT_WRITTEN ,
  WRITTEN_PROCNAME,
  WRITTEN_ALL ,
  WRITTEN_ERROR = -1 ,
};


int script_support(const char *procname,/* プログラム名(コピー) */
		   int period,		/* 最後のピリオドの位置 */
		   const char *&sp ,	/* プログラム名の直後のポインタ */
		   SmartPtr &dp )		/* 置換バッファ */
{
  if( scriptflag == 0 )
    return NOT_WRITTEN;

  ScriptCache *sc;
  char fullpath[ FILENAME_MAX ];
  int type=SearchEnv(procname,"SCRIPTPATH",fullpath);
  
  if( option_script_cache  &&  (sc=script_hash[procname]) != NULL ){
    dp = strcpy_tail(dp,sc->interpreter);
    *dp++ = ' ';
    copy_filename(fullpath,dp,NULL,&dp,SLASH_DEMILITOR);
    return WRITTEN_PROCNAME;
  }else if( type==FILE_EXISTS ){
    if( insert_interpretor(procname,fullpath,dp) < 0 ){
      /* -- ext 文によるスクリプト -- */
      suffix( fullpath , dp );
      copy_filename(fullpath,dp,NULL,&dp,BACKSLASH_DEMILITOR);
    }else{
      copy_filename(fullpath,dp,NULL,&dp,SLASH_DEMILITOR );
    }
    return WRITTEN_PROCNAME;
  }else if( type==COM_FILE && sos(sp,dp,fullpath)==0 ){
    return WRITTEN_ALL ;
  }
  return NOT_WRITTEN ;
}


#if JAVA_SUPPORT != 0

int option_java=1;
int java_support(char *procname,
		 int period,
		 const char *&sp,
		 SmartPtr &dp )
{
  if( option_java == 0 )
    return NOT_WRITTEN;

  char *name_wi_class=NULL; /* 「.class」付きファイル名 */
  char *name_wo_class=NULL; /* 「.class」無しファイル名 */
  
  if( period >= 0  &&  stricmp( procname+period , ".class" ) == 0 ){
    name_wi_class = procname;
    name_wo_class = (char*)alloca(period+1);
    memcpy( name_wo_class , procname , period );
    name_wo_class[ period ] = '\0';
  }else{
    int len=strlen(procname);
    name_wo_class = procname;
    name_wi_class = (char*)alloca(len+8);
    sprintf(name_wi_class,"%s.class",procname);
  }
  
  char fullpath[ FILENAME_MAX ];
  _searchenv( name_wi_class , "CLASSPATH" , fullpath );
  if( fullpath[0] == '\0' )
    return NOT_WRITTEN;
  
  const char *sq="java ";
  while( *sq != '\0' )
    *dp++ = *sq++;
  
  sq = name_wo_class;
  while( *sq != '\0' )
    *dp++ = *sq++;

  return WRITTEN_PROCNAME;
}
#endif

int replace_script( const char *sp , char *dst, int max  )
{
  SmartPtr dp(dst,max);
  for(;;){
    while( is_space(*sp) )
      *dp++ = *sp++;
    
    if( option_amp_start ){
      // 先行して、末尾が & かどうかしらべる。
      // もし、そうならば先頭に「start」を追加する。

      const char *p=sp;
      for(;;){
	switch( *p ){
	case '>':		/* >& というリダイレクトマークもあり */
	  if( *++p == '&' )
	    ++p;
	  break;
	  
	case '&':
	  while( is_space(*++p) )
	    ;
	  if( *p != '&' && *p != ';' ){
	    /* 「&&」,「&;」でない「&」なら start を挿入 */
	    char *s = "start ";
	    while( *s != '\0' )
	      *dp++ = *s++;
	  }
	  goto check_script;

	case '|':
	case '\0':
	  goto check_script;

	case '"':
	  do{
	    if( *++p == '\0' )
	      goto check_script;
	  }while( *p != '"' );
	  ++p;
	  break;

	default:
	  if( is_kanji(*p) )
	    ++p;
	  ++p;
	  break;
	}/* switch() */
      }/* for(;;) */
    } /* if option_amp... */

  check_script:
    /* コマンド実行支援：
     * int func(const char *progname, int period,  char *&dp);
     * 帰り値:
     *    変換した場合	: 1
     *    変換しない場合: 0
     *    エラー	: -1
     * progname:
     *    プログラム名の複写(null-terminated)
     * period:
     *    プログラム名の最後のドットの位置
     * dp:
     *    「インタプリタ名  プログラム名」
     *    を格納するバッファ名
     */

    /* --- プログラム名を複写 --- */
    char *procname;
    int period=-1;
    {
      const char *sq=sp;
      while( *sq != '\0'  &&  ! isspace(*sq & 255) ){
	if( *sq == '.' ){
	  period = sq-sp;
	}else if( *sq == '/' || *sq == '\\' ){
	  period = -1;
	}
	if( is_kanji( *sq & 255 ) )
	  ++sq;
	++sq;
      }
      int len_proc=sq-sp;
      procname=(char*)alloca(len_proc+10);
      memcpy( procname , sp , len_proc );
      procname[ len_proc ] = '\0';
      
      sp = sq;
    }

    int rc;
#if JAVA_SUPPORT != 0
    rc = java_support(procname,period,sp,dp);
    if( rc==NOT_WRITTEN )
#endif
      rc=script_support(procname,period,sp,dp);
    
    if( rc == NOT_WRITTEN ){
      while( *procname != '\0' )
	*dp++ = *procname++;
    }
    if( rc == WRITTEN_ERROR )
      return 0;
    else if( rc != WRITTEN_ALL )
      copyargs(sp,dp,&sp,&dp);

    /* -- 末尾文字の処理 -- */

    if( *sp == '\0' )
      break;
    
    if( *sp == '|' ){
      if( *(sp+1) == '&' ){	/*  `|&' -> '2>&1 |' */
	*dp++ = '2';	*dp++ = '>';
	*dp++ = '&';	*dp++ = '1';
	*dp++ = ' ';	*dp++ = '|';
	sp += 2;
      }else{
	*dp++ = *sp++;
      }
    }else if( *sp == '&' ){
      *dp++ = *sp++;
      if( *sp == '&' )
	*dp++ = *sp++;
      else if( *sp == ';' )
	++sp;
    }
    if( *sp=='\0' )
      break;
  }/* パイプで区切られた各コマンド毎のループ */
  *dp = '\0';
  return 0;
}
