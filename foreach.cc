#include <io.h>
#include <ctype.h>

#include "finds.h"
#include "edlin.h"
#include "nyaos.h"
#include "parse.h"
#include "strbuffer.h"
#include "errmsg.h"
#include "prompt.h"

extern volatile int ctrl_c;

struct Line{
  Line *next;
  char buffer[1];
};

enum{
  OPTION_I = 1,
  OPTION_N = 2,
  OPTION_V = 4,
};
static unsigned option=0;

/* value を opt の内容に従って加工し、結果を line に流し込む。
 *	value 取り出されるベース
 *	opt 内容は次のものを受けつける。
 *		NULL等 全体
 *		h ディレクトリ部分
 *		t ディレクトリ部分以外
 *		r 拡張子部分以外
 *		e 拡張子部分
 * return
 *	 0 : 成功
 *	-1 : opt の内容が不適である。
 */
int word_design(StrBuffer &line ,const char *value,int option)
     throw(StrBuffer::MallocError)
{
  switch( option ){
  case 'e':
  case 'E': /* 「:e」拡張子のみ */
    line << _getext2(value);
    break;

  case 'r':
  case 'R': /* 「:r」拡張子除く */
    {
      const char *ext=_getext(value);
      if( ext != 0 ){
	line.paste( value , ext-value );
      }else{
	line << value;
      }
    }
    break;

  case 'h':
  case 'H': /* 「:h」ディレクトリのみ */
    {
      const char *name=_getname(value);
      if( name != 0 )
	line.paste( value , name-value );
    }
    break;

  case 't':
  case 'T':/* 「:t」ディレクトリ除く */
    line << _getname(value);
    break;

  default:
    return -1;
  }
  return 0;
}

static int word_design(StrBuffer &line ,const char *value,const char *opt)
     throw(StrBuffer::MallocError)
{
  if( opt==NULL  ||  opt[0] == '\0' ||  !isalpha(opt[0])  || opt[1] !='\0' )
    return -1;

  return word_design(line,value,opt[0]);
}

