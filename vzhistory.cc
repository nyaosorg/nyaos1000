/* -*- C++ -*-
 * Vz Editor like な ヒストリ参照を行う。
 */

#include <assert.h>
#include <alloca.h>
#include <ctype.h>
#include "edlin.h"
#include "quoteflag.h"

//#define DEBUG1(x) (x)
#define DEBUG1(x) /**/

extern int option_single_quote;

struct WHist{
  WHist *prev,*next;
  const char *buffer;
};

Edlin::Status Shell::vz_next_history()
{
  return CONTINUE;
}

/* 候補の単語を表示して、ユーザーに指示を請う。
 * 確定キーを押されたら、単語の残り部分の追加も実際に
 * 行う。
 *
 *	tmp		候補の単語を保持するリスト要素
 *	rightmove	カーソル位置から、入力した単語末尾までの長さ
 * return
 *	0	確定した/中止した。
 *	1	本単語は却下された。次の単語を断固、要求する。
 */
static int history_core(WHist *tmp,int rightmove,Edlin &ed)
{
  /* カーソルを右へ移動させるための文字列を作成する。
   * エスケープシーケンスによらず、カーソルが通過する位置の
   * 文字列を再描画することによって、右へ移動させるため。
   *
   * message 関数は、FEP のインライン変換のように、
   * 入力文字列の上に、別の文字列を重ねて表示させ、
   * cleanmsg 関数によって、下の文字列を回復させるように
   * なっている。
   *
   * この為に、message 関数にて、上書きした文字列の位置
   * と長さを記録しているのだが、エスケープシーケンスを
   * 使ったのでは、これが分からなくなってしまうわけだ。
   *
   * と書いても分からんわな。
   */
  assert( rightmove >= 0 );
  char *cursor_right=(char*)alloca( rightmove + 1 );
  memcpy( cursor_right , ed.getText()+ed.getPos() , rightmove );
  cursor_right[ rightmove ] = '\0';

  for(;;){ /* キー入力 */
    ed.message( "%s%s" , cursor_right , tmp->buffer );

    int key=::getkey();
    ed.cleanmsg();
    
    Edlin::Status (Shell::*function)() = Shell::get_bindkey_function(key);
    
    if(   function==&Shell::next_history
       || function==&Shell::vz_next_history ){
      /* 次候補 */
      if( tmp->next != NULL ){
	/*     AAA(1) → BBB(2) → BBB(3) → BBB(4:現在)
	 * と登録されている時、最初のループで、(2) まで移動
	 * 継いで、(1) へ移動 */
	tmp = tmp->next;
      }else{
	return 0;
      }
    }else if(   function != &Shell::vz_prev_history 
	     && function != &Shell::previous_history ){
      /* 単語もバッチソ。大団円 */
      ed.forward( rightmove );
      ed.insert_and_forward(tmp->buffer);
      ungetkey(key);
      return 0;
    }else if( tmp->prev != NULL ){
      /* 前候補 */
      tmp = tmp->prev;
    }else{
      return 1;
    }
  }
}

/* 空白を読み飛ばす。
 * return
 *	 0 単語の頭まで、ポインタを移動した
 *	-1 '\0' が現れた。
 */
