#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/kbdscan.h>

#define INCL_VIO
#include <os2.h>

#include "edlin.h"
#include "complete.h"
#include "macros.h"
#include "keyname.h"
#include "strbuffer.h"

const char *getShellEnv(const char *);

#define KEY(x)	(0x100 | K_##x )
#define CTRL(x)	((x) & 0x1F )

extern void debugger(const char *,...);

Edlin::Edlin()
{
  pos = len = markpos = msgsize = 0;
  max = DEFAULT_BUFFER_SIZE;
  has_marked = false;

  strbuf = (char*)malloc(max);
  atrbuf = (char*)malloc(max);

  /* ここで、strbuf,atrbuf が NULL か否かを調べていないが、
   * これは変数宣言直後に、
   * Edlin edlin;
   * if( !edlin ){
   *    :
   * }という形でユーザー側に検出してもらう
   */
}

Edlin::~Edlin()
{
  if( strbuf ) free(strbuf);
  if( atrbuf ) free(atrbuf);
}

void Edlin::putchrs(const char *s)
{
  while( *s != '\0' )
    putchr(*s++);
}

static char mark_on[32]="\x1B[45m";
static char mark_off[32]="\x1B[40m";

void Edlin::init()
{
  markpos=pos=len=0;
  has_marked = false;
  strbuf[0]=0;
  
  const char *env_markattr = getShellEnv("NYAOSMARKCOLOR");
  if( env_markattr == NULL )
    return;

  char *dempos=strchr(env_markattr,'/');
  if( dempos == NULL )
    return;

  int len_on = dempos-env_markattr;
  snprintf(  mark_on ,sizeof(mark_on) ,"\x1B[%*.*sm"
	   , len_on , len_on , env_markattr);
  snprintf(  mark_off,sizeof(mark_off),"\x1B[%sm"
	   , dempos+1 );
}

void Edlin::putnth(int nth)
{
  if( ! has_marked ){
    putchr( strbuf[nth] );
    return;
  }

  if( nth==markpos  &&  atrbuf[nth]  != DBC2ND )
    putchrs( mark_on );
  
  putchr( strbuf[nth] );
  
  if(   (nth==markpos   && atrbuf[nth] != DBC1ST )
     || (nth==markpos+1 && atrbuf[nth] == DBC2ND ) )
    putchrs( mark_off );
}

void Edlin::marking(void)
{
  bool prev_has_marked = has_marked;
  int prev_markpos = markpos;

  has_marked = true;
  markpos = pos;
  
  if( prev_has_marked  &&  prev_markpos < pos ){
    /* 前回のマークがカーソル位置より左にある場合に、マークを消す */
    putbs( pos-prev_markpos );
    for(int i=prev_markpos ; i<pos ; i++ )
      putchr( strbuf[i] );
  }
  /* マークを表示(明示的に実行していないが、putnth が自動判断してくれる) */
  putnth(pos);
  if( atrbuf[pos] == SBC ){
    putbs(1);
  }else{
    putnth(pos+1);
    putbs(2);
  }
  /* 前回のマークがカーソル位置より右にある場合に、マークを消す */
  if( prev_has_marked  &&  prev_markpos > pos )
    after_repaint(0);
}

/* at の位置に bytes 分だけのスペースを確保する。
 * 空白文字を入れるわけではなく、空間を作るという意味。
 * 戻り値を必ずしないと、オーバーフロー/アンダーフローが検知できない。
 *	at	スペースを作成する位置
 *	bytes	スペースのサイズ(バイト)。負の値でもよい。
 * return 0:成功  -1:失敗(オーバーフロー/アンダーフロー)
 */
