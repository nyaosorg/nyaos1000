#include <stdlib.h>      /*** for _osmode             ***/
#include <ctype.h>       /*** for isspace             ***/
#include <sys/nls.h>     /*** for _nls_is_dbcs_lead() ***/
#include <dos.h>         /*** for _int86              ***/
#include <sys/kbdscan.h> /*** for _read_kbd()         ***/
#include <string.h>

#include "Edlin.h"
#include "complete.h"

int Edlin::complete_tail_char='\\';

void Edlin::push()
{
  if( len <= 0 )
    return;

  Edlin::History *tmp=(Edlin::History*)malloc(sizeof(Edlin::History)+len);
  if( tmp == NULL )
    return;

  memcpy(tmp->buffer , strbuf , len );
  tmp->buffer[len] = '\0';
  tmp->prev = history;
  tmp->next = NULL;

  if( history != NULL )
    history->next = tmp;
  history = tmp;
}

Edlin::~Edlin()
{
  while( history != 0 ){
    History *prev=history->prev;
    free(history);
    history = prev;
  }
  delete atrbuf;
}

void Edlin::right(int n)
{
  /* 右へ top が移動する --> 全体が左へ移動する。*/
  putbs( pos-top );

  while( n > 0 ){
    if( atrbuf[top] == DBC1ST ){
      top += 2;
      n -= 2;
    }else if( top < pos ){
      top++;
      n--;
    }
  }

  int i=0;
  while( i<windowsize &&  top+i < len )
    putchr( strbuf[top+(i++)] );
  
  putel();
  putbs( i-(pos-top) );
}

void Edlin::left(int n)
{
  /* 左へtopを移動する ---> 右へ全体が動く */

  /* 最初にカーソルを戻しておく */
  putbs( pos-top );

  /* n 文字分 top を後退させる */
  while( n > 0  &&  top > 0 ){
    if( atrbuf[top-1] == SBC ){
      top--;
      n--;
    }else{
      top -= 2;
      n -= 2;
    }
  }
  int i=0;
  while( i<windowsize  &&  top+i < len )
    putchr( strbuf[top + i++] );
  putel();
  putbs( i-(pos-top) );
}

void Edlin::insert(int ch)
{
  if( ch > 0x1FF ){
    insert(ch>>8 , ch & 0xFF);
    return;
  }

  if( len >= max )
    return;

  int i;
  for(i=len ; i>pos ; i-- ){
    strbuf[ i ] = strbuf[i-1];
    atrbuf[ i ] = atrbuf[i-1];
  }
  strbuf[ i ] = ch;
  atrbuf[ i ] = SBC;
  strbuf[++len] = '\0';
  atrbuf[  len] = SBC;

  after_repaint(0);  /* 挿入したときは、右へ動くので末端のクリアはいらない */
}

void Edlin::insert_and_forward(const char *s)
{
  int shift=strlen(s);
  for(int i=len ; i >= pos ; i-- ){
    strbuf[ i+shift ] = strbuf[ i ];
    atrbuf[ i+shift ] = atrbuf[ i ];
  }
  len += shift;
  
  while( *s != '\0' ){
    if( _nls_is_dbcs_lead( *s & 0xFF ) ){
      putchr( strbuf[ pos ] = *s++ );
      atrbuf[ pos++ ] = DBC1ST;
      putchr( strbuf[ pos ] = *s++ );
      atrbuf[ pos++ ] = DBC2ND;
    }else{
      putchr( strbuf[ pos ] = *s++ );
      atrbuf[ pos++ ] = SBC;
    }
  }
  after_repaint(0);  /* 挿入したときは、右へ動くので末端のクリアはいらない */
}

