#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/kbdscan.h>
#include <process.h>

#define INCL_WIN
#define INCL_VIO
#include <os2.h>

#include "hash.h"
#include "edlin.h"
#include "complete.h"
#include "nyaos.h"
#include "keyname.h"

#define CTRL(a) ((a) & 0x1F)
#define KEY(a)  ((K_##a & 0xFF) | 0x100)

extern HAB hab;
extern int execute_result;
int printexitvalue=0;
Shell::History *Shell::history=NULL;
int Shell::nhistories=0;
int Shell::ctrl_d_eof=0;

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
  char *funcname;
} base_bind_table[]={
  { CTRL('H') , &Shell::backspace
      , "CTRL_H"  , "backward_delete_char  (default)"},
  { KEY(UP),&Shell::vz_prev_history, "UP","vz_prev_history (default)" },
  { KEY(DOWN) , &Shell::vz_next_history, "DOWN","vz_next_history  (default)" },
  { KEY(RIGHT), &Shell::forward_char, "RIGHT","forward_char  (default)" },
  { KEY(LEFT) , &Shell::backward_char,"LEFT","backward_char  (default)" },
  { KEY(DEL)  , &Shell::simple_delete,"DEL","delete_char  (default)"},
  { KEY(INS)  , &Shell::flip_over,"INS","flip_overwrite  (default)"},
  { KEY(ALT_F4) , &Shell::bye ,"CTRL_Z","bye  (default)"},
  { '\t'      , &Shell::tcshlike_complete,"TAB","complete  (default)" },
  { KEY(CTRL_TAB),&Shell::yaoslike_complete
      ,"CTRL_TAB","complete2  (default)" },
  {KEY(ALT_RETURN),&Shell::yaoslike_complete
     ,"CTRL_TAB","complete2  (default)"},
  { '\r'      , &Shell::input_terminate,"ENTER","newline  (default)" },
  { CTRL('L') , &Shell::repaint,"CTRL_L","clear_screen  (default)" },
  { KEY(HOME) , &Shell::go_ahead,"HOME","beginning_of_line  (default)" },
  { KEY(END)  , &Shell::go_tail,"END","end_of_line (default)" },
  { CTRL('U') , &Shell::cancel,"CTRL_U","kill_whole_line  (default)" },
  { '\x1B'    , &Shell::cancel,"ESCAPE","kill_whole_line  (default)" },
  { CTRL('C') , &Shell::abort, "CTRL_C","abort (default)" },
  { KEY(F1)   , &Shell::complete_to_fullpath , "F1","complete_to_fullpath" },
  { KEY(F2)   , &Shell::complete_to_url , "F2","complete_to_url" },
  { CTRL('V') , &Shell::quoted_insert , "CTRL_V" , "quoted_insert" },
  { KEY(CTRL_INS) , &Shell::copy , "CTRL_INS" , "copy" },
  { KEY(SHIFT_INS) , &Shell::paste , "SHIFT_INS" , "paste" },
  { KEY(CTRL_DEL) , &Shell::cut , "CTRL_DEL" , "cut" },
  { KEY(ALT_X) , &Shell::cut , "ALT_X" , "cut" },
  { KEY(ALT_C) , &Shell::copy , "ALT_C" , "copy" },
  { KEY(ALT_V) , &Shell::paste , "ALT_V" , "paste" },
  { KEY(ALT_N) , &Shell::keyname_insert , "F3" , "name" },
}, nyaos_bind_table[]={
  { CTRL('P') , &Shell::vz_prev_history,"CTRL_P","vz_prev_history  (nyaos)"},
  { CTRL('N') , &Shell::vz_next_history,"CTRL_N","vz_next_history  (nyaos)" },
  { CTRL('F') , &Shell::forward_char,"CTRL_F","forward_char  (nyaos)" },
  { CTRL('B') , &Shell::backward_char,"CTRL_B","backward_char  (nyaos)" },
  { CTRL('D'),&Shell::tcshlike_ctrl_d,"CTRL_D","delete_char_or_list  (nyaos)"},
  { CTRL('K') , &Shell::eraseline,"CTRL_K","kill_line  (nyaos)" },
  { CTRL('A') , &Shell::go_ahead,"CTRL_A","beginning_of_line  (nyaos)" },
  { KEY(ALT_F), &Shell::forward_word,"ALT_F","forward_word  (nyaos)" },
  { KEY(ALT_B), &Shell::backward_word,"ALT_B","backward_word  (nyaos)" },
  { CTRL('E') , &Shell::go_tail,"CTRL_E","end_of_line  (nyaos)" },
  { CTRL('S') , &Shell::i_search,"CTRL_S","i_search (nyaos)" },
  { CTRL('R') , &Shell::rev_i_search,"CTRL_R","rev_i_search (nyaos)" },
  { CTRL('T') , &Shell::swapchars,"CTRL_T","swapchars (nyaos)" },
  { CTRL('Y') , &Shell::paste,"CTRL_P","paste (nyaos)" },
  { KEY(ALT_W)  , &Shell::copy , "ALT_W" , "copy (nyaos)" },
  { KEY(CTRL_SPACE) , &Shell::marking , "CTRL_SPACE","mark" },
  { CTRL('W') , &Shell::cut,"CTRL_W","cut (nyaos)" },
}, tcsh_bind_table[]={
  { CTRL('P') , &Shell::previous_history,"CTRL_P","previous_history  (tcsh)"},
  { CTRL('N') , &Shell::next_history,"CTRL_N","next_history  (tcsh)" },
  { CTRL('F') , &Shell::forward_char,"CTRL_F","forward_char  (tcsh)" },
  { CTRL('B') , &Shell::backward_char,"CTRL_B","backward_char  (tcsh)" },
  { CTRL('D') , &Shell::tcshlike_ctrl_d
      ,"CTRL_D","delete_char_or_list  (tcsh)"},
  { CTRL('K') , &Shell::eraseline,"CTRL_K","kill_line  (tcsh)" },
  { CTRL('A') , &Shell::go_ahead,"CTRL_A","beginning_of_line  (tcsh)" },
  { KEY(ALT_F), &Shell::forward_word,"ALT_F","forward_word  (tcsh)" },
  { KEY(ALT_B), &Shell::backward_word,"ALT_B","backward_word  (tcsh)" },
  { CTRL('E') , &Shell::go_tail,"CTRL_E","end_of_line  (tcsh)" },
  { CTRL('S') , &Shell::i_search,"CTRL_S","i_search (tcsh)" },
  { CTRL('R') , &Shell::rev_i_search,"CTRL_R","rev_i_search (tcsh)" },
  { CTRL('T') , &Shell::swapchars,"CTRL_T","swapchars (tcsh)" },
  { CTRL('Y') , &Shell::paste,"CTRL_P","paste (tcsh)" },
  { KEY(ALT_W), &Shell::copy , "ALT_W" , "copy (tcsh)" },
  { KEY(CTRL_SPACE) , &Shell::marking , "CTRL_SPACE","mark (tcsh)" },
  { CTRL('W') , &Shell::cut,"CTRL_W","cut (tcsh)" },
}, wordstar_bind_table[]={
  { CTRL('E') , &Shell::vz_prev_history,"CTRL_E","vz_prev_history (vz)" },
  { CTRL('X') , &Shell::vz_next_history,"CTRL_X","vz_next_history (vz)" },
  { CTRL('D') , &Shell::forward_char,"CTRL_D","forward_char (vz)" },
  { CTRL('S') , &Shell::backward_char,"CTRL_S","backward_char (vz)" },
  { CTRL('G') , &Shell::simple_delete,"CTRL_G","delete_char (vz)" },
  { CTRL('A') , &Shell::backward_word,"CTRL_A","backward_word (vz)" },
  { CTRL('F') , &Shell::forward_word,"CTRL_F","forward_word (vz)" },
  { CTRL('P') , &Shell::paste,"CTRL_P","paste (vz)" },
  { CTRL('B') , &Shell::marking , "CTRL_B","mark (vz)" },  
  { CTRL('Y') , &Shell::cut,"CTRL_W","cut (vz)" },
  { CTRL('J') , &Shell::paste,"CTRL_J","paste (vz)" },
};