int Edlin::makeRoom(int at,int bytes)
{
  if( bytes > 0 ){
    while( len+bytes >= max ){
      char *new_strbuf=(char*)realloc(strbuf,max*2);
      if( new_strbuf == NULL )
	return -1;
      char *new_atrbuf=(char*)realloc(atrbuf,max*2);
      if( new_atrbuf == NULL ){
	free(new_strbuf);
	return -1;
      }
      strbuf = new_strbuf;
      atrbuf = new_atrbuf;
      max *= 2;
    }
    if( markpos > at )
      markpos += bytes;

    strbuf[ len+bytes ] = '\0';
    atrbuf[ len+bytes ] = SBC;
    for(int i=len-1 ; i >= at ; i-- ){
      strbuf[ i+bytes ] = strbuf[ i ];
      atrbuf[ i+bytes ] = atrbuf[ i ];
    }
  }else if( bytes < 0 ){
    if( at+(-bytes) > len )
      return -1;
    
    if( markpos > at ){
      if( markpos >= at+(-bytes) ){
	/* マークが削除範囲より右なら、左へ削除バイト分ずれる */
	markpos -= (-bytes);
      }else{
	/* マークが削除範囲内なら、削除範囲の先頭に移動する */
	markpos = at;
      }
    }

    for(int i=at ; i<len+bytes ; i++ ){
      strbuf[ i ] = strbuf[ i+(-bytes) ];
      atrbuf[ i ] = atrbuf[ i+(-bytes) ];
    }
    strbuf[ len+bytes ] = '\0';
    atrbuf[ len+bytes ] = SBC;
  }
  len += bytes;
  return 0;
}

int Edlin::complete_tail_char='\\';

/* tcsh の C-t に相当する処理を行う。
 * カーソル直前の二文字を入れ変える。
 */
void Edlin::swapchars()  /* DOSモード未対応メソッド */
{
  if( pos < len )
    forward();

  if( pos < 2 ) return;
  
  if( atrbuf[pos-1]==SBC ){
    if( atrbuf[pos-2] == SBC ){
      /* 半角半角 */
      int tmp=strbuf[pos-2];
      putbs( 2 );
      strbuf[pos-2] = strbuf[pos-1]; putnth(pos-2);
      strbuf[pos-1] = tmp;           putnth(pos-1);
    }else{
      if( markpos == pos-1 ) /* 全角の 2byte目にマークが移動しないように */
	markpos = pos-2;

      /* 全角半角 -> 半角全角 */
      int tmp1=strbuf[pos-3];
      int tmp2=strbuf[pos-2];
      putbs( 3 );

      strbuf[pos-3] = strbuf[pos-1];
      atrbuf[pos-3] = SBC;
      putnth(pos-3);

      strbuf[pos-2] = tmp1;
      atrbuf[pos-2] = DBC1ST;
      putnth(pos-2);

      strbuf[pos-1] = tmp2;
      atrbuf[pos-1] = DBC2ND;
      putnth(pos-1);
    }
  }else if( pos >= 3 ){
    if( atrbuf[pos-3] == SBC ){
      if( markpos == pos-2 ) /* 全角の 2byte目にマークが移動しないように */
	markpos = pos-1;

      /* 半角全角 -> 全角半角 */
      int tmp=strbuf[pos-3];
      putbs( 3 );
      strbuf[pos-3] = strbuf[pos-2];
      atrbuf[pos-3] = DBC1ST;
      putnth( pos-3 );
      
      strbuf[pos-2] = strbuf[pos-1];
      atrbuf[pos-2] = DBC2ND;
      putnth( pos-2 );

      strbuf[pos-1] = tmp;
      atrbuf[pos-1] = SBC;
      putnth( pos-1 );
    }else{
      /* 全角全角 */
      int tmp1=strbuf[pos-4];
      int tmp2=strbuf[pos-3];
      putbs(4);
      strbuf[pos-4] = strbuf[pos-2]; putnth(pos-4);
      strbuf[pos-3] = strbuf[pos-1]; putnth(pos-3);
      strbuf[pos-2] = tmp1;          putnth(pos-2);
      strbuf[pos-1] = tmp2;          putnth(pos-1);
    }
  }
}

/* 一文字挿入して、直ちに表示に反映させる。
 * 0x0000 ～ 0x00FF : SBCS とみなす。
 * 0x0100 ～ 0x01FF : 無視する(挿入しない)。
 * 0x0200 ～ 0xFFFF : DBCS とみなす。
 */
void Edlin::insert(int ch)
{
  if( (unsigned)ch > 0x1FF ){	/* DBCS */
    if( makeRoom(pos,2) != 0 )
      return;
    
    strbuf[ pos   ] = ch >> 8 ;
    atrbuf[ pos   ] = DBC1ST;
    strbuf[ pos+1 ] = ch & 255;
    atrbuf[ pos+1 ] = DBC2ND;
  }else if( (unsigned)ch < 0x100 ){ /* SBCS */
    if( makeRoom(pos,1) != 0 )
      return;
    
    strbuf[pos] = ch;
    atrbuf[pos] = SBC;
  }else{
    return;
  }
  after_repaint(0);  /* 挿入したときは、右へ動くので末端のクリアはいらない */
}

