#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>

#include "edlin.h"
#include "complete.h"

void ShellEdlin::complete_list()
{
  Complete com;
  int basesize=0;
  
  for( int fntop=pos-1 ; ; fntop--,basesize++ ){
    if(   fntop == -1        || isspace(strbuf[fntop])
       || strbuf[fntop]=='"' || strbuf[fntop]=='\''    ){
      
      char *buffer=(char*)alloca(basesize+1);
      char *bp=buffer;

      fntop++;
      while( fntop < pos  &&  !isspace(strbuf[fntop]) ){
	*bp++ = strbuf[fntop++];
      }
      *bp = '\0';

      int nfiles=com.makelist( buffer );
      if( nfiles < 0 )
	return;
      struct filelist *cur=com.findfirst();
      putchr('\n');

      int scrnsize[2];
      _scrsize(scrnsize);

      int files_per_line   = (scrnsize[0])/(com.get_max_name_length()+2);
      int files_per_column = (nfiles+files_per_line-1)/files_per_line;
      
      struct filelist **ptr =
	(struct filelist**)alloca(files_per_line*sizeof(struct filelist *));
      for(int i=0 ; i<files_per_line; i++ ){
	ptr[i] = NULL;
      }

      for(int i=0; i<files_per_line-1 && cur != NULL ; i++ ){
	ptr[i] = cur;
	for(int j=0 ; cur != NULL && j<files_per_column ; j++){
	  cur = cur->next;      
	}
      }
      ptr[files_per_line-1] = cur;
    
      for(int j=0; j<files_per_column ; j++ ){
	for(int i=0; i<files_per_line  &&  ptr[i] != NULL ; i++ ){
	  int n=fprintf(fp,"%s%c ",
			ptr[i]->name,
			ptr[i]->attr & A_DIR 
			? Complete::directory_split_char : ' '
			);
	  while( n++ < com.get_max_name_length()+2 )
	    putchr(' ');

	  ptr[i] = ptr[i]->next;
	}
	putchr('\n');
      }

#if 0
      int column=0;
      while( p != NULL ){
	int i=fprintf(fp,"%s%c ",
		      p->name,
		      p->attr & A_DIR ? '\\' : ' '
		      );
	while( i++ < com.get_max_name_length()+2 )
	  putchr(' ');
	column += i;

	if( column + com.get_max_name_length() >= 80 ){
	  putchr('\n');
	  column = 0;
	}
	p = com.findnext();
      }
#endif
      fprintf(fp,"\n%s",prompt);
      int i=0;
      while( top+i<len && i<windowsize ){
	putchr( strbuf[top + i++] );
      }
      putbs( i-(pos-top) );
      return;
    }
  }
}

int ShellEdlin::setprompt(const char *sp)
{
  prompt=sp;
  promptlen=0;
  while( *sp != '\0' ){
    /* エスケープシーケンスを除いた文字数を windowsize から引いておく */
    if( *sp++ == '\x1B' ){
      while( *sp != '\0' && !isalpha(*sp) )
	sp++;
      sp++;
    }else{
      promptlen++;
    }
  }
  return promptlen;
}

int ShellEdlin::simple_input(const char *prompt,int window)
{
  windowsize = window - setprompt(prompt);
  fputs(prompt,stdout);
  fflush(stdout);
  return simple_line_input();
}

void ShellEdlin::cls()
{
  fprintf( fp , "\x1B[2J%s" , prompt );
  int i=0;
  while( i<windowsize && top+i < len )
    putchr( strbuf[top+i++] );
  putbs( i - (pos-top) );
}
