/* -*- c++ -*-
 *
 * Edlin          入力の為の基本骨格。端末非依存。純粋仮想クラス
 *  ┗ Edlin2     ANSIエスケープシーケンスによる実装。かんなの対応含む。
 *      └ Shell  プロンプト処理や、キーバインド等も含む「シェル」
 *
 *  └ private継承    ┗ public継承
 */

#ifndef EDLIN_H
#define EDLIN_H

#include <stdio.h>
#include "macros.h"

/* ================ 参照クラス ================         */
class Complete;		/* ファイル名補完の為のクラス   */
class jrKanjiStatus;	/* かんな標準ライブラリの構造体 */

class Edlin{
  enum{ DEFAULT_BUFFER_SIZE = 80 };
protected:
  char *strbuf;        /* アスキーコード              */
  char *atrbuf;        /* 属性フラグコード            */
  
  int pos;             /* カーソルのある文字の桁位置  */
  int len;             /* 全体の byte数               */
  int max;             /* strbufのmax                 */
  int markpos;         /* マークのある桁位置          */

  int msgsize;         /* かんな等のインラインのメッセージのサイズ */
  int bottom_msgsize;  /* かんな等の最下段のメッセージのサイズ */

  bool has_marked;     /* マークがされていたら、true  */
  /*
   * ================ バッファ操作系メソッド ================ 
   */
  /* 場所を作る/削減する(バッファ操作のみ)。戻り値 != 0 で失敗 */
  int makeRoom(int at,int bytes);

  /* カーソル位置に、半角文字ｃを上書き */
  void writeSBChar(int c){
    strbuf[pos] = c ; atrbuf[pos] = SBC ; putnth(pos++);
  }

  /* カーソル位置に、全角文字c1:c2を上書き(バッファ操作のみ) */
  void writeDBChar(int c1,int c2){
    strbuf[pos]=c1; atrbuf[pos] = DBC1ST; putnth(pos++);
    strbuf[pos]=c2; atrbuf[pos] = DBC2ND; putnth(pos++);
  }

  /* カーソル位置の単語の先頭桁位置を得る */
  int  seek_word_top();
  
  /* ================ 表示更新系メソッド ================
   *   termclear を 1以上にすると、末尾をその桁数分消去する。
   */
  void after_repaint(int termclear=-1);   /* カーソル以降のみ更新 */
  void repaint(int termclear=-1);         /* 全行 repaint 更新    */

  virtual void putchr(int c)=0; /* 一文字出力               */
  virtual void putel()=0;       /* カーソル位置以降をクリア */
  virtual void putbs(int i)=0;  /* カーソルをｎ桁戻す       */
  virtual void alert()=0;       /* 警告(普通はbeep音)       */

  void putnth(int nth);         /* n 番目の文字を出力
				 * その位置にマークがあれば、
				 * ちゃんと色を変える */
  void putchrs(const char *s);  /* putchr の複数版 */

  static int setMarkAttr(const char *start,const char *end);

public:
  Edlin();
  virtual ~Edlin();
  bool operator ! () const { return strbuf==0 || atrbuf==0 ; }
  
  enum{ SBC , DBC1ST , DBC2ND };
  
  void init();
  void pack(); /* 入力した制御文字を1byte形式へ置換する。 */

  void insert(int ch);                     /*    半角文字挿入       */
  void insert(int ch1,int ch2);            /*    全角文字挿入       */
  void insert_and_forward(const char *s);  /*    文字列挿入         */
  void quoted_insert(int ch);              /*    制御文字挿入       */

  void cut();
  void erase();               /* ^D 一文字削除         */
  int  forward();             /* ^F カーソル右移動     */
  int  forward(int x);        /*    ｘ桁分右移動       */
  int  backward();            /* ^B カーソル左移動     */
  int  backward(int x);        /*    ｘ桁分左移動       */
  void forward_word();        /* @F カーソル右単語移動 */
  void backward_word();       /* @B カーソル左単語移動 */
  void go_ahead();            /* ^A 先頭へ             */
  void go_tail();             /* ^E 末尾へ             */
  void clean_up();            /* ^U 入力破棄           */
  void erasebol();            /*    カーソル手前を消す */
  void eraseline();           /* ^K カーソル以降を消す */
  void swapchars();           /* ^T カーソル手前二文字を入れ換える */
  virtual void cls(){};       /* ^L 画面クリア(何もしない) */

  /* これらは、導出クラスへ移項すべきもの */
  virtual int complete();	/* TCSH型の補完 */
  virtual int completeFirst();	/* 変換型の補完 */
  virtual int complete_to_fullpath(const char *header);
  /* フルパスへの補完 */

  virtual void complete_list(){}    /* ^D ファイル名リスト   */
  virtual int complete_hook(Complete &){ return 0; }
  /* ↑ ファイル名の他に加える候補があれば、このフック関数を導出する。*/
  
  /* カーソルを適切な位置に移動して入力待ち */
  virtual int getkey(void){ return ::getkey(); }
  
  /* 入力文字列以外のメッセージを表示するメソッド */
  int message(const char *fmt,...);
  void cleanmsg();

  void bottom_message( const char *fmt , ...);
  void clean_bottom();

  void locate(int x);
  void marking(void);

  /*
   * -------- リポート関数 --------
   */
  int getPos()          const throw() { return pos; }
  int getMarkPos()      const throw() { return markpos; }
  int getLen()          const throw() { return len; }
  const char *getText() const throw() { return strbuf; }
  const char *getAttr() const throw() { return atrbuf; }
  int operator[](int n) const throw() { return strbuf[n] & 255; }