static int eachcmd(FILE *srcfil, const char *var, const char *str, Line *src )
     throw(StrBuffer::MallocError)
{
  int rv=0;
  
  /* シェル変数に、argv[1] の内容を設定する */
  const char *orgEnvValue=getShellEnv(var);
  char *orgEnvValueDup=(orgEnvValue ? strdup(orgEnvValue) : 0 );
  setShellEnv(var,str);

  /* 各命令毎にループ */
  for( ; src != NULL ; src=src->next ){
    StrBuffer line;
    const char *sp=src->buffer;
	
    while( *sp != '\0' ){
      if( *sp != '$' ){	/* 通常の文字 */
	line << *sp++;
      }else if( *(sp+1) == '$' ){ /* 「$$」は「$」へ変換 */
	line << '$';
	sp += 2;
      }else{
	/* $... は、環境変数、あるいは、パラメータへの置換 */

	StrBuffer word;	/* $(VAR:OPT) の VAR を保存する */
	StrBuffer opt;	/* $(VAR:OPT) の OPT を保存する */

	if( *++sp == '{' ){		/**** ${...} のケース ****/

	  ++sp; /*  '{'を読み飛ばし */
	  while( *sp != '}' ){
	    if( *sp == '\0' ){
	      ErrMsg::say( ErrMsg::Missing , "foreach" , "}" , 0 );
	      rv = -1;
	      goto exit;
	    }else if( *sp == ':' ){ /* ${VAR:OPT} の場合 */
	      ++sp;
	      while( *sp != '}' ){
		if( *sp == '\0' ){
		  ErrMsg::say( ErrMsg::Missing , "foreach" , "}" , 0 );
		  rv = -1;
		  goto exit;
		}
		opt << *sp++;
	      }
	      break;
	    }
	    word << *sp++;
	  }
	  ++sp; /* '}'を読み飛ばし */

	}else if( *sp == '(' ){		/**** $(...) のケース ****/
	  
	  ++sp; /*  '('を読み飛ばし */
	  while( *sp != ')' ){
	    if( *sp == '\0' ){
	      ErrMsg::say(ErrMsg::Missing,"foreach",")",0);
	      rv = -1;
	      goto exit;
	    }else if( *sp == ':' ){ /* $(VAR:OPT) の場合 */
	      ++sp;
	      while( *sp != ')' ){
		if( *sp == '\0' ){
		  ErrMsg::say(ErrMsg::Missing,"foreach",")",0);
		  rv = -1;
		  goto exit;
		}
		opt << *sp++;
	      }
	      break;
	    }
	    word << *sp++;
	  }
	  ++sp; /* ')'を読み飛ばし */

	}else{				/**** $AAAA のケース *****/
	  if( *sp != '\0'  &&  (is_alpha(*sp) || *sp=='_' ) ){
	    do{
	      word << *sp++;
	      if( *sp == ':' ){ /**** $VAR:OPT のケース ****/
		++sp;
		while( *sp != '\0' && is_alpha(*sp) )
		  opt << *sp++;
		break;
	      }
	    }while( *sp != '\0' && ( is_alnum(*sp) || *sp=='_' ) );
	  }
	}
	
	const char *env;
	const char *value=0;
	if( strcmp(word,var)==0 ){
	  /* foreach の変数の場合 */
	  value = str;
	}else if( (env=getShellEnv(word)) != NULL ){
	  /* 環境変数の場合 */
	  value = env;
	}else{
	  /* さもなければ、エラーっすよ */
	  ErrMsg::say( ErrMsg::NoSuchEnvVar , "foreach" , word.getTop() , 0 );
	  rv = -1;
	  goto exit;
	}

	if( opt.getLength() <= 0 ){
	  /* 「：オプションが無い場合は、そのまま展開する */
	  line << value;
	}else{
	  if( word_design( line , value , opt ) != 0 ){
	    ErrMsg::say( ErrMsg::UnknownOption , "foreach",opt.getTop() ,0);
	    rv = -1;
	    goto exit;
	  }
	}
      }
    }
    
    if( ctrl_c ){
      puts( "\nCtrl-C Hit." );
      ctrl_c = 0;
      rv = -1;
      goto exit;
    }

    /* 置換して作成した、各コマンドを実行する。
     * オプション -n が指定されている時は、
     * 表示のみ行い、実際には実行しない。
     */
    if( option & OPTION_N ){
      puts(line);
    }else{
      /* execute の帰り値は、prompt で表示する為にグローバル変数に保存する。
       * この仕様、なんとかした方がいいね！
       */
      extern int execute_result; /* ← nyaos.cc */
      if( option & OPTION_V ){
	fputs(line,stderr);
	if( isatty(fileno(srcfil)) )
	  fputc('\n',stderr);
      }
      execute_result = execute(srcfil,line,1);
      
      if( execute_result != 0  &&  (option & OPTION_I)==0 ){
	ErrMsg::say( ErrMsg::ErrorInForeach , 0 );
	rv = -1;
	goto exit;
      }
    }
  }/* 命令ループ */

 exit:
  setShellEnv( var , orgEnvValueDup );
  free( orgEnvValueDup );

  return rv;
}

