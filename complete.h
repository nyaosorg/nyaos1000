/* -*- c++ -*- */
#ifndef COMPLETE_H
#define COMPLETE_H

#include <sys/types.h>
#include <dirent.h>

#include "finds.h"

int which_suffix(const char *path,...);

/* Complete : 補完候補となるファイルのリストを列挙・記憶するクラス
 * クラス Files の派生クラス(って見りゃ分かるか)。
 *
 * // 使用例
 * Complete com;			// インスタンス作成
 *
 * com.makelist("c:/hoge");		// 補完リストの作成
 * printf("c:/hoge%s",com.nextchar());	// 補完すべき文字列
 *
 * // 候補対象の列挙
 * for( Complete::Cursor cur(com) ; cur.isOk() ; cur++ ){
 *	printf("%s ",cur->get_name() );
 * }
 *
 */

#ifndef MALLOC_ERROR
#define MALLOC_ERROR
class MallocError{ };
#endif

class Complete : public Files {
  char directory[ FILENAME_MAX ];	/* 原ファイル名のディレクトリ部 */
  char fname[FILENAME_MAX ];		/* 原ファイル名の非ディレクトリ部 */
  int common_length;			/* 候補の共通部分の長さ */
  int max_length;			/* 最も長い名前の候補の長さ */
  int typed_split_char ;		/* 入力されたパス分離文字(0,/ or \ )*/
  int makelist_core(int command_complete, int is_with_dir );
public:
  enum{
    NOT_COMPLETED ,
    COMMAND_COMPLETED ,
    FILENAME_COMPLETED ,
    SIMPLE_COMMAND_COMPLETED ,
    ERROR
  } status;

  Complete() : common_length(0) , typed_split_char(0) 
    , status(NOT_COMPLETED) {  }
  ~Complete(){ clear(); }

  int makelist          (const char *path);	/* 部分ファイル名の補完 */
  int makelist_with_path(const char *path);	/* 部分コマンド名の補完 */
  int makelist_with_wildcard(const char *path);	/* ワイルドカードの補完 */

  int add_buildin_command(const char *name); /* after makelist only */
  
  char *nextchar();
  void unique();
  
  /* リポート関数 */
  int get_max_name_length() const { return max_length; }
  int get_split_char(void) const { return typed_split_char; }
  const char *get_real_name1() const ;
  int get_fname_common_length() const { return common_length; }

  /* オプション(静的メンバ)変数 */
  static int directory_split_char;
  static int complete_tail_tilda;
  static int complete_hidden_file;
  
  /* コマンド名補完の為のキャッシュを作成/更新する */
  static void make_command_cache() throw(MallocError);
  static unsigned queryBytes(); /* … キャッシュのメモリ使用量を得る */
  static unsigned queryFiles(); /* … キャッシュにあるファイル名の数 */
  
  /* 操作子オブジェクト(イタレータとういほどのものではない!?)
   */
  class Cursor{
    Complete &list;
    FileListT *findptr;
  public:
    Cursor &operator++(){ findptr=findptr->next; return *this;}
    Cursor &operator--(){ findptr=findptr->prev; return *this;}
    Cursor &utimawari() /* 内回り(終り無し！) */
      { findptr=(  findptr->next != 0 ? findptr->next : list.get_top() ); 
	return *this; }
    Cursor &sotomawari() /* 外回り(終り無し！) */
      { findptr=(  findptr->prev != 0 ? findptr->prev : list.get_tail() );
	return *this;
      }
   
    bool isOk() const { return findptr != NULL; }
    FileListT *toFileListT(){ return findptr; }
    FileListT *operator->(){ return findptr; }

    Cursor( Complete &c ) : list(c) , findptr(c.get_top()) { }
    ~Cursor(){ }
  };
};

#endif
