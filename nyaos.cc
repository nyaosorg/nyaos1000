#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/nls.h>

#define INCL_DOSFILEMGR
#include <os2.h>

#include "edlin.h"
#include "nyaos.h"
#include "complete.h"

#define USE_VIDEO_H  1

#if USE_VIDEO_H
#  include <sys/video.h>
#endif

int screen_width=80;
int screen_height=25;
int option_vio_cursor_control=1;
int cursor_start;
int cursor_end;
char *cursor_on_color_str=NULL;
char *cursor_off_color_str=NULL;
int option_nyaos_rc=1;

char *fgets_chop(char *dp, int max, FILE *fp)
{
  int ch;
  while( max-- >= 0  &&  (ch=getc(fp)) != '\n' ){
    if( ch==EOF ){
      *dp = '\0';
      return NULL;
    }
    *dp++ = ch;
  }
  *dp = '\0';
  return dp;
}

int main(int argc, char **argv)
{
  char directory[FILENAME_MAX];
  char thename[FILENAME_MAX];

  setvbuf(stdout,NULL,_IOLBF,BUFSIZ);

  for(int i=1;i<argc;i++){
    if( argv[i][0] == '-' ){
      switch(argv[i][1]){
      case 'e':
	{
	  int length=strlen(argv[i]+2);
	  for(int j=i+1;j<argc;j++)
	    length += strlen(argv[i])+1;
	  
	  char *oneline=(char*)alloca(length+1);
	  char *dp=oneline;
	  char *sp=&argv[i][2];
	  while( *sp != '\0' )
	    *dp++ = *sp++;
	  
	  for(int j=i+1;j<argc;j++){
	    *dp++ = ' ';
	    sp=argv[j];
	    while( *sp != '\0' )
	      *dp++ = *sp++;
	  }
	  *dp = '\0';
	  
	  return execute(stdin,oneline);
	}
	
      case 'f':
	option_nyaos_rc = 0;
	break;

      case 'k':
	if( i+1 < argc ){
	  char buffer[256];

	  _searchenv(argv[++i],"HOME",buffer);
	  FILE *fp=fopen(buffer,"rt");
	  if( fp==NULL ){
	    fprintf(stderr,"%s: %s: no such file\n",argv[0],argv[i]);
	    break;
	  }
	  option_nyaos_rc=0;

	  while( fgets_chop(buffer,sizeof(buffer),fp) != NULL ){
	    if( execute(fp,buffer) == RC_QUIT )
	      break;
	  }
	  fclose(fp);
	}else{
	  fprintf(stderr,"%s: -k option needs filename parameter\n",
		  argv[0],argv[1] );
	}
      }
    }else{
      fprintf(stderr,"%s: %s:invalid argument.\n",argv[0],argv[i]);
      return -1;
    }
  }
  if( option_nyaos_rc ){
    char buffer[FILENAME_MAX];
    FILE *fp;

    if(   (_searchenv(".nyaos","HOME",buffer),
	   buffer[0] != '\0' && (fp=fopen(buffer,"r"))!=NULL )
       || (_searchenv("nyaos.rc","HOME",buffer),
	   buffer[0] != '\0' && (fp=fopen(buffer,"r"))!=NULL )     ){
	 
      while( fgets_chop(buffer,sizeof(buffer),fp) != NULL ){
	if( execute(fp,buffer) == RC_QUIT )
	  break;
      }
      fclose(fp);
    }

  }
  if( option_vio_cursor_control ){
    v_init();
  }

  char cmdlin[1024]="";
  if( isatty(fileno(stdin)) ){
    printf("\x1b[2J\x1b[1m"
	   "     Free Software     ]]  ]] ]]  ]]  ]]]]   ]]]]   ]]]]] \n"
	   "  Nihongo Yet Another  ]]] ]] ]]  ]] ]]  ]] ]]  ]] ]]    ]\n"
	   "    Os/2 Shell 1.15    ]]]]]]  ]]]]  ]]]]]] ]]  ]]   ]]]  \n"
	   "         (C)           ]] ]]]   ]]   ]]  ]] ]]  ]] ]    ]]\n"
	   "  1996,97 HAYAMA,Kaoru ]]  ]]   ]]   ]]  ]]  ]]]]   ]]]]] \n"
	   "                                                          \n"
	   "    This version is compiled on " __DATE__ " " __TIME__"  \n"
	   "    Comments, suggestions, and bug reports are welcome.   \n"
	   "    Please mail to kaoru@ferrari6.cheme.kyoto-u.ac.jp     \n"
	   "\x1b[0m\n"
	   );

    /* DOSの場合、^Hで折り返した前の行へ戻れないので、
     * 一行のみの Window モードにする。
     */
    ShellEdlin edlin("NYAOS>",cmdlin,sizeof(cmdlin) );

    for(;;){
      char promptstr[256],*dp=promptstr,*sp;
      const char *promptenv=getenv("PROMPT");

      time_t now;
      time( &now );
      struct tm *thetime = localtime( &now );

      while( *promptenv != '\0' ){
	if( *promptenv == '$' ){
	  switch( promptenv++ , toupper(*promptenv) ){
	  case '$':
	    *dp++ = '$';
	    break;
	  case '_':
	    *dp++ = '\n';
	    break;
	  case 'A':
	    *dp++ = '&';
	    break;
	  case 'B':
	    *dp++ = '|';
	    break;
	  case 'C':
	    *dp++ = '(';
	    break;
	  case 'D':/* 現在の日付 */
	    dp += sprintf(dp,"%4d-%02d-%02d" ,
			  thetime->tm_year+1900 ,
			  thetime->tm_mon+1 ,
			  thetime->tm_mday );
	    break;
	  case 'E':
	    *dp++ = '\x1b';
	    break;
	  case 'F':
	    *dp++ = ')';
	    break;
	  case 'G':
	    *dp++ = '>';
	    break;
	  case 'H':
	    *dp++ = '\b';
	    break;
	  case 'L':
	    *dp++ = '<';
	    break;
	  case 'N':/* カレントドライブ */
	    *dp++ = _getdrive();
	    break;
	  case 'P':/* カレントディレクトリ */
	    *dp++ = _getdrive();
	    *dp++ = ':';
	    /* unsigned */ char cwd[256];

	    /* ULONG bufsize;
	     * bufsize=sizeof(cwd);
	     * if( DosQueryCurrentDir(0,cwd,&bufsize) == 0 ){
	     */
	    if( (sp=_getcwd(cwd,sizeof(cwd))) != NULL ){
	      /* char *sp=(char*)cwd; */
	      while( *sp != '\0' )
		*dp++ = *sp++;
	    }
	    break;
	       
	  case 'Q':
	    *dp++ = '=';
	    break;
	  case 'S':/* スペース */
	    *dp++ = ' ';
	    break;
	  case 'T':/* 現在の時刻 */
	    dp += sprintf(dp,"%02d:%02d:%02d",
			  thetime->tm_hour ,
			  thetime->tm_min ,
			  thetime->tm_sec );
	    break;
	  case 'V':/* OS/2のバージョン */
	    if( _osmode == OS2_MODE )
	      dp += sprintf(dp,"The Operating System/2 Version is %d.%d"
			    , _osmajor/10 , _osminor );
	    else
	      dp += sprintf(dp,"PC DOS Version is %d.%d"
			    , _osmajor , _osminor );
	    break;
	  }
	  promptenv++;
	}else{
	  *dp++ = *promptenv++;
	}
      }
      *dp = '\0';

      edlin.setcursor( cursor_on_color_str , cursor_off_color_str );
      
      if( option_vio_cursor_control ){
	v_getctype( &cursor_start , &cursor_end );
	v_ctype( cursor_start , cursor_end );
      }
      
      int rc=edlin.simple_input(promptstr,
				_osmode==OS2_MODE ? 32767:screen_width-1 );
      if( rc >= 0 ){
	putchar('\n');
	if( cmdlin[0] != '\0' && execute(stdin,cmdlin) == RC_QUIT ){
	  fputs("Good bye\n",stderr);
	  return 0;
	}
      }else{
	fputs("\nGood bye\n",stdout);
	return 0;
      }
    }
  }else{
    while( fgets_chop(cmdlin,sizeof(cmdlin),stdin) != NULL 
	  && execute(stdin,cmdlin) != RC_QUIT )
      ;
    return 0;
  }
}
