/* -*- c++ -*- */
#ifndef NYAOS_H
#define NYAOS_H

#include "macros.h"

#define VERSION "1.39"

/**** "callcmd.cc" ****/

enum{
  RC_QUIT = -32768,  /* exit コマンドなど */
  RC_HOOK = -32767,  /* 内蔵コマンドは別のコマンドへのフィルター */
  RC_ABORT= -32766,  /* Ctrl-C が押された */
};

int query_filesystem(int drivenum);
extern int screen_width , screen_height ;
int execute(FILE *srcfil, const char *cmdline, int use_spawn=0 );
int eadir(int argc, char **argv,FILE *fout);
char *fgets_chop(char *dp,int max,FILE *fp);
class ShellEdlin;
void setprompt(const char *promptenv,char *dp,ShellEdlin *edlin=NULL);
class Parse;

extern struct Command{
  const char *name;
  int (*func)( FILE *srcfil, Parse &params );
} jumptable[];

extern struct Alias{
  Alias *next;
  char *base;
  char name[1];
} *alias_hashtable[256];

extern int cursor_start , cursor_end;
extern char *cursor_on_color_str;
extern char *cursor_off_color_str;

extern volatile int ctrl_c;
void ctrl_c_signal(int sig);

char *strcpy_tail(char *dp,const char *sp);
extern int scriptflag,option_sos;

extern int option_tilda_is_home;
extern int option_replace_slash_to_backslash_after_tilda;
void replace_alias( const char *source, char *destinate ,int max );
void replace_envvar( const char *source , char *destinate ,int max );
int replace_script( const char *source , char *destinate , int max );
void buildin_command_to_complete_table(void);

extern int option_vio_cursor_control;
extern int option_prompt_even_piped;
extern int option_cmdlike_crlf;
extern char *cmdexe_path;

/* NYAOS.CC */
void truepath( char *dst , const char *src , int size );
char *getcwd_case(char *dst);
void get_scrsize(int *wh,FILE *f=0);

#endif
