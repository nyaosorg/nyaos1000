/* -*- c++ -*- */
#ifndef FINDS_H
#define FINDS_H

#define INCL_DOSFILEMGR
#include <os2.h>

#ifndef MALLOC_ERROR
#define MALLOC_ERROR
class MallocError{ };
#endif

/* FindFirst/Next のカバークラス : Dir 
 *   for(Dir dir("*") ; dir ; ++dir )
 *     puts( dir.get_name() );
 * で簡易lsができる。
 */
class Dir{
  HDIR		handle;
  FILEFINDBUF4	buffer;
  ULONG		count;
  int		rc;
  int           codepage;
public:
  enum{ 
    ARCHIVED	= 0x20,	AND_ARCHIVED	= 0x2000, OR_ARCHIVED	= 0x20,
    DIRECTORY	= 0x10,	AND_DIRECTORY	= 0x1000, OR_DIRECTORY	= 0x10,
    VOLUME	= 0x80,	AND_VOLUME	= 0x800,  OR_VOLUME	= 0x8,
    SYSTEM	= 0x4,	AND_SYSTEM	= 0x400,  OR_SYSTEM	= 0x4,
    HIDDEN	= 0x2,	AND_HIDDEN	= 0x200,  OR_HIDDEN	= 0x2,
    READONLY	= 0x1,	AND_READONLY	= 0x100,  OR_READONLY	= 0x1,
    ALL = 0x37 ,
  };
  int dosfindfirst(const char *fname,int attr=ALL)
    {
      return rc=DosFindFirst( (PUCHAR)fname , &handle , attr
			     , (PVOID)&buffer , sizeof(buffer)
			     , &count , (ULONG)FIL_QUERYEASIZE );
    }
  

  int findfirst(const char *fname,int attr=ALL);
  
  int findfirst_with_wildcard(const char *fname,int attr=ALL);

  int findnext()
    { return rc=DosFindNext(handle,&buffer,sizeof(buffer),&count );}

  operator const void *() const
    { return rc==0 ? this : NULL ; }
  int operator !() const
    { return rc; }
  int operator[](int n) const
    { return buffer.achName[n]; }
  const void *operator++()   
    { return findnext() ? NULL : this; }
  const void *operator++(int)
    { int rv=rc; findnext(); return rv ? NULL: this; }
  
  /* --------------- リポート関数 --------------- */
  const char *get_name() const { return buffer.achName; }
  int get_name_length()  const { return buffer.cchName; }
  bool ends_with(char c) const { return (*this)[get_name_length()-1] == c ; }

  int get_attr() const { return buffer.attrFile; }
  unsigned get_size() const { return buffer.cbFile; }
  unsigned get_size_alloc() const { return buffer.cbFileAlloc; }
  unsigned get_easize() const { return buffer.cbList; }

  int is_dir()      const { return buffer.attrFile & DIRECTORY; }
  int is_system()   const { return buffer.attrFile & SYSTEM; }
  int is_hidden()   const { return buffer.attrFile & HIDDEN; }
  int is_readonly() const { return buffer.attrFile & READONLY; }

  /* FDATE は、5bit:日 4bit:月 7bit:年
   * FTIME は、5bit:秒の2倍 4bit:分 5:時 */

  /* ファイル作成日時 */
  const FDATE &get_create_date() const { return buffer.fdateCreation; }
  const FTIME &get_create_time() const { return buffer.ftimeCreation; }
  /* 最終アクセス日時 */
  const FDATE &get_last_access_date() const { return buffer.fdateLastAccess; }
  const FTIME &get_last_access_time() const { return buffer.ftimeLastAccess; }
  /* 最終書きこみ日時 */
  const FDATE &get_last_write_date() const { return buffer.fdateLastWrite; }
  const FTIME &get_last_write_time() const { return buffer.ftimeLastWrite; }

  /* ------------- コンストラクタ/デストラクタ ------------ */
  Dir();
  Dir(const char *path,int attr=ALL);
  ~Dir();
};

char **fnexplode2(const char *path);
void fnexplode2_free(char **list);
void numeric_sort(char **list);
int strnumcmp(const char *s1,const char *s2); /* complete.cc */
int convroot(char *&dp,int &size,const char *sp) throw(size_t);

/* filelist.cc */

typedef struct filelist{
  struct filelist *next,*prev;
  long size,easize;
  unsigned short attr;
  struct DirDateTime{
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
	unsigned day:5;     // 0～31
	unsigned month:4;   // 0～15
	unsigned year:7;    // 0～127
      }d;
    };
    int getYear()  const { return (int)d.year+1980; }
    int getMonth() const { return (int)d.month; }
    int getDay()   const { return (int)d.day; }
    int getHour()  const { return (int)t.hour; }
    int getMinute()const { return (int)t.minute; }
    int getSecond()const { return (int)t.second; }

    void setTime(const FTIME &t)
      { time = *(unsigned short *)&t; }
    void setDate(const FDATE &d)
      { date = *(unsigned short *)&d; }

  } create , access , write ;

  int length;
  char name[1]; /* 可変長 */
} FileListT;

enum{
  SORT_BY_NAME,
  SORT_BY_CHANGE_TIME,
  SORT_BY_LAST_ACCESS_TIME,
  SORT_BY_SIZE,
  SORT_BY_SUFFIX,
  SORT_BY_MODIFICATION_TIME,
  SORT_BY_NAME_IGNORE,
  SORT_BY_NUMERIC,
  UNSORT,
  SORT_REVERSE = 0x100 ,
};

int dircompare(struct filelist *d1,struct filelist *d2);

FileListT *new_filelist(const char *fname);
FileListT *new_filelist(Dir &dir);
FileListT *dup_filelist(struct filelist *);

class Files{
  FileListT *top;
  char *dirname;
  int n;
public:
  void insert( FileListT *newone , int sort=UNSORT );
  void sort( int method=0 );

  void setDirName(const char *name);
  const char *getDirName() const { return dirname; }
  
  int get_num() const { return n; }
  FileListT *get_top() const { return top; }
  FileListT *get_tail() const;
  void clear();
  
  Files() : top(0) , dirname(0) , n(0) { }
  ~Files(){ clear(); }
};
			    
#endif
