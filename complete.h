#ifndef COMPLETE_H
#define COMPLETE_H

#include <sys/types.h>
#include <dirent.h>

#include "finds.h"

int pathsplit( const char *path, char *dir, char *fname );
int which_suffix(const char *path,...);

class Complete : public Files {
  char directory[ 256 ];
  char fname[ 256 ];
  int common_length;
  int max_length;
  int typed_split_char ; /* “ü—Í‚³‚ê‚½ƒpƒX•ª—£•¶Žš ( 0 , / or \ ) */
  FileListT *findptr ;

  static const char *errmsg[];

  int makelist_core(int command_complete, int is_with_dir );
public:
  enum{
    NOT_COMPLETED ,
    COMMAND_COMPLETED ,
    FILENAME_COMPLETED ,
    SIMPLE_COMMAND_COMPLETED ,
    ERROR
  } status;

  Complete() : common_length(0) , typed_split_char(0) 
    , status(NOT_COMPLETED) {  }
  ~Complete(){ clear(); }

  int makelist          (const char *path);
  int makelist_with_path(const char *path);
  int add_buildin_command(const char *name); /* after makelist only */
  
  char *nextchar();
  int get_fname_common_length()const{ return common_length; }
  const char *get_real_name1() const ;
  
  FileListT *findfirst(){ return findptr=get_top(); }
  FileListT *findnext(){  return findptr=findptr->next; }
  int get_max_name_length() const { return max_length; }
  
  static int directory_split_char;
  static int complete_tail_tilda;
  static int complete_hidden_file;

  int get_split_char(void){ return typed_split_char; }
};

#endif