Shell::Status (Shell::*Shell::bindmap[])();
const char *Shell::bindmap_usage_key[];
char *Shell::bindmap_usage_func[];

Shell::Shell( FILE *fp=stdout )
:      Edlin2(fp) , prompt(0) , topline_permission(true) 
     , changed(false) , overwrite(false) , prevchar(0x1FF)
     , prev_complete_num(0) , cur(NULL) 
{
  this->clean_up();
}

Shell::~Shell()
{
}

void Shell::bindkey_base()
{
  static bool firstcalled=true;
  if( ! firstcalled ){
    /* hotkey が bindされている場合、bindmap_usage_func には、
     * ヒープ内の文字列が格納されていることになるので、解放してやる。
     */
    for(unsigned i=0; i<numof(bindmap); i++ )
      if( bindmap[ i ] == &Shell::hotkey ){
	free( bindmap_usage_func[ i ] );
      }
    firstcalled = false;
  }

  for(unsigned i=0;i<numof(bindmap);i++){
    bindmap[ i ] = &self_insert;
    bindmap_usage_key[ i ]  = NULL;
    bindmap_usage_func[ i ] = NULL;
  }
  for(unsigned i=0;i<numof(base_bind_table);i++){
    bindmap[ base_bind_table[i].key ] = base_bind_table[i].method;
    bindmap_usage_key[ base_bind_table[i].key ] = base_bind_table[i].name;
    bindmap_usage_func[ base_bind_table[i].key] = base_bind_table[i].funcname;
  }
}

