#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#define INCL_DOSMISC
#include <os2.h>

#include "parse.h"
#include "nyaos.h"
#include "finds.h"

int option_cd_goto_home=0;
char prevdir[FILENAME_MAX]=".";

enum{
  BIT_CD_PATH      = 1,
  BIT_CD_SHORT_MID = 2,
  BIT_CD_SHORT_TOP = 4,
  BIT_CD_SHORT     = 6,
  BIT_CD_LAST	   = 8,
};

/* ドライブが有効かどうか
 * 
 */
static int changeDir(const char *s)
{
  int rc=0;
  if( s[1] == ':' ){
    char buffer[FILENAME_MAX];
    
    DosError( FERR_DISABLEHARDERR );
    if( _getcwd1(buffer,toupper(s[0])) == 0 ){
      rc = _chdir2( s );
    }else{
      fprintf(stderr,"Drive %c: is not ready\n",s[0]);
    }
    DosError( FERR_ENABLEHARDERR );    
  }else{
    rc=_chdir2(s);
  }
  return rc;
}

int cmd_pwd( FILE *source , Parse &params )
{
  char cwd[FILENAME_MAX];

  if( getcwd_case(cwd) == NULL )
    return 0;

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
       && changeDir(name)==0 ){
      
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

      case 0:
	modeflag |= BIT_CD_LAST;
      }
    }else{
      argc++;
    }
  }
  argv[argc] = NULL;

  char wd[FILENAME_MAX];
  getcwd_case(wd);

  if(argc<=0&&!(modeflag&BIT_CD_LAST)){
    if( option_cd_goto_home ){
      const char *home=getenv("HOME");
      if( home == NULL || _chdir2(home) != 0 ){
	fputs("nyaos: %HOME% does not point a right directory.\n",stderr);
	return-1;
      }
      strcpy(prevdir,wd);
      return 0;
    }else{
      return cmd_pwd(source,params);
    }
  }
  
  char *cwd= ( modeflag & BIT_CD_LAST ) ? prevdir : argv[0] ;
  if( _chdir2( cwd )==0 ){
    strcpy(prevdir,wd);
    return 0;
  }
  for( const char *p=cwd ; *p != '\0' ; p++ ){
    if( *p=='/' || *p=='\\' || *p==':' ){
      fprintf(stderr,"%s: no such directory.\n",cwd);
      return-1;
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
	if( access(cdpath,0)==0	 &&  _chdir2(cdpath)==0 ){
	  strcpy(prevdir,wd);
	  return 0;
	}
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
	  
	  if( cdshort_1(argv,modeflag)==0 ){
	    strcpy(prevdir,wd);
	    return 0;
	  }
	  chdir(pwd);
	}
	++env;
      }
      _chdrive( org_drive );
    }
  }
  
  fprintf(stderr,"%s : no such directory.\n",argv[0]);
  return-1;
}

/* 内蔵コマンド「cd」
 * in	srcfil コマンドが入っていた入力ストリーム
 *	params パラメータオブジェクト(see "parse.h")
 * return 常に 0
 */
int cmd_chdir( FILE *srcfil, Parse &params)
{
  if( params.get_argc() > 1 ){
    int thirdchar = params.get_argv(0)[2];
    if( thirdchar == 's' || thirdchar == 'S' )
      smart_chdir(srcfil,params,BIT_CD_SHORT_MID);
    else
      smart_chdir(srcfil,params);
  }else if( option_cd_goto_home ){
    char wd[FILENAME_MAX];
    getcwd_case(wd);
    const char *home=getenv("HOME");
    if( home == NULL || changeDir(home) != 0 )
      fprintf(stderr,"chdir: $HOME does not point a right directory.\n");
    strcpy(prevdir,wd);
  }else{
    return cmd_pwd(srcfil,params);
  }
  return 0;
}
  
/* ディレクトリスタック
 */
struct Dirstack{
  Dirstack *prev;
  char buffer[1];
} *dirstack=NULL;

/* params の定義する標準出力へ、ディレクトリスタックの内容を表示する。
 * in	params パラメータオブジェクト。出力ストリームを得るのに用いるだけ。
 * 	flag  bit0=1: 縦型表示をする(-v オプション用)
 */
int simple_dirs ( Parse &params , int flag=0 )
{
  FILE *fout=params.open_stdout();
  if( fout == NULL ){
    fputs("nyaos : cannot make a pipe or file\n",stderr);
    return 1;
  }

  char cwd[FILENAME_MAX];
  getcwd_case(cwd);
  
  if( flag & 1 )
    fprintf(fout,"%-8d%s\n",0,cwd);
  else
    fputs(cwd,fout);  

  Dirstack *tmp=dirstack;
  int i=1;
  while( tmp != NULL ){
    if( flag & 1 )
      fprintf(fout,"%-8d%s\n",i,tmp->buffer);
    else
      fprintf(fout," %s",tmp->buffer);

    tmp = tmp->prev;
    i++;
  }
  if( (flag & 1)==0 )
    putc('\n',fout);

  return 0;
}

