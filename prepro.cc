#include <ctype.h>
#include <io.h>
#include <stdlib.h>
// #include <stdio.h>

#include "parse.h"
#include "Edlin.h"
#include "macros.h"
#include "autofileptr.h"
#include "quoteflag.h"
#include "errmsg.h"

const char *getShellEnv(const char *);

int option_tilda_is_home=1;
int option_tilda_without_root=0;
int option_replace_slash_to_backslash_after_tilda=1;
int option_tcshlike_history=0;
int option_dots=1;
int option_history_in_doublequote=0;
int option_same_history=1;

extern int are_spaces(const char *s); /* ← edlin2.cc */

static struct PublicHistory {
  char *string;
  PublicHistory *prev,*next;

  PublicHistory() : string(0) , prev(0) , next(0) { }
} Oth , *public_history=&Oth;

int nhistories = 0;

/* 00h から 1Fh までの制御文字を ^H という形で表示するfputs。
 * 出力先が端末でない、ファイル等の時は、変換を行わない。
 *	p    文字列
 *	fout 出力先ファイルポインタ
 */
static void fputs_ctrl(const char *p , FILE *fout)
{
  for( ; *p != '\0' ; p++ ){
    if( 0 < *p && *p < ' ' && isatty(fileno(fout)) ){
      putc( '^' , fout );
      putc( '@'+*p , fout );
    }else{
      putc( *p , fout );
    }
  }
}

/* 「source -h ファイル名」を行う。
 *	fname 読み込むファイル名
 *
 * return 0:成功 , -1:失敗(ファイルが存在しない)
 *	メモリが確保できない場合などはエラーにならず、
 *	読み込まれないだけ。
 */
int source_history( const char *fname )
{
  AutoFilePtr fp(fname,"rt");
  if( fp==NULL )
    return -1;

  char buffer[1024];
  while( fgets(buffer,sizeof(buffer),fp) != NULL ){
    /* 先頭の数字と「:」を除く */
    char *p = strchr(buffer,':');
    if( p == NULL || *++p == '\0' || *++p == '\0' )
      continue;

    /* 末尾の \n を除く */
    char *q= strchr(p,'\n');
    if( q != NULL )
      *q = '\0';

    /* ---- public history ----- */
    PublicHistory *tmp=new PublicHistory;
    if( tmp != NULL ){
      tmp->string = strdup(p);
      if( tmp->string == NULL ){
	delete tmp;
      }else{
	tmp->prev = public_history;
	tmp->next = NULL;
	if( public_history != NULL )
	  public_history->next = tmp;
	public_history = tmp;
	++nhistories;
      }
    }
    
    /* ---- shell history ---- */
    Shell::append_history(p);
  }
  return 0;
}

/* ヒストリを検索する。
 *	str ... 検索文字列。だたし、先頭 len 分だけが有効
 *	len ... 検索文字列の対象となる文字数
 * return マッチしたヒストリ行。マッチしない時は NULL。
 */
static const char *seek_hist_top(const char *str,int len)
{
  PublicHistory *cur=public_history;
  while( cur != NULL && cur != &Oth ){
    if( cur->string[0] == str[0]  &&  memcmp(cur->string,str,len)==0 )
      return cur->string;
    cur = cur->prev;
  }
  return NULL;
}

/* ヒストリを検索する。
 *	str ... 検索文字列
 * return マッチしたヒストリ行
 */
static const char *seek_hist_mid(const char *str)
{
  PublicHistory *cur=public_history;
  while( cur != NULL && cur != &Oth ){
    if( strstr( cur->string , str ) != NULL )
      return cur->string;
    cur = cur->prev;
  }
  return NULL;
}

/* ヒストリの内容を、古い順の番号で得る。
 *	n ... 番号
 *
 * return ヒストリ内容の文字列。無い時は NULL。
 */
static const char *get_hist_f(int n)
{
  PublicHistory *cur=Oth.next;
  if( cur==NULL )
    return NULL;
  
  /* ユーザー指定のヒストリ番号は「1」から始まるので、i=1 */
  for(int i=1; i<n ; i++ ){
    if( cur == NULL )
      return NULL;
    // fprintf(stderr,"[%d]%s ",i,cur->string ?: "(null)" );
    cur = cur->next;
  }
  return (cur != NULL  && cur != &Oth ) ? cur->string : 0 ;
}

/* 過去方向へヒストリを検索する 
 *	n ... 遡るヒストリの数
 */
