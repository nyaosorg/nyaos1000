/* -*- c++ -*-
 * 関数
 *	replace_script		スクリプト変換処理を行う。
 *	is_innser_command	内蔵コマンドか否かをチェックする。
 *	getApprecationType	アプリタイプを得る。
 *	script_to_cache		スクリプトとインタプリタ名をキャッシングする。
 *	read_script_header	スクリプトからインタプリタ名を読み出す。
 *	expand_sos		SOSスクリプトからコマンドラインへ展開する。
 *	read_sos_header		SOSスクリプトならばフォーマットを読み出す。
 *	copy_args		引数をコピーする。
 *	copy_filename		ファイル名をコピーする。
 *
 *	cmd_rehash		rehash コマンドの処理
 *	cmd_cache		cache コマンドの処理
 */

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
#include "nyaos.h"	/* for Command class */
#include "complete.h"	/* cmd_rehash の為だけのみ */
#include "strbuffer.h"
#include "autofileptr.h"

int scriptflag=1;
int option_amp_start=1;
int option_amp_detach=0;
int option_sos=0;
int option_script_cache=1;
int option_auto_close=1;

extern void debugger(const char *,...);

struct ScriptCache{
  char *name;
  char *interpreter;

  ScriptCache() : name(0) , interpreter(0){ }
  ~ScriptCache(){ free(name); free(interpreter); }
};

Hash <ScriptCache> script_hash(1024);

extern int option_debug_echo;

/* コマンド「cache」：
 * スクリプトキャッシュに保存されているキャッシュ情報を
 * 画面に表示する。
 */
int cmd_cache(FILE *source, Parse &args )
{
  for(HashIndex<ScriptCache> hi(script_hash) ; *hi != NULL ; hi++ ){
    printf("%s = %s\n",hi->name,hi->interpreter);
  }
  return 0;
}

/* コマンド「rehash」：
 * スクリプトキャッシュの情報を全て破棄させる。
 */
int cmd_rehash(FILE *source , Parse &args )
{
  bool quiet=false;
  bool atzero=false;

  for(int i=0;i<args.get_argc();i++){
    if( args[i][0]=='-' ){
      for(int j=1;j<args[i].len;j++){
	switch( args[i][j] ){
	case 'q':
	case 'Q':
	  quiet = true;
	  break;
	  
	case 'n':
	case 'N':
	  atzero = true;
	  break;
	}
      }
    }
  }
  script_hash.destruct_all();

  if( atzero  &&  Complete::queryFiles() > 0 )
    return 0;

  Complete::make_command_cache();
  if( !quiet ){
    FILE *fout=args.open_stdout();
    if( fout != NULL ){
      fprintf(fout
	      , "%d bytes are used for %d commands'name cache.\n"
	      , Complete::queryBytes() , Complete::queryFiles()
	      );
    }
  }
  return 0;
}


/* copy_filename ：ファイル名を StrBuffer へ読み取る。
 *	buf 読み取るバッファ
 *	sp  ファイル名の頭の位置
 *	flag 制御フラグ(以下の enum 参照 )
 * return
 *	ファイル名の末尾の次の位置('\0'の位置)
 * throw
 *	NULL メモリ確保失敗(StrBuffer由来のもの)
 */
enum{
  SPACE_TERMINATE	= 1,	/* 空白を末尾とみなす。	*/
  SLASH_DEMILITOR	= 2,	/* ￥を／へ変換する。	*/
  BACKSLASH_DEMILITOR	= 4,	/* ／を￥へ変換する。	*/
};

