#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "errmsg.h"

#define numof(A) (sizeof(A)/sizeof((A)[0]))

static char *errorMessages[]={
  "Cannot find the specified drive.",
  "Too near terminate charactor.",
  "Nyaos's internal error.",
  "Can't remove file(s).",
  "Can't make directory(s).",

  "Can't write .SUBJECT extend attribute.",
  "Can't write .COMMENTS extend attribute.",
  "Invalid key name.",
  "Invalid keys-set name.",
  "Invalid function name.",

  "Too long variable name.",
  "Can't input-redirect on built-in commands.",
  "Invalid variable name.",
  "Can't open output-redirect/pipe.",
  "Too few arguments.",

  "Too many nesting.",
  "No such file.",
  "No such alias.",
  "Bad %HOME% directory.",
  "No such directory.",

  "Unknown Option.",
  "Dir Stack Empty.",
  "No such file or directory.",
  "Can't get current directory.",
  "Too large stack number.",

  "Missing.",
  "No such environment variable.",
  "Error occurs in foreach loop.",
  "Not available in REXX Script",
  "Memory allocation error.",

  "Current version of NYAOS does not support DOS/VDM.",
  "Can't get DBCS table from operating system.",
  "Not found CMD.EXE",
  "Bad parameter",
  "Ambiguous Output Redirect",

  "Event Not Found",
  "Too Many Errors.",
  "File exists.",
#ifdef VMAX
  "Bad Command or Filename.",
#endif
};

static char *mountedErrorMessages[ numof( errorMessages ) ];

static bool inited=false;

void ErrMsg::mount(int x,const char *s)
{
  if( inited==false ){
    memset( mountedErrorMessages , 0 , sizeof(mountedErrorMessages) );
    inited = true;
  }
  if( x >= (int)numof(errorMessages) )
    return;

  if( mountedErrorMessages[x] != NULL )
    free(mountedErrorMessages[x]);
  
  mountedErrorMessages[x] = strdup(s);
}

void ErrMsg::say(int x,...)
{
  if( inited==false ){
    memset( mountedErrorMessages , 0 , sizeof(mountedErrorMessages) );
    inited = true;
  }
  va_list vp;
  va_start(vp,x);
  
  const char *s;
  while( (s=va_arg(vp,const char*)) != NULL )
    fprintf(stderr,"%s: ",s);

  fprintf(stderr,"%s\n"
	  , mountedErrorMessages[x] != NULL
	  ? mountedErrorMessages[x]
	  : errorMessages[x]  );
  va_end(vp);
}

extern char *fgets_chop(char *,int,FILE*);

int source_errmsg(const char *fname)
{
  FILE *fp=fopen(fname,"rt");
  if( fp==NULL )
    return -1;
  
  char buffer[256];
  int n=0;
  while( !feof(fp)  &&  fgets_chop(buffer,sizeof(buffer),fp) != NULL ){
    if( buffer[0] != '#' && buffer[0] != '\0' )
      ErrMsg::mount(n++,buffer);
  }
  fclose(fp);
  return n;
}
