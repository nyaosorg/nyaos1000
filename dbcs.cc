#define INCL_DOSNLS
#include <os2.h>

#include <ctype.h>
#include <stdlib.h> /* for _osmode */
#include <string.h>

#include "macros.h"
char dbcs_table[128+256];
char toupper_table[256+128];
char tolower_table[256+128];

int dbcs_table_init()
{
  /* toupper/lower table */
  for(int i=-128 ; i < 256 ; i++ ){
    to_upper(i) = toupper( i & 255 );
    to_lower(i) = tolower( i & 255 );
  }
  
  /* DBCS table */
  memset(dbcs_table,0,sizeof(dbcs_table));
  if( _osmode ==  OS2_MODE ){
    // OS/2 の場合、API 関数を呼んで、設定する。

    char buffer[12];    

    COUNTRYCODE country;
    
    country.country = 0;
    country.codepage = 0;
    
    int rc=(int)DosQueryDBCSEnv((ULONG)numof(buffer)
				,&country
				,buffer);
    char *p=buffer;
    while( (p[0]!=0 || p[1] !=0) && p < buffer+sizeof(buffer) ){
      /* printf("DBCS %02x--%02x\n",p[0] & 255 ,p[1] & 255); */
      memset( (dbcs_table+128)+(p[0] & 255), 1 
	     , (p[1] & 255)-(p[0] & 255)+1 );
      p += 2;
    }
    memcpy( dbcs_table , dbcs_table+256 , 128 );
    return rc;

  }else{
    // DOSの場合、プロテクトモードで、DBCS テーブルを参照する方法が
    // 無いので、Shift-JIS にしてしまう。
    
    memset( (dbcs_table+128) + 0x81 , 1 , 0x9F - 0x80 );
    memset( (dbcs_table+128) + 0xE1 , 1 , 0xfc - 0xE0 );
    memcpy( dbcs_table , dbcs_table+256 , 128 );
    return 0;
  }
}
