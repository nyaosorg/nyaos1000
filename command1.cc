#include <stdlib.h>
#include <sys/ea.h>
// #include <sys/video.h>
#include <ctype.h>
#include <process.h>

#include "edlin.h"
#include "parse.h"
#include "complete.h"
#include "nyaos.h"
#include "errmsg.h"

extern int option_tilda_without_root;
extern int option_direct_key;
extern int option_complete_etc;
extern int option_single_quote;
extern int option_cd_goto_home;
extern int option_debug_echo;
extern int option_history_in_doublequote;
extern int option_amp_start;
extern int option_amp_detach;
extern int option_esc_key_sequences;
extern int option_tilda_is_home;
extern int option_tcshlike_history;
extern int option_dots;
extern int option_script_cache;
extern int option_backquote;
extern int option_backquote_in_quote;
extern int option_ignore_cases;
extern int option_auto_close;
extern int option_honest;
extern int printexitvalue;
extern int option_icanna;
extern int option_noclobber;

int echoflag=0;

int cmd_mode( FILE *source , Parse &args )
{
  if(   args.get_argc() >= 2
     && to_upper(args[1].ptr[0])=='C'
     && to_upper(args[1].ptr[1])=='O' ){
    char *p;
    screen_width  = strtol(args[1].ptr+2,&p,0);
    if( screen_width < 10 && screen_width > 300 ){
      screen_width = 80;
    }else{
      static char buffer[20];
      sprintf( buffer , "COLUMNS=%d" , screen_width );
      putenv( buffer );
      puts( buffer );
    }
    if( p != NULL  &&  *p == ',' && isdigit(*++p & 255 ) ){
      screen_height = atoi(p);
      if( screen_height < 10 & screen_height > 300 ){
	screen_height = 25;
      }else{
	static char buffer[20];
	sprintf( buffer , "LINES=%d" , screen_height );
	putenv( buffer );
	puts( buffer );
      }
    }
  }
  char buffer[ 1024 ];
  args.copyall(0,buffer);
  spawnl(P_WAIT,cmdexe_path,"CMD","/C",buffer,NULL);
#if 0
  if( option_vio_cursor_control ){
    v_getctype( &cursor_start , &cursor_end );
  }
#endif
  return 0;
}

int cmd_exec( FILE *source , Parse &params )
{
  int argc=params.get_argc();
  if( argc < 2 ){
    FILE *fout=params.open_stdout();
    fputs("exec: exec <command-name>\n",fout);
    return 0;
  }

  char **argv = (char**)alloca( sizeof(char*)*argc-- );
  
  for(int i=0;i<argc;i++){
    int len=params.get_length(i+1);
    
    argv[i] = (char*)alloca(len+1);
    params.copy(i+1,argv[i]);
  }
  argv[argc] = NULL;
  execvp(argv[0],argv);
  fprintf(stderr,"nyaos: exec: %s: bad commandname.\n", argv[0] );
  return 0;
}

static void cut_tail_root(char *p)
{
  char *q=NULL;
  while( *p != '\0' ){
    q=p;
    if( is_kanji(*p) )
      p++;
    p++;
  }
  if( q != NULL && (*q=='\\' || *q=='/') )
    *q = '\0';
}

int cmd_rmdir( FILE *source, Parse &params )
{
  int argc=params.get_argc();
  for(int i=1;i<argc;i++){
    char dirname[FILENAME_MAX];
    params.copy(i,dirname);
    cut_tail_root(dirname);
    if( rmdir(dirname) != 0 ){
      ErrMsg::say( ErrMsg::CantRemoveFile , "nyaos" , "rmdir" , dirname , 0 );
      return 1;
    }
  }
  return 0;
}

int cmd_mkdir ( FILE *source , Parse &params)
{
  int argc=params.get_argc();
  for(int i=1;i<argc;i++){
    char dirname[FILENAME_MAX];
    params.copy(i,dirname);
    cut_tail_root(dirname);
    if( mkdir(dirname,0777) != 0 ){
      ErrMsg::say(ErrMsg::CantMakeDir,dirname,0);
      return 1;
    }
  }
  return 0;
}