/* 制御文字を 2bytes 扱いする strlen。
 * コピーする際に「^H」という形にする為、本関数が必要。
 *	s 文字列
 * return 文字列長
 */
static int strlen2(const char *s)
{
  int len=0;
  while( *s ){
    if( 0 < *s  &&  *s < ' ' )
      len++;
    len++;
    s++;
  }
  return len;
}

/* 文字列をカーソル位置へ挿入し、表示も更新する。
 * カーソルは移動しない。
 */
void Edlin::insert_and_forward(const char *s)
{
  if( makeRoom(pos,strlen2(s)) != 0 )
    return;
  
  while( *s != '\0' ){
    if( is_kanji(*s & 255) ){
      writeDBChar( *s , *(s+1) );
      s+=2;
    }else if( 0 < *s && *s < ' ' ){
      writeDBChar( '^' , *s++ + '@' );
    }else{
      writeSBChar( *s++ );
    }
  }
  after_repaint(0);  /* 挿入したときは、右へ動くので末端のクリアはいらない */
}

/* コントロールキャラクタを入力する為のメソッド
 *	ch … キャラクターコード
 */
void Edlin::quoted_insert(int key)
{
  if( key >= 0x200 ){		/* 倍角文字 */
    if( makeRoom(pos,2) != 0 )
      return;
    writeDBChar( (key>>8)& 0xFF , key & 0xFF );
    
  }else if( key >= 0x100 ){	/* 制御文字でもキャラコードを持たないもの */
    return;
  }else if( key >= 0x20 ){	/* 半角文字 */
    if( makeRoom(pos,1) != 0 )
      return;
    writeSBChar( key );
  }else if( key >= 0 ){		/* 制御文字 */
    if( makeRoom(pos,2) != 0 )
      return;
    writeDBChar( '^' , key + '@' );
  }else{
    return;
  }
  after_repaint(0);
}

/* C-v によって入力された制御文字を本来の1byte形式に変換する。
 * 内部的には
 *	strbuf ... '^',('@'+key)
 *	atrbuf ... DBC1ST,DBC2ND
 * と倍角文字扱いになっている。
 * このメソッドは編集終了時にのみ呼ぶ。
 * これ以後の編集は正しく動作しない。
 */
void Edlin::pack()
{
  int si=0,di=0;
  int oldLength=len;

  while( si < oldLength ){
    if( strbuf[si] == '^'  &&  atrbuf[si]==DBC1ST  ){
      strbuf[di] = strbuf[++si] & 0x1F;
      atrbuf[di] = SBC;
      --len;
    }else{
      strbuf[di] = strbuf[si];
      atrbuf[di] = atrbuf[si];
    }
    ++si;  ++di;
  }
  strbuf[di] = '\0';
}

/* カーソル位置の名前の先頭位置を求める。
 * ただし、名前は、補完時のファイル名を前提としているので、
 * 「<」や「;」の直後も単語先頭とみなす。
 *	return 先頭位置
 */
int Edlin::seek_word_top()
{
  int wrdtop=0;
  int p=0;

  const char *punct=getShellEnv("NOTPATHCHAR");
  
  for(;;){
    // 空白を読みとばす。
    while( isspace(strbuf[p] & 255) ){
      if( p >= pos ){
	return wrdtop;
      }
      if( atrbuf[p] != SBC )
	p++;
      p++;
    }
    // 単語境界を設定する。 
    if(  strbuf[p]=='<' || strbuf[p]=='>' 
       || ( punct != NULL && strchr(punct,strbuf[p]) != NULL ) ){
      ++p;
    }
    wrdtop = p;
    
    // 空白以外を読みとばす。 
    while( !isspace(strbuf[p] & 255) ){
      if( p >= pos )
	return wrdtop;

      // 「+」や「;」の直後も単語境界とみなせるので、wrdtop を更新する。 
      if(   (punct != NULL  && strchr(punct,strbuf[p]) != NULL )
	 && strbuf[p+1] != '\0' ){
	wrdtop = ++p;
	continue;
      }

      if( strbuf[p] == '"' ){
	do{
	  if( atrbuf[p] != SBC )
	    p++;
	  p++;
	  if( p >= pos )
	    return wrdtop;
	}while( strbuf[p] != '"' );
      }
      if( atrbuf[p] != SBC )
	p++;
      p++;
    }
  }
}

