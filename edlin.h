/* -*- c++ -*- */
#ifndef EDLIN_H
#define EDLIN_H

#undef  numof
#define numof(A)  (sizeof((A))/sizeof((A)[0]))

#include <stdio.h>
#include "macros.h"

class Complete;

/* 最も基本的な行入力クラス。純粋仮想クラスなので、そのままでは使えない。
 * 実際の入出力部分は純粋仮想関数として切り放しているので環境非依存
 * (edlin.cc)
 *
 * のはずだったが、その理念は破綻している(笑)
 */
class Edlin{
protected:
  char *strbuf;    /* ASCII CODE */
  char *atrbuf;    /* KANJI FLAG */

  int top;         /* 表示している一文字目の桁位置     */
  int pos;         /*   カーソルのある文字の桁位置     */
  int len;         /* 全体の byte数                    */
  int max;         /* strbufのmax                      */
  int windowsize;  /* 表示領域のサイズ(スクロール機能) */
  
  int msgsize;     /* 入力文字列以外のメッセージが表示されている場合、
		    * その文字列の長さが入る。*/
  int bottom_msgsize;
  
  int makeRoom(int at,int bytes);
  void writeSBChar(int c){
    putchr(strbuf[pos]=c); atrbuf[pos++] = SBC;
  }
  void writeDBChar(int c1,int c2){
    putchr(strbuf[pos]=c1); atrbuf[pos++] = DBC1ST;
    putchr(strbuf[pos]=c2); atrbuf[pos++] = DBC2ND;
  }

public:
  void after_repaint(int termclear=-1);   /* カーソル位置移行を repaint */
  void repaint(int termclear=-1);         /* 全行 repaint               */
  void _repaint(int termclear);           /* カーソルを戻さない repaint */
  
#if 0
  void right(int n=1);                    /* 右へスクロール             */
  void left(int n=1);                     /* 左へスクロール             */
#endif
  int  seek_word_top();

  virtual void putchr(int c)=0; /* 一文字出力               */
  virtual void putel()=0;       /* カーソル位置以降をクリア */
  virtual void putbs(int i)=0;  /* カーソルをｎ桁戻す       */
  virtual void alert()=0;       /* 警告(普通はbeep音)       */
  
  /**** 継承用コンストラクタ ****/
  Edlin(int top_ , int pos_ , int len_,
	char *buffer, int max_, int windowsize_)
    : strbuf(buffer),atrbuf(new char[max_])
      ,top(top_),pos(pos_),len(len_),max(max_),windowsize(windowsize_)
	,msgsize(0) , bottom_msgsize(0)
	  { /* no-operation */ }
  
public:
  Edlin(char *buffer , int max_ , int windowsize_)
    : strbuf(buffer),atrbuf(new char[max_])
      ,top(0),pos(0),len(0),max(max_),windowsize(windowsize_)
	,msgsize(0)
	  { buffer[0]='\0'; }
  
  virtual ~Edlin(){ delete atrbuf; }

  enum{ SBC , DBC1ST , DBC2ND };
  
  void init(){ top=pos=len=0; strbuf[0]='\0'; atrbuf[0]=SBC; putel(); }
  void insert(int ch);                     /*    半角文字挿入       */
  void insert(int ch1,int ch2);            /*    全角文字挿入       */
  void insert_and_forward(const char *s);  /*    文字列挿入         */
  void quoted_insert(int ch);              /*    制御文字挿入       */
  void pack(); /* 入力した制御文字を1byte形式へ置換する。 */

  void erase();               /* ^D 一文字削除         */
  int  forward();             /* ^F カーソル右移動     */
  int  backward();            /* ^B カーソル左移動     */
  void forward_word();        /* @F カーソル右単語移動 */
  void backward_word();       /* @B カーソル左単語移動 */
  void go_ahead();            /* ^A 先頭へ             */
  void go_tail();             /* ^E 末尾へ             */
  void clean_up();            /* ^U 入力破棄           */
  void erasebol();		  /*	カーソル手前を消す */
  void eraseline();           /* ^K カーソル以降を消す */
  void swapchars();           /* ^T カーソル手前二文字を入れ換える */
  virtual void cls(){};       /* ^L 画面クリア(何もしない) */

  /* これらは、導出クラスへ移項すべきもの */
  virtual int complete();	/* TCSH型の補完 */
  virtual int completeFirst();	/* 変換型の補完 */
  virtual int complete_to_fullpath(const char *header=0);
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

  // -------- リポート関数 --------
  int length() const { return len; }    /* 現在入力されている文字列のbytes */
  int position() const { return pos; }  /* カーソルの位置(bytes) */
  
  int operator[](int nth) const { return strbuf[nth]; }
  int gettype(int nth)    const { return atrbuf[nth]; }

  const char *getbuffer() const { return strbuf; }

  static int complete_tail_char;
  
  int simple_line_input();

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

/* ANSI エスケープシーケンス/かんな 版 Edlin */

class jrKanjiStatus;

class Edlin2 : public Edlin {
  static int canna_inited;       /* 初期化されていたら not 0 */
protected:
  FILE *fp;
  const char *cursor_on;
  const char *cursor_off;
  
