#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/kbdscan.h>

#include "edlin.h"
#include "complete.h"
#include "nyaos.h"

#define CTRL(a) ((a) & 0x1F)
#define KEY(a)  (K_##a | 0x100)

/* 帰り値は、文字数。キャンセルの時は (-1)を返す。 */

History *Shell::history=NULL;
int Shell::nhistories=0;
int Shell::ctrl_d_eof=0;
int Shell::ctrl_z_eof=1;

const char *Shell::get_nth_history(int n)
{
  History *p=history;

  while( n-- > 0 ){
    if( p==0 )
      return 0;
    p=p->prev;
  }
  return (p != 0 ? p->buffer : 0) ;
}

static struct bind_t{
  unsigned key;
  Shell::Status (Shell::*method)();
  const char *name;
  const char *funcname;
} base_bind_table[]={
  { CTRL('H') , Shell::backspace ,
    "CTRL_H"  , "backward_delete_char  (default)"},
  { KEY(UP)   , Shell::previous_history,
    "UP","previous_history  (default)" },
  { KEY(DOWN) , Shell::next_history,
    "DOWN","next_history  (default)" },
  { KEY(RIGHT), Shell::forward,
    "RIGHT","forward_char  (default)" },
  { KEY(LEFT) , Shell::backward,"LEFT","backward_char  (default)" },
  { KEY(DEL)  , Shell::tcshlike_ctrl_d,"DEL","delete_char_or_list  (default)"},
  { CTRL('Z') , Shell::bye ,"CTRL_Z","bye  (default)"},
  { '\t'      , Shell::tcshlike_complete,"TAB","complete  (default)" },
  { '\r'      , Shell::input_terminate,"ENTER","newline  (default)" },
  { CTRL('J') , Shell::input_terminate,"ENTER","newline  (default)" },
  { CTRL('L') , Shell::repaint,"CTRL_L","clear_screen  (default)" },
  { KEY(HOME) , Shell::go_ahead,"HOME","beginning_of_line  (default)" },
  { CTRL('U') , Shell::cancel,"CTRL_U","kill_whole_line  (default)" },
  { '\x1B'    , Shell::cancel,"ESC","kill_whole_line  (default)" },
  { CTRL('C') , Shell::abort, "CTRL_C","abort (default)" },
}, tcsh_bind_table[]={
  { CTRL('P') , Shell::previous_history,"CTRL_P","previous_history  (tcsh)"},
  { CTRL('N') , Shell::next_history,"CTRL_N","next_history  (tcsh)" },
  { CTRL('F') , Shell::forward,"CTRL_F","forward_char  (tcsh)" },
  { CTRL('B') , Shell::backward,"CTRL_B","backward_char  (tcsh)" },
  { CTRL('D') , Shell::tcshlike_ctrl_d,"CTRL_D","delete_char_or_list  (tcsh)" },
  { CTRL('K') , Shell::eraseline,"CTRL_K","kill_line  (tcsh)" },
  { CTRL('A') , Shell::go_ahead,"CTRL_A","beginning_of_line  (tcsh)" },
  { KEY(ALT_F), Shell::forward_word,"ALT_F","forward_word  (tcsh)" },
  { KEY(ALT_B), Shell::backward_word,"ALT_B","backward_word  (tcsh)" },
  { CTRL('E') , Shell::go_tail,"CTRL_E","end_of_line  (tcsh)" },
  { CTRL('S') , Shell::i_search,"CTRL_S","i_search (tcsh)" },
  { CTRL('R') , Shell::rev_i_search,"CTRL_R","rev_i_search (tcsh)" },
  { CTRL('T') , Shell::swapchars,"CTRL_T","swapchars (tcsh)" },
}, wordstar_bind_table[]={
  { CTRL('E') , Shell::previous_history,"CTRL_E","previous_history  (ws)" },
  { CTRL('X') , Shell::next_history,"CTRL_X","next_history  (ws)" },
  { CTRL('D') , Shell::forward,"CTRL_D","forward_char  (ws)" },
  { CTRL('S') , Shell::backward,"CTRL_S","backward_char  (ws)" },
  { CTRL('G') , Shell::simple_delete,"CTRL_G","delete_char  (ws)" },
  { CTRL('A') , Shell::backward_word,"CTRL_A","backward_word  (ws)" },
  { CTRL('F') , Shell::forward_word,"CTRL_F","forward_word  (ws)" },
};