Edlin::CompleteFunc Edlin::completeBindmap[ 0x200 ];

/* 変換型補完の、キーバインドを初期化する。
 */
void Edlin::initComplete()
{
  static int firstcalled=1;
  if( ! firstcalled  )
    return;

  firstcalled = 0;

  for(unsigned int i=0;i<numof(completeBindmap);i++)
    completeBindmap[ i ] = COMPLETE_FIX_PLUS;
  
  struct{
    int key;
    CompleteFunc func;
  } defaultBindmap[] = {
    { CTRL('G')			, COMPLETE_CANCEL },
    { CTRL('[')			, COMPLETE_CANCEL },
    { KEY(LEFT)			, COMPLETE_CANCEL },

    { KEY(UP)			, COMPLETE_PREV },
    { KEY(ALT_BACKSPACE)	, COMPLETE_PREV },
    
    { KEY(DOWN)			, COMPLETE_NEXT },
    { KEY(CTRL_TAB)		, COMPLETE_NEXT },
    { KEY(ALT_RETURN)		, COMPLETE_NEXT },
    { '\t'			, COMPLETE_NEXT },

    { KEY(RIGHT)		, COMPLETE_FIX },
    { '\r'			, COMPLETE_FIX },
    { '\n'			, COMPLETE_FIX },
  };
  
  for(unsigned int i=0;i<numof(defaultBindmap);i++){
    completeBindmap[ defaultBindmap[i].key ] = defaultBindmap[i].func;
  }
}

static struct CompleteFuncName {
  const char *name;
  Edlin::CompleteFunc func;
} completeFuncName[] = {
  { "complete_cancel"	, Edlin::COMPLETE_CANCEL },
  { "complete_fix"	, Edlin::COMPLETE_FIX },
  { "complete_next"	, Edlin::COMPLETE_NEXT },
  { "complete_prev"	, Edlin::COMPLETE_PREV },
  { "complete_default"	, Edlin::COMPLETE_FIX_PLUS },
};

/* 補完モード時のキーバインドを設定する(静的メンバ関数)
 *	key	キー名称文字列
 *	func	機能名称文字列
 * return  0:正常終了 1:キー名称不適 2:機能名称不適
 */
int Edlin::bindCompleteKey(const char *key,const char *func )
{
  initComplete();

  KeyName *keyinfo=KeyName::find(key);
  if( keyinfo == NULL )
    return 1;

  CompleteFuncName *funcPtr
    = (CompleteFuncName*)bsearch(  func
				 , completeFuncName
				 , numof(completeFuncName)
				 , sizeof(completeFuncName[0])
				 , &KeyName::compareWithTop );
  if( funcPtr == NULL )
    return 2;

  completeBindmap[ keyinfo->code ] = funcPtr->func;
  return 0;
}

/* 変換型の補完
 * return 0:補完しなかった 1:補完した
 */
