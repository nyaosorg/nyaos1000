#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/nls.h>
#include "macros.h"
#include "finds.h"

int scriptflag=1;
int option_amp_start=1;
int option_sos=0;

// copyargs :
//	全ての引数をコピーする
//      この関数の実行後、*sp_tail は \0 , & , | のどれかを差している。

static void copyargs(  const char *sp       , char *dp
		     , const char **sp_tail , char **dp_tail )
{
  while( *sp != '\0' && *sp != '&' && *sp != '|' ){
    if( *sp == '"' ){
      do{
	if( is_kanji(*sp) )
	  *dp++ = *sp++;
	*dp++ = *sp++;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
    }
    if( is_kanji(*sp) )
      *dp++ = *sp++;
    *dp++ = *sp++;
  }
 exit:
  *dp = '\0';
  if( sp_tail != NULL ) *sp_tail = sp;
  if( dp_tail != NULL ) *dp_tail = dp;
}

// skipargs:
//	実際にコピーしない、copyargs

static void skipargs(  const char *&sp )
{
  while( *sp != '\0' && *sp != '&' && *sp != '|' ){
    if( *sp == '"' ){
      do{
	if( is_kanji(*sp) )
	  ++sp;
	++sp;
	if( *sp == '\0' )
	  return;
      }while( *sp != '"' );
    }
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
}


// SOS 処理
//   sp は スクリプト名の後、パラメーターの前
//   dp は スクリプト名を書く直前
//   path は スクリプトの絶対パス

static int sos(const char *&sp , char *&dp ,  const char *path )
{
  FILE *fp=fopen(path,"r");
  if( fp==NULL )
    return 1;
  
  int ch,nlines=0;
  for(;;){
    ch=getc(fp);
    if( ch=='\n' ){
      int ch=getc(fp); /* 先頭の # を読みとばす */
      if( ch !='#' && ch !=';' && ch !='%' && ch !=':' && ch !='\'' ){
	fclose(fp);
	return 1;
      }
      if( ++nlines==1 ){
	for( const char *s="soshdr/Nide" ; *s != '\0' ; s++ ){
	  if( getc(fp) != *s ){
	    fclose(fp);
	    return 1;
	  }
	}
      }else if( nlines >= 14 ) /* SOS.HDR は 13行 */
	break;
    }else if( !isprint(ch) || ch==EOF ){
      fclose(fp);
      return 1;
    }
  }
  
  /* ここで、ポインタは、コマンド名の直前にあるはず */
  while( (ch=getc(fp)) != EOF && ch != '\n' ){
    if( ch != '%' ){
      *dp++ = ch;
    }else{
      switch( ch=getc(fp) ){
      case '0':
	dp = strcpy_tail(dp,path);
	break;
      case '@':
	copyargs(sp,dp,NULL,&dp);
	break;
      case '%':
	*dp++ = '%';
	break;
      default:
	*dp++ = '%';
	*dp++ = ch;
	break;
      }
    }
  }
  fclose(fp);

  skipargs(sp);
  
  return 0;
}

/* インタープリタ名を挿入する */
static int insert_interpretor(const char *fname , char *&dp)
{
  FILE *fp=fopen(fname,"r");
  if( fp==NULL )
    return -1;
  
  if( getc(fp) != '#' || getc(fp) != '!' ){
    fclose(fp);
    return -2;
  }
    
  /* 環境変数 USRDRIVE の最初の一文字を複写 */
  const char *usp;
  int ch;
  if( (ch=getc(fp))=='/' && (usp=getenv("SCRIPTDRIVE")) != NULL ){
    while( *usp != '\0' && *usp != ':' )
      *dp++ = *usp++;
    *dp++ = ':';
  }
  
  /* perlやawkなどの実行ファイル名の複写 */
  while( ch != EOF  &&  ch != '\n' ){
    if( ch == '/' )
      *dp++ = '\\';
    else
      *dp++ = ch;
    ch=getc(fp);
  }
  fclose(fp);
  *dp++ = ' ';
  return 0;
}

// ファイル名を、'/' <--> '\\' 変換しながら、コピーする
// 空白や、ヌルをファイル名末尾とみなす。

enum{
  SPACE_TERMINATE	= 1,
  SLASH_DEMILITOR	= 2,
  BACKSLASH_DEMILITOR	= 4,
};

static void copy_filename(  const char *sp , char *dp
			  , const char **sp_tail=NULL 
			  , char **dp_tail=NULL 
			  , int flag=SPACE_TERMINATE )
{
  while(    *sp != '\0' && *sp != '|' && *sp != '&' 
	&& ! ((flag & SPACE_TERMINATE)!=0 && is_space(*sp)) ){

    /* コマンド名 : "/"-->"\\"に置換 */
    if( *sp == '"' ){
      do{
	*dp++ = *sp++;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
    }
    if( *sp == '/' && (flag & BACKSLASH_DEMILITOR) !=0 ){
      *dp++ = '\\';
      sp++;
    }else if( *sp == '\\' && (flag & SLASH_DEMILITOR) !=0 ){
      *dp++ = '/';
      sp++;
    }else{
      if( is_kanji(*sp) )
	*dp++ = *sp++;
      *dp++ = *sp++;
    }
  }
 exit:
  *dp = '\0';
  if( sp_tail != NULL ) *sp_tail = sp;
  if( dp_tail != NULL ) *dp_tail = dp;
}

int replace_script( const char *sp , char *dp )
{
  for(;;){
    while( is_space(*sp) )
      *dp++ = *sp++;
    
    if( option_amp_start ){
      // 先行して、末尾が & かどうかしらべる。
      // もし、そうならば先頭に「start」を追加する。

      const char *p=sp;
      for(;;){
	switch( *p ){
	case '>':		/* >& というリダイレクトマークもあり */
	  if( *++p == '&' )
	    ++p;
	  break;
	  
	case '&':
	  while( is_space(*++p) )
	    ;
	  if( *p != '&' ){ /* 「&&」でない「&」なら start を挿入 */
	    char *s = "start ";
	    while( *s != '\0' )
	      *dp++ = *s++;
	  }
	  goto check_script;

	case '|':
	case '\0':
	  goto check_script;

	case '"':
	  do{
	    if( *++p == '\0' )
	      goto check_script;
	  }while( *p != '"' );
	  ++p;
	  break;

	default:
	  if( is_kanji(*p) )
	    ++p;
	  ++p;
	  break;
	}/* switch() */
      }/* for(;;) */
    } /* if option_amp... */

  check_script:
    if( scriptflag != 0  ){
      // ---------------------------
      //   スクリプト実行支援機能
      // ---------------------------

      char fname[FILENAME_MAX];
      char path[FILENAME_MAX];
      
      copy_filename(sp,fname,&sp,NULL);
      
      int type=SearchEnv(fname,"SCRIPTPATH",path);
      
      if( type == FILE_EXISTS ){
	// --- おそらく、スクリプト ---
	insert_interpretor(path,dp);
	/* dp = strcpy_tail(dp,path); */
	copy_filename(path,dp,NULL,&dp, SLASH_DEMILITOR );
	copyargs(sp,dp,&sp,&dp);
      }else if( type != COM_FILE  || sos(sp,dp,path) != 0 ){
	// --- OS/2 の実行ファイル ---
	/* dp = strcpy_tail(dp,path); */
	copy_filename(path,dp,NULL,&dp, BACKSLASH_DEMILITOR );
	copyargs(sp,dp,&sp,&dp);
      }
    }else{
      copy_filename(sp,dp,&sp,&dp);
      copyargs(sp,dp,&sp,&dp);
    }
    if( *sp == '\0' )
      break;
    
    if( *sp == '|' ){
      if( *(sp+1) == '&' ){	/*  `|&' -> '2>&1 |' */
	*dp++ = '2';	*dp++ = '>';
	*dp++ = '&';	*dp++ = '1';
	*dp++ = ' ';	*dp++ = '|';
	sp += 2;
      }else{
	*dp++ = *sp++;
      }
    }else if( *sp == '&' ){
      *dp++ = *sp++;
      if( *sp == '&' )
	*dp++ = *sp++;
    }
    if( *sp=='\0' )
      break;
  }/* パイプで区切られた各コマンド毎のループ */
  *dp = '\0';
  return 0;
}
