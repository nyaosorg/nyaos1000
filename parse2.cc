#include "parse.h"

class Parse2 : public Parse{
  int org_fd[3];
public:
  Parse2(const char *s) : Parse(s)
    { org_fd[0] = org_fd[1] = org_fd[2] = -1; }

  FILE *open_stdin();
  FILE *open_stdout();

  Parse2();
};

FILE *Parse2::open_stdout()
{
  if( redirect[1] != NULL ){
    char *name=(char*)alloca( redirect[1].len+1 );
    redirect.quote(name);
    
    int fd;
    if( appendflag[1] )
      fd = open( name , O_WRONLY | O_APPEND
		

  }
  return stdout;
}