int Edlin::completeFirst()
{
  initComplete();

  /* 原ファイル名(補完前のファイル名)の先頭位置・長さを求めておく */
  int fntop=seek_word_top();
  int basesize=pos-fntop;

  /* 候補リスト列挙オブジェクトと人は言う */
  Complete com;

  /* 原ファイル名が行の先頭ならば、それはコマンド名補完と人は言う */
  bool command_complete = (fntop <= 1);

  /* 原ファイル名がクォートで囲まれているか否か
   * 囲まれていなければ、後々、場合によっては
   * 囲い直す作業が必要となるわけだ。
   */
  bool quoted=false;
  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = true;
  }

  /* 補完前のファイル名を以下「原ファイル名」とする。
   *   fntop    … 原ファイル名の先頭(クォートを含まない)
   *   basesize … 原ファイル名の長さ(クォートを含まない)
   */

  /* ファイル名を、\0 で終わる形へコンバート */
  char *buffer=(char*)alloca(basesize+1);
  memcpy( buffer , &strbuf[fntop] , pos-fntop );
  buffer[ pos-fntop ] = '\0';
  
  /* 補完リストを作る。
   * フラグによって、コマンド名補完か、ファイル名補完かを選択する
   */
  int nfiles = (  command_complete 
		? com.makelist_with_path( buffer )
		: com.makelist( buffer ) );

  /* 内部コマンドの名前なども補完リストに加えておく 
   */
  nfiles += complete_hook(com);
  
  /* 候補が無いが、名前中にワイルドカードが含まれている場合は、
   * ワイルドカードにマッチするファイル名を補完リストへ加える
   * さもなければ、おしまい。
   */
  if( nfiles <= 0 ){
    if(    strpbrk(buffer,"*?") == NULL
       || (nfiles+=com.makelist_with_wildcard( buffer )) <= 0 ){
      alert();
      return 0;
    }
  }
  com.sort();

  /* basesize は、総フルパス分の長さとなる。
   * com.get_fname_common_length() は、ファイル名の共通部分の長さ。
   */
  backward( com.get_fname_common_length() );

  /* ファイルを一つずつ、表示してゆくループ…見りゃ分かるって */
  Complete::Cursor cur(com);
  for(;;){
    if( cur->attr & A_DIR ){
      message(  "%s%c"
	      , cur->name
	      , com.get_split_char() ?: complete_tail_char );
    }else{
      message("%s",cur->name );
    }
    CompleteFunc completeFunc;
    unsigned int key=::getkey();
    if( key >= numof(completeBindmap) )
      completeFunc = COMPLETE_FIX_PLUS;
    else
      completeFunc = completeBindmap[ key ];
    
    switch( completeFunc ){
    case COMPLETE_CANCEL:
      cleanmsg();
      forward( com.get_fname_common_length() );
      
      return 0;
      
    case COMPLETE_PREV:
      cur.sotomawari();
      break;
      
    case COMPLETE_NEXT:
      cur.utimawari();
      break;
      
    default:
      ::ungetkey(key);
	/* continue to next case */
      
    case COMPLETE_FIX:
      cleanmsg();
      
      if( !quoted && strpbrk( cur->name , " ^!") != NULL ){
	insert('"');
	quoted = true;
	forward();
      }
      /* 補完のベース文字列も、大文字・小文字を合わせるために
       * 上書きを行う */
      const char *sp=cur->name;
      for(int i=0;i<com.get_fname_common_length();i++ ){
	strbuf[pos] = *sp++; putnth(pos++);
      }
      
      insert_and_forward( sp );
      
      if( cur->attr & A_DIR ){
	insert( com.get_split_char() ?: complete_tail_char );
	forward();
      }
      if( quoted ){
	insert('"');
	forward();
      }
      return 1;
    } /* end-switch */
  }/* end-for(;;) */
}

int Edlin::complete()
{
  int fntop=seek_word_top();
  int basesize=pos-fntop;

  Complete com;

  int  command_complete = (fntop <= 1);
  int  quoted=false;
  
  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = 1;
  }
  /* ファイル名を、\0 で終わる形へコンバート */
  char *buffer=(char*)alloca(basesize+1);
  memcpy( buffer , &strbuf[fntop] , pos-fntop );
  buffer[ pos-fntop ] = '\0';

  /* 補完リストを作る。
   * フラグによって、コマンド名補完か、ファイル名補完かを選択する
   */

  int nfiles = (  command_complete
		? com.makelist_with_path( buffer ) 
		: com.makelist( buffer ) );

  nfiles += complete_hook(com);

  if( nfiles <= 0 ){
    alert();
    return 0;
  }

  com.sort();

  backward( basesize );
  
  const char *nextstr=com.nextchar();

  if( !quoted && strpbrk(nextstr," ^!") != NULL ){
    insert('"');
    quoted = 1;
    forward();
  }

  forward( basesize-com.get_fname_common_length() );

  const char *realname=com.get_real_name1();
  for(int i=0 ; i<com.get_fname_common_length(); i++ ){
    strbuf[pos] = *realname++;
    putnth(pos++);
  }

  insert_and_forward(nextstr);

  if( nfiles == 1 ){
    Complete::Cursor cursor(com);
    
    if( cursor->attr & A_DIR ){
      insert( com.get_split_char() ?: complete_tail_char );
    }else{
      if( quoted ){
	insert('"');
	forward(); 
      }
      insert(' ');
    }
    forward();
  }
  return nfiles;
}

/* フルパス変換という奴。
 * これなんか、シェルクラス(Shell)に入れた方がよさそうな…)
 */
