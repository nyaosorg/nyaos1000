/* -*- c++ -*- */
#define INCL_DOSFILEMGR
#include <os2.h>

#if 0
typedef struct _FILEFINDBUF3
{
  ULONG oNextEntryOffset;
  FDATE fdateCreation;
  FTIME ftimeCreation;
  FDATE fdateLastAccess;
  FTIME ftimeLastAccess;
  FDATE fdateLastWrite;
  FTIME ftimeLastWrite;
  ULONG cbFile;			// ファイルサイズ
  ULONG cbFileAlloc;		// ファイルに割り振られたサイズ
  ULONG attrFile;		// アトリビュート
  UCHAR cchName; 		// ファイル名の長さ
  CHAR	achName[CCHMAXPATHCOMP];// ファイル名
  
} FILEFINDBUF3;
#endif

class Dir{
  HDIR		handle;
  FILEFINDBUF3	buffer;
  ULONG		count;
  int		rc;
public:
  enum{ 
    ARCHIVED		= 0x20,
    DIRECTORY		= 0x10,
    SYSTEM		= 0x4,
    HIDDEN		= 0x2,
    READONLY		= 0x1,

    AND_ARCHIVED	= 0x2000,
    AND_DIRECTORY	= 0x1000,
    AND_SYSTEM		= 0x400,
    AND_HIDDEN		= 0x200,
    AND_READONLY	= 0x100,

    OR_ARCHIVED		= 0x20,
    OR_DIRECTORY	= 0x10,
    OR_SYSTEM		= 0x4,
    OR_HIDDEN		= 0x2,
    OR_READONLY		= 0x1,

    ALL = 0x37 ,
  };
  int _findfirst(const char *fname,int attr=ALL)
    { return rc=DosFindFirst( (PUCHAR)fname , &handle , attr
			     , (PVOID)&buffer , sizeof(buffer)
			     , &count , (ULONG)FIL_STANDARD );
    }

  int findfirst(const char *fname,int attr=ALL);
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

  // report functions
  const char *get_name() const { return buffer.achName; }
  int get_name_length() const { return buffer.cchName; }

  int get_attr() const { return buffer.attrFile; }
  unsigned get_size() const { return buffer.cbFile; }
  unsigned get_size_alloc() const { return buffer.cbFileAlloc; }


  // FDATE は、5bit:day 4bit:month 7bit:year という構造
    const FDATE &get_last_write_date() const { return buffer.fdateLastWrite; }
  // short型で返すバージョン
    unsigned short get_last_write_date_by_short() const {
      return *(unsigned short *)&buffer.fdateLastWrite;
    }

  // FTIME は、5bit:2sec 4bit:minutes 5:hours という構造  
    const FTIME &get_last_write_time() const { return buffer.ftimeLastWrite; }
  // short型で返すバージョン
    unsigned short get_last_write_time_by_short() const{
      return *(unsigned short *)&buffer.ftimeLastWrite; 
    }

  Dir() : handle(0xFFFFFFFF) , count(1) 
    { }
  Dir(const char *path,int attr=ALL) : handle(0xFFFFFFFF),count(1)
    { this->findfirst(path,attr); }
  ~Dir()
    { DosFindClose(handle); }
};

char **fnexplode2(const char *path);
void fnexplode2_free(char **list);
