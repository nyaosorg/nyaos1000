#include <stdio.h>
#include <stdlib.h>
#include <sys/ea.h>
#include <sys/nls.h>
#include <sys/video.h>

#include "edlin.h"
#include "parse.h"
#include "complete.h"
#include "nyaos.h"
#include "edlin.h"

static int option_dir_tail_is_forward_slash;

extern int option_amp_start;
extern int option_tilda_is_home;
extern int option_tcshlike_history;

int option_cd_goto_home=0;
int echoflag=0;

int cmd_mode( FILE *source , Parse &params )
{
  char buffer[ 1024 ];
  params.copyall(0,buffer);
  system( buffer );
  if( option_vio_cursor_control ){
    v_getctype( &cursor_start , &cursor_end );
  }
  return 0;
}

int cmd_pwd( FILE *source , Parse &params )
{
  char cwd[FILENAME_MAX];

  _getcwd2(cwd,sizeof(cwd));

  FILE *fout=params.open_stdout();
  fputs(cwd,fout);
  putc('\n',fout);
  return 0;
}

static void cut_tail_root(char *p)
{
  char *q=NULL;
  while( *p != '\0' ){
    q=p;
    if( is_kanji(*p) )
      p++;
    p++;
  }
  if( q != NULL && (*q=='\\' || *q=='/') )
    *q = '\0';
}

int cmd_rmdir( FILE *source, Parse &params )
{
  int argc=params.get_argc();
  for(int i=1;i<argc;i++){
    char dirname[FILENAME_MAX];
    params.copy(i,dirname);
    cut_tail_root(dirname);
    if( rmdir(dirname) != 0 ){
      fprintf(stderr,"nyaos: cannot remove directory `%s'\n",dirname);
    }
  }
}

int cmd_mkdir( FILE *source , Parse &params)
{
  int argc=params.get_argc();
  for(int i=1;i<argc;i++){
    char dirname[FILENAME_MAX];
    params.copy(i,dirname);
    cut_tail_root(dirname);
    if( mkdir(dirname,0777) != 0 ){
      fprintf(stderr,"nyaos: cannot make directory `%s'\n",dirname);
    }
  }
}

int chdir_with_cdpath(const char *cwd)
{
  if( _chdir2( cwd ) != 0  &&  cwd[0] != '\0'  &&  cwd[1] !=':' ){
    /* CDPATH */
    char cdpath[FILENAME_MAX];
    _searchenv(cwd,"CDPATH",cdpath);
    if( cdpath[0] == '\0'  ||  _chdir2(cdpath) )
      fprintf(stderr,"%s : no such directory.\n",cwd);
  }
}

int cmd_chdir( FILE *srcfil, Parse &params)
{
  char cwd[FILENAME_MAX];

  if( params.get_argc() > 1 ){
    params.copy(1,cwd);
    chdir_with_cdpath(cwd);
  }else if( option_cd_goto_home ){
    const char *home=getenv("HOME");
    if( home == NULL || _chdir2(home) != 0 )
      fprintf(stderr,"chdir: $HOME does not point a right directory.\n");
  }else{
    return cmd_pwd(srcfil,params);
  }
  return 0;
}

int cmd_comment(FILE *source, Parse &params)
{
  if( params.get_argc() < 2 ){
    fprintf(stderr,"comment filename comment...\n");
    return 0;
  }

  char *fname=(char*)alloca(params.get_length(1)+1);
  params.copy(1,fname,Parse::REPLACE_SLASH);

  struct _ea eavalue;

  eavalue.flags = 0;
  if( params.get_argc() == 2 ){
    eavalue.size = 0;
    eavalue.value = "";
  }else{
    int size=params.get_length_later(2);

    union{
      void           *value;
      unsigned short *word;
      char  *byte;
    } ptr;

    ptr.value = eavalue.value = alloca( (eavalue.size = size+10)+1 );

    *ptr.word++ = 0xFFDF;
    *ptr.word++ = 932;
    *ptr.word++ = 1;
    *ptr.word++ = 0xFFFD;
    *ptr.word++ = (unsigned short)
      ( params.copyall( 2 , ptr.byte+2 , Parse::QUOTE_NOT_COPY ) - (ptr.byte+2) );
    printf("%s --> %s\n",fname,ptr.byte);
  }
  
  int rc=_ea_put( &eavalue , fname , 0 , ".COMMENTS" );
  if( rc != 0 ){
    fprintf(stderr,"comment: cannot write comment on the file:%s\n" , fname );
  }
  return rc;
}

struct{
  const char *name;
  int *pointor;
  int true_value;
  int false_value;
} optlist[]={
  { "amp_start"            , &option_amp_start                 , 1  , 0 },
  { "anywhere_history"     , &option_tcshlike_history          , 1  , 0 },
  { "beep"                 , &ShellEdlin::beep_ok              , 1  , 0 },
  { "echo"                 , &echoflag                         , 1  , 0 },
  { "complete_hidden"      , &Complete::complete_hidden_file   , 1  , 0 },
  { "complete_tail_slash"  , &Edlin::complete_tail_char        ,'/','\\'}, 
  { "complete_tilda"       , &Complete::complete_tail_tilda    , 1  , 0 },
  { "ctrl_d_eof"           , &Shell::ctrl_d_eof                , 1  , 0 },
  { "ctrl_z_eof"           , &Shell::ctrl_z_eof                , 1  , 0 },
  { "cd_goto_home"         , &option_cd_goto_home              , 1  , 0 },
  { "ls_tail_slash"        , &Complete::directory_split_char   ,'/','\\'},
  { "script"               , &scriptflag                       , 1  , 0 },
  { "sos"                  , &option_sos                       , 1  , 0 },
  { "tilda_home"           , &option_tilda_is_home             , 1  , 0 },
  { "slash_to_backslash_after_tilda"
      , &option_replace_slash_to_backslash_after_tilda , 1 , 0 },
  { "vio"                  , &option_vio_cursor_control        , 1  , 0 },
};

int cmd_option(FILE *source, Parse &params)
{
  FILE *fout=params.open_stdout();

  if( params.get_argc() < 2 ){
    for(int i=0;i<numof(optlist);i++){
      fprintf(fout,"%c%s\n",
	      (*optlist[i].pointor == optlist[i].true_value) ? '+' : '-' ,
	      optlist[i].name
	      );
    }
    return 0;
  }
  
  for(int j=1;j<params.get_argc() ; j++ ){
    char *name=(char*)alloca( params.get_length(j)+1 );
    params.copy(j,name);
    bool value=true;

    if( name[0] == '-' ){
      value = false;
      name++;
    }else if( name[0] =='+' ){
      name++;
    }
    
    for(int i=0; i<numof(optlist); i++){
      if( strcmp(optlist[i].name,name)==0 ){
	*optlist[i].pointor = 
	  ( value ? optlist[i].true_value : optlist[i].false_value );
	fprintf(fout,"%c%s\n",
	       value ? '+' : '-' ,
	       optlist[i].name 
	       );
	break;
      }
    }
  }
  return 0;
}