int Edlin::complete_to_fullpath(const char *header)
{
  /* 「nyaos ■」のように間に空白がある場合に、
   * この空白を無視するわけだ */
  int spaces=0;
  if( strbuf[pos-1] == ' ' )
    spaces=backward();

  int fntop=seek_word_top();
  int basesize=pos-fntop;
  bool quoted=false;
  
  StrBuffer sbuf;
  // char *buffer=(char*)alloca(basesize*3);

  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = true;
  }
  // char *bp=buffer;
  /* ローカルバッファに原ファイル名を展開する。
   * この時、チルダや ... も展開する。
   */
  if( strbuf[fntop] == '~' ){
    const char *home=getShellEnv("HOME");
    if( home != NULL ){
      // ++bp;
      while( *home != '\0' ){
	// *bp++ = *home++;
	sbuf << *home++;
      }
      ++fntop; // skip tilda.
    }
  }else if( strbuf[fntop]=='.' && strbuf[fntop+1]=='.' ){
    // *bp++ = strbuf[fntop++];
    // *bp++ = strbuf[fntop++];
    sbuf << strbuf[fntop++];
    sbuf << strbuf[fntop++];
    while( strbuf[fntop] == '.' ){
      sbuf << "\\..";
      // *bp++ = '\\';
      // *bp++ = '.';
      // *bp++ = '.';
      ++fntop;
    }
  }
  while( fntop < pos ){
    sbuf << strbuf[fntop++];
    // *bp++ = strbuf[fntop++];
  }
  // *bp = '\0';

  // フルパスを得る。得られなかったら、終了
  char fullpath[ FILENAME_MAX ];
  if( _fullpath( fullpath , (const char *)sbuf , sizeof(fullpath) ) != 0 ){
    while( spaces > 0 )
      spaces -= forward();
    return 0;
  }

  int len_fullpath = strlen(fullpath);
  int delta = len_fullpath-basesize;

  // フルパスの長さがバッファに収まらない場合も終了
  if( max-len <= delta-3 ){
    while( spaces > 0 )
      spaces -= forward();
    return 0;
  }
  
  // カーソルを単語先頭へ移動
  for(int i=0 ; i<basesize ; )
    i += backward();

  // 全体長さを調整
  if( makeRoom(pos,delta) != 0 )
    return 0;
  
  // 単語先頭に「”」が無いけれども「”」で囲まなくてはいけない文字がある
  // 場合、ここで「”」を加える。
  // header が NULL で無い場合は、| が : の代わりに入ってくるので必須となる。
  if( !quoted && (strpbrk(fullpath," ^!") != NULL || header != NULL ) ){
    insert('"');
    quoted = 1;
    forward();
  }

  // URL 型にする場合などの処理
  if( header != NULL )
    insert_and_forward(header);
  
  // 新しいパスを書き書き
  for(int i=0 ; i<len_fullpath ; i++ ){
    if( is_kanji(fullpath[ i ] ) ){
      writeDBChar( fullpath[i] , fullpath[i+1] );
      ++i;
    }else{
      writeSBChar( fullpath[i] );
    }
  }

  // 新しいパス以降の文字列をここで表示
  after_repaint(delta >= 0 ? 0 : -delta );
  
  if( header != NULL ){
    insert('"');
    forward();
  }

  while( spaces > 0 )
    spaces -= forward();
  return 1;
}

#if 0
void Edlin::insert(int ch1,int ch2)
{
  if( makeRoom(pos,2) != 0 )
    return;

  strbuf[ pos   ] = ch1;
  atrbuf[ pos   ] = DBC1ST;
  strbuf[ pos+1 ] = ch2;
  atrbuf[ pos+1 ] = DBC2ND;
  
  after_repaint(0);
}
#endif

void Edlin::erase()
{
  /* 末尾では動作せず */
  if( pos >= len )
    return;

  int ndels=(atrbuf[pos]==SBC ? 1 : 2);
  
  if( makeRoom(pos,-ndels) != 0 )
    return;

  after_repaint(ndels);
}

void Edlin::repaint(int termclear)
{
  /* 表示している一文字目までカーソルを戻す */
  putbs( pos );

  int i=0;
  while( i < len )
    putnth( i++ );
  
  if( has_marked  &&  markpos == len  ){
    putchrs(mark_on);
    putchr(' ');
    putchrs(mark_off);
  }else{
    putchr(' ');
    --termclear;
  }
  ++i;

  if( termclear >= 0 ){
    while( termclear-- > 0 ){
      putchr( ' ' );
      i++;
    }
  }else{
    putel();
  }
  putbs( i - pos );
}

