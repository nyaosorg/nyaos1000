#include <stdlib.h>
#include <alloca.h>
#include "parse.h"
#include "hash.h"

/* let コマンドの概要
 *
 *	set VAR=VALUE VAR=VALUE
 *
 * ・「=」の前後に空白を置いてはいけない。
 * ・「=」が無い場合は、その変数の値を表示する。
 */

class LocalVar {
  char *name;
  char *value;
public:
  const char *getName() const { return name; }
  const char *getValue() const { return value; }

  LocalVar( const char *_name,const char *_value )
    : name( strdup(_name) ) , value( strdup(_value) ) { }

  ~LocalVar()
    {  free(name) ; free(value);  }
};

Hash <LocalVar> localvars(256);

const char *getShellEnv( const char *varname )
{
  char *varname2upr=strdup(varname);
  strupr(varname2upr);

  LocalVar *shellenv=localvars[ varname2upr ];
  const char *rv;
  if( shellenv != NULL )
    rv=shellenv->getValue();
  else
    rv=getenv( varname2upr );

  free(varname2upr);
  return rv;
}

void setShellEnv( const char *varname , const char *value )
{
  char *varname2upr=strdup(varname);
  strupr(varname2upr);
  if( value == NULL ){
    localvars.destruct( varname2upr );
  }else{
    LocalVar *newVar=new LocalVar(varname2upr,value);
    localvars.destruct( varname2upr );
    localvars.insert( varname2upr , newVar );
  }
  free(varname2upr);
}

int cmd_let(FILE *source , Parse &parser)
{
  FILE *fout=parser.open_stdout();

  if( parser.get_argc() <= 1 ){
    /* 引数が無い場合、登録されている全てのシェル変数を
     * 表示する。*/

    for(  HashIndex <LocalVar> ptr(localvars) ; *ptr != NULL ; ++ptr )
      fprintf(fout,"%s=%s\n",ptr->getName(),ptr->getValue() );
    
    return 0;
  }

  for(int i=1 ; i<parser.get_argc() ; i++ ){
    char *argv=(char*)alloca( parser[i].len+1 );
    parser[i].quote( argv );

    char *equalchar=strchr( argv , '=' );
    if( equalchar == NULL ){
      /* 「=」が文字列に含まれない ⇒ 変数の値を表示する */
      strupr( argv );

      LocalVar *obj=localvars[ argv ];
      if( obj == NULL )
	fprintf(fout,"%s=\n" , argv );
      else
	fprintf(fout,"%s=%s\n" , argv , obj->getValue() );
      
    }else if( equalchar[1] == '\0' ){
      /* 「変数名=」で終結している ⇒ 変数自体を削除する */
      *equalchar = '\0';
      strupr( argv );

      localvars.destruct( argv );

    }else{
      /* 「=」が文字列に含まれる ⇒ 変数へ値を代入する */
      *equalchar = '\0';
      strupr( argv );
      
      LocalVar *newVar=new LocalVar( argv , equalchar+1 );
      localvars.destruct( argv );
      localvars.insert( argv , newVar );
    }
  }
  return 0;
}
