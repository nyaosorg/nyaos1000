#include <assert.h>
#include <stdio.h>
#include "finds.h"

struct Samename{
  Samename *next;
  unsigned size;
  unsigned short date,time;
  char *directory;
};

struct Files{
  Files *lower,*higher;
  Samename first;
  char name[1];
} *tree;

Files *mktrees()
{
  struct DirList{
    DirList *next;
    char name[1];
  } *first=NULL;

  char buffer[256];
  _getcwd2(buffer,sizeof(buffer));
  char *cwd=strdup(buffer);
  assert( cwd != NULL );
  
  for(Dir dir(".") ; dir ; dir++ ){
    const char *name=dir.get_name();
    if( name[0] == '.' )
      continue;

    if( dir.is_dir() ){
      /* ディレクトリ */
      DirList *tmp=(DirList*)alloca(sizeof(DirList)+dir.get_name_length());
      tmp->next = first;
      strcpy(tmp->name , name );
      first = tmp;
    }else{
      /* ファイル */
      if( tree == NULL ){
	tree=malloc( sizeof(Files) + dir.get_name_length() );
	assert(tree != NULL );
	tree->lower = tree->higher = NULL;
	tree->first.next = NULL;
	tree->first.size = 

    }  
  }
  while( first != NULL ){
    if( chdir( first->name ) != 0 ){
      mktrees();
      chdir("..");
    }
    first = first->next;
  }
}
