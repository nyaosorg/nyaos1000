#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "complete.h"
#include "macros.h"
#include "finds.h"
#include "nyaos.h"
#include "strtok.h"

#ifndef S2NYAOS
#  define SHARED_CACHE  /* ← コマンド名キャッシュを共有する */
#endif

extern int option_tilda_without_root;

int Complete::directory_split_char='\\';
int Complete::complete_tail_tilda=0;
int Complete::complete_hidden_file=0;

inline int stricmpfast( const char *s1 , const char *s2 )
{
  int c1=tolower(*s1 & 255) & 255;
  int c2=tolower(*s2 & 255) & 255;
  return (c1==c2) ? stricmp(s1,s2) : (c1-c2) ;
}

/* 最初の候補の真の名前(大文字・小文字が正しい)を得る
 * 候補が一つも無い場合のみ、NULL を返す。
 */
const char *Complete::get_real_name1() const
{
  FileListT *p=get_top();
  if( p != NULL ){
    FileListT *q=p->next;
    while( q != NULL ){
      if(     q->length < p->length 
	 || ( q->length == p->length && (q->attr & A_DIR) != 0 ) ){
	p=q;
      }
      q = q->next;
    }
  }
  return p->name;
}

/* 与えられたファイル名が、続く拡張子のいずれかにマッチするかを判定する関数
 *	path	対象のファイル名
 *	"..."	拡張子リスト(可変長引数)。末尾を NULL にする。
 * return マッチした拡張子の番目。マッチしていなければ 0 。
 */
int which_suffix(const char *path,...)
{
  /* 拡張子を取得 */
  const char *ext=_getext(path);
  if( ext == NULL )
    return 0;
  
  if( *ext == '.' )
    ++ext; /* ピリオドをスキップ */

  /* 拡張子を発見、以下比較 */
  const char *q;
  va_list varptr;
  va_start(varptr,path);

  for(int i=1; (q=va_arg(varptr,const char *)) != NULL ; i++ ){
    if( stricmp(ext,q) == 0 ){
      va_end(varptr);
      return i;
    }
  }
  va_end(varptr);
  return 0;
}

/* パスを〈ドライブ＋ディレクトリ〉 と 〈ファイル名〉 に分ける。
 * in	path	オリジナルのパス
 *	dir	ドライブ＋ディレクトリ
 * out	fname	ファイル名
 * return 最後のパス区切文字('/','\\',or'\0')
 */
static int pathsplit( const char *path, char *dir, char *fname )
{
  const char *lastroot=NULL;
  if( path[0]=='~' ){
    lastroot = path;
  }
  for(const char *p=path ; *p != '\0' ; p++ ){
    if( is_kanji(*p) ){
      ++p;
    }else if( *p=='\\' || *p=='/' || *p==':' ){
      lastroot = p;
    }
  }
  const char *p=path;

  if( lastroot != NULL ){
    if( *p == '~' 
       && (option_tilda_without_root || *(p+1)=='\\' || *(p+1)=='/' ) ){
      
      if( *(p+1) == ':' ){
	const char *system_ini=getShellEnv("SYSTEM_INI");
	if( system_ini != NULL ){
	  *dir++ = *system_ini;
	}else{
	  *dir++ = '~';
	}
	++p;
      }else{
	const char *home=getShellEnv("HOME");
	if( home != NULL ){
	  while( *home != '\0' )
	    *dir++ = *home++;

	  if( *++p != '/' && *p != '\\' ){
	    *dir++ = '\\';
	    *dir++ = '.';
	    *dir++ = '.';
	    *dir++ = '\\';
	  }

	}else{
	  *dir++ = '~';
	  ++p;
	}
      }
    }else if( *p=='.' && *(p+1)=='.' && *(p+2)=='.' ){
      *dir++ = *p++;
      *dir++ = *p++;
      while( *p == '.' ){
	*dir++ = '\\';
	*dir++ = '.';
	*dir++ = '.';
	p++;
      }
    }
    while( p <= lastroot )
      *dir++ = *p++;
  }
  /* '.'を付けることで 末尾が ':','/'でも有効に働く (^_^) */
  *dir++ = '.';
  *dir   = '\0';
  
  if( *p != '\0' ){
    do{
      *fname++ = *p++;
    }while( *p != '\0' );
  }
  *fname = '\0';

  return (lastroot != NULL ? *lastroot : '\0');
}

/* 絶対パスやカレントディレクトリのファイル名をインスタンスに読み込むメソッド。
 * in	command_complete !0ならば、実行可能ファイル名のみ読み込む
 *	is_with_dir !0ならば、ディレクトリ名も読み込む
 * return 読み込んだファイル名の数
 */
