#ifndef PARSE_H /* -*- c++ -*- */
#define PARSE_H

#include <stdio.h>
#include <string.h>

#include "smartptr.h"

/* Substr : 部分文字列参照用クラス(Pascal型文字列) */
class Substr{
 public:
  int len;
  const char *ptr;

  int operator[](int n) const
    { return ptr[n] & 255; }

  /* 初期化 */
  Substr(void) : len(0), ptr(NULL) { }
  Substr(const char *p,int l) :  len(l) ,ptr(p) { }
  void clean(){ len = 0; ptr = NULL; }

  /* テスト */
  operator const void* () const
    { return ptr; }
  int operator ! () const
    { return ptr == NULL; }

  /* 単純コピー */
  void operator >> (char *dp) const
    { memcpy(dp,ptr,len); dp[len] = '\0'; }
  
  void operator >> (SmartPtr dp) const;

  char *dup() const;

  /* 単一文字列の空白分離による切り出し 
   *   const char *sp = ソース文字列 ;
   *   Substr a,b,c,d;
   *
   *   const char *tail = (sp >> a >> b >> c >> d);
   * なんてことが可能。だが、「&」とか「|」には対応していないので、
   * Parse では使用していない。おいおい。
   */
  friend const char *operator >> (const char *sp,Substr &);

  /* 引用コピー */
  char *quote(char *dp) const;
};

/* 字句解析クラス */
class Parse{
public:
  enum Terminal{
    NOT_TERMINAL,
    NULL_TERMINAL,	/* \0 */
    SEMI_TERMINAL,	/* ;  */
    AMP_TERMINAL,	/* &  */
    AND_TERMINAL,	/* && */
    OR_TERMINAL,	/* || */
    PIPE_TERMINAL,	/* |  */
    PIPEALL_TERMINAL,	/* |& */
  };
  static int option_semicolon_terminate;
  static int is_terminal_char(int c)
    { return c=='\0' || c=='&' || c=='|';  }
private:
  const char *sp;

  const char *nextcmds,*tail;
  int tailcheck();

  Terminal terminal;
  int argc,limit;

  Substr argbase[30],*args;
  Substr redirect[3]; /* 0:stdin  1:stdout  2:stderr */

protected:
  int appendflag[3];
  FILE *output_fp , *input_fp;
  enum{ STD , PIPE , REDIRECT } pipemode;

  int err;
  int check();
  int check_redirect();

public:
  Parse(const char *source)
    :  sp(source)  , terminal(NOT_TERMINAL), argc(0), limit(30), args(argbase)
      , output_fp(stdout) , input_fp(stdin) , pipemode(STD) ,err(0)
	{ check(); }

  ~Parse();
  
  operator const void* () const { return err ? NULL : this; }
  int operator ! () const { return err; }
  const Substr &operator [](int n){ return args[n]; }
  const Substr *get_redirect(){ return redirect; }
  
  enum{
    QUOTE_NOT_COPY = 0,
    QUOTE_COPY     = 1,
    SLASH_REPLACE  = 2,
    REPLACE_SLASH  = 2,
  };

  const char *get_tail(){ return tail; }
  const char *get_nextcmds(){ return nextcmds; }
  Terminal get_terminal(){ return terminal; }
  
  int get_argc(){ return argc; }
  const char *get_argv(int n){ return n < argc ? args[n].ptr : NULL; }
  int   get_length(int n){ return n < argc ? args[n].len : 0; }
  
  int get_length_later(int n){ return n < argc ? sp-args[n].ptr : 0; }
  const char *get_parameter(){ return args[1].ptr; }
  const char *get_source(){ return args[0].ptr; }
  
  SmartPtr copy(int n, SmartPtr dp,int flag=0 ) throw();
  SmartPtr copyall(int n,SmartPtr dp,int flag=QUOTE_COPY);
  SmartPtr betacopy(SmartPtr dp,int n=0);

  char *copy   (int n, char *dp, int flag=0 )
    { return copy(n,SmartPtr(dp,10000),flag).rawptr(); }
  char *copyall(int n, char *dp, int flag=QUOTE_COPY)
    { return copyall(n,SmartPtr(dp,10000),flag).rawptr(); }
  char *betacopy(char *dp,int n=0)
    { return betacopy(SmartPtr(dp,10000),n).rawptr(); }

  int call_as_main(int (*routine)(int argc,char **argv));
  int call_as_main(int (*routine)(int argc,char **argv,FILE *fp));

  FILE *open_stdin();
  FILE *open_stdout();

  int is_append_redirect(int i) const { return appendflag[i]; }
};

#endif
