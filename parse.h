#ifndef PARSE_H /* -*- c++ -*- */
#define PARSE_H

#include <stdio.h>
#include <string.h>

class StrBuffer;
class Noclobber;

#include "smartptr.h"
#include "substr.h"
#include "strbuffer.h"

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

  const char *nextcmds; /* ターミネータ文字の次の位置まで */
  const char *tail;	/* ターミネータ文字まで */
  int tailcheck();

  Terminal terminal;
  int argc,limit;

  Substr argbase[30],*args;

protected:
  int err;
  int parseAll();
  int parseRedirect();

public:  
  class RedirectInfo {
    char flag,handle;
    enum{
      APPEND=0x1 , /* >> mark */
      FORCED=0x2 , /*  ! mark */
      PIPE  =0x4 , /* to close with pclose() */
    };
  public:
    FILE *fp;
    Substr path;

    bool isAppend() const { return flag & APPEND ; }
    bool isForced() const { return flag & FORCED ; }

    bool isToFile()   const { return path != 0; }
    bool isToHandle() const { return handle != -1; }
    bool isRedirect() const { return isToFile() || isToHandle(); }

    void setAppend(){ flag |= APPEND; }
    void setForced(){ flag |= FORCED; }
    void setHandle(int n){ handle = n; }
    int  getHandle(){ return handle; }

    void close();
    void reset() { path.reset(); handle = -1 ; this->close(); }

    FILE *openFileToWrite();
    FILE *openFileToRead();
    FILE *openPipe(const char *cmds,const char *mode);
			    
    RedirectInfo() : flag(0),handle(-1),fp(0) { }
    ~RedirectInfo(){ this->close(); }
  } redirect[3] ;
  
  void restoreRedirects(StrBuffer &) throw(Noclobber);

  Parse(const char *source)
    :sp(source),terminal(NOT_TERMINAL),argc(0),limit(30),args(argbase),err(0)
      { parseAll(); }
  ~Parse();
  
  bool isForce(int n) const { return redirect[n].isForced(); }
  bool isErr2Out() const { return redirect[2].isToHandle(); }

  operator const void* () const { return err ? 0 : this; }
  int operator ! () const { return err; }
  const Substr &operator [](int n){ return args[n]; }
  
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
  const char *get_argv(int n){ return n < argc ? args[n].ptr : 0; }
  int   get_length(int n){ return n < argc ? args[n].len : 0; }
  
  int get_length_later(int n){ return n < argc ? sp-args[n].ptr : 0; }
  const char *get_parameter(){ return args[1].ptr; }
  const char *get_source(){ return args[0].ptr; }
  
  SmartPtr copy(int n, SmartPtr dp,int flag=0 ) throw();
  SmartPtr copyall(int n,SmartPtr dp,int flag=QUOTE_COPY);
  SmartPtr betacopy(SmartPtr dp,int n=0);

  void betacopy(StrBuffer &,int n=0 );
  void copy(int n,StrBuffer &,int flag=false ) throw(MallocError);
  void copyall(int n,StrBuffer &,int flag=QUOTE_COPY) throw(MallocError);

  Substr getAfter(int n) const
    { return Substr(args[n].ptr,tail-args[n].ptr); }

  char *copy   (int n, char *dp, int flag=0 )
    { return copy(n,SmartPtr(dp,10000),flag).rawptr(); }
  char *copyall(int n, char *dp, int flag=QUOTE_COPY)
    { return copyall(n,SmartPtr(dp,10000),flag).rawptr(); }
  char *betacopy(char *dp,int n=0)
    { return betacopy(SmartPtr(dp,10000),n).rawptr(); }

  int call_as_main(int (*routine)(int argc,char **argv,FILE *fp,Parse &));

  FILE *open_stdout();
  void close_stdout();

  // int is_append_redirect(int i) const { return appendflag[i]; }
  bool is_append_redirect(int i) const { return redirect[1].isAppend(); }

  static bool isRedirectMark(const char *sp){
    if( sp[0]=='>' || sp[0]=='<' )
      return 1;
    if( (sp[0]=='1' || sp[0]=='2') && sp[1] == '>' )
      return 2;
    return 0;
  }
};

#endif
