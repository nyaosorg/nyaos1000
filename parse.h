#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <string.h>

/* 部分文字列参照用クラス */
class Substr{
 public:
  int len;
  const char *ptr;

  /* 初期化 */
  Substr(void) : ptr(NULL) , len(0) { }
  Substr(const char *p,int l) : ptr(p) , len(l) { }
  void clean()
    { len = 0; ptr = NULL; }

  /* テスト */
  operator const void* () const
    { return ptr; }
  int operator ! () const
    { return ptr == NULL; }

  /* 単純コピー */
  void operator >> (char *dp) const
    { memcpy(dp,ptr,len); dp[len] = '\0'; }

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
  const char *sp;

  const char *nextcmds,*tail;
  int tailcheck();

  int terminal;
  int argc,limit;

  Substr argbase[30],*args;
  Substr redirect[3]; /* 0:stdin  1:stdout  2:stderr */
  FILE *redirect_fp[3];
  bool isappend;
  FILE *output_fp , *input_fp;
  enum{ STD , PIPE , REDIRECT } pipemode;

  int err;
  int check();
  int check_redirect();

public:
  Parse(const char *source)
    : args(argbase) , argc(0) , sp(source) , terminal(-1) , limit(30) ,err(0)
      , output_fp(stdout) , input_fp(stdin) , pipemode(STD)
	,isappend(false)
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

  int get_argc(){ return argc; }
  const char *get_argv(int n){ return n < argc ? args[n].ptr : NULL; }
  int   get_length(int n){ return n < argc ? args[n].len : 0; }

  char *copy   (int n, char *dp, int flag=0 );
  char *copyall(int n, char *dp, int flag=QUOTE_COPY);

  /* 何も置換せずに、そのまま、ベタでコピーする。*/
  char *betacopy(char *dp,int n=0);

  int get_length_later(int n){ return n < argc ? sp-args[n].ptr : 0; }
  const char *get_parameter(){ return args[1].ptr; }
  const char *get_source(){ return args[0].ptr; }

  int call_as_main(int (*routine)(int argc,char **argv));
  int call_as_main(int (*routine)(int argc,char **argv,FILE *fp));

  FILE *open_stdin();
  FILE *open_stdout();
};

class Pipe{
  FILE *fp;
  const char *cmdline;
  const char *mode;
  char *tmpfname;
 public:
  Pipe() : fp(NULL) , mode(NULL) , tmpfname(NULL) { }

  void open(const char *cmdline,const char *mode);
  Pipe(const char *cmdl,const char *mode){ open(cmdl,mode); }
  ~Pipe();

  operator FILE * () { return fp; }
};

#endif