static int foreach(FILE *srcfil,const char *parameter, int argc, char **argv)
{
  if( srcfil == NULL ){
    ErrMsg::say( ErrMsg::NotAvailableInRexx , "foreach" , 0 );
    return 0;
  }
  
  if( argc < 3 ){
    fputs("foreach [-ivn] var param1 param2 ... paramN\n",stderr);
    return 0;
  }

  option = 0;

  while( argv[1][0] == '-' ){
    const char *p=argv[1]+1;
    while( *p != '\0' ){
      switch(*p){
      case 'i':
      case 'I':
	option |= OPTION_I;
	break;
      case 'n':
      case 'N':
	option |= OPTION_N;
	break;
      case 'v':
      case 'V':
	option |= OPTION_V;
	break;
      }
      p++;
    }
    argv++;
    argc--;
  }

  struct Line dummyfirst , *cur=&dummyfirst;

  dummyfirst.next = NULL;

  int org_fd1 = -1;
  Parse *args=NULL;


  /** 繰り返す命令群を全て入力させる。 **/
  if( isatty(fileno(srcfil)) ){
    /* キーボード入力 */
    Prompt prompt;
    Shell shell;
    const char *promptenv=getShellEnv("NYAOSPROMPT2");
    int rc;
    
    for(;;){
      Prompt prompt( promptenv ? promptenv : "? " );

      if( prompt.isTopUsed() )
	shell.forbid_use_topline();
      else
	shell.allow_use_topline();

      shell.setcursor( cursor_on_color_str , cursor_off_color_str );
      
      const char *buffer;
      rc=shell.line_input(prompt.get2(),"and..",&buffer);
      putchar('\n');
      if( rc < 0 )
	break;
      else if( rc == 0 )
	continue;
      
      if(   (buffer[0] == 'e' || buffer[0] == 'E' )
	 && (buffer[1] == 'n' || buffer[1] == 'N' )
	 && (buffer[2] == 'd' || buffer[2] == 'D' )
	 && (   buffer[3] =='\0' || buffer[3] == '>'
	     || buffer[3] == '|' || isspace(buffer[3] & 255) ) ){
	args=new Parse(buffer);
	FILE *fout=args->open_stdout();
	if( fout != NULL && fout != stdout ){
	  org_fd1 = dup(1);
	  dup2( fileno(fout) , 1 );
	}
	break;
      }
      if( buffer[0] != '\0' ){
	cur = cur->next = 
	  (struct Line*)alloca(sizeof(struct Line)+strlen(buffer));
	strcpy( cur->buffer  , buffer );
      }
    }/* end:for */
    if( rc==Edlin::ABORT || rc==RC_ABORT ){
      puts("^C");
      return 1;
    }
    if( rc==Edlin::FATAL ){
      ErrMsg::say( ErrMsg::InternalError , "foreach" , 0 );
      return 1;
    }
  }else{
    /* ファイル入力 */
    for(;;){
      char buffer[1024];
      if( fgets_chop(buffer,sizeof(buffer),srcfil) == NULL )
	break;
      
      char *sp=buffer;

      while( *sp != '\0' && is_space(*sp) )
	sp++;
      
      if (   (sp[0] == 'e' || sp[0] == 'E' )
	  && (sp[1] == 'n' || sp[1] == 'N' )
	  && (sp[2] == 'd' || sp[2] == 'D' )
	  && (sp[3] =='\0' || is_space(sp[3]) 
	      || sp[3] == '>' || sp[3]=='|' ) ){
	args = new Parse(sp);
	FILE *fout=args->open_stdout();
	if( fout != NULL && fout != stdout ){
	  org_fd1 = dup(1);
	  close(1);
	  dup2( fileno(fout) , 1 );
	}
	break;
      }

      if( *sp != '\0' ){
	cur = cur->next =
	  (struct Line*)alloca(sizeof(struct Line)+strlen(buffer));
	strcpy( cur->buffer  , buffer );
      }/* 空行は無視する */
    }
  }
  cur->next   = NULL;

  /* 各引数毎にループ */
  int rv=0;
  for(int i=2;i<argc;i++){
    /* 展開したファイル名ごとのループ */
    char **list = 0;
    list = (char**)fnexplode2(argv[i]);
    try{
      if( list==NULL ){
	if( eachcmd(srcfil,argv[1],argv[i],dummyfirst.next) != 0 )
	  goto exit;
      }else{
	numeric_sort(list);
	for(char **listptr=list ; *listptr != NULL ; listptr++ ){
	  int rv=eachcmd(srcfil,argv[1],*listptr,dummyfirst.next);
	  if( rv != 0 ){
	    fnexplode2_free(list);
	    goto exit;
	  }
	}
	fnexplode2_free(list);
      }/* 展開後の名前ループ */
    }catch(StrBuffer::MallocError){
      if( list != NULL )
	fnexplode2_free(list);
      ErrMsg::say( ErrMsg::MemoryAllocationError , "foreach" , 0 );
      break;
    }
  }/* パラメータループ */

 exit:
  if( org_fd1 != -1 ){
    close( 1 );
    dup2( org_fd1 , 1 );
    close( org_fd1 );
    delete args;
  }
  return rv;
}

static int compatible(FILE *source , Parse &params ,
		      int (*routine)(  FILE * , const char*,int,char**) )
{
  int argc=params.get_argc();
  char **argv=(char**)alloca( (argc+1)*sizeof(char*) );

  for(int i=0 ; i<argc ; i++){
    argv[i] = (char*)alloca( params[i].len + 1 );
    memcpy( argv[i] , params[i].ptr , params[i].len );
    argv[i][ params[i].len ] = '\0';
  }
  return (*routine)( source , params.get_parameter() , argc , argv );
}

int cmd_foreach(FILE *source, Parse &params )
{
  return compatible(source,params,foreach); 
}
