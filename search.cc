#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <stdlib.h>
#include "macros.h"
#include "finds.h"

const char *getShellEnv(const char *);

#undef CACHE
#ifdef CACHE
extern PathCache *script_cache;
#endif

/* ファイルが存在していて、しかもディレクトリ名でなければ、真(1)を返す。*/

static int is_file_not_dir(const char *path)
{
  if( access(path,0) != 0 )
    return 0;

  struct stat statbuf;
  if( stat(path,&statbuf)==0  && (statbuf.st_attr & 0x10)==0 )
    return 1;
  else
    return 0;
}

static int cmdexe_check(const char *path, char *tail )
{
  /* exe , cmd について、個別に存在するかをチェックする */
  tail[0]='.'; tail[4]='\0';
  
  tail[1]='e';  tail[2]='x';  tail[3]='e';
  if( is_file_not_dir(path) )
    return EXE_FILE;
  
  tail[1]='c';  tail[2]='m';  tail[3]='d';
  if( is_file_not_dir(path) )
    return CMD_FILE;

  tail[2]='o'; tail[3]='m';
  if( is_file_not_dir(path) )
    return COM_FILE;
  
  tail[0]='\0';
  if( is_file_not_dir(path) )
    return FILE_EXISTS;

  return NO_FILE;
}

static int _SearchEnv(const char *fname,const char *envname,char *path)
{
  int absolute_path=0;

  const char *period = NULL;
  char *dp=path;
  int fnlen=0;
  
  for( const char *p=fname ; *p != '\0' ; p++,fnlen++ ){
    if( *p=='\\' || *p=='/' || *p==':' ){
      absolute_path = 1;
      period = NULL;
    }else if( *p=='.' ){
      period = p;
    }
    
    if( is_kanji(*p) )
      *dp++ = *p++;
    *dp++ = *p;
  }
  *dp = '\0';
  
  /* 拡張子が存在している場合、そのタイプを調べておく */
  int suffix_type = FILE_EXISTS;
  if( period != NULL  &&  period[4] == '\0' ){
    if(   (period[1]=='e' || period[1]=='E' )
       && (period[2]=='x' || period[2]=='X' )
       && (period[3]=='e' || period[3]=='E' ) ){
      
      suffix_type = EXE_FILE ;
      
    }else if( period[1]=='c' || period[1]=='C' ){
      // 一文字目が C ならば CMD か COM
      
      if(   (period[2]=='m' || period[2]=='M' )
	 && (period[3]=='d' || period[3]=='D' ) ){
	
	suffix_type = CMD_FILE ;
	
      }else if(   (period[2]=='o' || period[2]=='O' )
	       && (period[3]=='m' || period[3]=='M' ) ){

	suffix_type = COM_FILE;
      }
    }
  }

  /* まず、現在のディレクトリで調べる */
     
  if( period==NULL ){
    /* 拡張子が無い場合、補完して調べる */
    int rc=cmdexe_check(path,dp);
    if( rc != NO_FILE )
      return rc;
  }else{
    /* 拡張子が有る場合、そのままで調べる */
    if( is_file_not_dir(path) )
      return suffix_type;
  }
  
  /* 絶対パス指定で、この時点で見付かっていない場合は終了 */
  if( absolute_path )
    return NO_FILE;

  /* ファイル名のみの記述の場合、検索する */
#ifdef CACHE
  if( script_cache != NULL ){
    /* キャッシュ有りの場合 */
    if( period == NULL ){
      char *buf=(char*)alloca(fnlen+5);
      const char *sp=fname;
      char *dp=buf;

      while( *sp != '\0' )
	*dp++ = *sp++;

      dp[0]='.'; dp[1]='E'; dp[2]='X'; dp[3]='E'; dp[4]='\0';
      const char *result=script_cache->find(fname);
      if( result != NULL ){
	strcpy( path , result );
	return EXE_FILE;
      }
      dp[1]='C'; dp[2]='M'; dp[3]='D';
      result = script_cache->find(fname);
      if( result != NULL ){
	strcpy( path, result );
	return CMD_FILE;
      }
      dp[2]='O'; dp[3]='M';
      result = script_cache->find(fname);
      if( result != NULL ){
	strcpy( path, result );
	return COM_FILE;
      }
      dp[0] = '\0';
      result = script_cache->find(fname);
      if( result != NULL ){
	strcpy( path , result );
	return FILE_EXISTS;
      }
      return NO_FILE;
    }else{
      const char *p = script_cache->find(fname);
      if( p != NULL){
	strcpy( path , p);
	return suffix_type;
      }
      return NO_FILE;
    }
  }else{
#endif
    /* キャッシュ無しの場合 */
    const char *env=getShellEnv(envname);
    if( env == NULL )
      return NO_FILE;
    
    int lastchar=0;
    dp=path;
    
    for(;;){
      if( *env != '\0' && *env != ';' ){
	if( is_kanji(lastchar=*env) )
	  *dp++ = *env++;
	*dp++ = *env++;
	continue;
      }
      if( lastchar != '/' && lastchar != '\\' )
	*dp++= '\\';
      
      /* ファイル名をコピー */
      for( const char *sp=fname ; *sp != '\0' ; sp++ ){
	*dp++ = *sp;
      }
      *dp = '\0';
      
      if( period != NULL ){
	/* 拡張子がある場合は、そのままで Ok! */
	if( is_file_not_dir(path) )
	  return suffix_type;
      }else{
	/* 拡張子が無い場合は、EXE , CMD , 拡張子無しについて調べる */
	int rc=cmdexe_check(path,dp);
	if( rc != NO_FILE )
	  return rc;
      }
      if( *env == '\0' )
	return NO_FILE;
      
      ++env;           /* ; をスキップ */
      dp = path;
      lastchar = 0;
    }
#ifdef CACHE
  }/* キャッシュ無しの場合の終了 */
#endif
}

int SearchEnv(const char *fname,const char *envname,char *path)
{
  int rc=_SearchEnv(fname,envname,path);
  if( rc == NO_FILE )
    strcpy( path , fname );
  return rc;
}

#if 0
#include <stdio.h>
int main(int argc,char **argv)
{
  for(int i=1;i<argc;i++){
    char buffer[FILENAME_MAX];
    int rc=SearchEnv(argv[i],"SCRIPTPATH",buffer);
    if( rc != NO_FILE ){
      printf("%d : %s on SCRIPTPATH\n",rc,buffer);
    }
    rc=SearchEnv(argv[i],"PATH",buffer);
    if( rc != NO_FILE ){
      printf("%d : %s on PATH\n",rc,buffer);
    }
  }
  return 0;
}
#endif