Shell::Status (Shell::*Shell::bindmap[0x200])();
const char *Shell::bindmap_usage_key[0x200];
const char *Shell::bindmap_usage_func[0x200];

Shell::Shell(ShellEdlin &e)
: ed(e) , changed(0) , prevchar(0x1FF) , cur(NULL)
{ ed.clean_up(); }

Shell::~Shell()
{
#if 0
  while( history != 0 ){
    History *prev=history->prev;
    free(history);
    history = prev;
  }
#endif
}

void Shell::bindkey_base()
{
  for(int i=0;i<numof(bindmap);i++){
    bindmap[ i ] = self_insert;
    bindmap_usage_key[ i ]  = NULL;
    bindmap_usage_func[ i ] = NULL;
  }
  for(int i=0;i<numof(base_bind_table);i++){
    bindmap[ base_bind_table[i].key ] = base_bind_table[i].method;
    bindmap_usage_key[ base_bind_table[i].key ] = base_bind_table[i].name;
    bindmap_usage_func[ base_bind_table[i].key] = base_bind_table[i].funcname;
  }
}

void Shell::bindkey_tcshlike()
{
  bindkey_base();
  for(int i=0;i<numof(tcsh_bind_table);i++){
    bindmap[ tcsh_bind_table[i].key ] = tcsh_bind_table[i].method;
    bindmap_usage_key[ tcsh_bind_table[i].key ] = tcsh_bind_table[i].name;
    bindmap_usage_func[ tcsh_bind_table[i].key ]= tcsh_bind_table[i].funcname;
  }
}

void Shell::bindkey_wordstar()
{
  bindkey_base();
  for(int i=0;i<numof(wordstar_bind_table);i++){
    bindmap[ wordstar_bind_table[i].key ] = wordstar_bind_table[i].method;
    bindmap_usage_key[ wordstar_bind_table[i].key ]
      = wordstar_bind_table[i].name;
    bindmap_usage_func[ wordstar_bind_table[i].key ]
      = wordstar_bind_table[i].funcname;
  }
}

Shell::Status Shell::self_insert()
{
  if( (ch >= ' '  &&  ch < 0x100) || ch >= 0x200  ){
    ed.insert(ch);
    ed.forward();
    changed = 1;
  }
  return CONTINUE;
}

static const char *stristr(const char *p,const char *q)
{
  int firstchar = tolower( *q & 255 );
  while( *p != '\0' ){
    if( tolower(*p) == firstchar ){
      const char *p1=p,*q1=q;
      do{
	p1++; q1++;
	if( *q1 == '\0' )
	  return p;
      }while( tolower( *p1 & 255 ) == tolower( *q1 & 255 ) );
    }
    ++p;
  }
  return NULL;
}

#if 0
Shell::Status Shell::vz_prev_history()
{
  if( history == NULL )
    return CONTINUE;

  char *target;
  int word_seek_mode = 0;
  if( ed.seek_word_seek() == 0 ){
    word_seek_mode = 0;
    target = ed.getbuffer();
  }else{
    word_seek_mode = 1;
    target = ed.get_current_word();
  }
  
  History *p=history;
  for(;;){
    if( p == NULL )
      return CONTINUE;
    if( stristr(p->buffer,target ) != NULL )
      break;
    p = p->prev;
  }
  
  ed.go_ahead();
  for(;;){
    ed.message( "%s", p->buffer );
    int key=::getkey();
    if( key == '\r' || key == '\n' ){
      break;
    }else if( key < 0 || key >= 0x200 ){
      ungetkey(key);
      break;
    }else if(   bindmap[key] == vz_prev_history 
	     || bindmap[key] == previous_history ){
      for(History *q=p ;  ; q=q->prev ){
	if( q == NULL ){
	  q = history;
	}
	if( stristr(q->buffer , ed.getbuffer() ) != NULL ){
	  p = q;
	  break;
	}
      }
    }else if(   bindmap[key]==vz_next_history 
	     || bindmap[key]==next_history     ){
      for(History *q=p ;  ; q=q->next ){
	if( q == NULL )
	  break;
	if( stristr(q->buffer , ed.getbuffer() ) != NULL ){
	  p = q;
	  break;
	}
      }
    }else if( key=='\007' || key=='\033' ){
      ed.cleanmsg();
      ed.go_tail();
      return CONTINUE;
    }else{
      ungetkey(key);
      break;
    }
  }
  ed.cleanmsg();
  ed.clean_up();
  ed.insert_and_forward(p->buffer);
  changed = 0;
  return CONTINUE;
}