  static int complete_tail_char;
  
  // -------- 変換型ファイル名補完 --------
public:
  enum CompleteFunc {
    COMPLETE_CANCEL,
    COMPLETE_PREV,
    COMPLETE_NEXT,
    COMPLETE_FIX,
    COMPLETE_FIX_PLUS,
  };
private:
  static CompleteFunc completeBindmap[ 0x200 ];
public:
  static void initComplete();
  static int  bindCompleteKey(const char *key,const char *func);
};

class Edlin2 : public Edlin {
  static int canna_inited;       /* 初期化されていたら not 0 */
  int print_henkan_koho( jrKanjiStatus &status , const char *mode_string );
protected:
  FILE *fp;
  const char *cursor_on;
  const char *cursor_off;
  
  void putchr(int c);
  void putel();
  void putbs(int i);
  void alert(){ putchr('\a'); }
public:
  Edlin2(FILE *_fp=stdout) : fp(_fp) , cursor_on("") , cursor_off(""){}
#if 0
  Edlin2(char *buffer, int max, FILE *Fp=stdout )
    : Edlin(buffer,max) , fp(Fp) , cursor_on("") , cursor_off(""){}
#endif
  int getkey(void);
  
  void setcursor(char *on,char *off="\x1B[0m")
    { cursor_on = on ; cursor_off = off; }

  static int option_canna;
  static void canna_to_alnum();  /* 強制的に英数モードへ  */
};

extern char dbcstable[256];
int dbcs_table_init();

class Shell : private Edlin2 {
  const char *prompt;
  bool topline_permission;

  void complete_list();
  int complete_hook(Complete &com);
  void cls();
  void re_prompt();
  void paste_to_clipboard(int at,int length);
  
  void alert(){ if( beep_ok ) putchr('\a'); }
public:
  struct History{
    History *prev,*next;
    char buffer[1];
  };
  
  /* $I の為にトップライン上を上書きするか否かの設定メソッド */
  void  allow_use_topline(){ topline_permission = true;  }
  void forbid_use_topline(){ topline_permission = false; }

  /* 元 Shell のパート */
  enum Status {
    CONTINUE,	// 編集続行
    TERMINATE,	// ^M ^J (ヒストリに登録する)
    CANCEL,	// ^M ^J (ヒストリに登録しない)
    QUIT  = -1,	// ^D
    ABORT = -2,	// ^C
    FATAL = -3, // 未知のトラブル
  };
private:
  bool changed;		/* 変更フラグ   */
  bool overwrite;	/* 上書きモード */
  int ch;
  int prevchar;
  int prev_complete_num;

  enum{ NUMOF_BINDMAP = 0x200 };
  static void bindkey_base();
  static Status (Shell::*bindmap[ NUMOF_BINDMAP ])();
  static const char *bindmap_usage_key[ NUMOF_BINDMAP ];
  static char *bindmap_usage_func[ NUMOF_BINDMAP ];
  
  Status search_engine(int isrev);
  int line_input(const char *prompt);
public:
  void setcursor(char *on,char *off="\x1B[0m")
    { Edlin2::setcursor(on,off); }

  static int ctrl_d_eof;
  static void bindkey_wordstar();
  static void bindkey_tcshlike();
  static void bindkey_nyaos();
  static int bindkey(const char *key,const char *funcname);
  static Status (Shell::*get_bindkey_function(int key))()
    { return bindmap[ key ]; }
  static int bind_hotkey(const char *key,const char *program);
  static void bindlist(FILE *fp);

  Shell( FILE *fp=stdout );
  ~Shell();
  bool operator ! () const { return Edlin2::operator !(); }

  int line_input(const char *prompt1,const char *prompt2,const char **str);
private:
  static History *history;
  static int nhistories;
  History *cur;
public:
  static int get_history_number() { return nhistories; }
  static const char *get_nth_history(int n);
  
  // 最新のヒストリ内容を引数の内容と置きかえる。
  static int replace_last_history(const char *s);
  int regist_history(const char *s=0);
  // ヒストリに文字列を加える。
  static int append_history(const char *s);

  bool isOverWrite(){ return overwrite; }

  /* ================ キーにバインド可能なコマンド ================ */

  Status i_search();
  Status rev_i_search();
  Status self_insert();
  Status previous_history();
  Status next_history();
  Status bye();
  Status forward_char();
  Status backward_char();
  Status tcshlike_ctrl_d();
  Status backspace();
  Status tcshlike_complete();
  Status yaoslike_complete();
  Status complete_to_fullpath();
  Status complete_to_url();
  Status input_terminate();
  Status repaint();
  Status go_ahead();
  Status go_forward();
  Status go_tail();
  Status flip_over();
  Status cancel();
  Status erasebol();
  Status eraseline();
  Status forward_word();
  Status backward_word();
  Status simple_delete();
  Status abort(){ return ABORT; }
  Status swapchars(){ Edlin2::swapchars(); return CONTINUE; }
  Status vz_prev_history();
  Status vz_next_history();
  Status quoted_insert();
  Status keyname_insert();
  Status hotkey();
  Status copy();
  Status cut();
  Status paste();
  Status marking();
  Status paste_test();

  /* option命令用。Edlin から参照されるのみ */
  static int beep_ok;
};

#endif /* EDLIN_H */
