#ifndef EDLIN_H
#define EDLIN_H

#undef  numof
#define numof(A)  (sizeof((A))/sizeof((A)[0]))

#include <stdio.h>

/* 最も基本的な行入力クラス */
class Edlin{
protected:
  char *strbuf;    /* ASCII CODE */
  char *atrbuf;    /* KANJI FLAG */

  int top;         /* 表示している一文字目の桁位置     */
  int pos;         /*   カーソルのある文字の桁位置     */
  int len;         /* 全体の byte数                    */
  int max;         /* strbufのmax                      */
  int windowsize;  /* 表示領域のサイズ(スクロール機能) */

  void after_repaint(int termclear=-1);   /* カーソル位置移行を repaint */
  void repaint(int termclear=-1);         /* 全行 repaint               */
  void right(int n=1);                    /* 右へスクロール             */
  void left(int n=1);                     /* 左へスクロール             */
  void complete_core(int fntop,int basesize);

/* デフォルトでは 次のエスケープシーケンス を使う。
 *      "\b"            : バックスペース
 *      "\x1B[K"   (ce) : カーソル位置から行末尾までをクリア
 */

  virtual void putchr(int c)=0; /* 一文字出力               */
  virtual void putel()=0;       /* カーソル位置以降をクリア */
  virtual void putbs(int i)=0;  /* カーソルをｎ桁戻す       */
public:
  enum{ SBC , DBC1ST , DBC2ND };
  struct History{
    History *prev,*next;
    char buffer[1];
  };
private:
  History *history;
protected:
  /**** 継承用コンストラクタ ****/
  Edlin(int top_ , int pos_ , int len_,
	char *buffer, int max_, int windowsize_)
    : strbuf(buffer),atrbuf(new char[max_]),history(0)
      ,top(0),pos(0),len(0),max(max_),windowsize(windowsize_)
        { /* no-operation */ }

public:
  Edlin(char *buffer , int max_ , int windowsize_)
    : strbuf(buffer),atrbuf(new char[max_]),history(0)
      ,top(0),pos(0),len(0),max(max_),windowsize(windowsize_)
        { buffer[0]='\0'; }

  ~Edlin();

  void insert(int ch);                     /*    半角文字挿入       */
  void insert(int ch1,int ch2);            /*    全角文字挿入       */
  void insert_and_forward(const char *s);  /*    文字列挿入         */
  void erase();                            /* ^D 一文字削除         */
  void forward();                          /* ^F カーソル右移動     */
  void backward();                         /* ^B カーソル左移動     */
  void forward_word();                     /* @F カーソル右単語移動 */
  void backward_word();                    /* @B カーソル左単語移動 */
  void go_ahead();                         /* ^A 先頭へ             */
  void go_tail();                          /* ^E 末尾へ             */
  void clean_up();                         /* ^U 入力破棄           */
  void eraseline();                        /* ^K カーソル以降を消す */
  virtual void complete();                 /* ^I ファイル名補完     */
  virtual void complete_list(){};          /* ^D ファイル名リスト   */
  void push();                             /*    Historyにpush      */
  History *pop(){ return history; }        /* ^P Historyを呼び出す  */
  virtual void cls(){};                    /* ^L 画面クリア(何もしない) */

  /* カーソルを適切な位置に移動して入力待ち */
  virtual int getkey(int wait=1);
 
  /* リポート関数 */
  int length() const { return len; }    /* 現在入力されている文字列のbytes */
  int position() const { return pos; }  /* カーソルの位置(bytes) */
  int gettype(int nth) const { return atrbuf[nth]; }

  int simple_line_input();

  static int ctrl_d_eof;
  static int complete_tail_char;
};

/* エスケープシーケンスを使った入力クラス */
class EscEdlin : public Edlin {
  const char *cursor_on;
  const char *cursor_off;
 protected:
  FILE *fp;
  void putchr(int c);
  void putel();
  void putbs(int i);
 public:
  EscEdlin(char *buffer, int max, int windowsize=32767, FILE *Fp=stdout )
    : Edlin(buffer,max,windowsize),fp(Fp),cursor_on(NULL),cursor_off(NULL)
      { /* no-operation */ }
  int getkey(int wait=1);
  void setcursor(const char *on,const char *off="\x1B[0m")
    { cursor_on = on ; cursor_off = off; }
  
};

class ShellEdlin : public EscEdlin {
  const char *prompt;
  int promptlen;
 public:
  int setprompt(const char *prompt);

  ShellEdlin(const char *pro,char *buffer,int max,
	     int windowsize=32767,FILE *fp=stdout)
    : EscEdlin(buffer,max,windowsize,fp)
      { }

  /* 帰り値 : 文字数 , キャンセル時(-1) 
   * windowsizeはpromptの長さで調整される */

  void complete_list();
  int simple_input(const char *prompt,int windowsize=32767);
  void cls();
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
