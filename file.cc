#include <stdio.h>

#define INCL_DOSSESMGR
#include <os2.h>

int main(int argc,char **argv)
{
  for(int i=1;i<argc;i++){
    ULONG proctype;
    (void)DosQueryAppType( (PUCHAR)argv[i] , &proctype );
    printf("%s : proctype = %4x\n" , argv[i] , proctype );
  }
  return 0;
}
