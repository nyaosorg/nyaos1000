/* -*- c++ -*- */
#ifndef NYAOS_H
#define NYAOS_H

#include "macros.h"
#define VERSION "1.65"

class Parse;

/* 内蔵コマンドの戻り値のうち、特別なもの */
enum{
  RC_QUIT = -32768,  /* exit コマンドなど */
  RC_HOOK = -32767,  /* 内蔵コマンドは別のコマンドへのフィルター */
  RC_ABORT= -32766,  /* Ctrl-C が押された */
};

extern struct Command{
  const char *name;
  int (*func)( FILE *srcfil, Parse &params );
} jumptable[];

extern struct Alias{
  Alias *next;
  char *base;
  char name[1];
} *alias_hashtable[256];

extern int screen_width , screen_height ;
class Noclobber{}; // error object
int execute(FILE *srcfil,const char *cmdline,int use_spawn=0 )
     throw(Noclobber) ;
char *replace_alias( const char *src ) throw(Noclobber);

class StrBuffer;
class MallocError;

char *replace_history( const char *src );
char *preprocess( const char *src );
char *replace_script( const char *src ) throw(MallocError);
char *fgets_chop(char *dp,int max,FILE *fp);

extern int cursor_start , cursor_end;
extern char *cursor_on_color_str;
extern char *cursor_off_color_str;

extern volatile int ctrl_c;
void ctrl_c_signal(int sig);

char *strcpy_tail(char *dp,const char *sp);

extern int scriptflag,option_sos;
extern int option_tilda_is_home;
extern int option_replace_slash_to_backslash_after_tilda;
extern int option_vio_cursor_control;
extern int option_prompt_even_piped;
extern int option_cmdlike_crlf;

void buildin_command_to_complete_table(void);
extern char *cmdexe_path;

/* NYAOS.CC */
void truepath( char *dst , const char *src , int size );
char *getcwd_case(char *dst);
void get_scrsize(int *wh,FILE *f=0);

const char *getShellEnv(const char *varname);
void setShellEnv(const char *varname,const char *value);

int changeDir(const char *s);
void resetCWD();

#endif