int Complete::makelist_core(int command_complete, int is_with_dir)
{
  common_length = strlen(fname);
  
  for(Dir dir(directory) ; dir != NULL ; ++dir ){
    
    /* 「.」と「..」を除く */
    if( dir[0]=='.' && ( dir[1]=='.' || dir[1]=='\0' ) )
      continue;
    
    if( common_length == 0
       || ( dir.get_name_length() >= common_length
	   && strnicmp( fname , dir.get_name() , common_length ) == 0 
	   ) ){
      
      /* コマンド名補完の場合、拡張子が、EXE,CMD,BAT,COM以外は除く。
       * (スクリプト名は、コマンド名補完モ－ドで実行していない)
       */
      if( command_complete ){
	if(  dir.is_dir()
	   ? is_with_dir == 0
	   : which_suffix(dir.get_name(),"EXE","CMD","BAT","COM","CLASS",0)==0
	   ) 
	  continue;
      }
      
      /* HIDDEN属性を除く */
      if( dir.is_hidden()  &&  complete_hidden_file == 0 )
	continue;

      /* 名前の末尾がチルダのファイルを除く */
      if( dir.ends_with('~')  && complete_tail_tilda==0 )
	continue;
      
      if( dir.get_name_length() > max_length )
	max_length = dir.get_name_length();
      
      insert( new_filelist(dir) );
    }
  }
  return get_num();
}

inline bool strequ(const char *x,const char *y)
{
  return *x==*y  &&  strcmp(x,y)==0;
}

/* 重複ファイルを削除するメソッド。sort の後に実行しないと意味なし。
 */
void Complete::unique()
{
  FileListT *cur=get_top();
  if( cur == NULL )
    return;

  while( cur->next != NULL ){
    /* if ではなく、while にしているのは、2連続だけでなく、
     * 3連続して、重複があるかもしれないから
     */
    while( strequ(cur->name+common_length,cur->next->name+common_length) ){
      FileListT *nxt=cur->next->next;
      free( cur->next );
      cur->next = nxt;
      if( nxt == NULL )
	return;
      nxt->prev = cur;
    }
    cur = cur->next;
  }
}


/* ファイル名補完を行うの為の、ファイル名リストを作成するメソッド。
 * in	path 不完全なファイル名
 * return 候補となるファイルの数
 */
int Complete::makelist(const char *path)
{
  status = FILENAME_COMPLETED;
  max_length=0;
  clear();
  
  typed_split_char = pathsplit( path , directory , fname );
  if (typed_split_char == ':' )
     typed_split_char = 0;

  int rc=makelist_core(false,true);
  this->sort();
  this->unique();
  return rc;
}

/* 1KB 単位で malloc して、malloc用ヘッダ分のメモリを節約するクラス。
 *	sbrk.alloc(サイズ)
 * で任意サイズのメモリを確保できるが、解放は、Sbrkのインスタンス単位
 * でしか行えない＝ clear メソッドか、デストラクタのみが行える。
 */
class Sbrk{
  struct Block{
    Block *next;
    char buffer[1024];
  }*block;
  unsigned left,n;
public:
  Sbrk() : block(0) , left(0) , n(0) { }
  void *alloc(unsigned size) throw(MallocError);
  void clear();

  unsigned int queryBytes() const { return n*sizeof(Block); }
  ~Sbrk() { clear(); }
};

void *Sbrk::alloc(unsigned size) throw(MallocError)
{
  if( block == 0 || left <= size ){
    Block *neo=(Block*)malloc(sizeof(Block));
    if( neo == NULL )
      throw MallocError();
    neo->next = block;
    left = sizeof(neo->buffer);
    block = neo;
    ++n;
  }
  return &block->buffer[ left -= size ];
}

void Sbrk::clear()
{
  while( block != NULL ){
    Block *nxt=block->next;
    free(block);
    block=nxt;
  }
}

#ifndef SHARED_CACHE

/* PathCache は Complete ヘ順にコピーされるだけだから
 * 双方向リストである必要は無い
 */

class PathCache {
public:
  struct Node{
    Node *next;
    unsigned short length;
    char name[1];
  };
private:
  int nfiles;
  Sbrk sbrk;
  Node *top;
  Node *newNode(int len) throw (MallocError)
    { return (Node*)sbrk.alloc(sizeof(Node)+len); }
public:
  void clear();
  void insert( const char *name ) throw(MallocError);
  Node *get_top(){ return top; }

  class Cursor {
    Node *cur;
  public:
    Cursor(PathCache &pc) : cur(pc.get_top()) { }
    Node *operator->(){ return cur; }
    Node *operator*(){ return cur; }
    void operator++(){ if( cur ) cur = cur->next; }
    operator const void*() const { return cur != NULL ? this : NULL; }
    bool operator ! () const { return cur == NULL; }
  };
  PathCache() :  nfiles(0) , top(NULL)  { }
  unsigned queryBytes() const { return sbrk.queryBytes(); }
  unsigned queryFiles() const { return nfiles; }
};

