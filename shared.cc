#include <stdio.h>
#include "shared.h"

#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>

//#define DEBUG1(x) (x)
#define DEBUG1(x) /**/
//#define DEBUG2(x) (x)
#define DEBUG2(x) /**/

void SharedMem::lock() throw (SharedMem::SemError)
{
  if( nlocked <= 0 ){
    int rc=DosRequestMutexSem( hmtx, 1200000u );
    if( rc != 0 )
      throw SemError(rc);
    ++nlocked;
  }
}

void SharedMem::unlock()
{
  if( --nlocked <= 0 ){
    DosReleaseMutexSem( hmtx );
    nlocked = 0;
  }
}

SharedMem::~SharedMem()
{
  if( nlocked > 0 )
    DosReleaseMutexSem( hmtx );
}


SharedMem::SharedMem(  const char *semname
		     , const char *memname
		     , int staticAreaSize) 
     throw(SharedMem::Fail,SharedMem::SemError) : nlocked(0)
{
  DEBUG1( fputs("(SharedMem::SharedMem) enter.\n",stderr) );
  static_area_size = staticAreaSize;

  /* セマフォの作成 */
  if(   DosOpenMutexSem((PSZ)semname , &hmtx ) 
     && DosCreateMutexSem((PSZ)semname,&hmtx,DC_SEM_SHARED,0)){
    DEBUG1( fputs("(SharedMem::SharedMem) Dos**MutexSem failed.\n",stderr));
    throw SharedMem::Fail();
  }
  DEBUG1( fputs("(SharedMem::SharedMem) made semaphore\n",stderr ));
  
  /* 共有メモリの作成 */
  if( DosGetNamedSharedMem(  (PPVOID)&first,(PUCHAR)memname
			   , PAG_READ | PAG_WRITE ) ){
    
    /* 共有領域はまだ存在していないので、作る */
    int rc;
    if( (rc=DosAllocSharedMem( (PPVOID)&first , (PUCHAR)memname
			  , default_size , fALLOC )) != 0 ){
      DEBUG1( fprintf(stderr
		      ,"(SharedMem::SharedMem) Dos**SharedMem failed(%d)\n"
		      , rc ));
      throw SharedMem::Fail();
    }
    lock();
    first->next = NULL;
    first->allsize  = default_size;
    first->usedsize = sizeof(Header)+staticAreaSize;
    last = first;
    unlock();
    isvirgin = true;
  }else{
    /* すでに共有領域が出来ている！*/
    lock();

    /* 続くヒープを全て共有化する */
    last=first;
    while( last->next != NULL ){
      last = last->next;
      if( DosGetSharedMem(last,PAG_READ | PAG_WRITE ) != 0 ){
	unlock();
	throw SharedMem::Fail();
      }
    }
    unlock();
    isvirgin = false;
  }
}

void *SharedMem::alloc(int size) throw( SharedMem::SemError )
{
  lock();
  if( last->allsize < last->usedsize + size ){
    if(   last->next == NULL
       && (   DosAllocSharedMem( (PPVOID)&last->next , NULL , default_size
				, PAG_COMMIT | OBJ_GIVEABLE | OBJ_GETTABLE 
				| PAG_READ | PAG_WRITE ) !=0 
	   || last->next == NULL ) ){
      DEBUG2( fputs("(Sharedmem::alloc) failed...\n",stderr) );
      unlock();
      return NULL;
    }
    DEBUG2( fputs("(Sharedmem::alloc) object gets\n",stderr) );

    last = last->next;
    last->allsize = default_size;
    last->usedsize = sizeof(Header);
    last->next = NULL;
    DEBUG2( fputs("(Sharedmem::alloc) object gets2\n",stderr) );
  }
  void *result = (char*)last + last->usedsize ;
  last->usedsize += size;
  unlock();
  return result;
}

void SharedMem::clear() throw(SharedMem::SemError)
{
  DEBUG1( fputs("(SharedMem::clear) enter.\n",stderr) );
  lock();
  
  last = first;
  first->allsize  = default_size;
  first->usedsize = sizeof(Header)+static_area_size;
  
  unlock();
}

PathCacheShared::PathCacheShared() throw(SharedMem::SemError)
: SharedMem("\\sem32\\nyaos\\cache","\\sharemem\\nyaos\\cache",sizeof(Common)) 
{
  common = (Common*)getStaticArea();
  lock();
  if( isVirgin() ){
    common->nfiles = common->nbytes = 0;
    common->first  = common->last   = NULL;
  }
  unlock();
}

void PathCacheShared::clear() throw(SharedMem::SemError)
{
  SharedMem::clear();
  lock();
  common->nfiles = 0;
  common->nbytes = 0;
  common->first  = 0;
  common->last   = 0;
  unlock();
}

int PathCacheShared::insert(const char *s) throw(SharedMem::SemError)
{
  DEBUG1( fputs("(PathCacheShared::insert) enter.\n",stderr) );
  int len=strlen(s);
  if( len == 0 ){
    DEBUG1( fputs("(PathCacheShared::insert) leave(0).\n",stderr) );
    return 0;
  }
  
  Node *newone=(Node*)alloc(sizeof(Node)+len);
  if( newone == NULL ){
    DEBUG1( fputs("(PathCacheShared::insert) fail to alooc.\n",stderr) );
    return -1;
  }
  
  lock();
  DEBUG1( fputs("(PathCacheShared::insert) lock success.\n",stderr) );
  newone->next = NULL;
  newone->length = len;
  strcpy( newone->name , s );

  DEBUG1( fputs("(PathCacheShared::insert) made new node .\n",stderr) );

  if( common->last != NULL ){
    DEBUG1( fputs("(PathCacheShared::insert) common->last != NULL\n",stderr));
    common->last->next = newone;
  }

  common->last = newone;
  if( common->first == NULL )
    common->first = newone;

  ++common->nfiles;
  ++common->nbytes += len+sizeof(Node);
  unlock();

  DEBUG1( fputs("(PathCacheShared::insert) leave.\n",stderr) );
  return 0;
}