static const char *get_hist_r(int n)
{
  PublicHistory *cur=public_history;
  if( cur==NULL || cur==&Oth )
    return NULL;
  for(int i=0; i<n ; i++ ){
    if( cur == NULL || cur==&Oth )
      return NULL;
    cur = cur->prev;
  }
  return cur ? cur->string : 0 ;
}

/* パスをオプションの値によって、切り刻むルーチン
 * option
 *	'h' : ディレクトリ部分
 *	't' : 非ディレクトリ部分
 *	'r' : 非拡張子部分
 *	'e' : 拡張子部分
 * return
 *	非NULL : 取り出した部分の先頭位置
 *	NULL   : option が h,t,r,e 以外
 */
char *cut_with_designer(char *path,int option)
{
  switch( option ){
  case 'H':
  case 'h': /* ディレクトリ部分 */
    *( _getname( path ) ) = '\0';
    return path;

  case 'T':
  case 't': /* ディレクトリ部分以外 */
    return _getname( path );

  case 'R':
  case 'r': /* 拡張子部分以外 */
    *( _getext2( path ) ) = '\0';
    return path;
    
  case 'E':
  case 'e': /* 拡張子部分 */
    return _getext2( path );
    
  default:
    return NULL;
  }
}


/* 環境変数の内容をコピーする
 *	env ... 環境変数名(NULL可)
 *	dp ... コピー先(スマートポインタ)
 * return コピー先の末尾
 */
static void insert_env(const char *env,StrBuffer &buf)
{
  const char *sp;
  const char *opt=strchr(env,':');
  if( opt != NULL  &&  opt[1] != '\0' ){
    /* %bar:h% ... %bar%のディレクトリ部分
     * %bar:t% ... %bar%のディレクトリ部分以外
     * %bar:r% ... %bar%の拡張子以外
     * %bar:e% ... %bar%の拡張子
     */

    /* %bar:x% の bar の部分だけ、抜き出す */
    char *envtmp = (char*)alloca( opt-env+1 );
    memcpy( envtmp , env , opt-env );
    envtmp[ opt-env ] = '\0';

    /* %bar% の値を得る。未定義ならば終了 */
    const char *value = getShellEnv(envtmp);
    if( value == NULL )
      return;
    
    /* %bar% の値を加工する為に、別の領域にコピーする。*/
    int vallen=strlen(value);
    char *valtmp=(char*)alloca(vallen+1);
    memcpy( valtmp , value , vallen );
    valtmp[ vallen ] = '\0';

    /* %bar:x% を得る。
     * ただし、「:x」の部分が不適な場合は「bar:x」全体を環境変数とみなす。
     */
    if( opt[2]!='\0' || (sp=cut_with_designer( valtmp , opt[1] ))==NULL ){
      sp = getShellEnv(env);
    }
  }else{
    /* ⇒ 単純な %bar% の場合 */
    sp = getShellEnv(env);
  }
  buf << sp;
}

/* 単語修飾子、すなわち「!」の後に続く「$」「^」などで、
 * 行から単語を切り出す作業を行う。
 *	sp ... 「:」の直後を差す。
 *	histring ... 切り出されうる行
 *	dp ... コピー先(スマートポインタ)
 */

static void word_designator(const char *&sp , const char *histring ,
			    StrBuffer &buf )
{
  /* sp は、':' の後にあるとする */

  Parse argv(histring);
  int argc=argv.get_argc();

  if( *sp=='$' ){
    ++sp;

    buf.paste( argv[ argc-1 ].ptr , argv[ argc-1 ].len );
    return ;
  }else if( *sp=='^' ){
    ++sp;

    if( 1 < argc )
      buf.paste( argv[ 1 ].ptr , argv[ 1 ].len );

    return;

  }else if( *sp=='*' ){
    ++sp;
    
    for( int i=1 ; i<argc ; i++ ){
      buf.paste( argv[ i ].ptr , argv[ i ].len );
      buf << ' ';
    }
    return;

  }else if( *sp=='-' && isdigit(sp[1] & 255) ){
    ++sp;
    
    int n=0;
    do{
      n = n*10 + (*sp-'0');
    }while( isdigit( *++sp & 255 ) );
    
    if( n >= argc )
      n = argv.get_argc()-1;

    for(int i=0 ; i<=n ; i++ ){
      buf.paste( argv[ i ].ptr , argv[ i ].len );
      buf << ' ';
    }
    return;

  }else if( isdigit(*sp) ){

    int n=0;
    do{
      n = n*10 + (*sp-'0');
    }while( isdigit(*++sp & 255) );
      
    if( n < argc )
      buf.paste( argv[ n ].ptr , argv[ n ].len );

    if( *sp == '-' ){
      int end=0;
      if( isdigit( *++sp & 255 ) ){
	do{
	  end = end*10 + (*sp-'0');
	}while( isdigit( *++sp & 255) );
	if( end >= argc-1 )
	  end = argc-1;
      }else{
	end = argc-1;
      }
      while( ++n <= end ){
	buf << ' ';
	buf.paste( argv[ n ].ptr , argv[ n ].len );
      }
    }
    return;
  }
  buf << histring;
  return;
}
/* 「!」で始まるヒストリ参照子を、対応するヒストリ内容に置換する。
 *	sp ... 置換前の「!」を差すポインタ
 *	dp ... 置換後の結果をコピーするスマートポインタ
 * return コピーした末尾を差すスマートポインタ
 */
