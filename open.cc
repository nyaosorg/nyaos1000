// #define INCL_WINWORKPLACE
#define INCL_DOSNLS
//#define INCL_WINWINDOWMGR
#define INCL_WINSWITCHLIST
#define INCL_WINMESSAGEMGR
// #define INCL_WINFRAMEMGR
#include <os2.h>

/* for stat() */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h> /* for A_DIR */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <process.h>
#include <alloca.h>

#include "macros.h"
#include "nyaos.h"
#include "parse.h"
#include "hash.h"
#include "heapptr.h"
#include "errmsg.h"

extern Hash <Command> command_hash;
extern HAB hab;

#if 0
class SwitchList{
  int count;
  PSWBLOCK pswblock;
public:
  SwitchList();
  ~SwitchList(){ free(pswblock); }

  SWENTRY &operator[] (int n){ return pswblock->aswentry[n]; }
  int N() const { return count; }
};

SwitchList::SwitchList()
{
  count = WinQuerySwitchList(hab,NULL,0);
  int size = count*sizeof(SWENTRY) + sizeof( ULONG );
  
  pswblock = (PSWBLOCK)malloc( size );
  if( pswblock != NULL ){
    WinQuerySwitchList(hab,pswblock,size);
  }else{
    count = 0;
  }
}

int cmd_jobs(FILE *source , Parse &argv )
{
  SwitchList slist;
  
  const static char *progtype[] = {
    "DEF" , "FUL" , "VIO/W" , "PM" ,
    "VDM" , "???" , "???"   , "VDM/W" };

  FILE *fout=argv.open_stdout();

  fprintf(fout,
	  "%2s%5s%4s %-3s %-5s Title\n"
	  , "No"
	  , "PID"
	  , "SID"
	  , "VIS"
	  , "TYPE"
	  );

  for(int i=0; i<slist.N() ; i++ ){
    if( slist[i].swctl.fbJump == SWL_NOTJUMPABLE )
      continue;
    
    int top=
      fprintf(fout
	      , "%2d%5ld%4ld %-3s %-5s "
	      , i
	      , slist[i].swctl.idProcess
	      , slist[i].swctl.idSession
	      , ( slist[i].swctl.uchVisibility == SWL_VISIBLE 
		 ? "YES"
		 : (slist[i].swctl.uchVisibility == SWL_INVISIBLE
		    ? "NO" : "???" ) )
	      , (  slist[i].swctl.bProgType > 8 
		 ? "???" : progtype[ slist[i].swctl.bProgType ] )
	      );
    
    for(const char *p=slist[i].swctl.szSwtitle ; *p != '\0' ; p++ ){
      putc( *p , fout );
      if( *p == '\n' )
	for(int j=0;j<top;j++)
	  putc( ' ' , fout );
    }
    putc( '\n' ,fout);
  }
  return 0;
}

static int cmd_fg_bg( Parse &argv , BOOL fSuccess )
{
  SwitchList slist;

  for(int i=1;i<argv.get_argc() ; i++ ){
    if( isdigit(*argv[i].ptr & 255) ){
      int x=atoi(argv[i].ptr);
      if( x < slist.N() ){
	if( fSuccess != FALSE )
	  WinSwitchToProgram(slist[x].hswitch);
	WinShowWindow( slist[x].swctl.hwnd , fSuccess );
      }    
    }else{
      char *title=(char*)alloca( argv[i].len + 1 );
      argv[i].quote(title);

      for(int j=0;j<slist.N();j++){
	if( strstr( slist[j].swctl.szSwtitle , title ) != NULL ){
	  if( fSuccess != FALSE )
	    WinSwitchToProgram(slist[j].hswitch);
	  WinShowWindow( slist[j].swctl.hwnd , fSuccess );
	  break;
	}
      }
    }
  }
  return 0;
}

int cmd_fg(FILE *source , Parse &argv )
{
  return cmd_fg_bg( argv , TRUE );
}

int cmd_bg(FILE *source , Parse &argv )
{
  return cmd_fg_bg( argv , FALSE );
}
#endif

int eadir( int argc, char **argv,FILE *fout,Parse &parser);

static int print_file(char *s,Parse &parser)
{
  char *eadir_argv[]={
    "eadir",
    s,
    NULL,
  };

  struct stat statbuf;
  stat(s ,&statbuf);
  
  if( statbuf.st_attr & A_DIR ){
    return 1;
  }else{
    eadir(2,eadir_argv,parser.open_stdout(),parser);
    return 0;
  }
}

