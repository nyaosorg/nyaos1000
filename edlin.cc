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

#define KEY(x)	(0x100 | K_##x )
#define CTRL(x)	((x) & 0x1F )

Edlin::Edlin()
{
  pos = len = markpos = msgsize = 0;
  max = DEFAULT_BUFFER_SIZE;

  strbuf = (char*)malloc(max);
  atrbuf = (char*)malloc(max);
}

Edlin::~Edlin()
{
  if( strbuf ) free(strbuf);
  if( atrbuf ) free(atrbuf);
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
      putchr( strbuf[pos-2] = strbuf[pos-1] );
      putchr( strbuf[pos-1] = tmp );
    }else{
      /* 全角半角 -> 半角全角 */
      int tmp1=strbuf[pos-3];
      int tmp2=strbuf[pos-2];
      putbs( 3 );
      putchr( strbuf[pos-3] = strbuf[pos-1] );
      atrbuf[pos-3] = SBC;
      putchr( strbuf[pos-2] = tmp1 );
      atrbuf[pos-2] = DBC1ST;
      putchr( strbuf[pos-1] = tmp2 );
      atrbuf[pos-1] = DBC2ND;

      if( markpos == pos-1 ) /* 全角の 2byte目にマークが移動しないように */
	markpos = pos-2;
    }
  }else if( pos >= 3 ){
    if( atrbuf[pos-3] == SBC ){
      /* 半角全角 -> 全角半角 */
      int tmp=strbuf[pos-3];
      putbs( 3 );
      putchr( strbuf[pos-3] = strbuf[pos-2] );
      atrbuf[pos-3] = DBC1ST;
      putchr( strbuf[pos-2] = strbuf[pos-1] );
      atrbuf[pos-2] = DBC2ND;
      putchr( strbuf[pos-1] = tmp );
      atrbuf[pos-1] = SBC;

      if( markpos == pos-2 ) /* 全角の 2byte目にマークが移動しないように */
	markpos = pos-1;
    }else{
      /* 全角全角 */
      int tmp1=strbuf[pos-4];
      int tmp2=strbuf[pos-3];
      putbs(4);
      putchr( strbuf[pos-4] = strbuf[pos-2] );
      putchr( strbuf[pos-3] = strbuf[pos-1] );
      putchr( strbuf[pos-2] = tmp1 );
      putchr( strbuf[pos-1] = tmp2 );
    }
  }
}

