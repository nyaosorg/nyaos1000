#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/kbdscan.h>
#include <sys/video.h>

#include "hash.h"
#include "edlin.h"
#include "complete.h"
#include "nyaos.h"

#define CTRL(a) ((a) & 0x1F)
#define KEY(a)  ((K_##a & 0xFF) | 0x100)

extern int execute_result;
int printexitvalue=1;

/* 帰り値は、文字数。キャンセルの時は (-1)を返す。 */

int overwrite=0;

Shell::History *Shell::history=NULL;
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
  { KEY(UP),Shell::vz_prev_history, "UP","vz_prev_history (default)" },
  { KEY(DOWN) , Shell::vz_next_history, "DOWN","vz_next_history  (default)" },
  { KEY(RIGHT), Shell::forward, "RIGHT","forward_char  (default)" },
  { KEY(LEFT) , Shell::backward,"LEFT","backward_char  (default)" },
  { KEY(DEL)  , Shell::simple_delete,"DEL","delete_char  (default)"},
  { KEY(INS)  , Shell::flip_over,"INS","flip_overwrite  (default)"},
  { CTRL('Z') , Shell::bye ,"CTRL_Z","bye  (default)"},
  { '\t'      , Shell::tcshlike_complete,"TAB","complete  (default)" },
  { KEY(CTRL_TAB),Shell::yaoslike_complete,"CTRL_TAB","complete2  (default)" },
  {KEY(ALT_RETURN),Shell::yaoslike_complete,"CTRL_TAB","complete2  (default)"},
  { '\r'      , Shell::input_terminate,"ENTER","newline  (default)" },
  { CTRL('J') , Shell::input_terminate,"ENTER","newline  (default)" },
  { CTRL('L') , Shell::repaint,"CTRL_L","clear_screen  (default)" },
  { KEY(HOME) , Shell::go_ahead,"HOME","beginning_of_line  (default)" },
  { KEY(END)  , Shell::go_tail,"END","end_of_line (default)" },
  { CTRL('U') , Shell::cancel,"CTRL_U","kill_whole_line  (default)" },
  { '\x1B'    , Shell::cancel,"ESC","kill_whole_line  (default)" },
  { CTRL('C') , Shell::abort, "CTRL_C","abort (default)" },
  { KEY(F1)   , Shell::complete_to_fullpath , "F1","complete_to_fullpath" },
  { KEY(F2)   , Shell::complete_to_url , "F2","complete_to_url" },
}, nyaos_bind_table[]={
  { CTRL('P') , Shell::vz_prev_history,"CTRL_P","vz_prev_history  (nyaos)"},
  { CTRL('N') , Shell::vz_next_history,"CTRL_N","vz_next_history  (nyaos)" },
  { CTRL('F') , Shell::forward,"CTRL_F","forward_char  (nyaos)" },
  { CTRL('B') , Shell::backward,"CTRL_B","backward_char  (nyaos)" },
  { CTRL('D'),Shell::tcshlike_ctrl_d,"CTRL_D","delete_char_or_list  (nyaos)"},
  { CTRL('K') , Shell::eraseline,"CTRL_K","kill_line  (nyaos)" },
  { CTRL('A') , Shell::go_ahead,"CTRL_A","beginning_of_line  (nyaos)" },
  { KEY(ALT_F), Shell::forward_word,"ALT_F","forward_word  (nyaos)" },
  { KEY(ALT_B), Shell::backward_word,"ALT_B","backward_word  (nyaos)" },
  { CTRL('E') , Shell::go_tail,"CTRL_E","end_of_line  (nyaos)" },
  { CTRL('S') , Shell::i_search,"CTRL_S","i_search (nyaos)" },
  { CTRL('R') , Shell::rev_i_search,"CTRL_R","rev_i_search (nyaos)" },
  { CTRL('T') , Shell::swapchars,"CTRL_T","swapchars (nyaos)" },
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
  { CTRL('E') , Shell::vz_prev_history,"CTRL_E","vz_prev_history  (ws)" },
  { CTRL('X') , Shell::vz_next_history,"CTRL_X","vz_next_history  (ws)" },
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
: ed(e) , changed(0) , prevchar(0x1FF) , cur(NULL) , prev_complete_num(0)
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

void Shell::bindkey_nyaos()
{
  bindkey_base();
  for(int i=0;i<numof(nyaos_bind_table);i++){
    bindmap[ nyaos_bind_table[i].key ] = nyaos_bind_table[i].method;
    bindmap_usage_key[ nyaos_bind_table[i].key ] = nyaos_bind_table[i].name;
    bindmap_usage_func[ nyaos_bind_table[i].key ]= nyaos_bind_table[i].funcname;
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

/* insert modeとoverwrite modeを切り換えたら、カーソル形状を
 * 変えるくらいの芸をするのが一般的だが、面倒くさいから…。(^^;
 * ---> 葉山もトライしてみたのですが、どうも半分サイズの
 * カーソルがうまく表示できないんですよねぇ。う～む。
 */
Shell::Status Shell::flip_over(){
  overwrite=!overwrite;
  return CONTINUE;
}

Shell::Status Shell::self_insert()
{
  if( (ch >= ' '  &&  ch < 0x100) || ch >= 0x200  ){
    if( overwrite )
      ed.erase();
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
  if( prevchar=='\t'  &&  prev_complete_num != 1 ){
    if( ed.length() != 0 )
      ed.complete_list();
  }else{
    prev_complete_num = ed.complete1();
    changed = 1;
  }
  return CONTINUE;
}
Shell::Status Shell::yaoslike_complete()
{
  prev_complete_num = ed.complete2();
  return CONTINUE;
}

Shell::Status Shell::complete_to_fullpath()
{
  prev_complete_num = ed.complete_to_fullpath(NULL);
  return CONTINUE;
}

Shell::Status Shell::complete_to_url()
{
  prev_complete_num = ed.complete_to_fullpath("file:///");
  return CONTINUE;
}


Shell::Status Shell::input_terminate()
{
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
  ed.go_tail();

  return TERMINATE;
}

int Shell::replace_last_history(const char *s)
{
  int len=strlen(s);
  History *tmp=(History*)malloc(sizeof(History)+len);
  if( tmp == NULL )
    return -1;
  
  strcpy( tmp->buffer , s );
  if( history != NULL ){
    tmp->next = history->next;
    if( history->next != NULL )
      history->next->prev = tmp;
    tmp->prev = history->prev;    
    if( history->prev != NULL )
      history->prev->next = tmp;
    free( history );
  }else{
    tmp->next = NULL;
    tmp->prev = NULL;
  }
  history = tmp;

  return 0;
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
  /* Edlin2::canna_to_alnum(); */
  ed.clean_up();
  changed = 0;
  cur = NULL;
  return CONTINUE;
}

Shell::Status Shell::erasebol()
{
  if( ed.position() > 0 ){
    ed.erasebol();
    changed = 1;
  }
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
  if(printexitvalue&&execute_result){
    printf("Exit %i\n",execute_result);
  }
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

struct keytable_tg {
  const char *name;
  int code;
} keytable[] ={
#include "keynames.cc"
};

struct functable_tg {
  const char *name;
  Shell::Status (Shell::*method)();
} functable[] ={
#include "bindfunc.cc"
};

int compare_with_top(const void *key,const void *el)
{
  /* この比較関数は、構造体の最初のメンバが
   * キー文字列へのポインタであるのが前提
   * (Cでは許されるが、C++でも大丈夫?)
   */
  const unsigned char *s1= (const unsigned char *)key;
  const unsigned char *s2=*(const unsigned char **)el;
  
  for(;;){
    int c1=tolower(*s1) , c2=tolower(*s2);
    if( c1 != c2 )
      return c1-c2;
    if( c1 == '\0' )
      return 0;
    ++s1,++s2;
  }
}

int Shell::bindkey(const char *keyname, const char *funcname )
{
  struct keytable_tg  *key;
  struct functable_tg *func;
  
  key=(struct keytable_tg *)bsearch(  keyname
				    , keytable 
				    , numof(keytable)
				    , sizeof(keytable[0])
				    , compare_with_top );
  if( key == NULL )
    return 1;
  
  func = (struct functable_tg *)bsearch(  funcname
					, functable
					, numof(functable)
					, sizeof(functable[0])
					, compare_with_top );
  if( func == NULL )
    return 2;
  
  bindmap[ key->code ] = func->method;
  bindmap_usage_key[ key->code ] = key->name;
  bindmap_usage_func[ key->code ] = func->name;

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

static Shell::History *i_search_core( Shell::History *cur,const char *sekstr
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
  
static Shell::History *rev_i_search_core(  Shell::History *cur
					 , const char *sekstr
					 , int &findpos )
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