void Shell::bindkey_nyaos()
{
  bindkey_base();
  for(unsigned i=0;i<numof(nyaos_bind_table);i++){
    bindmap[ nyaos_bind_table[i].key ] = nyaos_bind_table[i].method;
    bindmap_usage_key[ nyaos_bind_table[i].key ] = nyaos_bind_table[i].name;
    bindmap_usage_func[ nyaos_bind_table[i].key ]= nyaos_bind_table[i].funcname;
  }
}

void Shell::bindkey_tcshlike()
{
  bindkey_base();
  for(unsigned i=0;i<numof(tcsh_bind_table);i++){
    bindmap[ tcsh_bind_table[i].key ] = tcsh_bind_table[i].method;
    bindmap_usage_key[ tcsh_bind_table[i].key ] = tcsh_bind_table[i].name;
    bindmap_usage_func[ tcsh_bind_table[i].key ]= tcsh_bind_table[i].funcname;
  }
}

void Shell::bindkey_wordstar()
{
  bindkey_base();
  for(unsigned i=0;i<numof(wordstar_bind_table);i++){
    bindmap[ wordstar_bind_table[i].key ] = wordstar_bind_table[i].method;
    bindmap_usage_key[ wordstar_bind_table[i].key ]
      = wordstar_bind_table[i].name;
    bindmap_usage_func[ wordstar_bind_table[i].key ]
      = wordstar_bind_table[i].funcname;
  }
}

/* 上書きモードと挿入モードの切り換えを行う 
 * return 常に CONTINUE
 */
Shell::Status Shell::flip_over(){
  overwrite = !overwrite;

  if( option_vio_cursor_control ){
    VIOCURSORINFO info;
    
    if( this->isOverWrite() ){ /* 上書きモードの時は半分サイズ */
      info.yStart = (unsigned short)-50;
    }else{			 /* 挿入モードの時はフルサイズ */
      info.yStart = 0;
    }
    info.cEnd   = (unsigned short)-100;
    info.cx = info.attr = 0;
    
    VioSetCurType( &info , 0 );
  }
  return CONTINUE;
}

Shell::Status Shell::self_insert()
{
  if( (ch >= ' '  &&  ch < 0x100) || ch >= 0x200  ){
    if( overwrite )
      erase();
    Edlin::insert(ch);
    Edlin::forward();
    changed = true;
  }
  return CONTINUE;
}

Shell::Status Shell::quoted_insert()
{
  Edlin::quoted_insert( getkey() );
  return CONTINUE;
}

Shell::Status Shell::keyname_insert()
{
  KeyName *name=KeyName::find( getkey() );
  if( name != 0 )
    Edlin::insert_and_forward( name->name );
  return CONTINUE;
}

Shell::Status Shell::previous_history()
{
  if( cur==NULL || changed )
    cur = history;
  else
    cur=cur->prev;
  clean_up();
  if( cur != NULL )
    insert_and_forward(cur->buffer);
  changed = false;
  return CONTINUE;
}