int Edlin::seek_word_top()
{
  int wrdtop=0;
  int p=0;

  for(;;){
    while( isspace(strbuf[p]) ){
      if( p >= pos ){
	return wrdtop;
      }
      if( atrbuf[p] != SBC )
	p++;
      p++;
    }
    wrdtop = p;
    while( !isspace(strbuf[p]) ){
      if( p >= pos ){
	return wrdtop;
      }
      if( strbuf[p] == '"' ){
	do{
	  if( atrbuf[p] != SBC )
	    p++;
	  p++;
	  if( p >= pos ){
	    return wrdtop;
	  }
	}while( strbuf[p] != '"' );
      }
      if( atrbuf[p] != SBC )
	p++;
      p++;
    }
  }
}

void Edlin::complete_core(int fntop,int basesize)
{
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

  if( nfiles <= 0 ){
    alert();
    return;
  }

  if( nfiles == 1 ){
    const char *nextstr=com.nextchar();

    for(int i=0 ; i<basesize ;  )
      i += backward();

    if( !quoted && (   strchr(nextstr,' ') != NULL
		    || strchr(nextstr,'^') != NULL) ){
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

    if( com.findfirst()->attr & A_DIR ){
      insert( complete_tail_char );
    }else{
      if( quoted ){
	insert('"');
	forward(); 
      }
      insert(' ');
    }
    forward();
  }else{
    const char *nxtstr = com.nextchar();
    if( nxtstr == NULL || nxtstr[0]=='\0' )
      alert();
    else
      insert_and_forward(nxtstr);
  }
}

void Edlin::complete()
{
  int fntop=seek_word_top();
  int basesize=pos-fntop;
  complete_core(fntop,basesize);
}

void Edlin::insert(int ch1,int ch2)
{
  if( len-1 > max )
    return;

  for(int i=len+1 ; i>=pos+2 ; i-- ){
    strbuf[ i ] = strbuf[i-2];
    atrbuf[ i ] = atrbuf[i-2];
  }
  strbuf[ pos   ] = ch1;
  atrbuf[ pos   ] = DBC1ST;
  strbuf[ pos+1 ] = ch2;
  atrbuf[ pos+1 ] = DBC2ND;
  strbuf[ len+=2] = '\0';
  atrbuf[ len   ] = SBC;

  after_repaint(0);
}

void Edlin::erase()
{
  /* 末尾では動作せず */
  if( pos >= len )
    return;

  int ndels=(atrbuf[pos]==SBC ? 1 : 2);
  
  len -= ndels;
  for(int i=pos ; i<len ; i++ ){
    strbuf[ i ] = strbuf[i+ndels];
    atrbuf[ i ] = atrbuf[i+ndels];
  }
  strbuf[ len ] = '\0';
  atrbuf[ len ] = SBC;

  after_repaint(ndels);
}

void Edlin::repaint(int termclear)
{
  /* 表示している一文字目までカーソルを戻す */
  putbs( pos-top );

  int i=0;
  while( i<windowsize  &&  top+i < len )
    putchr( strbuf[top+i++] );

  if( termclear >= 0 ){
    while( termclear-- > 0 ){
      putchr( ' ' );
      i++;
    }
  }else{
    putel();
  }
  putbs( i - (pos-top) );
}

void Edlin::after_repaint(int termclear)
{
  int i=0;
  if( pos < len ){
    while( pos+i < len ){
      if( i == windowsize-1 ){
	if( atrbuf[pos+i] != SBC ){
	  /* 末端で漢字の左側だけが表示されそうな場合は、空白を表示 */
	  putchr(' ');
	}else{
	  putchr(strbuf[pos+i]);
	}
	i++;
	break;
      }
      putchr( strbuf[pos + i++] );
    }
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
  strbuf[ pos ] = '\0';
  atrbuf[ pos ] = SBC;
#if 0
  putel();
#endif
}

void Edlin::forward_word()
{
  int nextpos = pos;
  /* 単語の読み飛ばし */
  while( !isspace(strbuf[nextpos]) ){
    if( nextpos >= len )
      return;
    ++nextpos;
  }
  /* 空白の読み飛ばし */
  while( isspace(strbuf[nextpos]) ){
    if( nextpos >= len )
      return;
    ++nextpos;
  }
  if( nextpos < top+windowsize ){
    /* スクロールの必要なし */
    while( pos < nextpos )
      putchr(strbuf[pos++]);
  }else{
    /* スクロールしなければならない
     * で、windowsizeは有限の値と仮定できるわけだ。
     */
    putbs( pos-top );
    top = nextpos - windowsize;
    if( atrbuf[top] == DBC2ND )
      top++;

    int i=0;
    while( i < windowsize ){
      if( top+i >= len ){
	while( i++ < windowsize )
	  putchr(' ');
	break;
      }
      putchr( strbuf[top+i++] );
    }
    putbs( top+windowsize-(nextpos=pos) );
  }
}
void Edlin::backward_word()
{
  int nextpos=pos;
  /* 空白の読み飛ばし */
  while( nextpos > 0  &&  isspace(strbuf[--nextpos]) )
    ;
  while( nextpos > 0  &&  !isspace(strbuf[nextpos-1]) )
    --nextpos;

  if( top <= nextpos ){
    /* スクロールの必要なし */
    putbs( pos-nextpos );
    pos = nextpos;
  }else{
    /* 右方向へのスクロールの必要があるが、右端をクリアする必要はなし */
    putbs( pos-top );
    top = pos = nextpos;
    int i=0;
    while( i < windowsize && top+i < len )
      putchr( strbuf[top+i] );
    putbs(i);
  }
}

int Edlin::forward()
{
  if( pos+1 <= len  &&  atrbuf[pos] == SBC ){
    if( pos+1 >= top+windowsize )
      right(1);
    
    /* 同じ文字の二度打ちによる右移動 */
    putchr( strbuf[pos++] );
    return 1;
  }else if( pos+2 <= len ){
    if( pos+2 >= top+windowsize )
      right(2);

    putchr( strbuf[pos++] );
    putchr( strbuf[pos++] );
    return 2;
  }
  return 0;
}

int Edlin::backward()
{
  if( 0 < pos  &&  atrbuf[pos-1] == SBC ){
    if( pos-1 < top )
      left(1);
    --pos;
    putbs(1);
    return 1;
  }else if( 2 <= pos ){
    if( pos-2 < top )
      left(2);
    pos -= 2;
    putbs(2);
    return 2;
  }
  return 0;
}

void Edlin::go_ahead()
{
  putbs( pos-top );
  if( top != 0 ){
    top = pos = 0;
    /* 先頭に行く場合、表示文字列が長くなる場合はあっても、
     * 短くなる場合はない */
    repaint(0);
  }else{
    pos = 0;
  }
}


void Edlin::go_tail()
{
  if( len < windowsize ){
    /* 全文字列が、画面中にでている場合、右移動だけでよい */
    if( top != 0 ){
      putbs( pos-top );
      top = pos = 0;
    }
    while( pos < len )
      putchr( strbuf[pos++] );
  }else{
    putbs( pos-top );
    top = pos = len-windowsize;
    while( pos < len )
      putchr( strbuf[pos++] );
  }
}

void Edlin::clean_up()
{
  putbs( pos-top );
  int i=0;
  while( i < len  &&  i < windowsize ){
    putchr(' ');
    i++;
  }
  putbs( i );
  top = len = pos = 0;
  strbuf[ 0 ] = '\0';
  atrbuf[ 0 ] = SBC;
}

int get86key(int wait)
{
  /* DOSでは_read_kbdで、漢字の第二バイト目が取得できない。*/
  if( _osmode == DOS_MODE ){
    union REGS regs;
    regs.h.ah = 0x7;
    return _int86( 0x21, &regs , &regs ) & 0xFF ;
  }
  return _read_kbd(0,wait,0);
}

int Edlin::getkey(int wait)
{
  int ch = (get86key(wait) & 0xFF );
  if( ch == 0 )
    ch = (get86key(wait)|0x100);
  else if( _nls_is_dbcs_lead(ch & 255) )
    ch = ((ch << 8)|(get86key(wait) & 0xFF));

  return ch;
}