static void history_copy(const char *&sp, StrBuffer &buf)
{
  /* 引数 sp は、「!」を指していると仮定 */
  const char *histring=0;
  const char *event=sp; /* ← エラーメッセージ用 */
  
  switch( *++sp ){

  case '!':
    sp++;
  case '*':
  case ':':
  case '$':
  case '^':

    histring = get_hist_r(0);
    if( histring == NULL )
      ErrMsg::say(ErrMsg::EventNotFound,"!",0);
    break;

  default:
    int minus=0;
    if( *sp == '-' ){
      minus=1;
      ++sp;
    }
    if( is_digit(*sp) ){
      int n=0;
      do{
	n = n*10+(*sp-'0');
      }while( is_digit(*++sp) );
      
      if( minus ){
	histring = get_hist_r(n>0 ? n-1 : 0 );
	if( histring == NULL )
	  ErrMsg::say(ErrMsg::EventNotFound,event,0);
      }else{
	histring = get_hist_f(n);
	if( histring == NULL )
	  ErrMsg::say(ErrMsg::EventNotFound,event,0);
      }
      

    }else if( *sp == '?' ){
      char buffer[1024] , *bp = buffer;
      ++sp; /* skip '?' */
      while( *sp != '\0' ){
	if( *sp == '?' ){
	  ++sp; /* skip '?' */
	  break;
	}
	*bp++ = *sp++;
      }
      *bp = '\0';

      histring = seek_hist_mid(buffer);
      if( histring == NULL )
	ErrMsg::say(ErrMsg::EventNotFound,buffer,0);
	
    }else{
      char buffer[1024] , *bp = buffer;
      int len=0;
      while( *sp != '\0' && !isspace(*sp) ){
	*bp++ = *sp++;
	len++;
      }
      *bp = '\0';
      histring = seek_hist_top(buffer,len);
      if( histring == NULL )
	ErrMsg::say(ErrMsg::EventNotFound,buffer,0);
    }
    break;
  }/* end of switch */

  if( histring == NULL )
    return;

  switch( *sp ){
  case ':':
    ++sp;
  case '^':
  case '$':
  case '*':
    word_designator( sp , histring , buf );
    return;
    
  default:
    buf << histring;
    return;
  }
}

/* ヒストリ変換を行うプリプロセッサ
 *	sp ... 置換前文字列
 *	_dp .. 置換結果を入れるバッファ
 *	max .. バッファサイズ
 */
