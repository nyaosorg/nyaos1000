#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>

#include "hash.h"
#include "edlin.h"
#include "complete.h"
#include "nyaos.h"

int ShellEdlin::beep_ok=1;

int ShellEdlin::complete_hook(Complete &com)
{
  int n=0;
  if( com.status == Complete::SIMPLE_COMMAND_COMPLETED ){
    /* build-in command */
    for( const Command *p=jumptable; p->name != NULL ; p++ ){
      if( com.add_buildin_command(p->name) == 0 )
	n++;
    }

    /* alias */
    extern Hash<Alias> alias_hash;

    for(HashIndex<Alias> hi(alias_hash) ; *hi != NULL ; hi++ ){
      if( com.add_buildin_command(hi->name) == 0 )
	n++;
    }
  }
  return n;
}

void ShellEdlin::complete_list()
{
  Complete com;

  int fntop=seek_word_top();  
  int basesize=pos-fntop;
  int command_complete=(fntop <= 0) ;
  char *buffer=(char*)alloca(basesize+6);

  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
  }
  char *bp=buffer;
  while( fntop < pos )
    *bp++ = strbuf[fntop++];
  *bp = '\0';
  
  int nfiles=(command_complete
	      ? com.makelist_with_path( buffer ) 
	      : com.makelist( buffer )
	      );
  
  nfiles += complete_hook(com);

  if( nfiles <= 0 )
    return;

  struct filelist *cur=com.findfirst();
  putchr('\n');
  
  int scrnsize[2];
  get_scrsize(scrnsize);

  int files_per_line = 
    scrnsize[0]-1 < com.get_max_name_length()+2
      ? 1 : (scrnsize[0]-1)/(com.get_max_name_length()+2);
  int files_per_column = (nfiles+files_per_line-1)/files_per_line;

  struct filelist **ptr =
    (struct filelist**)alloca(files_per_line*sizeof(struct filelist *));

  for(int i=0 ; i<files_per_line; i++ )
    ptr[i] = NULL;
  
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
      if( (i+1) < files_per_line && ptr[i+1] != NULL )
	while( n++ < com.get_max_name_length()+2 )
	  putchr(' ');
      
      ptr[i] = ptr[i]->next;
    }
    putchr('\n');
  }
  
  fprintf(fp,"\n%s",prompt);
  int i=0;
  while( top+i<len && i<windowsize ){
    putchr( strbuf[top + i++] );
  }
  putbs( i-(pos-top) );
}

int ShellEdlin::setprompt(const char *sp , int window )
{
  prompt=sp;
  promptlen=0;
  while( *sp != '\0' ){
    /* エスケープシーケンスを除いた文字数を windowsize から引いておく */
    if( *sp++ == '\x1B' ){
      while( *sp != '\0' && !is_alpha(*sp) )
	sp++;
      sp++;
    }else{
      promptlen++;
    }
  }
  windowsize = window - promptlen;
  return promptlen;
}

void ShellEdlin::cls()
{
  fprintf( fp , (using_i_mark ? "\x1B[2J\x1B[H\n%s" : "\x1B[2J\x1B[H%s")
	  , prompt );
  int i=0;
  while( i<windowsize && top+i < len )
    putchr( strbuf[top+i++] );
  putbs( i - (pos-top) );
}