static int skipSpace(const char *&sp)
{
  for(;;){
    if( *sp=='\0' )
      return -1;
    if( !isspace(*sp & 255) )
      return 0;
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
}

/* 単語を読み飛ばす。
 *	sp 単語の任意の位置を差すポインタ。
 *	   コール後、末尾へ移動する。
 * return
 *	 0 単語の末尾まで、ポインタを移動した。
 *	-1 '\0'が現れた。
 */
static int skipWord(const char *&sp)
{
  QuoteFlag qf;
  while( *sp != '\0' ){
    if( isspace(*sp & 255 ) && !qf.isInQuote() )
      return 0;
    qf.eval( *sp );
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
  return -1;
}

/* 現在カーソル位置にある単語の先頭位置と長さを得る。
 *	wordtop ← 単語の先頭位置
 *	wordlen ← 単語の長さ
 *	ed : シェルオブジェクト
 */
static void get_current_word(int &wordtop,int &wordlen,Edlin &ed)
{
  for(int i=0;;){
    /* 空白スキップ */
    for(;;){
      if( i >= ed.getPos() ){
	wordtop = i;
	wordlen = 0;
	return;
      }
      if( !isspace(ed[i] & 255) )
	break;
      i++;
    }
    /* 文字列部分の取得 */
    wordtop = i;
    wordlen = 0;
    QuoteFlag qf;
    
    while( i < ed.getLen() ){
      if( isspace(ed[i] & 255 ) && !qf.isInQuote() )
	break;
      
      qf.eval( ed[i] );
      if( is_kanji(ed[i]) ){
	i++;
	wordlen++;
      }
      i++;
      wordlen++;
    }
    if( i >= ed.getPos() )
      return;
  }
}

Edlin::Status Shell::vz_prev_history()
{
  /* 補完対象となる、入力済み文字列の範囲 */
  int targetTop;
  int targetLen;

  /* カーソルが乗っかっている単語の位置と長さを得る */
  get_current_word(targetTop,targetLen,*this);

  /* もし、最初の単語であった場合は比較対象は、行全体になるので、
   * targetLen を行の長さに変えておく。*/
  if( targetTop == 0 )
    targetLen = getLen();

  /* カーソル位置～単語末の長さを求めておく */
  int rightmove=targetLen-(getPos()-targetTop);

  /* 単語単位の検索を行う */

  WHist *whist=NULL , *tmp;
  
  /* 大検索走査線 */
  // for(Histories::Cursor cur(histories) ;; ++cur )
  for(History *cur=history ;; cur=cur->prev ){ /* 行レベルのループ */
      
    if( cur==NULL ){
      if( whist==NULL )
	return CONTINUE;
      else
	cur=history;
    }
    const char *sp=cur->buffer;
    
    if( targetTop == 0 ){
      /******** 行単位での検索 *********/
      for(int i=0 ;; i++){
	if( i >= targetLen ){
	  /* 行の既に入力した部分と一致したら… */
	  tmp = (WHist*)alloca(sizeof(WHist));
	  tmp->buffer = sp;
	  tmp->next = whist;
	  tmp->prev = NULL;
	  if( whist != NULL )
	    whist->prev = tmp;
	  whist=tmp;

	  if( history_core( whist , targetLen-getPos() , *this )==0 ){
	    after_repaint(0);
	    return CONTINUE;
	  }else{
	    break;
	  }
	}
	if( *sp == '\0' || *sp++ != getText()[i] )
	  goto nextline;
      }
      
    }else{ 
      /*
       ******* 単語単位での検索 *********
       */
      
      /* 最初の単語、すなわち、コマンド名を無視する */
      if( skipSpace(sp) != 0  ||  skipWord(sp) != 0 )
	goto nextline;

      /* 以下で、ヒストリの文字列を、単語単位で切り出してゆく。*/
      for(;;){
	/* 一行の単語レベルのループ */

	/* 空白スキップ */
	if( skipSpace(sp) != 0 )
	  goto nextline;

	/* 文字単位で比較してゆくループ。
	 * ループが続いている間は、単語の各文字と一致している。
	 */
	for(int i=0 ;; i++,sp++){
	  if( i >= targetLen ){
	    /* ⇒ 単語中に検索文字列有り!(ループ終了条件)
	     *    よって、ここから、残りの文字列をコピーしてゆく。
	     *    そのために、まずは残りの文字列の長さを調べる。
	     */

	    const char *tail=sp;
	    skipWord(tail);
	    int completeLen=tail-sp;

	    /* 行中の単語を、スタックへコピー */
	    char *dp=(char*)alloca(completeLen+1);
	    memcpy( dp , sp , completeLen );
	    dp[completeLen] = '\0';

	    /* コピーした単語を、リストに加える */
	    tmp=(WHist*)alloca( sizeof(WHist) );
	    tmp->buffer = dp;
	    tmp->next = whist;
	    tmp->prev = NULL;
	    if( whist != NULL )
	      whist->prev = tmp;
	    whist=tmp;
	    
	    if( history_core( whist , rightmove , *this ) == 0 ){
	      after_repaint(0);
	      return CONTINUE;
	    }
	    break;
	  }
	  if( *sp == '\0' )
	    goto nextline;
	  if( *sp != getText()[targetTop+i] )
	    break;
	}/* 文字レベル比較ループ */

	skipWord( sp );

      }/* 単語レベル比較ループ */

    }/* 行レベル比較ループ */

  nextline:

    /* 重複行をスキップする */
    while(    cur->prev != NULL  
	  &&  strcmp(cur->buffer,cur->prev->buffer)==0 ){
      cur = cur->prev;
    }
  }
  return CONTINUE;
}