int cmd_which( FILE *source , Parse &params )
{
  int argc = params.get_argc();

  if( argc < 2 ){
    fprintf(stderr,"nyaos: which: usage ...which command-name\n\n");
    return 0;
  }

  char buffer[FILENAME_MAX];
  int type=0;
  
  char *argv=0; /* なんでか知らんが、ここにおかんと変なワーニングが
		   出たりするんだな。gcc のバグかな */
  for(int i=1;i<argc;i++){
    int len=params.get_length(i);
    argv=(char *)alloca(len+1);

    params.copy(i,SmartPtr(argv,len+1));
    
    try{
      heapchar_t pass1(replace_alias( argv ));
      heapchar_t pass2(replace_script( pass1 ));

      char *sp=pass2; /* 置換後のコマンドライン全体が入っている   */
      StrBuffer cmdname;

      while( *sp != '\0' && !is_space(*sp) )
	cmdname << *sp++;

      if( strcmp(cmdname,argv) != 0 ){
	printf("replaced to `%s'\n",(const char*)pass2 );
	continue;
      }

      if( command_hash[ params[i] ] != NULL ){
	printf( "%s: nyaos built-in command\n",argv);
      }else{
	/* 実行ファイルの検索 */
	type=SearchEnv(argv,"PATH",buffer);
	if(   type == NO_FILE
	   || type == FILE_EXISTS
	   || print_file(buffer,params) !=0 ){
	  printf( "%s : not found %s.\n"
		 , argv
		 , scriptflag ? "in PATH and SCRIPTPATH" : "in PATH" );
	}
      }
    }catch( Noclobber ){
      ErrMsg::say(ErrMsg::FileExists,argv,0);
    }
  }
  return 0;
}

static void the_open( char *fname , const char *setup_string )
{
  char *p=fname;
  char *lastp=NULL , *last2p=NULL;

  while( *p != '\0' ){
    last2p = lastp;
    lastp  = p;
    
    if( *p == '/' )
      *p = '\\';
    
    if( is_kanji(*p) )
      p++;
    p++;
  }
  *p = '\0';
  
  if(   lastp != NULL  &&  *lastp  == '\\' 
     && last2p != NULL &&  *last2p != ':'  ){
    *lastp = '\0' ;
  }
  
  HOBJECT hObject=WinQueryObject( (PSZ)fname );
  WinSetObjectData( hObject , (PCSZ) setup_string );
}
extern char *getcwd_case(char *dst);
extern void correct_case(char *dst,const char *src,int size);

int cmd_open( FILE *source , Parse &params )
{
  int argc = params.get_argc();
  const char *setup_string="OPEN=DEFAULT";
  int nopens=0;

  FILE *fout=params.open_stdout();

  for(int i=1;i<argc;i++){
    const char *arg=params.get_argv(i);
    
    if( arg[0] == '-' ){
      switch( arg[1] ){
      default:
	fprintf(fout,"open: bad option `%s'\n",arg);
	break;

      case 'p': /* プロパティーオプション */
      case 's':
	setup_string = "OPEN=SETTINGS";
	break;

      case 't':
	setup_string = "OPEN=TREE";
	break;

      case 'd':
	setup_string = "OPEN=DETAILS";
	break;
	
      case 'i':
	setup_string = "OPEN=ICON";
	break;

      case 'o':
	if( i+1 < argc ){
	  int len=params.get_length(++i);
	  char *tmp=(char*)alloca(len+7);
	  for(int j=0;j<5;j++){
	    tmp[j] = "OPEN="[j];
	  }
	  params.copy(i,tmp+5);
	  for(char *q=tmp+5 ; *q != '\0' ; ){
	    if( is_kanji(*q) ){
	      q += 2;
	    }else{
	      *q = toupper(*q);
	      q++;
	    }
	  }
	  setup_string = tmp;
	}
	break;
      }
    }else{
      int len=params.get_length(i);
      char *fname=(char*)alloca(len+3);
      char absfname[512];
      
      params.copy(i,fname);
      if( fname[0] == '[' ){
	fname[0] = '<';
	for(char *q=fname+1; *q != '\0' ; ){
	  if( is_kanji(*q) ){
	    *q += 2;
	  }else{
	    *q = toupper(*q);
	    q++;
	  }
	}
	fname[ len-1 ] = '>';
      }else if( fname[0] == '<' ){
	for(char *q=fname+1; *q != '\0' ; ){
	  if( is_kanji(*q) ){
	    *q += 2;
	  }else{
	    *q = toupper(*q);
	    q++;
	  }
	}
	fname[ len-1 ] = '>';
      }else{
	correct_case( absfname , fname , sizeof(absfname) );
	fname = absfname;
      }
      
      fprintf(fout,"open %s\n", fname );
      the_open(fname , setup_string );
      nopens++;
    }
  }
  if( nopens == 0 ){
    char cwd[512];
    correct_case( cwd , "." , sizeof(cwd) );
    fprintf(fout,"open %s\n",cwd);
    the_open(cwd , setup_string  );
  }
  return 0;
}

int cmd_chcp( FILE *source , Parse &params )
{
  int argc = params.get_argc();
  if( argc > 1 ){
    int len=params.get_length(1);
    char *cp_str=(char*)alloca(len+1);
    params.copy(1,cp_str);
    unsigned int cp=atoi(cp_str);
    if( cp != 0  &&  spawnlp(P_WAIT,"cmd.exe",
			     "cmd","/C","chcp",cp_str,NULL)==0  ){
      DosSetProcessCp( cp );
      dbcs_table_init();
    }
  }else{
    spawnlp(P_WAIT,"cmd.exe","cmd","/C","chcp",NULL);
  }
  return 0;
}