void PathCache::clear()
{
  sbrk.clear();
  top = 0;
  nfiles = 0;
}


void PathCache::insert( const char *name ) throw(MallocError)
{
  int len=strlen(name);
  Node *node=newNode(len);
  strcpy( node->name , name );
  node->length = len;

  if( top == NULL  ||  stricmpfast(node->name,top->name) < 0 ){
    node->next = top;
    top = node;
  }else{
    Node *pre=top;
    Node *cur=pre->next;
    while( cur != NULL ){
      int diff=stricmpfast(node->name,cur->name);
      if( diff == 0 ){
	goto exit;
      }else if( diff < 0 ){
	pre->next = node;
	node->next = cur;
	goto exit;
	break;
      }
      pre = cur;
      cur = cur->next;
    }
    pre->next = node;
    node->next = NULL;
  }
 exit:
  ++nfiles;
}

/* コマンド名補完の為のキャッシュっす。
 * 本来は、静的メンバ変数にでもすべきところだが、
 * あまり、ほいほい、ヘッダファイルに宣言するのも
 * ヘッダファイルが太り過ぎてやなので、
 * 敢えて、ファイルスコープに陥れるんぢゃよ、ぎゃわ～。
 */
static PathCache path_cache;

#else

#include "shared.h"
typedef PathCacheShared PathCache;
static PathCacheShared path_cache;

#endif

unsigned Complete::queryBytes()
{
#ifdef SHARED_CACHE
  try{
#endif
    return path_cache.queryBytes();
#ifdef SHARED_CACHE
  }catch( SharedMem::SemError ){
    return ~0u;
  }
#endif
}
unsigned Complete::queryFiles()
{
#ifdef SHARED_CACHE
  try{
#endif
    return path_cache.queryFiles();
#ifdef SHARED_CACHE
  }catch( SharedMem::SemError ){
    return ~0u;
  }
#endif  
}

/* コマンド名補完の為に、PATH,SCRIPTPATH 上のコマンド名を
 * グローバル変数 path_cache に設定する。
 */
void Complete::make_command_cache() throw(MallocError)
{
  path_cache.clear();

  // 環境変数 PATH 上のコマンドの登録
  const char *envpath=getShellEnv("PATH");
  if( envpath != NULL ){
    char *env=(char*)alloca(strlen(envpath)+1);
    strcpy(env,envpath);

    for(  const char *dirname=strtok(env,";")
	; dirname != NULL
	; dirname = strtok(NULL,";") ){

      for( Dir dir(dirname); dir ; dir++ ){
	if(   which_suffix(dir.get_name(),"EXE","CMD","COM",NULL) != 0
	   && !dir.ends_with('~') && ! dir.is_dir()  && !dir.is_hidden()){
	  
	  path_cache.insert( dir.get_name() );
	}
      }
    }
  }
  /* 環境変数 SCRIPTPATH 上のコマンドの登録
   * こんなコード書いている時点で、NYAOS 専用になってしまうのだな
   */
  envpath=getShellEnv("SCRIPTPATH");
  if( envpath != NULL ){
    char *env=(char*)alloca(strlen(envpath)+1);
    strcpy(env,envpath);
    
    for(  const char *dirname=strtok(env,";")
	; dirname != NULL
	; dirname = strtok(NULL,";") ){
      
      for( Dir dir(dirname); dir ; dir++ ){
	// 末尾がチルダのファイル、隠しファイル以外のファイルは
	// 全て登録する。
	if( !dir.ends_with('~') && !dir.is_dir() && !dir.is_hidden() )
	  path_cache.insert( dir.get_name() );
      }
    }
  }
  /* 環境変数 CLASSPATH 上の Javaアプリケーションの登録
   * これでは ZIP/JAR ファイルの中などは実行できないが、
   * そこまでする必要もないであろう。
   */
  envpath=getShellEnv("CLASSPATH");
  if( envpath != NULL ){
    char *env=(char*)alloca(strlen(envpath)+1);
    strcpy(env,envpath);
    for(  const char *dirname=strtok(env,";")
	; dirname != NULL
	; dirname = strtok(NULL,";") ){
      for( Dir dir(dirname) ; dir ; dir++ ){
	if(   which_suffix(dir.get_name(),"CLASS",0) != 0
	   && !dir.ends_with('~') && !dir.is_dir() && !dir.is_hidden() )

	  path_cache.insert( dir.get_name() );
      }
    }
  }
}

/* ワイルドカードを含んだパス名のマッチリストを作成するメソッド
 * in	path ワイルドカードを含んだパス名
 * return 候補となるファイル名の数
 */
