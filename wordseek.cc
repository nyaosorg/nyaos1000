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
    
    if( bindmap[key]==next_history || bindmap[key]==vz_next_history ){
      if( tmp->next != NULL )
	tmp = tmp->next;
      else 
	return 0;
    }else if(   bindmap[key] != vz_prev_history 
	     && bindmap[key] != previous_history ){
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

Shell::Status Shell::vz_prev_history()
{
  /* 検索文字列を編集行から取得する */
  int wordtop=0;
  int wordlen=0;

  for(int i=0;;){
    for(;;){ /* 空白スキップ */
      if( i >= ed.position() ){
	wordtop = i;
	wordlen = 0;
	goto Break;
      }
      if( !isspace(ed[i] & 255 ) )
	break;
      i++;
    }
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

      i++;
      wordlen++;
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
    int left;

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
      for(;;){
	if( *sp=='\0' ) goto nextline;
	if( !isspace(*sp & 255) ) break;
	++sp;
      }
      for(int quote=0;;){
	if( *sp=='\0' )
	  goto nextline;
	if( isspace(*sp & 255 ) && quote == 0 )
	  break;
	if( *sp == '"'  &&  (quote & 2) == 0 )
	  quote ^= 1;
	if( *sp == '\'' &&  (quote & 1) == 0  &&  option_single_quote )
	  quote ^= 2;
	++sp;
      }

      for(;;){ /* 一行の単語レベルのループ */

	/* 空白スキップ */
	while( *sp != '\0' && isspace(*sp & 255))
	  ++sp;
	
	if( *sp == '\0' )
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
