#ifndef COMPLETE_H
#define COMPLETE_H

#include <sys/types.h>
#include <dirent.h>

struct filelist{
  struct filelist *next;
  long size;
  unsigned short attr;
  union{
    unsigned short time;
    struct{
      unsigned second:5;
      unsigned minute:6;
      unsigned hour:5;
    }t;
  };
  union{
    unsigned short date;
    struct{
      unsigned day:5;
      unsigned month:4;
      unsigned year:7;
    }d;
  };
  int length;
  char name[1]; /* ‰Â•Ï’· */
};

struct filelist *fsort_and_insert(struct filelist *first,struct filelist *tmp);
int dircompare(struct filelist *d1,struct filelist *d2);
int pathsplit( const char *path, char *dir, char *fname );
int which_suffix(const char *path,...);

class Complete {
  char directory[ 256 ];
  char fname[ 256 ];
  int common_length;
  int nlists;
  int max_length;

  struct filelist *list , *findptr ;
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

  Complete() : common_length(0) , nlists(0)
     , list((struct filelist*)0) , status(NOT_COMPLETED){  }
  ~Complete(){ cleanup(); }

  int makelist          (const char *path);
  int makelist_with_path(const char *path);
  int add_buildin_command(const char *name); /* after makelist only */

  void cleanup();
  char *nextchar();
  int get_fname_common_length()const{ return common_length; }
  const char *get_real_name1() const { return list->name; }

  struct filelist *findfirst(){ return findptr=list; }
  struct filelist *findnext(){  return findptr=findptr->next; }
  int get_max_name_length() const { return max_length; }

  static int directory_split_char;
  static int complete_tail_tilda;
  static int complete_hidden_file;
};

#endif