void Edlin::after_repaint(int termclear)
{
  int i=0;
  if( pos < len ){
    while( pos+i < len )
      putnth( pos+i++ );
  }

  if( has_marked  &&  markpos == len ){
    putchrs(mark_on);
    putchr(' ');
    putchrs(mark_off);
  }else{
    putchr(' ');
    --termclear;
  }
  ++i;

  if( termclear >= 0 ){
    while( termclear-- > 0 ){
      putchr( ' ' );
      i++;
    }
  }else{
    putel();
  }
  putbs( i );
}
/* cmd.exeでkeys on時のCtrl-Home同様に、カーソル位置手前から行頭を消す。
 * ディフォルトではキーバインドしない。活性化するには、例えば、
 *	bindkey CTRL_END  kill_line
 *	bindkey CTRL_HOME kill_top_of_line
 */
void Edlin::erasebol()
{
  if( pos == 0 )
    return;

  int i=pos;  
  if( makeRoom(0,-pos) != 0 )
    return;

  pos = 0;
  putbs(i);
  after_repaint(i);
}

void Edlin::eraseline()
{
  /* 2行にまたがる場合、Eraselineコードが1行分しか効かない */
  int i=0;
  while( pos+i < len+1 ){ /* '+1' は、末尾のマークの為 */
    putchr(' ');
    i++;
  }
  putbs(i);

  len = pos ;
  if( markpos > pos ){
    markpos = pos;
    putchrs(mark_on);
    putchr(' ');
    putchrs(mark_off);
    putbs(1);
  }

  strbuf[ pos ] = '\0';
  atrbuf[ pos ] = SBC;
}

void Edlin::forward_word()
{
  int nextpos = pos;
  /* 単語の読み飛ばし */
  while( nextpos < len  &&  !isspace(strbuf[nextpos] & 255) )
    ++nextpos;

  /* 空白の読み飛ばし */
  while( nextpos < len  &&  isspace(strbuf[nextpos] & 255) )
    ++nextpos;
  
  while( pos < nextpos )
    putnth( pos++ );
}

void Edlin::backward_word()
{
  int nextpos=pos;
  /* 空白の読み飛ばし */
  while( nextpos > 0  &&  is_space(strbuf[--nextpos]) )
    ;
  /* 非空白文字の読み飛ばし */
  while( nextpos > 0  &&  !is_space(strbuf[nextpos-1]) )
    --nextpos;

  putbs( pos-nextpos );
  pos = nextpos;
}

int Edlin::forward()
{
  /* 同じ文字の二度打ちによる右移動 */
  if( pos+1 <= len  &&  atrbuf[pos] != DBC1ST ){
    /* 半角 */
    putnth( pos++ );
    return 1;
  }else if( pos+2 <= len ){
    /* 全角 */
    putnth( pos++ );
    putnth( pos++ );
    return 2;
  }
  return 0;
}

int Edlin::forward(int w)
{
  int i;
  for(i=0 ; i<w ; i+=forward() ){
    if( pos >= len )
      break;
  }
  return i;
}

int Edlin::backward(int w)
{
  int i;
  for(i=0 ; i<w ; i+=backward() ){
    if( pos <= 0 )
      break;
  }
  return i;
}


int Edlin::backward()
{
  if( 0 < pos  &&  atrbuf[pos-1] == SBC ){
    --pos;
    putbs(1);
    return 1;
  }else if( 2 <= pos ){
    pos -= 2;
    putbs(2);
    return 2;
  }
  return 0;
}

Edlin::Status Edlin::go_ahead()
{
  putbs( pos );
  pos = 0;
  return CONTINUE;
}

Edlin::Status Edlin::go_tail()
{
  /* 全文字列が、画面中にでている場合、右移動だけでよい */
  while( pos < len )
    putnth( pos++ );
  return CONTINUE;
}

void Edlin::clean_up()
{
  int cleaning_size;
  if( msgsize != 0 ){
    cleaning_size = msgsize;
    putbs( msgsize );
    msgsize = 0;
  }else{
    putbs( pos );
    cleaning_size = len;
    if( has_marked )
      ++cleaning_size;
  }

  for(int i=0; i<cleaning_size ; i++ )
    putchr(' ');
  
  putbs( cleaning_size );
  markpos = len = pos = 0;
  has_marked = false;
  strbuf[ 0 ] = '\0';
  atrbuf[ 0 ] = SBC;
}