Shell::Status Shell::vz_next_history()
{
  return CONTINUE;
}
#endif

Shell::Status Shell::previous_history()
{
  if( cur==NULL || changed )
    cur = history;
  else
    cur=cur->prev;
  ed.clean_up();
  if( cur != NULL )
    ed.insert_and_forward(cur->buffer);
  changed = 0;
  return CONTINUE;
}

Shell::Status Shell::next_history()
{
  ed.clean_up();
  if( cur != NULL  &&  (cur=cur->next) != NULL  )
    ed.insert_and_forward(cur->buffer);
  
  changed = 0;
  return CONTINUE;
}

Shell::Status Shell::bye()
{
  if( ctrl_z_eof )
    return QUIT;
  return CONTINUE;
}

Shell::Status Shell::forward()
{
  ed.forward();
  return CONTINUE;
}
Shell::Status Shell::backward()
{
  ed.backward();
  return CONTINUE;
}
Shell::Status Shell::simple_delete()
{
  ed.erase();
  return CONTINUE;
}

Shell::Status Shell::tcshlike_ctrl_d()
{
  if( ed.position() < ed.length() ){
    changed = 1;
    ed.erase();
  }else if( ed.length() == 0 ){
    return ctrl_d_eof ? QUIT : CONTINUE ;
  }else{
    ed.complete_list();
  }
  return CONTINUE;
}
Shell::Status Shell::backspace()
{
  backward();
  ed.erase();
  changed = 1;
  return CONTINUE;
}
Shell::Status Shell::tcshlike_complete()
{
  if( prevchar == '\t' ){
    if( ed.length() != 0 )
      ed.complete_list();
  }else{
    ed.complete();
    changed = 1;
  }
  return CONTINUE;
}
Shell::Status Shell::input_terminate()
{
#if 0
  if( changed ){ /* 変更があれば、履歴に放り込む */
#endif
    int len=ed.length();
    if( len <= 0 )
      return TERMINATE;
    
    History *tmp=(History*)malloc(sizeof(History)+len);
    if( tmp == NULL )
      return FATAL;

    /* 古い方が prev , 新しい方が next側 */
    memcpy(tmp->buffer , ed.getbuffer() , len );
    tmp->buffer[len] = '\0';
    tmp->prev = history;
    tmp->next = NULL;
    
    if( history != NULL )
      history->next = tmp;
    history = tmp;
    
    /* curが NULL の時、次回のヒストリ参照で、
     * 最初に現れる文字列がトップになる。 */

    cur = NULL;

    nhistories++;
#if 0
  }else if( history != NULL  &&  cur != NULL  &&  cur->next != NULL ){
    /* 変更がされていない場合、参照した履歴を先頭に持ってくる。*/
    /* -- cur を history から切り放す --*/
    cur->next->prev = cur->prev;
    if( cur->prev != NULL )
      cur->prev->next = cur->next;

    /* cur 自身のポインターを合わす */
    cur->prev = history;
    cur->next = NULL;

    /* 先頭ポインタを合わす */
    history->next = cur;
    history = cur;
    cur = NULL;
  }
#endif
  Edlin2::canna_to_alnum();
  return TERMINATE;
}
Shell::Status Shell::repaint()
{
  ed.cls();
  return CONTINUE;
}
Shell::Status Shell::go_ahead()
{
  ed.go_ahead();
  return CONTINUE;
}

Shell::Status Shell::go_tail()
{
  ed.go_tail();
  return CONTINUE;
}

Shell::Status Shell::cancel()
{
  Edlin2::canna_to_alnum();
  ed.clean_up();
  changed = 0;
  cur = NULL;
  return CONTINUE;
}