#if 0
union MultiPtr {
  void *value;
  unsigned short *word;
  char  *byte;
};

/* ASCII タイプの拡張属性を設定する。
 *	fname	拡張属性を設定するファイルのファイル名
 *	eatype	拡張属性のタイプ(".SUBJECT"等)
 *	value	設定する内容。NULL の場合、その属性を削除する。
 */
int set_asciitype_ea(  const char *fname , const char *eatype 
		     , const char *value )
{
  struct _ea eavalue;

  eavalue.flags = 0;
  if( value == 0 || value[0] == '\0' ){
    /* EA の値を消す */
    eavalue.size = 0;
    eavalue.value = "";
  }else{
    MultiPtr ptr;

    int len = strlen(value);
    ptr.value = eavalue.value = alloca( (eavalue.size = len+4)+1 );
    /* 4 はヘッダのバイト数 */
    
    *ptr.word++ = 0xFFFD;
    *ptr.word++ = len;
    memcpy(ptr.value , value , len );
  }
  return _ea_put( &eavalue , fname , 0 , eatype );
}

/* コマンド subject : ファイルに .SUBJECT属性を設定する。
 */
int cmd_subject(FILE *source, Parse &params)
{
  if( params.get_argc() < 2 ){
    fputs("subject filename subject...\n",stderr);
    return 1;
  }
  /* ファイル名を ASCIZ 文字列にする。*/
  char *fname=(char*)alloca(params.get_length(1)+1);
  params.copy(1,fname,Parse::REPLACE_SLASH);
  
  /* 設定するサブジェクト値を ASCIZ 文字列にする */
  char *subject=(char*)alloca(params.get_length_later(2)+1);
  params.copyall(2,subject,Parse::QUOTE_NOT_COPY);

  int rc=set_asciitype_ea(fname,".SUBJECT",subject);
  if( rc== 0 ){
    printf("%s --> %s\n" , fname , subject );
  }else{
    ErrMsg::say(ErrMsg::CantWriteSubject,fname,0);
  }
  return rc; 
}

int cmd_comment(FILE *source, Parse &params)
{
  if( params.get_argc() < 2 ){
    fprintf(stderr,"comment filename comment...\n");
    return 1;
  }

  char *fname=(char*)alloca(params.get_length(1)+1);
  params.copy(1,fname,Parse::REPLACE_SLASH);

  struct _ea eavalue;

  eavalue.flags = 0;
  if( params.get_argc() == 2 ){
    eavalue.size = 0;
    eavalue.value = "";
  }else{
    int size=params.get_length_later(2);

    MultiPtr ptr;
    ptr.value = eavalue.value = alloca( (eavalue.size = size+10)+1 );
    /* 10 はヘッダのバイト数 */

    *ptr.word++ = 0xFFDF;
    *ptr.word++ = 0; /* 932: コードページ */
    *ptr.word++ = 1; /* EA の数 */
    *ptr.word++ = 0xFFFD;
    *ptr.word++ = (unsigned short)
      ( params.copyall( 2 , ptr.byte+2 , Parse::QUOTE_NOT_COPY ) - (ptr.byte+2) );
    printf("%s --> %s\n",fname,ptr.byte);
  }
  
  int rc=_ea_put( &eavalue , fname , 0 , ".COMMENTS" );
  if( rc != 0 ){
    ErrMsg::say(ErrMsg::CantWriteComments,fname,0);
  }
  return rc;
}
#endif

/* オプション一覧：
 *	option 文で使用するオプションを増す時は、option結果を代入する変数
 *	を宣言して、ここで登録しさえすればよい。
 */