char *replace_history(const char *sp) throw(StrBuffer::MallocError)
{
  StrBuffer buf;
  
  int is_history_refered=0;
  int quote=0;
  int prevchar=' ';

  if( *sp=='!' ){
    history_copy(sp,buf);
    is_history_refered = 1 ;
  }

  // ---------------- CD と DIR に対する例外処理 -------------
  
  //「cd/usr/local/bin」などという入力に対応するための処理
  // この場合、cd と「/」の間に空白を挿入する。
  
  if(   (sp[0]=='c' || sp[0]=='C')
     && (sp[1]=='d' || sp[1]=='D')
     && (sp[2]=='.' || sp[2]=='\\' || sp[2]=='/' ) ){
    buf << sp[0] << sp[1] << ' ' << sp[2];
    sp += 3;
  }else if(   (sp[0]=='d' || sp[0]=='D')
	   && (sp[1]=='i' || sp[1]=='I')
	   && (sp[2]=='r' || sp[2]=='R')
	   && (sp[3]=='.' || sp[3]=='\\' || sp[3]=='/' ) ){
    buf << sp[0] << sp[1] << sp[2] << ' ' << sp[3];
    sp += 4;
  }
  
  while( *sp != '\0' ){
    switch( *sp ){
    case '\'':
      if( (quote & 1)==0 )
	quote ^= 2;
      break;
      
    case '"':
      if( (quote & 2)==0 )
	quote ^= 1;
      break;

    case '>': /* リダイレクトに関わる「!」がひっかからないようにする */
      if( sp[1] == '!' ){
	buf << ">!";
	sp+=2;
      }else if( sp[1]=='&'  &&  sp[2] == '!' ){
	buf << ">&!";
	sp+=3;
      }else if( sp[1]=='>'  &&  sp[2] == '!' ){
	buf << ">>!";
	sp+=3;
      }else if( sp[1]=='>'  &&  sp[2] == '&' &&  sp[3] == '!' ){
	buf << ">>&!";
	sp+=4;
      }else{
	break;
      }
      continue;
      
    case '!':
      if(  option_tcshlike_history
	 && (   option_history_in_doublequote
	     ?  (quote & 2)==0  :  quote == 0 ) ) {
	/* history_in_doublequote が有効(not 0)ならば、
	 *    "～!～"はヒストリ変換する。
	 */
	
	history_copy(sp,buf);
	/* is_history_refered = 1; */
      }
      break;
    } // end of switch

    buf << (char)(prevchar = *sp++);
    if( is_kanji(prevchar) )
      buf << *sp++;
  } // end of while

  // ヒストリが参照されている場合は、変換後文字列を画面に表示する。
  if( is_history_refered ){
    fputs_ctrl( (const char *)buf , stdout );
    putc( '\n' , stdout );
  }

  /* ヒストリを登録する。*/
  PublicHistory *tmp=new PublicHistory;
  if(   tmp != NULL
     && !are_spaces((const char*)buf)
     && (tmp->string=strdup((const char*)buf))!=NULL ){
    /* (prev)   旧public_history ≪ tmp ≪ public_history->next  (next) 
     *  古い                     (1)    (2)   (=NULL)            新しい
     */
    tmp->prev = public_history ;

    if( public_history != NULL ){
      tmp->next = public_history->next ; /*  多分 == NULL */
      if( public_history->next != NULL ){
	public_history->next->prev = tmp;
      }
      public_history->next = tmp;
    }else{
      tmp->next = NULL;
    }
    public_history = tmp;

#if 0
    public_history = public_history->next = tmp ;
#endif
    nhistories++;
    if( option_same_history )
      Shell::replace_last_history( tmp->string );
  }
  return buf.finish();
}
void replace_history(const char *sp, char *_dp , int max )
{
  try{
    char *result=replace_history(sp);
    strncpy( _dp , result , max );
    free(result);
  }catch( StrBuffer::MallocError ){
    strncpy( _dp , sp , max );
  }
  _dp[ max-1 ] = '\0';
}


/* プリプロセス：環境変数、チルダ、「...」などの展開を行う。
 *	sp 変換前文字列
 *	_dp コピー先バッファ
 *	max バッファサイズ
 */
