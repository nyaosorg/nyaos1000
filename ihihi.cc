#include <stdio.h>

int main(void)
{
  char buffer[1025];

  FILE *cmdexe=popen("cmd.exe","w");

  while( !feof(stdin)  &&  fgets(buffer,sizeof(buffer),stdin)!= NULL ){
    fputs(buffer,cmdexe);
    fflush(cmdexe);
  }
  pclose(cmdexe);
}