/* 内蔵コマンド dirs
 * in	srcfil コマンドの入っていたストリーム
 *	params パラメータオブジェクト
 */
int cmd_dirs ( FILE *srcfil , Parse &params )
{
  int flag=0;

  for(int i=1; i<params.get_argc() ; i++){
    if( params[i][0]=='-' ){
      for(int j=1 ; j<params[i].len ; j++){
	switch( params[i][j] ){
	case 'v':
	  flag |= 1;
	  break;
	  
	default:
	  fprintf(stderr,"-%c : unknown option\n",params[i][j]);
	  break;
	}
      }
    }
  }
  return simple_dirs( params , flag );
}

/* スタックトップをスタック末尾に移動する。
 * ここでいうトップは、カレントディレクトリではなく、dirs の二番目。
 */
static void move_stacktop_to_stacktail()
{
  /* スタックが二つ以上、つまれていないと意味がない */
  if( dirstack==NULL  ||  dirstack->prev == NULL )
    return;
  
  Dirstack *one=dirstack;
  dirstack = dirstack->prev;
  
  Dirstack *cur=dirstack;
  while( cur->prev != NULL )
    cur = cur->prev;
  
  cur->prev = one;
  one->prev = NULL;
}

/* n 番目のディレクトリスタックへ移動する。
 * なお、それまでのカレントディレクトリ名は prevdir に保存される。
 * in	n  カレントディレクトリを0とした場合の番目。0 だと何もしない。
 * out	error_dir  移動できなかった時に(-3)、そのディレクトリ名が入る。
 *		   省略するか、NULL だと入らない。
 * return
 *	 0: 正常終了
 *	-1: n が大きすぎる ,
 *	-2: カレントディレクトリ名を取得できない。
 *	-3: n番目のディレクトリへ移動できない。
 */
static int chdir_to_nth_stack(int n,char *error_dir=NULL)
{
  if( n==0 )
    return 0;
  
  char cwd[ FILENAME_MAX ];
  if( getcwd_case( cwd ) == NULL )
    return -2;
  
  Dirstack *cur=dirstack;
  for(int i=1;;i++){
    if( cur == NULL )
      return -1;
    if( i >= n )
      break;
    cur = cur->prev;
  }
  
  if( _chdir2( cur->buffer ) != 0 ){
    if( error_dir != NULL )
      strcpy( error_dir , cur->buffer );
    return -3;
  }
  
  strcpy( prevdir , cwd );
  return 0;
}

/* Dirstack のノードを作成する。スタックには、まだ積まない。
 * in	pwd ディレクトリ名。省略するとカレントディレクトリ名となる。
 */
static Dirstack *getcwd_as_dirstack_node(const char *pwd=NULL)
{
  char curdir[ FILENAME_MAX ];
  if( pwd == NULL ){
    if( getcwd_case( curdir ) == NULL )
      return NULL;
    pwd = curdir;
  }
  
  Dirstack *tmp=(Dirstack*)malloc(sizeof(Dirstack)+strlen(pwd));
  if( tmp == NULL )
    return NULL;
  
  strcpy( tmp->buffer , pwd );
  tmp->prev = NULL;
  
  return tmp;
}

/* ディレクトリをスタック末尾に入れる 
 * in	pwd ディレクトリ名。省略すると、カレントディレクトリ名となる。
 */
static void append_stack_tail(const char *pwd=NULL)
{
  Dirstack *tmp=getcwd_as_dirstack_node(pwd);
  if( tmp == NULL )
    return;
  
  Dirstack *cur=dirstack;
  if( cur == NULL ){
    dirstack = tmp;
  }else{
    while( cur->prev != NULL )
      cur = cur->prev;
    cur->prev = tmp;
  }
}

/* ディレクトリスタックのトップを捨てるだけ
 * return 0:正常終了 -1:ディレクトリスタックは空
 */
static int drop_stacktop()
{
  Dirstack *tmp=dirstack;
  if( tmp == NULL )
    return -1;
  dirstack = dirstack->prev;
  free(tmp);
  return 0;
}

/* 画面にメッセージを出さない popd 
 * in	nth スタックn番目をpopd する。
 * return 0:正常終了 , -1:スタックが空 , -2:ディレクトリがもはや存在しない
 */
