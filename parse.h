#ifndef PARAMS_H
#define PARAMS_H

#include <stdio.h>
#include <string.h>

class Parse{
  const char *sp;

  int terminal;
  int argc;

  struct Array{
    const char *pointor;
    int length;
  } argbase[30] , *args;

  int limit;

  const char *output_redirect , *input_redirect;
  int output_redirect_length , input_redirect_length;
  bool isappend;
  FILE *output_fp , *input_fp;
  enum{ STD , PIPE , REDIRECT } pipemode;

  int err;
  int check();
  int check_redirect();

public:
  Parse(const char *source)
    : args(argbase) , argc(0) , sp(source) , terminal(-1) , limit(30)
      ,output_redirect(NULL) , output_redirect_length(0) , err(0)
	,input_redirect(NULL) , input_redirect_length(0)
	  , output_fp(stdout) , input_fp(stdin) , pipemode(STD)
	    ,isappend(false)
      { check(); }

  ~Parse();

  const char *get_tail(){ return sp; }
  int get_argc(){ return argc; }
  const char *get_argv(int n){ return n < argc ? args[n].pointor : NULL; }
  int   get_length(int n){ return n < argc ? args[n].length : 0; }

  char *copy   (int n, char *dp, bool quote_copy_flag=false );
  char *copyall(int n, char *dp, bool quote_copy_flag=true  );

  int get_length_later(int n){ return n < argc ? sp-args[n].pointor : 0; }
  const char *get_parameter(){ return args[1].pointor; }
  const char *get_source(){ return args[0].pointor; }

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
