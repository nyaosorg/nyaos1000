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

class Complete {
  char directory[ 256 ];
  char fname[ 256 ];
  int nlists;
  int max_length;
  int common_length;

  struct filelist *list , *findptr ;
  static const char *errmsg[];

  int makelist_core(const char *path,int command_complete);

public:
  enum{
    NO_PROBLEM,
    MEMORY_ERROR,
  } err ;

  Complete() : common_length(0) , nlists(0) , err(NO_PROBLEM) 
    , list((struct filelist*)0) {  }
  ~Complete(){ cleanup(); }

  int makelist          (const char *path);
  int makelist_with_path(const char *path);

  void cleanup();
  char *nextchar();
  int get_fname_common_length()const{ return common_length; }
  const char *get_real_name1() const { return list->name; }

  struct filelist *findfirst(){ return findptr=list; }
  struct filelist *findnext(){  return findptr=findptr->next; }
  int get_max_name_length() const { return max_length; }

  operator const void*() const { return err ? NULL : this ;}
  int operator!() const { return err; }
  const char *get_errmsg() const { return errmsg[err]; }

  static int directory_split_char;
};

#endif
