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
    if( scriptflag != 0  ){
      // ---------------------------
      //   スクリプト実行支援機能
      // ---------------------------

      char fname[FILENAME_MAX];
      char path[FILENAME_MAX];
      
      copy_filename(sp,SmartPtr(fname,sizeof(fname)),&sp,NULL);
      
      ScriptCache *sc;
      int type=SearchEnv(fname,"SCRIPTPATH",path);

      if( option_script_cache  &&  (sc=script_hash[fname]) != NULL ){
	/* ---- スクリプト(キャッシュヒット) ---- */
	if( option_debug_echo ){
	  fputs( "Script cache hit\n",stderr);
	  fflush(stderr);
	}
	dp = strcpy_tail(dp,sc->interpreter);
	*dp++ = ' ';
	copy_filename(path,dp,NULL,&dp, SLASH_DEMILITOR );
	copyargs(sp,dp,&sp,&dp);
      }else if( type==FILE_EXISTS ){
	// --- おそらく、スクリプト ---
	if( insert_interpretor(fname,path,dp) < 0 ){
	  /* -- ext文によるスクリプトの可能性あり */
	  suffix(path,dp);
	  copy_filename(path,dp,NULL,&dp,BACKSLASH_DEMILITOR);
	}else{
	  /* -- #!によるスクリプトである -- */
	  copy_filename(path,dp,NULL,&dp, SLASH_DEMILITOR );
	}
	copyargs(sp,dp,&sp,&dp);

      }else if(  type != COM_FILE || sos(sp,dp,path) != 0 ){
	// --- OS/2 の実行ファイル ---
	copy_filename(fname,dp,NULL,&dp, BACKSLASH_DEMILITOR );
	copyargs(sp,dp,&sp,&dp);
      }
    }else{
      if( suffix(sp,dp) == 0 ){
	char path[FILENAME_MAX];
	char fname[FILENAME_MAX];

	copy_filename(sp,SmartPtr(path,sizeof(fname)),&sp,NULL);
	SearchEnv(fname,"SCRIPTPATH",path);
	copy_filename(path,dp,NULL,&dp);
      }else{
	copy_filename(sp,dp,&sp,&dp);
      }
      copyargs(sp,dp,&sp,&dp);
    }
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
