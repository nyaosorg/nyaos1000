#include <stdio.h>

int main(int argc,char **argv)
{
  for(int i=1;i<argc;i++){
    FILE *fp=fopen(argv[i],"r");
    if( fp==NULL ){
      perror(argv[i]);
      return i;
    }
    for(;;){
      int ch=getc(fp);
      if( feof(fp) )
	break;
      putchar( ch );
      if( ch & 128 ){
	if( (ch=getc(fp))=='\\' ){
	  putchar(ch);
	  putchar('\\');
	}else if( ch == EOF ){
	  break;
	}else{
	  putchar(ch);
	}
      }
    }
    fclose(fp);
  }
  return 0;
}