struct Option{
  const char *name;	/* オプションの名前 */
  int *pointor;		/* オプションの結果を入れる変数を差す */
  int true_value;	/* option +XXX の時に変数に代入する値 */
  int false_value;	/* option -XXX の時に変数に代入する値 */
} optlist[]={
  { "amp_detach"	   , &option_amp_detach		       , 1  , 0 },
  { "amp_start"            , &option_amp_start                 , 1  , 0 },
  { "anywhere_history"     , &option_tcshlike_history          , 1  , 0 },
  { "auto_close"	   , &option_auto_close                , 1  , 0 },
  { "backquote"            , &option_backquote                 , 1  , 0 },
  { "backquote_in_quote"   , &option_backquote_in_quote        , 1  , 0 },
  { "beep"                 , &Shell::beep_ok                   , 1  , 0 },
  { "cd_goto_home"         , &option_cd_goto_home              , 1  , 0 },
  { "cmdlike_crlf"         , &option_cmdlike_crlf              , 1  , 0 },
  { "complete_etc"         , &option_complete_etc              , 1  , 0 },
  { "complete_hidden"      , &Complete::complete_hidden_file   , 1  , 0 },
  { "complete_tail_slash"  , &Edlin::complete_tail_char        ,'/','\\'}, 
  { "complete_tilda"       , &Complete::complete_tail_tilda    , 1  , 0 },
  { "ctrl_d_eof"           , &Shell::ctrl_d_eof                , 1  , 0 },
  { "debug"                , &option_debug_echo                , 1  , 0 },
  { "direct_key"           , &option_direct_key		       , 1  , 0 },
  { "dots"                 , &option_dots                      , 1  , 0 },
  { "echo"                 , &echoflag                         , 1  , 0 },
  { "esc_key_sequences"	   , &option_esc_key_sequences	       , 1  , 0 },
  { "history_in_doublequote" , &option_history_in_doublequote  , 1  , 0 },
  { "honest"               , &option_honest                    , 1  , 0 },
  { "icanna"		   , &option_icanna		       , 1  , 0 },
  { "ignore_cases"         , &option_ignore_cases              , 1  , 0 },
  { "noclobber"		   , &option_noclobber		       , 1  , 0 },
  { "printexitvalue"       , &printexitvalue                   , 1  , 0 },
  { "prompt_even_piped"    , &option_prompt_even_piped         , 1  , 0 },
  { "script"               , &scriptflag                       , 1  , 0 },
  { "script_cache"         , &option_script_cache              , 1  , 0 },
  { "semicolon"            , &Parse::option_semicolon_terminate, 1  , 0 },
  { "single_quote"         , &option_single_quote              , 1  , 0 },
  { "slash_to_backslash_after_tilda"
      , &option_replace_slash_to_backslash_after_tilda , 1 , 0 },
  { "sos"                  , &option_sos                       , 1  , 0 },
  { "tilda_home"           , &option_tilda_is_home             , 1  , 0 },
  { "tilda_without_root"   , &option_tilda_without_root        , 1  , 0 },
  { "vio"                  , &option_vio_cursor_control        , 1  , 0 },
  { NULL , NULL , 1 , 0 },
};

/* option コマンドのオプションをセットする。
 *	name ... オプションの名称
 *	flag ... Onならば非0 , Offならば 0
 * return 0 ... 成功   !0 ... オプションは存在しない
 */
int set_option(const char *name,int flag)
{
  for( const Option *p=optlist ; p->name != NULL ; p++ ){
    if( stricmp(p->name,name)==0 ){
      *p->pointor = ( flag ? p->true_value : p->false_value );
      return 0;
    }
  }
  return -1;
}

int cmd_option(FILE *source, Parse &params)
{
  FILE *fout=params.open_stdout();

  if( params.get_argc() < 2 ){
    for(const Option *p=optlist ; p->name != NULL ; p++ )
      fprintf(fout,"%c%s\n", *p->pointor == p->true_value ?'+':'-',p->name );
    return 0;
  }
  
  for(int j=1;j<params.get_argc() ; j++ ){
    char *name=(char*)alloca( params.get_length(j)+1 );
    params.copy(j,name);
    bool value=true;

    if( name[0] == '-' ){
      value = false;
      name++;
    }else if( name[0] =='+' ){
      name++;
    }
    if( set_option(name,value) != 0 ){
      fprintf(fout,"%s : no such option.\n",name);
      return 1;
    }
  }
  return 0;
}