int Complete::makelist_with_wildcard(const char *path)
{
  int nfiles=0;
  
  Dir dir;
  for(dir.findfirst_with_wildcard(path) ; dir ; ++dir ){
    this->insert( new_filelist(dir) );
    if( dir.get_name_length() > max_length )
      max_length = dir.get_name_length();
    nfiles++;
  }
  return nfiles;
}


/* コマンド名補完を行う為の、コマンド名リストを作成するメソッド
 * in	path 不完全なコマンド名
 * return 候補となるコマンドの数
 */
int Complete::makelist_with_path(const char *path)
{
  status = COMMAND_COMPLETED;
  max_length=0;
  this->clear();

  typed_split_char = pathsplit( path , directory , fname );
  if( typed_split_char == ':' )
    typed_split_char = 0;

  const char *p=path;
  while( *p != '\0'){
    if( is_kanji(*p) ){
      p++;
    }else if( *p==':' || *p=='/' || *p=='\\'){
      /* フルパスで記述されている場合、
       * PATHを検索するのは無意味なので、打ちきる
       */
      int rc=makelist_core(true,true);
      return rc;
    }
    p++;
  }

  /* ASSERT : path には、ディレクトリ名が含まれていない。*/
  strcpy( fname , path );

  if( path_cache.queryFiles() <= 0 ){
    try{
      make_command_cache();
    }catch(MallocError){
      ;
    }
  }
  
  common_length=strlen(fname);
  
#ifdef SHARED_CACHE
  try{
#endif
    for(PathCache::Cursor cur(path_cache) ; cur ; ++cur ){
      if(    cur->length >= common_length
	 &&  strnicmp(fname,cur->name,common_length ) == 0 ){
	
	FileListT *tmp=(FileListT *)malloc(sizeof(FileListT)+cur->length);
	if( tmp != 0 ){
	  strcpy(tmp->name,cur->name);
	  tmp->length = cur->length;
	  tmp->attr = tmp->size = tmp->easize = 0;
	  insert( tmp );
	  
	  if( cur->length > max_length )
	    max_length = cur->length;
	}
      }
    }
#ifdef SHARED_CACHE
  }catch( SharedMem::SemError e){
    fprintf(stderr,
	    "\n\a<<< NYAOS Internal Error >>>"
	    "\nCannot lock the shared memory for command-name completion.\n"
	    "\nerror code = %d\n"
	    , e.errcode );
  }
#endif

  for(Dir dir(".") ; dir != NULL ; ++dir ){
    if(   dir.get_name_length() >= common_length
       && strnicmp( fname , dir.get_name() ,common_length )==0
       && ( dir.is_dir()
	   || which_suffix(dir.get_name(),"EXE","CMD","COM","CLASS",0) !=0 )
       && !dir.ends_with('~')  &&  !dir.is_hidden()  ){
    
      insert( new_filelist(dir) );
      if( dir.get_name_length() > max_length )
	max_length = dir.get_name_length();
    }
  }
  status = SIMPLE_COMMAND_COMPLETED;
  this->sort();
  this->unique();
  return get_num();
}

int Complete::add_buildin_command(const char *name)
{
  int length=strlen(name);
  
  if(   length >= common_length
     && strnicmp( fname , name ,common_length )==0 ){
    
    struct filelist *tmp=
      (struct filelist *)malloc(sizeof(struct filelist)+length);
    
    if( tmp != NULL ){
      strcpy( tmp->name , name );
      tmp->length = length;
      tmp->attr = 0;
      tmp->size = 0;
      if( length > max_length )
	max_length = length;

      insert( tmp );
      return 0;
    }
  }
  return -1;
}

/* 補完すべき残りの文字列を得るメソッド。
 * return 文字列（固定staticバッファ）
 */
char *Complete::nextchar()
{
  static char buffer[FILENAME_MAX];

  if( get_num() <= 0 )
    return "\0";

  FileListT *p=get_top();
  
  if( p == NULL )
    return "\0";

  strcpy( buffer , get_real_name1() + common_length );

  /* ここの、get_real_name1() は、p でもよいのだが、
     表示時の大文字・小文字の統一をうんたらかんたら...*/
  
  for( ; p != NULL ; p=p->next ){
    const char *q = p->name+common_length ;
    char *r=buffer;
    
    while( *r != '\0' ){
      if( is_kanji(*r) ){
	/****** 倍角文字 ******/
	if( q[0] != r[0]  ||  q[1] != r[1] ){
	  *r = '\0';
	  break;
	}
	q += 2;
	r += 2;
      }else{
	/****** 半角文字 ******/
	if( to_upper(*q) != to_upper(*r) ){
	  *r = '\0';
	  break;
	}
	q++;
	r++;
      }
    }
  }
  return buffer;
}