static int simple_popd(int nth=0)
{
  char wd[FILENAME_MAX];
  getcwd_case(wd);
  
  if( dirstack == NULL )
    return -1;
  
  if( nth == 0 ){
    if( _chdir2(dirstack->buffer) != 0 )
      return -2;
    drop_stacktop();
  }else{
    Dirstack *pre=dirstack , *cur;
    for(int i=0;;){
      if( (cur = pre->prev) == NULL )
	return -1;
      if( ++i >= nth-1 )
	break;
      pre = cur;
    }
    if( _chdir2(cur->buffer) != 0 )
      return -2;
    pre->prev = cur->prev;
    free(cur);
  }
  strcpy(prevdir,wd);
  return 0;
}


/* 内蔵コマンド pushd
 * in	srcfil コマンドの入っていたストリーム
 *	params パラメータ
 */
int cmd_pushd( FILE *srcfil , Parse &params)
{
  char cwd[FILENAME_MAX];
  getcwd_case(cwd);

  int smart_chdir_flag=0;
  int flag=0;
  int target=-1;
  for(int i=1; i < params.get_argc() ; i++ ){
    if( params[i][0] == '-' ){
      // オプションの長さが 1 の時：「-」のみの引数の時
      if( params[i].len == 1 ){
	smart_chdir_flag |= BIT_CD_LAST;
	target = i;
      }else{
	for(int j=1; j<params[i].len ; j++ ){
	  switch( params[i][j] ){
	  case 'v':
	    flag |= 1;
	    break;
	  }
	}
      }
    }else{
      target = i;
    }
  }

  // 「pushd↓」：スタックトップとカレントディレクトリを入れ換える
  if( target == -1 ){
    if( dirstack == NULL ){
      fputs("pushd: No other directory.",stderr);
      return 0;
    }
    
    if( _chdir2(dirstack->buffer) != 0){
      fprintf(stderr,"%s: Specified directory in the stack is not found.\n"
	      , dirstack->buffer );
      return 0;
    }
    Dirstack *tmp=dirstack;
    strcpy(prevdir,cwd);
    dirstack = dirstack->prev;
    free(tmp);
  }else if( params[target][0]=='+' ){
    /* 「pushd +2」
     * 0 1 2 3 4
     *    ↓
     * 2 3 4 0 1
     */
    
    int n=atoi(params[target].ptr+1);
    if( n <= 0 ){
      fprintf(stderr,"+%s: No such file or directory.\n",n);
      return 0;
    }

    /* 前もって、先に n 番目のディレクトリに移動しておく。
     * なぜなら、そうしないと、エラーになった際に、スタック構造を元に戻すのが
     * たいへんだから... */

    char errdir[ FILENAME_MAX ];
    switch( chdir_to_nth_stack(n,errdir) ){
    case -3:
      fprintf(stderr,"%s : Specified directory in the stack is not found.\n"
	      , errdir );
      return 0;
    case -2:
      fputs("pushd: can not get current directory.\n",stderr);
      return 0;
    case -1:
      fputs("+%d: more than the number of directory stack.\n",stderr);
      return 0;
    }
    strcpy( prevdir , cwd );
    
    /* ディレクトリは移動したので、あとは、ディレクトリスタックを
     * 回転させるだけ */
    append_stack_tail( cwd );
    for(int i=1 ; i<n ; i++ )
      move_stacktop_to_stacktail();
    drop_stacktop();
    return simple_dirs(params,flag);

  }else{
    if( smart_chdir(srcfil,params,smart_chdir_flag) )
      return 0;
    strcpy(prevdir,cwd);
  }
  
  Dirstack *tmp=(Dirstack*)malloc(sizeof(Dirstack)+strlen(cwd));
  tmp->prev = dirstack;
  strcpy( tmp->buffer , cwd );
  dirstack = tmp;
  
  return simple_dirs(params,flag);
}

/* 内蔵コマンド popd
 * in	srcfil コマンドの入っていたストリーム
 * 	param パラメータオブジェクト
 */
int cmd_popd( FILE *srcfil, Parse &params)
{
  int nth=0;
  int flag=0;
  for( int i=1 ; i<params.get_argc() ; i++ ){
    if( params[i][0] == '-' ){
      for( int j=1; j<params[i].len ; j++ ){
	switch( params[i][j] ){
	case 'v':
	  flag |= 1;
	  break;
	}
      }
    }else if( params[i][0] == '+' ){
      nth = atoi( params[i].ptr+1 );
    }
  }

  switch( simple_popd(nth) ){
  case -2:
    fprintf(stderr,"%s : Specified directory in the stack is not found.\n"
	    , dirstack->buffer);
    break;
    
  case -1:
    fputs("popd : directory stack is empty!\n",stderr);
    break;
    
  case 0:
    return simple_dirs(params,flag);
    
  default:
    fputs("popd : unknown error\n",stderr);
    break;
  }
  return 0;
}
