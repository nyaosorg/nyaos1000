#include <ctype.h>
#include "edlin.h"

extern int option_single_quote;

/*
 * Vz Editor like な ヒストリ参照っすよ。
 */

struct WHist{
  WHist *prev,*next;
  const char *buffer;
};

Shell::Status Shell::vz_next_history()
{
  return CONTINUE;
}

/* 帰り値  0:確定/やんぴ  1:次の新候補を要求 */
int Shell::vz_history_core(WHist *tmp)
{
  for(;;){ /* キー入力 */
    ed.message("%s",tmp->buffer);
    int key=::getkey();
    ed.cleanmsg();
    
    if( bindmap[key]==&next_history || bindmap[key]==&vz_next_history ){
      if( tmp->next != NULL )
	tmp = tmp->next;
      else 
	return 0;
    }else if(   bindmap[key] != &vz_prev_history 
	     && bindmap[key] != &previous_history ){
      /* 単語もバッチソ。大団円 */
      ed.insert_and_forward(tmp->buffer);
      ungetkey(key);
      return 0;
    }else if( tmp->prev != NULL ){
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
 * return
 *	 0 単語の末尾まで、ポインタを移動した。
 *	-1 '\0'が現れた。
 */
static int skipWord(const char *&sp)
{
  int quote=0;
  for(;;){
    if( *sp=='\0' )
      return -1;
    if( isspace(*sp & 255 ) && quote == 0 )
      return 0;
    if( *sp == '"'  &&  (quote & 2) == 0 )
      quote ^= 1;
    if( *sp == '\'' &&  (quote & 1) == 0  &&  option_single_quote )
      quote ^= 2;
    
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
}

Shell::Status Shell::vz_prev_history()
{
  /* 検索文字列を編集行から取得する */
  int wordtop=0;
  int wordlen=0;

  for(int i=0;;){
    /* 空白スキップ */
    for(;;){
      if( i >= ed.position() ){
	wordtop = i;
	wordlen = 0;
	goto Break;
      }
      if( !isspace(ed[i] & 255 ) )
	break;
      i++;
    }
    /* 文字列部分の取得 */
    wordtop = i;
    wordlen = 0;
    for(int quote=0;;){
      if( i >= ed.position() )
	goto Break;
      if( isspace(ed[i] & 255 ) && quote==0 )
	break;
      if( ed[i] == '"' && (quote & 2)==0 )
	quote ^= 1;
      if( ed[i] == '\'' && (quote & 1)==0  &&  option_single_quote )
	quote ^= 2;

      if( is_kanji(ed[i]) ){
	i++; wordlen++;
      }
      i++; wordlen++;
    }
  }
 Break:
  /* 単語単位の検索を行う */

  WHist *whist=NULL , *tmp;
  
  /* 大検索走査線 */
  for(History *cur=history ;; cur=cur->prev ){ /* 行レベルのループ */
    if( cur==NULL ){
      if( whist==NULL )
	return CONTINUE;
      else
	cur=history;
    }
    const char *sp=cur->buffer;


    if( wordtop == 0 ){
      /******** 行単位での検索 *********/
      for(int i=0 ;; i++){
	if( i >= wordlen ){
	  tmp = (WHist*)alloca(sizeof(WHist));
	  tmp->buffer = sp;
	  tmp->next = whist;
	  tmp->prev = NULL;
	  if( whist != NULL )
	    whist->prev = tmp;
	  whist=tmp;

	  if( vz_history_core( whist )==0 )
	    return CONTINUE;
	  else
	    break;
	}
	if( *sp == '\0' || *sp++ != ed[i] )
	  goto nextline;
      }
      
    }else{ 
      /******** 単語単位での検索 **********/

      /* 最初の単語、すなわち、コマンド名を無視する */
      if( skipSpace(sp) != 0  ||  skipWord(sp) != 0 )
	goto nextline;

      for(;;){ /* 一行の単語レベルのループ */

	/* 空白スキップ */
	if( skipSpace(sp) != 0 )
	  goto nextline;
	
	for(int i=0 ;; i++,sp++){
	  if( i >= wordlen ){ /* 単語中に検索文字列有り! */
	    int left=0;
	    for(int quote=0 ;;){
	      if( sp[left] == '\0' )
		break;
	      if( isspace(sp[left])  &&  quote == 0 )
		break;
	      if( sp[left] == '"'  &&  (quote & 2)==0 )
		quote ^= 1;
	      if( sp[left] == '\'' &&  (quote & 1)==0  && option_single_quote )
		quote ^= 2;
	      
	      if( is_kanji(sp[left]) )
		++left;
	      ++left;
	    }
	    
	    char *dp=(char*)alloca(left+1);
	    memcpy( dp , sp , left);
	    dp[left] = '\0';

	    tmp=(WHist*)alloca( sizeof(WHist) );
	    tmp->buffer = dp;
	    tmp->next = whist;
	    tmp->prev = NULL;
	    if( whist != NULL )
	      whist->prev = tmp;
	    whist=tmp;
	    
	    if( vz_history_core( whist ) == 0 )
	      return CONTINUE;
	    break;
	  }
	  if( *sp == '\0' )
	    goto nextline;
	  if( *sp != ed[wordtop+i] )
	    break;
	}
	while( *sp != '\0' && !isspace(*sp & 255) )
	  ++sp;
      }
    }
  nextline:
    ;
  }
  return CONTINUE;
}