static const char *copy_filename(  StrBuffer &buf
				 , const char *sp 
				 , int flag=SPACE_TERMINATE )
     throw(StrBuffer::MallocError)
{
  for(;;){
    /* 「&」や「|」、「\0」など、コマンド末尾の文字列なら終了 */
    if( Parse::is_terminal_char(*sp) )
      break;

    /* スペースでも終了する場合がある */
    if( (flag & SPACE_TERMINATE) != 0  &&  is_space(*sp) )
      break;

    /* コマンド名 : "/"-->"\\"に置換 */
    if( *sp == '"' ){
      do{
	buf << *sp++;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
    }
    if( *sp == '/' && (flag & BACKSLASH_DEMILITOR) !=0 ){
      buf << '\\';
      ++sp;
    }else if( *sp == '\\' && (flag & SLASH_DEMILITOR) !=0 ){
      buf << '/';
      ++sp;
    }else{
      if( is_kanji(*sp) )
	buf << *sp++;
      buf << *sp++;
    }
  }
 exit:
  return sp;
}


/* copy_args ：全ての引数(|か&か、\0までの文字列sp)を BUF へコピーする。
 *	buf コピー先
 *	sp  コピー元
 * return
 *	sp を読んだ後の末尾('\0'か & , | などの位置)
 * throw
 *	NULL メモリ確保エラー
 */
static const char *copy_args( StrBuffer &buf , const char *sp )
     throw (StrBuffer::MallocError)
{
  while( ! Parse::is_terminal_char(*sp) ){
    if( *sp == '"' ){
      do{
	if( is_kanji(*sp) )
	  buf << *sp++;
	buf << *sp++;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
    }
    if( is_kanji(*sp) )
      buf << *sp++;
    buf << *sp++;
  }
 exit:
  return sp;
}

/* sos_script :
 * 　ファイル PATH が SOSスクリプトならば、
 * インタプリタ名記述行を Heap 文字列で返す。
 * さもなければ、NULL を返す
 */
static char *read_sos_header( const char *path ) throw(StrBuffer::MallocError)
{
  AutoFilePtr fp(path,"r");
  if( fp == NULL )
    return NULL;

  int nlines=0;
  for(;;){
    int ch=getc(fp);
    if( ch=='\n' ){
      /* 1カラム目は「#」「;」「%」「:」「'」のみ
       * それ意外の場合、スクリプトとはみなせない。
       */
      int ch=getc(fp);
      if( ch !='#' && ch !=';' && ch !='%' && ch !=':' && ch !='\'' )
	break;

      /* SOSスクリプトは２行目に soshdr/Nide というサインがある！*/
      if( ++nlines==1 ){
	for( const char *s="soshdr/Nide" ; *s != '\0' ; s++ ){
	  if( getc(fp) != *s )
	    return NULL;
	}
      }else if( nlines >= 14 ){
	/* SOS.HDR は 13行であることから、この行こそ、
	 * 実行プログラム名が記述された行である。
	 * というわけで、その行を抜き出して返す！
	 */
	StrBuffer buf;
	while( (ch=getc(fp)) != EOF  &&  ch != '\n' )
	  buf << ch;

	return buf.finish();
      }
    }else if( !isprint(ch) || ch==EOF ){
      break;
    }
  }
  /* break でこのループを脱出するものは、
   * いずれも SOSスクリプトでなかったケース
   */
  return 0;
}

/* expand_sos :
 *   SOSのヘッダ文字列を元にコマンドラインへ展開する。
 *
 *	buf	展開先
 *	fmt	SOSのヘッダ文字列
 *	prog	スクリプト名
 *	argv	引数
 * throw NULL メモリエラー
 */
static void expand_sos(  StrBuffer &buf 
		       , const char *fmt
		       , const char *prog
		       , const char *argv )
     throw(StrBuffer::MallocError)
{
  for( ; *fmt != '\0' ; ++fmt ){
    if( *fmt != '%' ){
      buf << *fmt;
    }else{
      switch( *++fmt ){
      case '0':
	copy_filename( buf , prog , SLASH_DEMILITOR );
	break;
      case '@':
	buf << argv;
	break;
      case '%':
	buf << '%';
	break;
      default:
	buf << '%' << *fmt;
	break;
      }
    }
  }
}

/* read_script_header :
 * スクリプトファイルを実際にオープンして、「#!」の後に記述されている
 * インタープリタ名を読み出し、それを buf へ書き出す。
 *	fname スクリプトのファイル名
 *	return インタプリタ名(ヒープの文字列:freeが必要)
 */
static char *read_script_header( const char *fname )
     throw(StrBuffer::MallocError)
{
  AutoFilePtr fp(fname,"r");
  if( fp==NULL )
    return NULL;
  
  if( getc(fp) != '#' || getc(fp) != '!' )
    return NULL;

  StrBuffer buf; /* インタプリタ名を保存 */

  /* 環境変数 SCRIPTDRIVE の最初の一文字を複写 */
  int ch;
  const char *usp=0;
  if( (ch=getc(fp))=='/' && (usp=getShellEnv("SCRIPTDRIVE")) != NULL ){
    while( *usp != '\0' && *usp != ':' ){
      buf << *usp++;
    }
    buf << ':';
  }
  /* perlやawkなどの実行ファイル名の複写 */
  while( ch != EOF  &&  ch != '\n' ){
    buf << (char)(ch=='/' ? '\\' : ch);
    ch=getc(fp);
  }
  return buf.finish();
}
 
/* スクリプトとインタプリタ名をキャッシュ
 * (グローバル変数 script_hash)に保存する。
 *	script		スクリプト名(Heap文字列)
 *	interpreter	インタープリタ名(Heap文字列)
 * throw NULL  メモリ確保エラー
 *
 */
static void script_to_cache( char *script , char *interpreter ) 
     throw(StrBuffer::MallocError)
{
  ScriptCache *sc=new ScriptCache;
  if( sc == NULL )
    throw StrBuffer::MallocError();

  sc->name = script;
  sc->interpreter = interpreter;
  
  /* 同じ内容のハッシュがあれば、それを破棄させてから、
   * 新規登録 */
  script_hash.destruct( sc->name );
  script_hash.insert( sc->name , sc );
}

/* アプリケーションのタイプを得る
 *	fname 実行ファイルの名前
 * return
 *	0 タイプ不明		 
 *	1 非ウインドウ互換
 *	2 ウインドウ互換（ＶＩＯ）
 *	3 ウインドウAPI（ＰＭ）
 *	-1 エラー
 * throw 無し
 */
static int getApplicationType(const char *fname) throw()
{
  ULONG apptype;
  if( DosQueryAppType(  (const unsigned char *)fname , &apptype ) != 0 )
    return -1;
  return apptype & 7;
}

/* 内臓コマンドか、どうかをチェックする。
 *	fname コマンド名
 * return
 *	false 内臓コマンドではなかった
 *	true  内臓コマンドだった。
 */

static bool is_inner_command( const char *name ) throw()
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

/* スクリプト変換を行う。
 *
 * return
 *	変換後のテキスト。ヒープ文字列なので、使用後に
 *	free することが必要。
 * throw
 *	StrBuffer::MallocError 文字通り
 */
char *replace_script( const char *sp ) throw(MallocError)
{
  StrBuffer buf;
  
  for(;;){  /* コマンド毎のループ */
    while( *sp != '\0'  &&  is_space(*sp) )
      buf << *sp++;

    /* true ならば、VIOアプリの時に"/C /F" が挿入される*/
    bool auto_close=false;

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
	    if(option_amp_detach){
	      buf << "detach ";
	    }else{
	      buf << "start ";
	      if( option_auto_close )
		auto_close = true;
	    }
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

      StrBuffer fname;
      char path[FILENAME_MAX];
      ScriptCache *sc;
      char *header=0;
      int type;
      const char *suffix=0;

      /* とりあえず、コマンド名を別のバッファに保存しておいて、 
       * ポインタを進める。(「$0」→ fname) 
       */
      sp = copy_filename( fname , sp );

      if( is_inner_command( fname.getTop() )) {

	/* ------ 内臓コマンド ------*/
	copy_filename( buf , fname.getTop() , BACKSLASH_DEMILITOR );
	sp = copy_args( buf , sp );
	
      }else if( stricmp(suffix=_getext2(fname),".class") == 0 ){
	/*
	 * ------ Java Application (*.class な時) -----
	 */
	buf << "java ";
	const char *javaopt=getShellEnv("JAVAOPT");
	if( javaopt != NULL  &&  javaopt[0] != '\0' )
	  buf << javaopt << ' ';
	
	buf.paste( fname , suffix-fname );
	buf << ' ';
	sp = copy_args( buf , sp );

      }else if(    option_script_cache
	      &&  (sc=script_hash[fname.getTop()]) != NULL ){

	/* ------ スクリプト(キャッシュヒット) ------- */

	if( option_debug_echo ){
	  fputs( "Script cache hit\n",stderr);
	  fflush(stderr);
	}
	type = SearchEnv(fname,"SCRIPTPATH",path);
	if( auto_close )
	  buf << "/C /F ";
	
	buf << sc->interpreter << ' ';
	copy_filename( buf , path , SLASH_DEMILITOR );
	sp = copy_args( buf , sp );

      }else if(   (type=SearchEnv(fname,"SCRIPTPATH",path)) == COM_FILE
	       && (header=read_sos_header(path)) !=0 ){
	/*
	 * -------- SOSスクリプト --------
	 */
	try{
	  StrBuffer argv;
	  sp = copy_args( argv , sp );
	  expand_sos( buf , header , path , argv );
	}catch(...){
	  free(header);
	  throw;
	}
	free(header);
      }else if(    type==FILE_EXISTS 
	       && (header=read_script_header(path)) != 0 ){
	/*
	 * ------- 「#!」スクリプト --------
	 */
	if( auto_close )
	  buf << "/C /F ";
	
	/* 「#!」行内のインタプリタ名をペースト */
	copy_filename( buf , header , BACKSLASH_DEMILITOR );
	buf << ' ';
	/* スクリプト名自身をペースト */
	copy_filename( buf , path   , SLASH_DEMILITOR );
	
	buf << ' ';

	/* オプションが立っていれば、
	 * キャッシュに、読み出した結果を保存する */
	if( option_script_cache ){
	  char *name = strdup( fname );
	  if( name == 0 ){
	    free( header );
	    throw StrBuffer::MallocError();
	  }
	  script_to_cache( name , header );
	}else{
	  free( header );
	}
	sp = copy_args( buf , sp );

      }else{
	/* -------- OS/2 の実行ファイル ---------- */
	if(   auto_close 
	   && (getApplicationType(fname) == 2 || type == CMD_FILE)  ){
	  buf << "/C /F ";
	}
	copy_filename( buf , fname , BACKSLASH_DEMILITOR );
	sp = copy_args( buf , sp );
      }

    }else{
      /* ================= option -script の場合 =============== */
      StrBuffer fname;
      sp = copy_filename( fname , sp );
      if(   auto_close && getApplicationType(fname)== 2 )
	buf << "/C /F ";
      buf << fname;
      sp = copy_args( buf , sp );
    }

    /* ================= 終結文字の処理 (\0, | , & など) =============*/

    if( *sp == '\0' )
      break;
    
    if( *sp == '|' ){
      if( *(sp+1) == '&' ){	/*  `|&' -> '2>&1 |' */
	buf << "2>&1 |";
	sp += 2;
      }else{
	buf << *sp++;
      }
    }else if( *sp == '&' ){
      buf << *sp++;
      if( *sp == '&' )
	buf << *sp++;
      else if( *sp == ';' )
	++sp;
    }
    if( *sp=='\0' )
      break;
  }/* パイプで区切られた各コマンド毎のループ */
  return buf.finish();
}

int replace_script( const char *sp , char *dst , int max )
{
  char *kekka;
  try{
    kekka = replace_script(sp);
  }catch(StrBuffer::MallocError){
    strncpy( dst , sp , max );
    return 0;
  }
  if( kekka != NULL ){
    strncpy( dst , kekka , max );
    free(kekka);
  }else{
    strncpy( dst , sp , max );
  }
  return 0;
}