  void putchr(int c);
  void putel();
  void putbs(int i);
  void alert(){ putchr('\a'); }
  int getkey_with_cursor();
 public:
  Edlin2(char *buffer, int max, int windowsize=32767, FILE *Fp=stdout )
    : Edlin(buffer,max,windowsize),fp(Fp),cursor_on(""),cursor_off("")
      { /* no-operation */ }
  int getkey();
  
  void setcursor(char *on,char *off="\x1B[0m")
    { cursor_on = on ; cursor_off = off; }

  static int option_canna;
  static void canna_to_alnum();  /* 強制的に英数モードへ  */
private:
  int print_henkan_koho( jrKanjiStatus &status , const char *mode_string );
};

extern char dbcstable[256];
int dbcs_table_init();

/* シェルに特化した Edlin クラス (shell.cc) */
class ShellEdlin : public Edlin2 {
  const char *prompt;
  int promptlen;
public:
  static int beep_ok;
  int using_i_mark;
protected:
  void alert(){ if( beep_ok ) putchr('\a'); }
public:
  int setprompt(const char *prompt,int windowsize=32767);

  ShellEdlin(const char *pro,char *buffer,int max,
	     int windowsize=32767,FILE *fp=stdout)
    : Edlin2(buffer,max,windowsize,fp),prompt(pro),using_i_mark(0)
      { }

  /* 帰り値 : 文字数 , キャンセル時(-1) 
   * windowsizeはpromptの長さで調整される */

  void complete_list();
  int complete_hook(Complete &com);
  void cls();
};

/* 実際にキ－入力などをうけて、ShellEdlinのメソッドを呼び出すクラス
 * ShellEdin と統合すべきかもしれない。(bindkey.cc)
 */
struct WHist;

class Shell{
public:
  enum Status {
    CONTINUE,	// 編集続行
    TERMINATE,	// ^M ^J
    QUIT  = -1,	// ^D
    ABORT = -2,	// ^C
    FATAL = -3, // 未知のトラブル
  };
private:
  ShellEdlin &ed;

  int ch;
  int changed;
  int prevchar;
  int prev_complete_num;
  int overwrite;

  enum{ NUMOF_BINDMAP = 0x200 };
  static void bindkey_base();
  static Status (Shell::*bindmap[ NUMOF_BINDMAP ])();
  static const char *bindmap_usage_key[ NUMOF_BINDMAP ];
  static char *bindmap_usage_func[ NUMOF_BINDMAP ];
  
  Status search_engine(int isrev);
public:
  static int ctrl_d_eof;
  static int ctrl_z_eof;
  // static int keyNameToCode( const char *name );
  static void bindkey_wordstar();
  static void bindkey_tcshlike();
  static void bindkey_nyaos();
  static int bindkey(const char *key,const char *funcname);
  static int bind_hotkey(const char *key,const char *program);
  static void bindlist(FILE *fp);

  Shell(ShellEdlin &e) ;
  ~Shell();
  int line_input(const char *prompt,int window=32767);

  Status i_search();
  Status rev_i_search();

  Status self_insert();
  Status previous_history();
  Status next_history();
  Status bye();
  Status forward();
  Status backward();
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
  Status swapchars(){ ed.swapchars(); return CONTINUE; }
  Status vz_prev_history();
  Status vz_next_history();
  Status quoted_insert();
  Status hotkey();
private:
  int vz_history_core(struct WHist *);

  /** ヒストリ関係 **/
public:
  struct History{
    History *prev,*next;
    char buffer[1];
  };
private:
  static History *history;
  static int nhistories;
  History *cur;
public:
  static int get_history_number() { return nhistories; }
  static const char *get_nth_history(int n);
  
  // 最新のヒストリ内容を引数の内容と置きかえる。
  static int replace_last_history(const char *s);
  // ヒストリに文字列を加える。
  static int append_history(const char *s);

  int isOverWrite(){ return overwrite; }
};

/* TERMCAP & エスケープシーケンス メモ
 * co#80:li#25          端末の大きさ
 * am                   右端に移動すると自動的に次の行へ移動する。
 * km                   メタキーあり(どこに?)
 * bs                   BS(^H)でカーソル後退可能
 * ho=\E[H              ホーム位置移動
 * bl=^G                ベル
 * cl=\E[2J             画面クリア
 * ce=\E[K              カーソル位置から行末尾までをクリア
 * cm=\E[%i%2;%2H       行列指定のカーソル移動 (%i...「1」から数値を始める)
 * up=\E[A              カーソル上移動
 * xd=\E[B              カーソル下移動
 * nd=\E[C              カーソル右移動
 * bc=\E[D              カーソル左移動
 * ti=\E[0;37;44m       カーソル移動の許可       
 * te=\E[0;37;40m       カーソル移動の禁止
 * so=\E[1;37;46m       強調開始(普通は反転)
 * se=\E[0;1;37;44m     強調終了
 * us=\E[1;33;44m       アンダーライン開始
 * ue=\E[0;1;37;44m     アンダーライン終了
 * md=\E[1;31;44m       ボールド開始
 * me=\E[0;1;37;44m     ボールド終了
 */

#endif /* EDLIN_H */
