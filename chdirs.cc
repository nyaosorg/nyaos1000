#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>

#include "parse.h"
#include "nyaos.h"
#include "finds.h"

int option_cd_goto_home=0;

enum{
  BIT_CD_PATH      = 1,
  BIT_CD_SHORT_MID = 2,
  BIT_CD_SHORT_TOP = 4,
  BIT_CD_SHORT     = 6,
};

int cmd_pwd( FILE *source , Parse &params )
{
  char cwd[FILENAME_MAX];

  getcwd_case(cwd);

  FILE *fout=params.open_stdout();
  fputs(cwd,fout);
  putc('\n',fout);
  return 0;
}

static int cdshort_1(char *list[] , int modeflag );

static int cdshort_2(const char *cwdx,char *list[] , int modeflag )
{
  Dir dir;
  for( dir._findfirst(cwdx) ; dir != NULL ; ++dir ){
    const char *name=dir.get_name();
    if(   name[0] != '.'
       && (dir.get_attr() & Dir::DIRECTORY) != 0
       && (dir.get_attr() & (Dir::HIDDEN|Dir::SYSTEM)) == 0
       && _chdir2(name)==0 ){
      
      if( cdshort_1(list+1,modeflag) == 0 ){
	return 0;
      }else{
	chdir("..");
      }
    }
  }
  return -1;
}
static int cdshort_1( char *list[] ,int modeflag )
{
  if( *list == NULL )
    return 0;
  
  if( list[0][0] == '*' && list[0][1] == '\0' )
    return cdshort_2("*",list,modeflag);

  char *cwdx = (char*)malloc( strlen(*list)+3 );
  
  *cwdx = '*';
  strcpy_tail(strcpy_tail( cwdx+1 , *list ) , "*" );

  int rc=cdshort_2(cwdx+1,list,modeflag);
  if( rc != 0 && (modeflag & BIT_CD_SHORT_MID) !=0 ){
    rc = cdshort_2(cwdx  ,list,modeflag);
  }
  return rc;
}

/* cd [-sp] path */

static int smart_chdir(FILE *source , Parse &params , int modeflag=0 )
{
  // パラメータを全て、ポインタ配列に変換する。
  int n = params.get_argc();
  char **argv = (char**)alloca(sizeof(char*)*(n+1));
  int argc=0;
  for(int i=1;i<n;i++){
    argv[argc] = (char*)alloca( params.get_length(i)+1 );
    params.copy(i,argv[argc]);
    if( argv[argc][0] == '-' ){
      switch( argv[argc][1] ){
      case 's':
      case 'S':
	modeflag |= BIT_CD_SHORT_MID;
	break;
	
      case 't':
      case 'T':
	modeflag |= BIT_CD_SHORT_TOP;
	break;
	
      case 'p':
      case 'P':
	modeflag |= BIT_CD_PATH;
	break;
      }
    }else{
      argc++;
    }
  }
  argv[argc] = NULL;

  if( argc <= 0 ){
    if( option_cd_goto_home ){
      const char *home=getenv("HOME");
      if( home == NULL || _chdir2(home) != 0 )
	fputs("nyaos: %HOME% does not point a right directory.\n",stderr);
      return 0;
    }else{
      return cmd_pwd(source,params);
    }
  }
  
  char *cwd=argv[0];
  if( _chdir2( cwd )==0 )
    return 0;
  
  for( const char *p=cwd ; *p != '\0' ; p++ ){
    if( *p=='/' || *p=='\\' || *p==':' ){
      fprintf(stderr,"%s: no such directory.\n",cwd);
      return 0;
    }
  }
  
  // ------- CDPATH を検索する。 --------
  
  if( modeflag & BIT_CD_PATH ){
    const char *sp=getenv("CDPATH");
    
    if( sp != NULL ){
      char cdpath[FILENAME_MAX];
      char *dp=cdpath;
      int lastchar = 0;
      
      for(;;){
	if( *sp != '\0' && *sp != ';' ){
	  if( is_kanji(lastchar=*sp) )
	    *dp++ = *sp++;
	  *dp++ = *sp++;
	  continue;
	}
	if( lastchar != '\\' && lastchar != '/' && lastchar != ':' )
	  *dp++ = '\\';
	strcpy( dp , cwd );
	if( access(cdpath,0)==0  &&  _chdir2(cdpath)==0 )
	  return 0;
	
	if( *sp == '\0' )
	  break;
	
	++sp; /* for semicolon */
	dp = cdpath;
      }
    }
  }

  if( modeflag & BIT_CD_SHORT ){
    /* CD-SHORT モード */
    const char *env=getenv("CDSHORT");
    if( env != NULL ){
      int org_drive=_getdrive();
      
      // 環境変数 CDSHORT を走査する。
      while( *env != '\0' ){
	if( isalpha(*env) ){
	  char pwd[256];
	  
	  _chdrive(*env);
	  getcwd(pwd,sizeof(pwd));
	  chdir("/");
	  
	  if( cdshort_1(argv,modeflag)==0 )
	    return 0;
	  
	  chdir(pwd);
	}
	++env;
      }
      _chdrive( org_drive );
    }
  }
  
  fprintf(stderr,"%s : no such directory.\n",argv[0]);
  return 0;
}

int cmd_chdir( FILE *srcfil, Parse &params)
{
  if( params.get_argc() > 1 ){
    int thirdchar = params.get_argv(0)[2];
    if( thirdchar == 's' || thirdchar == 'S' )
      smart_chdir(srcfil,params,BIT_CD_SHORT_MID);
    else
      smart_chdir(srcfil,params);
  }else if( option_cd_goto_home ){
    const char *home=getenv("HOME");
    if( home == NULL || _chdir2(home) != 0 )
      fprintf(stderr,"chdir: $HOME does not point a right directory.\n");
  }else{
    return cmd_pwd(srcfil,params);
  }
  return 0;
}
  

struct Dirstack{
  Dirstack *prev;
  char buffer[1];
} *dirstack=NULL;

int cmd_dirs( FILE *srcfil , Parse &params )
{
  char cwd[FILENAME_MAX];
  getcwd_case(cwd);
  
  FILE *fout=params.open_stdout();
  if( fout == NULL ){
    fputs("nyaos : cannot make a pipe or file\n",stderr);
    return 1;
  }
  fputs(cwd,fout);

  Dirstack *tmp=dirstack;
  while( tmp != NULL ){
    putc(' ',fout);
    fputs(tmp->buffer,fout);

    tmp = tmp->prev;
  }
  putc('\n',fout);

  return 0;
}

int cmd_pushd( FILE *srcfil , Parse &params)
{
  char cwd[FILENAME_MAX];
  getcwd_case(cwd);
  
  if( params.get_argc() > 1 ){
    if( smart_chdir(srcfil,params) )
      return 0;
  }

  Dirstack *tmp=(Dirstack*)malloc(sizeof(Dirstack)+strlen(cwd));
  tmp->prev = dirstack;
  strcpy( tmp->buffer , cwd );
  dirstack = tmp;

  return cmd_dirs(srcfil,params);
}

int cmd_popd( FILE *srcfil, Parse &params)
{
  if( dirstack != NULL ){
    _chdir2( dirstack->buffer );
    Dirstack *tmp=dirstack;
    dirstack = dirstack->prev;
    free(tmp);

    return cmd_dirs( srcfil , params );
  }else{
    printf("dirs : directory stack is empty!\n");
    return 0;
  }
}