Shell::Status Shell::eraseline()
{
  if( ed.position() < ed.length() ){
    ed.eraseline();
    changed = 1;
  }
  return CONTINUE;
}
Shell::Status Shell::forward_word()
{
  ed.forward_word();
  return CONTINUE;
}
Shell::Status Shell::backward_word()
{
  ed.backward_word();
  return CONTINUE;
}

int Shell::line_input(const char *prompt,int window)
{
  raw_mode();
  ed.setprompt(prompt,window);
  fputs(prompt,stdout);
  fflush(stdout);
  ed.init();
  for(;;){
    ch=ed.getkey();
    if( ch < numof(bindmap) ){
      Status rc=(this->*bindmap[ch])();
      switch( rc ){
      case TERMINATE:
	cocked_mode();
	return ed.length();
	
      case CONTINUE:
	prevchar = ch;
	continue;

      default:
	cocked_mode();
	return rc;
      }
    }else{
      self_insert();
      prevchar = ch;
    }
  }
}
struct {
  const char *name;
  int code;
} keytable[] ={
  { "ALT_0",                 0x181 },   /* <Alt>+<0> */
  { "ALT_1",                 0x178 },   /* <Alt>+<1> */
  { "ALT_2",                 0x179 },   /* <Alt>+<2> */
  { "ALT_3",                 0x17a },   /* <Alt>+<3> */
  { "ALT_4",                 0x17b },   /* <Alt>+<4> */
  { "ALT_5",                 0x17c },   /* <Alt>+<5> */
  { "ALT_6",                 0x17d },   /* <Alt>+<6> */
  { "ALT_7",                 0x17e },   /* <Alt>+<7> */
  { "ALT_8",                 0x17f },   /* <Alt>+<8> */
  { "ALT_9",                 0x180 },   /* <Alt>+<9> */
  { "ALT_A",                 0x11e },   /* <Alt>+<A> */
  { "ALT_B",                 0x130 },   /* <Alt>+<B> */
  { "ALT_BACKSLASH",         0x12b },   /* <Alt>+<\> */
  { "ALT_BACKSPACE",         0x10e },   /* <Alt>+<Backspace> */
  { "ALT_C",                 0x12e },   /* <Alt>+<C> */
  { "ALT_COMMA",             0x133 },   /* <Alt>+<,> */
  { "ALT_D",                 0x120 },   /* <Alt>+<D> */
  { "ALT_DEL",               0x1a3 },   /* <Alt>+<Del> */
  { "ALT_DOWN",              0x1a0 },   /* <Alt>+<Down arrow> */
  { "ALT_E",                 0x112 },   /* <Alt>+<E> */
  { "ALT_END",               0x19f },   /* <Alt>+<End> */
  { "ALT_EQUAL",             0x183 },   /* <Alt>+<=> */
  { "ALT_ESC",               0x101 },   /* <Alt>+<Esc>    [DOS]*/
  { "ALT_F",                 0x121 },   /* <Alt>+<F> */
  { "ALT_F1",                0x168 },   /* <Alt>+<F1> */
  { "ALT_F10",               0x171 },   /* <Alt>+<F10> */
  { "ALT_F11",               0x18b },   /* <Alt>+<F11> */
  { "ALT_F12",               0x18c },   /* <Alt>+<F12> */
  { "ALT_F2",                0x169 },   /* <Alt>+<F2> */
  { "ALT_F3",                0x16a },   /* <Alt>+<F3> */
  { "ALT_F4",                0x16b },   /* <Alt>+<F4> */
  { "ALT_F5",                0x16c },   /* <Alt>+<F5> */
  { "ALT_F6",                0x16d },   /* <Alt>+<F6> */
  { "ALT_F7",                0x16e },   /* <Alt>+<F7> */
  { "ALT_F8",                0x16f },   /* <Alt>+<F8> */
  { "ALT_F9",                0x170 },   /* <Alt>+<F9> */
  { "ALT_G",                 0x122 },   /* <Alt>+<G> */
  { "ALT_H",                 0x123 },   /* <Alt>+<H> */
  { "ALT_HOME",              0x197 },   /* <Alt>+<Home> */
  { "ALT_I",                 0x117 },   /* <Alt>+<I> */
  { "ALT_INS",               0x1a2 },   /* <Alt>+<Ins> */
  { "ALT_J",                 0x124 },   /* <Alt>+<J> */
  { "ALT_K",                 0x125 },   /* <Alt>+<K> */
  { "ALT_L",                 0x126 },   /* <Alt>+<L> */
  { "ALT_LEFT",              0x19b },   /* <Alt>+<Left arrow> */
  { "ALT_LEFT_BRACKET",      0x11a },   /* <Alt>+<[> */
  { "ALT_LEFT_QUOTE",        0x129 },   /* <Alt>+<`> */
  { "ALT_M",                 0x132 },   /* <Alt>+<M> */
  { "ALT_MINUS",             0x182 },   /* <Alt>+<-> */
  { "ALT_N",                 0x131 },   /* <Alt>+<N> */
  { "ALT_O",                 0x118 },   /* <Alt>+<O> */
  { "ALT_P",                 0x119 },   /* <Alt>+<P> */
  { "ALT_PAD_ASTERISK",      0x137 },   /* <Alt>+<*> (numeric keypad) */
  { "ALT_PAD_ENTER",         0x1a6 },   /* <Alt>+<Enter> (numeric keypad) */
  { "ALT_PAD_MINUS",         0x14a },   /* <Alt>+<-> (numeric keypad) */
  { "ALT_PAD_PLUS",          0x14e },   /* <Alt>+<+> (numeric keypad) */
  { "ALT_PAD_SLASH",         0x1a4 },   /* <Alt>+</> (numeric keypad) */
  { "ALT_PAGEDOWN",          0x1a1 },   /* <Alt>+<Page down> */
  { "ALT_PAGEUP",            0x199 },   /* <Alt>+<Page up> */
  { "ALT_PERIOD",            0x134 },   /* <Alt>+<.> */
  { "ALT_Q",                 0x110 },   /* <Alt>+<Q> */
  { "ALT_R",                 0x113 },   /* <Alt>+<R> */
  { "ALT_RETURN",            0x11c },   /* <Alt>+<Return> */
  { "ALT_RIGHT",             0x19d },   /* <Alt>+<Right arrow> */
  { "ALT_RIGHT_BRACKET",     0x11b },   /* <Alt>+<]> */
  { "ALT_RIGHT_QUOTE",       0x128 },   /* <Alt>+<'> */
  { "ALT_S",                 0x11f },   /* <Alt>+<S> */
  { "ALT_SEMICOLON",         0x127 },   /* <Alt>+<;> */
  { "ALT_SLASH",             0x135 },   /* <Alt>+</> */
  { "ALT_SPACE",             0x139 },   /* <Alt>+<Space>  [OS2] */
  { "ALT_T",                 0x114 },   /* <Alt>+<T> */
  { "ALT_TAB",               0x1a5 },   /* <Alt>+<Tab>  [DOS] */
  { "ALT_U",                 0x116 },   /* <Alt>+<U> */
  { "ALT_UP",                0x198 },   /* <Alt>+<Up arrow> */
  { "ALT_V",                 0x12f },   /* <Alt>+<V> */
  { "ALT_W",                 0x111 },   /* <Alt>+<W> */
  { "ALT_X",                 0x12d },   /* <Alt>+<X> */
  { "ALT_Y",                 0x115 },   /* <Alt>+<Y> */
  { "ALT_Z",                 0x12c },   /* <Alt>+<Z> */
  { "BACKSPACE", 0x8 },
  { "BACKTAB",               0x10f },   /* <Shift>+<Tab> */
  { "CENTER",                0x14c },   /* Center cursor */
  { "CTRL_A", 0x1 },
  { "CTRL_AT",               0x103 },   /* <Ctrl>+<@> */
  { "CTRL_B", 0x2 },
  { "CTRL_C", 0x3 },
  { "CTRL_CENTER",           0x18f },   /* <Ctrl>+<Center> */
  { "CTRL_D", 0x4 },
  { "CTRL_DEL",              0x193 },   /* <Ctrl>+<Del> */
  { "CTRL_DOWN",             0x191 },   /* <Ctrl>+<Down arrow> */
  { "CTRL_E", 0x5 },
  { "CTRL_END",              0x175 },   /* <Ctrl>+<End> */
  { "CTRL_F", 0x6 },
  { "CTRL_F1",               0x15e },   /* <Ctrl>+<F1> */
  { "CTRL_F10",              0x167 },   /* <Ctrl>+<F10> */
  { "CTRL_F11",              0x189 },   /* <Ctrl>+<F11> */
  { "CTRL_F12",              0x18a },   /* <Ctrl>+<F12> */
  { "CTRL_F2",               0x15f },   /* <Ctrl>+<F2> */
  { "CTRL_F3",               0x160 },   /* <Ctrl>+<F3> */
  { "CTRL_F4",               0x161 },   /* <Ctrl>+<F4> */
  { "CTRL_F5",               0x162 },   /* <Ctrl>+<F5> */
  { "CTRL_F6",               0x163 },   /* <Ctrl>+<F6> */
  { "CTRL_F7",               0x164 },   /* <Ctrl>+<F7> */
  { "CTRL_F8",               0x165 },   /* <Ctrl>+<F8> */
  { "CTRL_F9",               0x166 },   /* <Ctrl>+<F9> */
  { "CTRL_G", 0x7 },
  { "CTRL_H", 0x8 },
  { "CTRL_HOME",             0x177 },   /* <Ctrl>+<Home> */
  { "CTRL_I", 0x9 },
  { "CTRL_INS",              0x192 },   /* <Ctrl>+<Ins> */
  { "CTRL_J", 0xa },
  { "CTRL_K", 0xb },
  { "CTRL_L", 0xc },
  { "CTRL_LEFT",             0x173 },   /* <Ctrl>+<Left arrow> */
  { "CTRL_M", 0xd },
  { "CTRL_N", 0xe },
  { "CTRL_O", 0xf },
  { "CTRL_P", 0x10 },
  { "CTRL_PAD_ASTERISK",     0x196 },   /* <Ctrl>+<*> (numeric keypad) */
  { "CTRL_PAD_MINUS",        0x18e },   /* <Ctrl>+<-> (numeric keypad) */
  { "CTRL_PAD_PLUS",         0x190 },   /* <Ctrl>+<+> (numeric keypad) */
  { "CTRL_PAD_SLASH",        0x195 },   /* <Ctrl>+</> (numeric keypad) */
  { "CTRL_PAGEDOWN",         0x176 },   /* <Ctrl>+<Page down> */
  { "CTRL_PAGEUP",           0x184 },   /* <Ctrl>+<Page up> */
  { "CTRL_PRTSC",            0x172 },   /* <Ctrl>+<PrtSc> */
  { "CTRL_Q", 0x11 },
  { "CTRL_R", 0x12 },
  { "CTRL_RIGHT",            0x174 },   /* <Ctrl>+<Right arrow> */
  { "CTRL_S", 0x13 },
  { "CTRL_SPACE",            0x102 },   /* <Ctrl>+<Space> [OS2]*/
  { "CTRL_T", 0x14 },
  { "CTRL_TAB",              0x194 },   /* <Ctrl>+<Tab> */
  { "CTRL_U", 0x15 },
  { "CTRL_UP",               0x18d },   /* <Ctrl>+<Up arrow> */
  { "CTRL_V", 0x16 },
  { "CTRL_W", 0x17 },
  { "CTRL_X", 0x18 },
  { "CTRL_Y", 0x19 },
  { "CTRL_Z", 0x1a },
  { "DEL",                   0x153 },   /* <Del> */
  { "DOWN",                  0x150 },   /* <Down arrow> */
  { "END",                   0x14f },   /* <End> */
  { "ENTER", 0xd },
  { "ESCAPE", 0x1b },
  { "F1",                    0x13b },   /* <F1> */
  { "F10",                   0x144 },   /* <F10> */
  { "F11",                   0x185 },   /* <F11> */
  { "F12",                   0x186 },   /* <F12> */
  { "F2",                    0x13c },   /* <F2> */
  { "F3",                    0x13d },   /* <F3> */
  { "F4",                    0x13e },   /* <F4> */
  { "F5",                    0x13f },   /* <F5> */
  { "F6",                    0x140 },   /* <F6> */
  { "F7",                    0x141 },   /* <F7> */
  { "F8",                    0x142 },   /* <F8> */
  { "F9",                    0x143 },   /* <F9> */
  { "HOME",                  0x147 },   /* <Home> */
  { "INS",                   0x152 },   /* <Ins> */
  { "LEFT",                  0x14b },   /* <Left arrow> */
  { "PAGEDOWN",              0x151 },   /* <Page down> */
  { "PAGEUP",                0x149 },   /* <Page up> */
  { "RETURN", 0xd },
  { "RIGHT",                 0x14d },   /* <Right arrow> */
  { "SHIFT_DEL",             0x105 },   /* <Shift>+<Del>  [OS2]*/
  { "SHIFT_F1",              0x154 },   /* <Shift>+<F1> */
  { "SHIFT_F10",             0x15d },   /* <Shift>+<F10> */
  { "SHIFT_F11",             0x187 },   /* <Shift>+<F11> */
  { "SHIFT_F12",             0x188 },   /* <Shift>+<F12> */
  { "SHIFT_F2",              0x155 },   /* <Shift>+<F2> */
  { "SHIFT_F3",              0x156 },   /* <Shift>+<F3> */
  { "SHIFT_F4",              0x157 },   /* <Shift>+<F4> */
  { "SHIFT_F5",              0x158 },   /* <Shift>+<F5> */
  { "SHIFT_F6",              0x159 },   /* <Shift>+<F6> */
  { "SHIFT_F7",              0x15a },   /* <Shift>+<F7> */
  { "SHIFT_F8",              0x15b },   /* <Shift>+<F8> */
  { "SHIFT_F9",              0x15c },   /* <Shift>+<F9> */
  { "SHIFT_INS",             0x104 },   /* <Shift>+<Ins>  [OS2]*/
  { "SPACE", 0x20 },
  { "TAB", 0x9 },
  { "UP",                    0x148 },   /* <Up arrow> */
};
struct {
  const char *name;
  Shell::Status (Shell::*method)();
} functable[] ={
  { "accept_line",               Shell::input_terminate },
  { "backward_char",             Shell::backward },
  { "backward_delete_char",      Shell::backspace },
  { "backward_word",             Shell::backward_word },
  { "beginning_of_line",         Shell::go_ahead },
  { "bye",                       Shell::bye },
  { "clear_screen",              Shell::repaint },
  { "complete",                  Shell::tcshlike_complete },
  { "delete_char",               Shell::simple_delete },
  { "delete_char_or_list",       Shell::tcshlike_ctrl_d },
  { "down_history",              Shell::next_history },
  { "end_of_line",               Shell::go_tail },
  { "forward_char",              Shell::forward },
  { "forward_word",              Shell::forward_word },
  { "kill_line",                 Shell::eraseline },
  { "kill_whole_line",           Shell::cancel },
  { "next_history",              Shell::next_history },
  { "newline",                   Shell::input_terminate },
  { "previous_history",          Shell::previous_history },
  { "self_insert",               Shell::self_insert },
  { "up_history",                Shell::previous_history },
  { "i_search",                  Shell::i_search },
  { "rev_i_search",              Shell::rev_i_search },
#if 0
  { "vz_prev_history",           Shell::vz_prev_history },
  { "vz_next_history",           Shell::vz_next_history },
#endif
};