Shell::Status Shell::next_history()
{
  clean_up();
  if( cur != NULL  &&  (cur=cur->next) != NULL  )
    insert_and_forward(cur->buffer);
  
  changed = false;
  return CONTINUE;
}

Shell::Status Shell::bye()
{
  return QUIT;
}

Shell::Status Shell::forward_char()
{
  Edlin::forward();
  return CONTINUE;
}
Shell::Status Shell::backward_char()
{
  Edlin::backward();
  return CONTINUE;
}
Shell::Status Shell::simple_delete()
{
  erase();
  return CONTINUE;
}

Shell::Status Shell::tcshlike_ctrl_d()
{
  if( getPos() < getLen() ){
    changed = true;
    erase();
  }else if( getLen() == 0 ){
    return ctrl_d_eof ? QUIT : CONTINUE ;
  }else{
    complete_list();
  }
  return CONTINUE;
}

Shell::Status Shell::backspace()
{
  backward();
  erase();
  changed = true;
  return CONTINUE;
}
Shell::Status Shell::tcshlike_complete()
{
  if( prevchar=='\t'  &&  prev_complete_num != 1 ){
    if( getLen() != 0 )
      complete_list();
  }else{
    prev_complete_num = complete();
    changed = true;
  }
  return CONTINUE;
}
Shell::Status Shell::yaoslike_complete()
{
  prev_complete_num = completeFirst();
  return CONTINUE;
}

Shell::Status Shell::complete_to_fullpath()
{
  prev_complete_num = Edlin::complete_to_fullpath(NULL);
  return CONTINUE;
}

Shell::Status Shell::complete_to_url()
{
  prev_complete_num = Edlin::complete_to_fullpath("file:///");
  return CONTINUE;
}

extern int are_spaces(const char *s);

int Shell::regist_history(const char *s)
{
  int length;
  const char *text;
  if( text == NULL ){
    text = Edlin::getText();
    length = len;
  }else{
    text = s;
    length = strlen(s);
  }

  /* ------- ヒストリに登録する ---------- */
  History *tmp=(History*)malloc(sizeof(History)+length);
  if( tmp == NULL )
    return -1;
  
  memcpy(tmp->buffer,text,length);
#if 0
  for(int i=0;i<length;i++){
    if( text[i] == '\n' )
      tmp->buffer[i] = ' ';
    else
      tmp->buffer[i] = text[i];
  }
#endif
  tmp->buffer[length] = '\0';

  /* 古い方が prev , 新しい方が next側 */
  tmp->prev = history;
  tmp->next = NULL;
  
  if( history != NULL )
    history->next = tmp;
  history = tmp;
  
  /* curが NULL の時、次回のヒストリ参照で、
   * 最初に現れる文字列がトップになる。 */
  
  cur = NULL;
  nhistories++;
  return 0;
}

Shell::Status Shell::input_terminate()
{
  go_tail();
  int len=getLen();
  if( len <= 0  ||  are_spaces( getText() ) )
    return CANCEL;
  else
    return TERMINATE;
}

/* シェルのヒストリに追加する。
 *	s ヒストリ文字列
 * return 0:成功 , -1:失敗
 */
int Shell::append_history(const char *s)
{
  int len=strlen(s);
  History *tmp=(History*)malloc(sizeof(History)+len);
  if( tmp == NULL )
    return -1;

  strcpy( tmp->buffer , s );
  if( history != NULL )
    history->next = tmp;
  tmp->prev = history;
  tmp->next = NULL;
  history = tmp;
  return 0;
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
  cls();
  return CONTINUE;
}
Shell::Status Shell::go_ahead()
{
  Edlin::go_ahead();
  return CONTINUE;
}

Shell::Status Shell::go_tail()
{
  Edlin::go_tail();
  return CONTINUE;
}

Shell::Status Shell::cancel()
{
  /* Edlin2::canna_to_alnum(); */
  clean_up();
  changed = false;
  cur = NULL;
  return CONTINUE;
}

Shell::Status Shell::erasebol()
{
  if( getPos() > 0 ){
    erasebol();
    changed = true;
  }
  return CONTINUE;
}

