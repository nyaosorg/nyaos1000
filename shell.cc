#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>

#include "hash.h"
#include "edlin.h"
#include "complete.h"
#include "nyaos.h"

/* option 命令と クラスEdlinから参照されるのみ */
int Shell::beep_ok=1;

/* 1.39 で追加した <WP_CONFIG> などを、補完対象に加えるオプション。
 * しかし、使ってみると、実際、使いにくくなるだけなので、デフォルトオフ。
 */
int option_complete_etc=0;

int Shell::complete_hook(Complete &com)
{
  int n=0;
  if( com.status == Complete::SIMPLE_COMMAND_COMPLETED ){
    /* build-in command */
    for( const Command *p=jumptable; p->name != NULL ; p++ ){
      if( com.add_buildin_command(p->name) == 0 )
	n++;
    }

    /* alias */
    extern Hash <Alias> alias_hash;

    for(HashIndex <Alias> hi(alias_hash) ; *hi != NULL ; hi++ ){
      if( com.add_buildin_command(hi->name) == 0 )
	n++;
    }
  }else if( option_complete_etc ){
    const char *workplace[]={
      "<WP_CONFIG>",	/* システム設定 */
      "<WP_DESKTOP>",	/* デスクトップ */
      "<WP_DRIVE>",	/* ドライブ設定 */
      "<WP_INFO>",	/* 情報 */
      "<WP_NOWHERE>",	/* その他 */
      "<WP_START>",	/* 始動 */
      "<WP_SYSTEM>",	/* システム */
      "<WP_TEMPS>",	/* テンプレート */
      NULL,
    };
    for(const char **p=workplace ; *p != NULL ; p++ ){
      if( com.add_buildin_command(*p) == 0 )
	n++;
    }
    struct Option{
      const char *name;
      int *pointor;
      int true_value;
      int false_value;
    } extern optlist[];
    
    for(const Option *p=optlist; p->name != NULL ; p++ ){
      if( com.add_buildin_command(p->name) == 0 )
	n++;
    }
  }
  return n;
}

/* プロンプトを再表示する */

void Shell::re_prompt()
{
  putchrs(prompt);
  int i=0;
  while( i<len )
    putnth( i++ );
  putbs( i-pos );
}


/* ^D や [TAB]^2 など、補完リストの表示を行うキーメソッド
 */
void Shell::complete_list()
{
  Complete com;

  int fntop=seek_word_top();
  int basesize=pos-fntop;
  int command_complete=(fntop <= 0) ;
  int has_a_wildcard_letter = 0;
  char *buffer=(char*)alloca(basesize+6);

  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
  }
  char *bp=buffer;
  while( fntop < pos ){
    if( strbuf[fntop] == '?' || strbuf[fntop] == '*' )
      has_a_wildcard_letter = 1;
    *bp++ = strbuf[fntop++];
  }
  *bp = '\0';
  
  int nfiles=(  command_complete
	      ? com.makelist_with_path( buffer ) 
	      : com.makelist( buffer )
	      );
  
  nfiles += complete_hook(com);
  
  if( nfiles <= 0 ){
    /* もし、マッチするファイル名が無くて、かつワイルドカード文字が使われ
     * ている場合は、そのワイルドカードにマッチするファイルを一覧する
     */
    if(    ! has_a_wildcard_letter 
       || (nfiles+=com.makelist_with_wildcard( buffer )) <= 0 )
      return;
  }

  com.sort();
  
  Complete::Cursor cur(com);
  putchr('\n');
  
  int scrnsize[2];
  get_scrsize(scrnsize);

  int files_per_line = 
    scrnsize[0]-1 < com.get_max_name_length()+2
      ? 1 : (scrnsize[0]-1)/(com.get_max_name_length()+2);
  int files_per_column = (nfiles+files_per_line-1)/files_per_line;

  struct filelist **ptr =
    (struct filelist**)alloca(files_per_line*sizeof(struct filelist *));

  for(int i=0 ; i<files_per_line; i++ )
    ptr[i] = NULL;
  
  for(int i=0; i<files_per_line-1 && cur.isOk() ; i++ ){
    ptr[i] = cur.toFileListT();
    for(int j=0 ; cur.isOk() && j<files_per_column ; j++){
      ++cur;
    }
  }
  ptr[files_per_line-1] = cur.toFileListT();
  
  for(int j=0; j<files_per_column ; j++ ){
    for(int i=0; i<files_per_line  &&  ptr[i] != NULL ; i++ ){
      int n=fprintf(fp,"%s%c ",
		    ptr[i]->name,
		    ptr[i]->attr & A_DIR 
		    ? Complete::directory_split_char : ' '
		    );
      if( (i+1) < files_per_line && ptr[i+1] != NULL )
	while( n++ < com.get_max_name_length()+2 )
	  putchr(' ');
      
      ptr[i] = ptr[i]->next;
    }
    putchr('\n');
  }
  re_prompt();
}

void Shell::cls()
{
  fprintf(  fp 
	  , topline_permission ? "\x1B[2J\x1B[H%s":"\x1B[2J\x1B[H\n%s"
	  , prompt );
  int i=0;
  while( i < len )
    putnth( i++ );
  putbs( i - pos );
}