int Shell::bindkey(const char *key, const char *funcname )
{
  int low=0;
  int high=numof(keytable);
  int center;

  for(;;){
    center=(low+high)/2;
    int diff=(keytable[center].name[0] - to_upper(key[0]) );
    if( diff==0 )
      diff=stricmp(keytable[center].name,key);

    if( diff > 0 ){       /* high > center > key > low */
      if( high == center )
	return 1;
      high = center;
    }else if( diff < 0 ){ /* low < center < key < high */
      if( low == center )
	return 1;
      low = center;
    }else{
      break;
    }
  }
  int code = keytable[center].code;
  int center_key = center;

  low  = 0;
  high = numof(functable);

  for(;;){
    center=(low+high)/2;
    int diff=(functable[center].name[0] - to_lower(funcname[0]) );
    if( diff==0 )
      diff=stricmp(functable[center].name,funcname);

    if( diff > 0 ){       /* high > center > key > low */
      if( high == center )
	return 2;
      high = center;
    }else if( diff < 0 ){ /* low < center < key < high */
      if( low == center )
	return 2;
      low = center;
    }else{
      break;
    }
  }
  bindmap[ code ] = functable[center].method ;
  bindmap_usage_key[ code ] = keytable[center_key].name ;
  bindmap_usage_func[ code ] = functable[center].name ;

  return 0;
}
void Shell::bindlist(FILE *fout)
{
  for(int i=0;i<numof(bindmap);i++){
    if( bindmap_usage_key[i] != NULL  &&  bindmap_usage_func[i] != NULL ){
      fprintf(fout,"%-8s : %s\n",
	      bindmap_usage_key[i],bindmap_usage_func[i] );
    }
  }
}