static void paste_to_clipboard(const char *s,int length)
{
  char *buffer;
  DosAllocSharedMem( (void**)(&buffer) 
		    , NULL
		    , (ULONG)length+1
		    , PAG_COMMIT | PAG_READ | PAG_WRITE 
		    | OBJ_TILE   | OBJ_GIVEABLE );

  memcpy( buffer , s , length );
  buffer[ length ]  = '\0';

  WinOpenClipbrd(hab);
  WinSetClipbrdOwner(hab,NULLHANDLE);
  WinEmptyClipbrd(hab);
  WinSetClipbrdData( hab, (ULONG)buffer, CF_TEXT, CFI_POINTER );
  WinCloseClipbrd(hab);
}


Shell::Status Shell::eraseline()
{
  int pos=getPos();
  int len=getLen();
  if( pos < len ){
    paste_to_clipboard(getText()+pos , len-pos );
    Edlin::eraseline();
    changed = true;
  }
  return CONTINUE;
}
Shell::Status Shell::forward_word()
{
  Edlin::forward_word();
  return CONTINUE;
}

Shell::Status Shell::backward_word()
{
  Edlin::backward_word();
  return CONTINUE;
}

/* 文字列 base が tail で終わっていれば、その位置を。
 * さもなければ NULL を返す。( DBCS 対応 )
 */
static char *endsWith(char *base,int tail)
{
  while( *base != '\0' ){
    if( *base == tail  &&  *(base+1) == '\0' )
      return base;
    
    if( is_kanji(*base) )
      ++base;
    ++base;
  }
  return NULL;
}

int Shell::line_input(const char *_prompt)
{
  raw_mode();
  prompt = _prompt;
  fputs(prompt,stdout);
  fflush(stdout);
  init();

  for(;;){
    ch=Edlin2::getkey();
    if( ch < (int)numof(bindmap) ){
      Status rc=(this->*bindmap[ch])();
      switch( rc ){
      case TERMINATE:
	cocked_mode();
	pack();
	return getLen();

      case CANCEL:
	return 0;
	
      case CONTINUE:
	prevchar = ch;
	continue;

      default:
	cocked_mode();
	pack();
	return rc;
      }
    }else{
      self_insert();
      prevchar = ch;
    }
  }
}

int Shell::line_input(const char *prompt1,const char *prompt2,const char **rv )
{
  int len=line_input(prompt1);
  if( len <= 0 )
    return len;
  
  char *s=strdup( Edlin2::getText() );
  if( s == NULL )
    return FATAL;
  
  char *cont;
  while( (cont=endsWith(s,'^')) != NULL ){
    putchr('\n');
    *cont = '\n';

    int len2=line_input(prompt2);
    if( len2 < 0 ){
      free(s);
      return len2;
    }

    char *new_s = (char*)realloc(s, len+len2 );
    if( new_s == NULL ){
      free(s);
      return FATAL;
    }
    strcpy(new_s+len , Edlin::getText() );
    s = new_s;
    len += len2;
  }

  if( rv != 0 )
    *rv = s;
  
  // ↓ そのうち外して、外部から明示的に登録させるようにする. 
  regist_history(s);
  return len;
}


struct functable_tg {
  char *name;
  Shell::Status (Shell::*method)();
} functable[] ={
#   include "bindfunc.cc"
};

Shell::Status Shell::hotkey()
{
  if( 0 < ch && ch < 0x200  &&  bindmap_usage_func[ ch ] != NULL ){
    clean_up();
    changed = false;
    cur = NULL;
    
    fputc('\n',stdout);
    execute( stdin , bindmap_usage_func[ch] );
    // spawnlp(P_WAIT, bindmap_usage_func[ch],bindmap_usage_func[ch],NULL);
    // system( bindmap_usage_func[ ch ] );
    return ABORT;
  }
  return CONTINUE;
}

int Shell::bind_hotkey(const char *keyname, const char *program )
{
  KeyName *keyinfo=KeyName::find( keyname );
  if( keyinfo == NULL )
    return 1;

  if( bindmap[ keyinfo->code ] == &hotkey )
    free( bindmap_usage_func[ keyinfo->code ] );
  
  bindmap[ keyinfo->code ] = &hotkey;
  bindmap_usage_key[ keyinfo->code ] = keyinfo->name;
  bindmap_usage_func[ keyinfo->code ] = strdup(program);
  return CONTINUE;
}

