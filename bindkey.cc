#include <stdlib.h>
#include <string.h>
#include <sys/kbdscan.h>

#include "hash.h"
#include "edlin.h"
#include "complete.h"
#include "nyaos.h"
#include "keyname.h"

#define CTRL(a) ((a) & 0x1F)
#define KEY(a)  ((K_##a & 0xFF) | 0x100)

extern int execute_result;
int printexitvalue=0;
#ifdef NEW_HISTORY
Shell::Histories history;
#else
Shell::History *Shell::history=NULL;
int Shell::nhistories=0;
#endif

class SelfInsert : public BindCommand0 {
  Edlin::Status operator() (Shell &s)
    { return s.self_insert(); }
  void printUsage(FILE *fp){}
  operator cmd_t () { return &Shell::self_insert; }
};

class BindCommand : public BindCommand0 {
  const char *keyname;
  const char *funcname;
  const char *from;
  Edlin::Status (Shell::*method)();
public:
  Edlin::Status operator() (Shell &s)
    {  return (s.*method)();  }
  void printUsage(FILE *fp){
    fprintf(fp,"%-8s : %s",keyname,funcname);
    if( from )
      fprintf(fp,"(%s)",from);
    putc('\n',fp);
  }
  BindCommand(  const char *_funcname 
	      , const char *_keyname 
	      , Edlin::Status (Shell::*_method)() 
	      , const char *_from=0 )
    : keyname(_keyname) , funcname(_funcname) 
      , from(_from)    , method(_method) { }

  operator cmd_t ()  {  return method; }
};

class HotkeyCommand : public BindCommand0 {
  const char *keyname;
  char *cmdline;
  int opt;
public:
  Edlin::Status operator() (Shell &s)
    {  return s.hotkey( cmdline , opt );  }
  void printUsage(FILE *fp){
    fprintf(fp,"%-8s : hotkey to call \"%s\".\n"
	    , keyname , cmdline );
  }

  HotkeyCommand( const char *_keyname , const char *_cmdline , int _opt )
    : keyname(_keyname) , cmdline( strdup(_cmdline) ) , opt(_opt) { }
  ~HotkeyCommand()
    {  free(cmdline); }

  operator cmd_t () { return 0; }
};

struct bind_t{
  unsigned key;
  Edlin::Status (Shell::*method)();
  const char *name;
  char *funcname;
};

BindCommand0 *Shell::bindmap[];

Shell::Shell( FILE *fp=stdout )
:      Edlin2(fp) , prompt(0) , topline_permission(true) 
     , changed(false) , overwrite(false) , prevchar(0x1FF)
     , prev_complete_num(0) , cur(NULL) 
{
  this->clean_up();
}

void Shell::bindkey_base()
{
  const static bind_t base_bind_table[]={
    { CTRL('H') , &Shell::backspace , "CTRL_H"  , "backward_delete_char"},
    { KEY(UP),&Shell::vz_prev_history, "UP","vz_prev_history" },
    { KEY(DOWN) , &Shell::vz_next_history, "DOWN","vz_next_history" },
    { KEY(RIGHT), &Shell::forward_char, "RIGHT","forward_char" },
    { KEY(LEFT) , &Shell::backward_char,"LEFT","backward_char" },
    { KEY(DEL)  , &Shell::simple_delete,"DEL","delete_char"},
    { KEY(INS)  , &Shell::flip_over,"INS","flip_overwrite"},
    { KEY(ALT_F4) , &Shell::bye ,"CTRL_Z","bye"},
    { '\t'      , &Shell::tcshlike_complete,"TAB","complete" },
    { KEY(CTRL_TAB),&Shell::yaoslike_complete,"CTRL_TAB","complete2" },
    {KEY(ALT_RETURN),&Shell::yaoslike_complete,"CTRL_TAB","complete2"},
    { '\r'      , &Shell::input_terminate,"ENTER","newline" },
    { CTRL('L') , &Shell::repaint,"CTRL_L","clear_screen" },
    { KEY(HOME) , &Shell::go_ahead,"HOME","beginning_of_line" },
    { KEY(END)  , &Shell::go_tail,"END","end_of_line" },
    { CTRL('U') , &Shell::cancel,"CTRL_U","kill_whole_line" },
    { '\x1B'    , &Shell::cancel,"ESCAPE","kill_whole_line" },
    { CTRL('C') , &Shell::abort, "CTRL_C","abort" },
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
    { KEY(ALT_Y) , &Shell::paste_test , "ALT_Y" , "paste_test" },
  };

  static bool firstcalled=true;
  if( firstcalled == false){
    for(unsigned i=0; i<numof(bindmap); i++ )
      delete bindmap[i];
  }
  
  for(unsigned i=0;i<numof(bindmap);i++)
    bindmap[ i ] = new SelfInsert;

  for(unsigned i=0;i<numof(base_bind_table);i++){
    bindmap[ base_bind_table[i].key ] 
      = new BindCommand(  base_bind_table[i].name
			, base_bind_table[i].funcname
			, base_bind_table[i].method
			, "default" );
  }
  firstcalled = false;
}