int Edlin::message(const char *fmt,...) /* ウインドウモード未対応 */
{
  char msg[1024];
  va_list vp;
  va_start(vp,fmt);

  /* msgsize は以前に表示したメッセージの長さ */
  if( msgsize > 0 )
    putbs(msgsize);

  (void)vsnprintf(msg,sizeof(msg),fmt,vp);  
  va_end(vp);

  int columns=0; /* 実際の表示桁数(エスケープシーケンス部分を除く) */
  int escape=0;  /* エスケープシーケンス内なら not 0 */

  /* メッセージ本体を表示 */
  for(const char *sp=msg ; *sp != '\0' ; sp++ ){
    if( *sp == '\x1b' )
      escape = 1;
    if( ! escape )
      columns++;
    if( isalpha(*sp & 255) )
      escape = 0;

    if( 0 < *sp && *sp < ' '  &&  ! escape ){
      putchr('^');
      putchr('@'+*sp);
      columns++;
    }else{
      putchr(*sp);
    }
  }

  /* 過去のメッセージの末尾を削除 */
  if( msgsize > columns ){
    if( pos+columns < len  &&  atrbuf[pos+columns] != DBC2ND )
      putnth( pos+columns );
    else
      putchr( ' ' );
    
    for(int i=columns+1 ; i < msgsize ; i++ ){
      if( pos+i < len  )
	putnth( pos+i );
      else
	putchr(' ');
    }
    putbs( msgsize - columns );
  }
  msgsize = columns;
  
  fflush(stdout);
  return len;
}

void Edlin::cleanmsg() /* ウインドウモード未対応 */
{
  /* 一時的に表示していたメッセージを消去し、
     本来表示すべき、入力文字列を再表示する */
  
  if( msgsize > 0 ){
    putbs( msgsize );
    
    int i=0;
    while( pos+i < len  &&  i<msgsize ){
      if( atrbuf[pos+i] != SBC )
	putnth( pos+i++ );
      putnth( pos+i++ );
    }

    while( i < msgsize ){
      putchr(' ');
      i++;
    }
    putbs( i );
  }
  msgsize = 0;
}

void Edlin::bottom_message( const char *fmt ,...)
{
  extern int screen_width , screen_height , option_vio_cursor_control;
  int bs=0;

  if( option_vio_cursor_control ){
    unsigned short X,Y;

    VioGetCurPos( &Y , &X , 0 );
    if( Y >= screen_height-1 ){
      static BYTE cell[2]={ ' ',0x00 };
      VioScrollUp(0,0,screen_height-1,screen_width-1,1, cell , 0);
      fputs("\033[1A",stdout);
    }

  }else{
    /* 画面サイズ分カーソルを進めることによって、
     * 次の行へ移動する。
     */
    
    if( pos+msgsize < len && atrbuf[pos+msgsize] == DBC2ND ){
      putnth( pos+msgsize );
      ++bs;
    }
    
    for( ; bs < screen_width ; bs++ ){
      if( pos+msgsize+bs < len )
	putnth( pos+msgsize+bs );
      else
	putchr( ' ' );
    }    
    fflush(stdout);
  }
  printf("\033[s\033[%d;1H" , screen_height );
  
  va_list vp;
  va_start(vp,fmt);
  vprintf(fmt,vp);
  va_end(vp);

  printf("\033[K\033[u");
  fflush(stderr);

  if( ! option_vio_cursor_control )
    putbs( bs );

  bottom_msgsize = 1;
}

void Edlin::clean_bottom()
{
  extern int screen_height;
  
  if( bottom_msgsize > 0 ){
    printf( "\033[s\033[%d;1H\033[K\033[u" , screen_height );
    bottom_msgsize = 0;
  }
}

void Edlin::cut()
{
  int at,length;

  if( pos < markpos ){
    at = pos;
    length = markpos - pos;
    markpos = pos;
  }else if( pos > markpos ){
    at = markpos;
    length = pos - markpos;
    putbs( length );
    pos = markpos;
  }else{
    return;
  }
  makeRoom( at , -length );
  after_repaint( length );
}

void Edlin::locate(int x)
{
  if( x > pos ){
    while( pos < x )
      putnth( pos++ );
  }else if( x < pos ){
    putbs( pos-x );
  }
  pos = x;
}