int Shell::bindkey(const char *keyname, const char *funcname )
{
  KeyName *keyinfo=KeyName::find( keyname );
  if( keyinfo == NULL )
    return 1;

  struct functable_tg *func
    = static_cast <functable_tg *>
      ( bsearch(  funcname
		, functable
		, numof(functable)
		, sizeof(functable[0])
		, KeyName::compareWithTop )
       );

  if( func == NULL )
    return 2;

  if( bindmap[ keyinfo->code ] == &hotkey )
    free( bindmap_usage_func[ keyinfo->code ] );
  
  bindmap[ keyinfo->code ] = func->method;
  bindmap_usage_key[ keyinfo->code ] = keyinfo->name;
  bindmap_usage_func[ keyinfo->code ] = func->name;

  return 0;
}

void Shell::bindlist(FILE *fout)
{
  for(unsigned int i=0;i<numof(bindmap);i++){
    if( bindmap_usage_key[i] != NULL &&  bindmap_usage_func[i] != NULL ){
      if( bindmap[i] == &hotkey ){
	fprintf(fout,"%-8s : hotkey to call \"%s\".\n",
		bindmap_usage_key[i] , bindmap_usage_func[i] );
      }else{
	fprintf(fout,"%-8s : %s\n",
		bindmap_usage_key[i],bindmap_usage_func[i] );
      }
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
    message("(%si-search)`%s':%s"
	       ,isrev ? "reverse-" : ""
	       ,sekstr
	       ,(cur==NULL ? "" : cur->buffer) );

    unsigned key=getkey();

    if( key < 0 || key > 0x1FF 
       || (bindmap[ key ] == &self_insert && isprint(key & 255) )){
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
      
    }else if( key >= 0 && bindmap[ key ] == &rev_i_search ){
      isrev = 1;

      if( cur==NULL || cur->prev==NULL )
	tmp = rev_i_search_core(history , sekstr , findpos );
      else
	tmp = rev_i_search_core(cur->prev , sekstr , findpos );

      if( tmp != NULL )
	cur = tmp;

    }else if( key >= 0 && bindmap[ key ] == &i_search ){
      isrev = 0;

      if( cur != NULL  &&  cur->next != NULL )
	tmp = i_search_core(cur->next , sekstr , findpos );

      if( tmp != NULL )
	cur = tmp;

    }else{ /* それ以外の機能キーの場合は、サーチを終結する。*/
      if( cur != NULL  &&  key != '\007' ){
	/* 入力バッファの中にペーストする。*/
	clean_up();
	insert_and_forward( cur->buffer );
	locate(findpos+seklen);
      }else{
	cleanmsg();
      }
      if( key < 0 || key > numof(bindmap) || bindmap[key]==&input_terminate ){
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


Shell::Status Shell::copy()
{
  int markpos=getMarkPos();

  if( markpos < 0  || markpos > getLen() )
    return CONTINUE;

  int pos =getPos();

  int at,length;
  if( pos < markpos ){
    at = pos;
    length = markpos - pos;
  }else if( pos > markpos ){
    at = markpos;
    length = pos - markpos;
  }else{
    return CONTINUE;
  }
  paste_to_clipboard( getText()+at , length );
  return CONTINUE;
}

Shell::Status Shell::cut()
{
  Shell::copy();
  Edlin::cut();
  return CONTINUE;
}

Shell::Status Shell::paste()
{
  /* クリップボードから取得するテスト	*/
  WinOpenClipbrd(hab);
  WinSetClipbrdOwner(hab,NULLHANDLE);
  ULONG	ulFmtInfo;
  if( WinQueryClipbrdFmtInfo( hab, CF_TEXT, &ulFmtInfo ) ){
    char *text = (char*)WinQueryClipbrdData( hab, CF_TEXT );
    if( text != NULL )
      insert_and_forward( text );
  }
  WinCloseClipbrd(hab);
  return CONTINUE;
}

Shell::Status Shell::marking()
{
  Edlin::marking();
  return CONTINUE;
}
