#include <stdio.h>
#include <sys/kbdscan.h>

#include "edlin.h"
#include "complete.h"

#define CTRL(a) ((a) & 0x1F)
#define KEY(a)  (K_##a | 0x100)

int Edlin::ctrl_d_eof=0;

/* 帰り値は、文字数。キャンセルの時は (-1)を返す。 */

int Edlin::simple_line_input()
{
  int changed=0;

  top = pos = len = 0;
  strbuf[0] = '\0';
  atrbuf[0] = SBC;
  putel();

  Edlin::History *cur=NULL;
  int prevchar=0x1FF;
  int i;
  for(;;){
    int ch;
    switch( ch=getkey() ){
    default:
      if( (ch >= ' '  &&  ch < 0x100) || ch >= 0x200  ){
	insert(ch);
      }
      forward();
      changed = 1;
      break;

    case CTRL('P'):case KEY(UP):
      if( cur == NULL )
	cur = pop();
      else
	cur=cur->prev;
      clean_up();
      if( cur != NULL )
	insert_and_forward(cur->buffer);
      changed = 0;
      break;
      
    case CTRL('N'):case KEY(DOWN):
      clean_up();
      if( cur != NULL  &&  (cur=cur->next) != NULL  )
	insert_and_forward(cur->buffer);

      changed = 0;
      break;

    case CTRL('Z'):case EOF:
      return -1;

    case CTRL('f'):case KEY(RIGHT):
      forward();
      break;

    case CTRL('b'):case KEY(LEFT):
      backward();
      break;

    case CTRL('d'):case KEY(DEL):
      if( pos < len ){
	changed = 1;
	erase();
      }else if( len == 0 && ctrl_d_eof ){
	return -1;
      }else{
	complete_list();
      }
      break;

    case CTRL('h'):
      backward();
      erase();
      changed = 1;
      break;

    case '\t':
      if( prevchar == '\t' ){
	complete_list();
      }else{
	complete();
	changed = 1;
      }
      break;

    case '\r':
      if( changed ){
	push();
      }else if( history != NULL  &&  cur != NULL  ){
	/* cur を history から切り放す */
	if( cur->next != NULL ){
	  cur->next->prev = cur->prev;
	}else{
	  history = NULL;
	}
	if( cur->prev != NULL )
	  cur->prev->next = cur->next;
	/* cur 自身のポインターを合わす */
	cur->prev = history;
	cur->next = NULL;
	/* 先頭ポインタを合わす */
	if( history != NULL )
	  history->next = cur;
	history = cur;
      }
      return len;

    case CTRL('l'):
      cls();
      break;

    case CTRL('a'):case KEY(HOME):
      go_ahead();
      break;

    case CTRL('e'):case KEY(END):
      go_tail();
      break;

    case CTRL('u'):case CTRL('['): /* キャンセル */
      clean_up();
      changed = 0;
      break;

    case CTRL('k'):
      if( pos < len ){
	eraseline();
	changed = 1;
      }
      break;

    case KEY(ALT_F):
      forward_word();
      break;
      
    case KEY(ALT_B):
      backward_word();
      break;
    }
    prevchar = ch;
  }
}