void Edlin::insert(int ch)
{
  if( ch > 0x1FF ){
    insert(ch>>8 , ch & 0xFF);
    return;
  }

  if( makeRoom(pos,1) != 0 )
    return;

  strbuf[pos] = ch;
  atrbuf[pos] = SBC;
  
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
    if( is_kanji(*s) ){
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
    if( strbuf[p]=='<' || strbuf[p]=='>' || strbuf[p]=='+' || strbuf[p]=='-' )
      ++p;
    wrdtop = p;
    
    // 空白以外を読みとばす。 
    while( !isspace(strbuf[p] & 255) ){
      if( p >= pos )
	return wrdtop;

      // 「+」や「;」の直後も単語境界とみなせるので、wrdtop を更新する。 
      if(   (strbuf[p]=='+' || strbuf[p]==';' || strbuf[p]=='=')
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

  int fntop=seek_word_top();
  int basesize=pos-fntop;

  Complete com;

  char *buffer=(char*)alloca(basesize+1);
  int  command_complete = (fntop <= 1);
  int  quoted=false;
  
  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = 1;
  }
  
  char *bp=buffer;
  while( fntop < pos )
    *bp++ = strbuf[fntop++];
  *bp = '\0';

  int nfiles = ( command_complete
		? com.makelist_with_path( buffer ) 
		: com.makelist( buffer ) );

  nfiles += complete_hook(com);

  if( nfiles <= 0 ){
    alert();
    return 0;
  }

  for(;;){
    struct filelist *cur=com.findfirst();
    while( cur != NULL ){
      if( cur->attr & A_DIR ){
	message(  "%s%c"
		, cur->name+com.get_fname_common_length()
		, com.get_split_char() ?: complete_tail_char );
      }else{
	message("%s",cur->name+com.get_fname_common_length() );
      }
      CompleteFunc completeFunc;
      unsigned int key=::getkey();
      if( key >= numof(completeBindmap) )
	completeFunc = COMPLETE_FIX_PLUS;
      else
	completeFunc = completeBindmap[ key ];
      
      switch( completeFunc ){
      case COMPLETE_CANCEL:
	/*  case '\007':  case '\033':   case KEY(LEFT):*/
	cleanmsg();
	return 0;

      case COMPLETE_PREV:
	/* case KEY(UP):   case KEY(ALT_BACKSPACE): */
	cur = com.findprev();
	break;

      case COMPLETE_NEXT:
	/* case KEY(DOWN): case KEY(CTRL_TAB): case KEY(ALT_RETURN):
	 * case '\t': */
	cur = com.findnext();
	break;
	
      default:
	::ungetkey(key);
	/* continue to next case */
	
      case COMPLETE_FIX:
	/* case KEY(RIGHT): case '\r':  case '\n':*/

	cleanmsg();
	for(int i=0;i<basesize;)
	  i += backward();

	if( !quoted && strpbrk( cur->name , " ^!") != NULL ){
	  insert('"');
	  quoted = 1;
	  forward();
	}
	for(int i=0;i<basesize-com.get_fname_common_length(); )
	  i += forward();
	
	/* 補完のベース文字列も、大文字・小文字を合わせるために
	 * 上書きを行う */
	const char *sp=cur->name;
	for(int i=0;i<com.get_fname_common_length();i++ )
	  putchr( strbuf[pos++] = *sp++ );
	
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

    }/* end-while */
  }/* end-for */
}

int Edlin::complete()
{
  int fntop=seek_word_top();
  int basesize=pos-fntop;

  Complete com;

  char *buffer=(char*)alloca(basesize+1);
  int  command_complete = (fntop <= 1);
  int  quoted=false;
  
  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = 1;
  }
  
  char *bp=buffer;
  while( fntop < pos )
    *bp++ = strbuf[fntop++];
  *bp = '\0';

  int nfiles = ( command_complete
		? com.makelist_with_path( buffer ) 
		: com.makelist( buffer ) );

  nfiles += complete_hook(com);

  if( nfiles <= 0 ){
    alert();
    return 0;
  }

  for(int i=0 ; i<basesize ;  )
    i += backward();
  
  const char *nextstr=com.nextchar();

  if( !quoted && strpbrk(nextstr," ^!") != NULL ){
    insert('"');
    quoted = 1;
    forward();
  }

  for(int i=0 ; i<basesize-com.get_fname_common_length() ;  )
    i +=forward();

  const char *realname=com.get_real_name1();
  for(int i=0 ; i<com.get_fname_common_length(); i++ )
    putchr( strbuf[pos++] = *realname++ );

  insert_and_forward(nextstr);

  if( nfiles == 1 ){
    if( com.findfirst()->attr & A_DIR ){
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

int Edlin::complete_to_fullpath(const char *header)
{
  /* 「nyaos ■」のように間に空白がある場合に、
   * この空白を無視する処理。
   */
  int spaces=0;
  if( strbuf[pos-1] == ' ' )
    spaces=backward();

  int fntop=seek_word_top();
  int basesize=pos-fntop;
  int  quoted=false;
  
  char *buffer=(char*)alloca(basesize+1);
  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = 1;
  }
  char *bp=buffer;
  while( fntop < pos )
    *bp++ = strbuf[fntop++];
  *bp = '\0';

  // フルパスを得る。得られなかったら、終了
  char fullpath[ FILENAME_MAX ];
  if( _fullpath( fullpath , buffer , sizeof(fullpath) ) != 0 ){
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
    putchr( strbuf[i++] );

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
      putchr( strbuf[pos + i++] );
  }
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
  while( pos+i<len ){
    putchr(' ');
    i++;
  }
  putbs(i);

  len = pos ;
  if( markpos > pos )
    markpos = pos;

  strbuf[ pos ] = '\0';
  atrbuf[ pos ] = SBC;
}

void Edlin::forward_word()
{
  int nextpos = pos;
  /* 単語の読み飛ばし */
  while( !isspace(strbuf[nextpos] & 255) ){
    if( nextpos >= len )
      return;
    ++nextpos;
  }
  /* 空白の読み飛ばし */
  while( isspace(strbuf[nextpos] & 255) ){
    if( nextpos >= len )
      return;
    ++nextpos;
  }
  while( pos < nextpos )
    putchr(strbuf[pos++]);
}
void Edlin::backward_word()
{
  int nextpos=pos;
  /* 空白の読み飛ばし */
  while( nextpos > 0  &&  is_space(strbuf[--nextpos]) )
    ;
  while( nextpos > 0  &&  !is_space(strbuf[nextpos-1]) )
    --nextpos;

  putbs( pos-nextpos );
  pos = nextpos;
}

int Edlin::forward()
{
  if( pos+1 <= len  &&  atrbuf[pos] == SBC ){
    
    /* 同じ文字の二度打ちによる右移動 */
    putchr( strbuf[pos++] );
    return 1;
  }else if( pos+2 <= len ){

    putchr( strbuf[pos++] );
    putchr( strbuf[pos++] );
    return 2;
  }
  return 0;
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

void Edlin::go_ahead()
{
  putbs( pos );
  pos = 0;
}


void Edlin::go_tail()
{
  /* 全文字列が、画面中にでている場合、右移動だけでよい */
  while( pos < len )
    putchr( strbuf[pos++] );
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
  }

  for(int i=0; i<cleaning_size ; i++ )
    putchr(' ');
  
  putbs( cleaning_size );
  markpos = len = pos = 0;
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

  (void)vsprintf(msg,fmt,vp);  
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
      putchr( strbuf[pos+columns] );
    else
      putchr( ' ' );
    
    for(int i=columns+1 ; i < msgsize ; i++ ){
      if( pos+i < len  )
	putchr( strbuf[pos+i] );
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
	putchr( strbuf[pos + i++] );
      putchr( strbuf[pos + i++] );
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
      putchr( strbuf[ pos+msgsize ] );
      ++bs;
    }
    
    for( ; bs < screen_width ; bs++ ){
      if( pos+msgsize+bs < len )
	putchr( strbuf[pos+msgsize+bs] );
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
      putchr(strbuf[pos++]);
  }else if( x < pos ){
    putbs( pos-x );
  }
  pos = x;
}
