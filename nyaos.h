/* -*- c++ -*- */
#ifndef NYAOS_H
#define NYAOS_H

#include "macros.h"

/**** "callcmd.cc" ****/

enum{
  RC_HOOK = -32767,
  RC_QUIT = -32768,
};

extern int screen_width , screen_height ;
int execute(FILE *srcfil, const char *cmdline, int use_spawn=0 );
int eadir(int argc, char **argv,FILE *fout);
char *fgets_chop(char *dp,int max,FILE *fp);
class ShellEdlin;
void setprompt(const char *promptenv,char *dp,ShellEdlin &edlin);
class Parse;

extern const struct commandtable_tag {
  const char *name;
  int (*func)( FILE *srcfil, Parse &params );
} jumptable[];

extern struct Alias{
  Alias *next;
  char *base;
  char name[1];
} *alias_hashtable[256];

extern int alias_nesting;
int do_alias(FILE *fin, const char *sp,
	     const char *parameter,int argc,char **argv );
int cmd_unalias(FILE *fin, const char *parameter,int argc,char **argv);
int cmd_alias(FILE *fin, const char *sp,int argc,char **argv);

extern int cursor_start , cursor_end;
extern char *cursor_on_color_str;
extern char *cursor_off_color_str;

extern volatile int ctrl_c;
void ctrl_c_signal(int sig);

struct Argument{
  struct Argument *next;
  int length;
  char buffer[1];
};

struct Command{
  struct Command *nextcmd;
  int key,argc,allsize;
  int terminator; /*  |  \0 , or  &  */
  struct Argument arg0;
};

void kill_filter(struct Command *list);
struct Command *make_filter( const char *sp );
struct Command *alias_filter(struct Command *dummyfirst);
char *strcpy_tail(char *dp,const char *sp);
extern int scriptflag,option_sos;

void alias_replace(const char *sp,char *dp);
int replace_script( const char *source , char *destinate );

extern int option_tilda_is_home;
extern int option_replace_slash_to_backslash_after_tilda;
int replace_envvar( const char *source , char *destinate );
void buildin_command_to_complete_table(void);

extern int option_vio_cursor_control;


/**** bindkey.cc *****/

#endif