void Shell::bindkey_nyaos()
{
  const static bind_t table[]={
    { CTRL('P') , &Shell::vz_prev_history,"CTRL_P","vz_prev_history"},
    { CTRL('N') , &Shell::vz_next_history,"CTRL_N","vz_next_history" },
    { CTRL('F') , &Shell::forward_char,"CTRL_F","forward_char" },
    { CTRL('B') , &Shell::backward_char,"CTRL_B","backward_char" },
    { CTRL('D') , &Shell::tcshlike_ctrl_d,"CTRL_D","delete_char_or_list"},
    { CTRL('K') , &Shell::eraseline,"CTRL_K","kill_line" },
    { CTRL('A') , &Shell::go_ahead,"CTRL_A","beginning_of_line" },
    { KEY(ALT_F), &Shell::forward_word,"ALT_F","forward_word" },
    { KEY(ALT_B), &Shell::backward_word,"ALT_B","backward_word" },
    { CTRL('E') , &Shell::go_tail,"CTRL_E","end_of_line" },
    { CTRL('S') , &Shell::i_search,"CTRL_S","i_search" },
    { CTRL('R') , &Shell::rev_i_search,"CTRL_R","rev_i_search" },
    { CTRL('T') , &Shell::swapchars,"CTRL_T","swapchars" },
    { CTRL('Y') , &Shell::paste,"CTRL_P","paste" },
    { KEY(ALT_W)  , &Shell::copy , "ALT_W" , "copy" },
    { KEY(CTRL_SPACE) , &Shell::marking , "CTRL_SPACE","mark" },
    { CTRL('W') , &Shell::cut,"CTRL_W","cut" },
  };

  bindkey_base();
  for(unsigned i=0;i<numof(table);i++){
    bindmap[ table[i].key ] 
      = new BindCommand(  table[i].name 
			, table[i].funcname 
			, table[i].method
			, "nyaos" );
  }
}

void Shell::bindkey_tcshlike()
{
  const static bind_t  tcsh_bind_table[]={
    { CTRL('P') , &Shell::previous_history,"CTRL_P","previous_history"},
    { CTRL('N') , &Shell::next_history,"CTRL_N","next_history" },
    { CTRL('F') , &Shell::forward_char,"CTRL_F","forward_char" },
    { CTRL('B') , &Shell::backward_char,"CTRL_B","backward_char" },
    { CTRL('D') , &Shell::tcshlike_ctrl_d,"CTRL_D","delete_char_or_list"},
    { CTRL('K') , &Shell::eraseline,"CTRL_K","kill_line" },
    { CTRL('A') , &Shell::go_ahead,"CTRL_A","beginning_of_line" },
    { KEY(ALT_F), &Shell::forward_word,"ALT_F","forward_word" },
    { KEY(ALT_B), &Shell::backward_word,"ALT_B","backward_word" },
    { CTRL('E') , &Shell::go_tail,"CTRL_E","end_of_line" },
    { CTRL('S') , &Shell::i_search,"CTRL_S","i_search" },
    { CTRL('R') , &Shell::rev_i_search,"CTRL_R","rev_i_search" },
    { CTRL('T') , &Shell::swapchars,"CTRL_T","swapchars" },
    { CTRL('Y') , &Shell::paste,"CTRL_P","paste" },
    { KEY(ALT_W), &Shell::copy , "ALT_W" , "copy" },
    { KEY(CTRL_SPACE) , &Shell::marking , "CTRL_SPACE","mark" },
    { CTRL('W') , &Shell::cut,"CTRL_W","cut" },
  };

  bindkey_base();
  for(unsigned i=0;i<numof(tcsh_bind_table);i++){
    bindmap[ tcsh_bind_table[i].key ] 
      = new BindCommand( tcsh_bind_table[i].name
			,tcsh_bind_table[i].funcname
			,tcsh_bind_table[i].method
			,"tcsh");
  }
}

void Shell::bindkey_wordstar()
{
  static bind_t  wordstar_bind_table[]={
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

  bindkey_base();
  for(unsigned i=0;i<numof(wordstar_bind_table);i++){
    bindmap[ wordstar_bind_table[i].key ] 
      = new BindCommand(  wordstar_bind_table[i].name
			, wordstar_bind_table[i].funcname
			, wordstar_bind_table[i].method
			, "ws" );
  }
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
      Status rc=(*bindmap[ch])(*this);
      switch( rc ){
      case TERMINATE:
	cocked_mode();
	pack();
	return getLen();

      case CANCEL:
	cocked_mode();
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
  if( len < 0 )
    return len;
  else if( len == 0 ){
    if( rv != 0 )
      *rv = NULL;
    return 0;
  }
  
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
  Edlin::Status (Shell::*method)();
} functable[] ={
#   include "bindfunc.cc"
};


Edlin::Status Shell::hotkey(const char *commands,int opt)
{
  extern int option_cmdlike_crlf;

  clean_up();
  changed = false;
  cur = NULL;
  
  fputc('\n',stdout);
  execute( stdin , commands );

  if( option_cmdlike_crlf )
    putc( '\n' , fp );
  fputs( prompt , fp );
  int i=0;
  while( i < len )
    putnth( i++ );
  putbs( i - pos );
  return CONTINUE;
}

int Shell::bind_hotkey(const char *keyname, const char *program , int opt)
{
  KeyName *keyinfo=KeyName::find( keyname );
  if( keyinfo == NULL )
    return 1;

  delete bindmap[ keyinfo->code ];
  bindmap[ keyinfo->code ]
    = new HotkeyCommand( keyinfo->name , program , opt);

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

  delete bindmap[ keyinfo->code ];
  bindmap[ keyinfo->code ]
    = new BindCommand(  keyinfo->name
		      , func->name
		      , func->method );
  return 0;
}

void Shell::bindlist(FILE *fout)
{
  for(unsigned int i=0;i<numof(bindmap);i++)
    bindmap[i]->printUsage(fout);
}
