#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#define INCL_DOSSESMGR

#include "macros.h"
#include "finds.h"
#include "hash.h"
#include "Parse.h"
#include "nyaos.h" /* for Command class */

int scriptflag=1;
int option_amp_start=1;
int option_amp_detach=0;
int option_sos=0;
int option_script_cache=1;
int option_auto_close=1;

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
  return 0;
}
int cmd_rehash(FILE *source , Parse &args )
{
  extern void make_command_cache(void);

  make_command_cache();
  script_hash.destruct_all();
  return 0;
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


/* SOSスクリプトのチェックを行う。
 *	sp パラメータへポインタ
 *	dp バッファでスクリプト名を書く直前を差す
 *	path スクリプト名の絶対パス
 * return
 *	1  SOS スクリプトではなかったので、何もしなかった。
 *	0  SOS スクリプトだったので、コマンドラインを置換した。
 */
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

  int ch;
  const char *usp=0;
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

static int is_pm_application(const char *fname)
{
  ULONG apptype;
  if( DosQueryAppType(  (const unsigned char *)fname , &apptype ) != 0 )
    return -1;
  return (apptype & 7)==3;
}

static void insert_close_option(SmartPtr &dp)
{
  *dp++ = '/';
  *dp++ = 'C';
  *dp++ = ' ';
  *dp++ = '/';
  *dp++ = 'F';
  *dp++ = ' ';
}

/* 内臓コマンドか、どうかをチェックする。
 *	fname コマンド名
 * return
 *	0  内臓コマンドではなかった
 *	!0 内臓コマンドだった。
 */

static int is_inner_command( const char *name )
{
  extern Hash <Command> command_hash;
  extern int option_ignore_cases;
  
  Command *buildinCommand;
  if( option_ignore_cases ){
    buildinCommand = command_hash.lookup_tolower( name );
  }else{
    buildinCommand = command_hash[ name ];
  }
  return buildinCommand != NULL;
}


int replace_script( const char *sp , char *dst, int max  )
{
  SmartPtr dp(dst,max);

  for(;;){  /* コマンド毎のループ */
    while( is_space(*sp) )
      *dp++ = *sp++;
    
    int start_inserted=0;
    if( option_amp_start || option_amp_detach ){
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
	    /* 「&&」,「&;」でない「&」なら start または detach を挿入 */
	    char *s;

	    if(option_amp_detach)
	      s="detach ";
	    else{
	      s="start ";
	      start_inserted=1;
	    }
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
      // option +script の場合
      // ---------------------------
      
      char fname[FILENAME_MAX];
      char path[FILENAME_MAX];
      ScriptCache *sc;
      int type;
      
      // とりあえず、コマンド名を別のバッファに保存しておいて、 
      // ポインタを進める。(「$0」→ fname) 
      SmartPtr smartptr(fname,sizeof(fname) );
      try{
	copy_filename(sp,smartptr,&sp,NULL);
      }catch( SmartPtr::BorderOut ){
	smartptr.terminate();
      }
      
      /* ------ 内臓コマンド ------*/
      if( is_inner_command(fname)) {
	copy_filename( fname , dp , NULL, &dp , BACKSLASH_DEMILITOR );
	copyargs(sp,dp,&sp,&dp);
      }

      /* ------ スクリプト(キャッシュヒット) ------- */
      else if( option_script_cache  &&  (sc=script_hash[fname]) != NULL ){
	if( option_debug_echo ){
	  fputs( "Script cache hit\n",stderr);
	  fflush(stderr);
	}
	type = SearchEnv(fname,"SCRIPTPATH",path);
	if( option_auto_close  &&  start_inserted )
	  insert_close_option(dp);
	dp = strcpy_tail(dp,sc->interpreter);
	*dp++ = ' ';
	copy_filename(path,dp,NULL,&dp, SLASH_DEMILITOR );
	copyargs(sp,dp,&sp,&dp);
	
      }

      /* ------- おそらく、スクリプト -------- */
      else if( (type=SearchEnv(fname,"SCRIPTPATH",path))==FILE_EXISTS ){
	
	if( option_auto_close  &&  start_inserted )
	  insert_close_option(dp);
	
	if( insert_interpretor(fname,path,dp) < 0 ){
	  /* -- ext文によるスクリプトの可能性あり */
	  suffix(path,dp);
	  copy_filename(path,dp,NULL,&dp,BACKSLASH_DEMILITOR);
	}else{
	  // -- #!によるスクリプトである
	  copy_filename(path,dp,NULL,&dp, SLASH_DEMILITOR );
	}
	copyargs(sp,dp,&sp,&dp);
	
      }

      /* -------- OS/2 の実行ファイル ---------- */
      else if(  type != COM_FILE || sos(sp,dp,path) != 0 ){

	if(    option_auto_close  &&  start_inserted 
	   &&  ! is_pm_application(fname)  ){
	  insert_close_option(dp);
	}
	copy_filename(fname,dp,NULL,&dp, BACKSLASH_DEMILITOR );
	copyargs(sp,dp,&sp,&dp);
      }

    }
    /* ================= option -script の場合 =============== */
    else{

      char fname[FILENAME_MAX];
      SmartPtr smartptr(fname,sizeof(fname));
      try{
	copy_filename(sp,smartptr,&sp,NULL);
      }catch( SmartPtr::BorderOut ){
	smartptr.terminate();
      }
      if( suffix(sp,dp) == 0 ){
	char path[FILENAME_MAX];
	
	SearchEnv(fname,"SCRIPTPATH",path);
	copy_filename(path,dp,NULL,&dp);
      }else{
	if(    option_auto_close  &&  start_inserted
	   &&  !is_pm_application(fname) )
	  insert_close_option(dp);
	
	copy_filename(fname,dp,NULL,&dp);
      }
      copyargs(sp,dp,&sp,&dp);
    }

    /* ================= 終結文字の処理 (\0, | , & など) =============*/

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