char *preprocess(const char *sp) throw (StrBuffer::MallocError)
{
  StrBuffer buf;
  QuoteFlag qf;
  int prevchar=' ';

  if( *sp == '@' )
    ++sp;
  
  while( *sp != '\0' ){
    switch( *sp ){
    case '\'':
    case '"':
      qf.eval( *sp );
      break;
      
    case ';': /* 空白＋「；」を「&;」に変換する */
      ++sp;
      if(   Parse::option_semicolon_terminate 
	 && !qf.isInQuote() && is_space(prevchar) ){
	buf << '&';
      }
      buf << ';';
      continue;
      
    case '.': /* 空白＋「...」を「..\..」に変換する */
      if(   option_dots 
	 && !qf.isInQuote()
	 && is_space(prevchar) && sp[1]=='.' && sp[2]=='.' ){
	
	++sp;
	/* sp は二つ目の . を差している。*/
	for(;;){
	  buf << "..";
	  if( *++sp != '.' )
	    break;
	  buf << '\\';
	}
	continue;
      }
      break;
      
    case '~':
      if(    option_tilda_is_home  &&  !qf.isInQuote()
	 &&  is_space(prevchar) ){
	if( *(sp+1) != '\\' && *(sp+1) != '/' && !option_tilda_without_root){
	  /* option tilda_without_root が off の時は
	   * "~hogehoge" で変換しない。 
	   * ちょっと、こんな書き方、醜いけど…。
	   */
	  break;
	}else if( *(sp+1) == ':' ){ /* `~:' をブートドライブに置換する */
	  ++sp;
	  const char *system_ini = getShellEnv("SYSTEM_INI");
	  if( system_ini == NULL ){
	    buf << '?';
	  }else{
	    buf << *system_ini;
	  }
	}else{ /* 普通の UNIX 的チルダの変換 */
	  insert_env("HOME",buf);
	  if( isalnum(*++sp&255) || is_kanji(*sp&255) ){
	    buf << (char) Edlin::complete_tail_char << ".."
	      << (char)(prevchar=Edlin::complete_tail_char) ;
	  }else{
	    prevchar = '~';
	  }
	}
	if( option_replace_slash_to_backslash_after_tilda ){
	  /* チルダの後の「/」を全て「\」に変換する。 */
	  for(;;){
	    if( *sp == '\0' )
	      return buf.finish();
	    if( is_space(*sp) )
	      break;
	    if( is_kanji(*sp) ){
	      buf << (char)(prevchar=*sp++);
	      buf << *sp++;
	    }else if( *sp=='/' ){
	      ++sp;
	      buf << (char)(prevchar='\\');
	    }else{
	      buf << (char)(prevchar = *sp++);
	    }
	  }
	}
	continue;
      }
      break;
      
    case '%':
      
      if( !qf.isInDoubleQuote() ){
	StrBuffer envname;
	
	++sp; /* 最初の％を読みとばす */
	while( *sp != '\0' ){
	  if( *sp=='%' ){
	    /* 最後の sp を読みとばす */
	    prevchar = *sp++;
	    break;
	  }
	  envname << (char)(prevchar = toupper(*sp & 255) );
	  ++sp;
	}
	if( envname[0] == '\0' ){
	  buf << '%';	/* 「%%」は一つの「%」へ変換する */
	}else{
	  insert_env((const char*)envname,buf);
	}
	continue;
      }
      break;
      
    } /* end of switch () */
    if( is_kanji(*sp) ){
      buf << (char)(prevchar=*sp++);
      buf << *sp++;
    }else{
      buf << (char)(prevchar=*sp++);
    }
  }
  return buf.finish();
}

void preprocess(const char *sp, char *_dp , int max )
{
  try{
    char *result=preprocess(sp);
    strncpy( _dp , result , max );
    free(result);
  }catch( StrBuffer::MallocError ){
    strncpy( _dp , sp , max );
  }
  _dp[ max-1 ] = '\0';
}


/* コマンド「history」
 *	source ... コマンド自身が入っていたストリーム
 *	param .... パラメータリストオブジェクト
 */
int cmd_history(FILE *source,Parse &param)
{
  int n=nhistories;
  /* パラメータ(参照するヒストリの数)がある場合、その数値を取得。
   */
  if( param.get_argc() >= 2 ){
    char *arg1=(char*)alloca(param.get_length(1)+1);
    param.copy(1,arg1);
    if( (n=atoi(arg1)) < 1 )
      n = nhistories;
  }
  FILE *fout=param.open_stdout();

  PublicHistory *cur=public_history;
  if( cur != NULL ){
    int i;
    for( i=0 ; i<n  && cur != NULL && cur != &Oth ; i++ )
      cur = cur->prev;

    for( ; i > 0  && cur !=NULL ; i-- ){
      cur = cur->next;
      fprintf( fout , "%4d : " , 1+nhistories-i );
      fputs_ctrl( cur->string , fout );
      putc('\n',fout);
    }
  }
  return 0;
}

/* それまでのヒストリを一掃して、無に戻してしまう。
 * main関数で、.nyaos から読み込まれてしまったヒストリを消すのに、
 * 一度呼ばれるのみ
 */
void killAllPublicHistory(void)
{
  PublicHistory *cur=public_history;
  while( cur != NULL && cur != &Oth ){
    PublicHistory *trash=cur;
    cur = cur->prev;
    free( trash->string );
    delete trash;
  }
  public_history = &Oth;
  Oth.next = NULL;
  Oth.prev = NULL;
  nhistories = 0;
}