static History *i_search_core( History *cur,const char *sekstr
			      ,int &findpos)
{
  for(;;){
    char *findptr;
    if( cur == NULL ){
      putc('\a',stderr);
      return NULL;
    }
    if( (findptr=strstr(cur->buffer,sekstr)) != NULL ){
      findpos = findptr - cur->buffer;
      return cur;
    }
    cur = cur->next;
  }
}
  

static History *rev_i_search_core( History *cur,const char *sekstr
				  ,int &findpos )
{
  for(;;){
    char *findptr;
    if( cur == NULL ){
      putc('\a',stderr);
      return NULL;
    }
    if( (findptr=strstr(cur->buffer,sekstr)) != NULL ){
      findpos = findptr - cur->buffer;
      return cur;
    }
    cur = cur->prev;
  }
}

Shell::Status Shell::search_engine(int isrev=1)
{
  if( history == NULL )
    return CONTINUE;

  /* インクリメンタルサーチモード開始 */
  History *cur=NULL,*tmp=NULL;
  char sekstr[256]="\0";
  int seklen=0;
  int findpos=0;
  
  for(;;){
    ed.message("(%si-search)`%s':%s"
	       ,isrev ? "reverse-" : ""
	       ,sekstr
	       ,(cur==NULL ? "" : cur->buffer) );

    unsigned key=ed.getkey();

    if( key < 0 || key > 0x1FF 
       || (bindmap[ key ] == self_insert && isprint(key & 255) )){
      /* 文字列の追加(increment) */
      if( key > 0x1ff || key < 0 ){
	sekstr[ seklen++ ] = (key >> 8);
      }
      sekstr[ seklen++ ] = (key & 0xFF);
      sekstr[ seklen   ] = '\0';

      if( isrev ){
	tmp=rev_i_search_core(cur != NULL ? cur : history 
				, sekstr , findpos );
      }else if( cur != NULL ){
	tmp = i_search_core(cur , sekstr , findpos );
      }
      if( tmp != NULL )
	cur = tmp;

    }else if( key >= 0 && bindmap[ key ] == rev_i_search ){
      isrev = 1;

      if( cur==NULL || cur->prev==NULL )
	tmp = rev_i_search_core(history , sekstr , findpos );
      else
	tmp = rev_i_search_core(cur->prev , sekstr , findpos );

      if( tmp != NULL )
	cur = tmp;

    }else if( key >= 0 && bindmap[ key ] == i_search ){
      isrev = 0;

      if( cur != NULL  &&  cur->next != NULL )
	tmp = i_search_core(cur->next , sekstr , findpos );

      if( tmp != NULL )
	cur = tmp;

    }else{ /* それ以外の機能キーの場合は、サーチを終結する。*/
      if( cur != NULL  &&  key != '\007' ){
	/* 入力バッファの中にペーストする。*/
	ed.clean_up();
	ed.insert_and_forward( cur->buffer );
	ed.locate(findpos+seklen);
      }else{
	ed.cleanmsg();
      }
      if( key < 0 || key > numof(bindmap) || bindmap[key] == input_terminate ){
	return CONTINUE;
      }else{
	return (this->*bindmap[key])();
      }
    }
  }
}

Shell::Status Shell::i_search()
{
  return search_engine(0);
}

Shell::Status Shell::rev_i_search()
{
  return search_engine(1);
}
