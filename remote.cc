#include "remote.h"
#include <process.h>

#define INCL_DOSNMPIPES
#include <os2.h>

int RemoteNyaos::create( const char *baseName )
{
  /* キューの名前を作る */
  free(name);
  StrBuffer abuf;
  abuf << "\\PIPE\\" << baseName << '.' << getpid();
  name = abuf.finish();

  errCode=DosCreateNPipe(  (unsigned char*) name
			 , &handle
			 , NP_ACCESS_INBOUND | NP_NOINHERIT
			 , NP_NOWAIT
			 | NP_TYPE_BYTE
			 | NP_READMODE_BYTE
			 | 1
			 , 0
			 , 512
			 , 0 );
  return errCode;
};

int RemoteNyaos::connect()
{
  if( ! ok() ) return 0;
  connectFlag = true;
  return errCode=DosConnectNPipe( handle );
}

int RemoteNyaos::disconnect()
{
  if( ! ok() ) return 0;
  connectFlag = false;
  return errCode=DosDisConnectNPipe( handle );
}

/* 名前付きパイプより、1文字読みとる。
 * return
 *	>= 0 :読みとった1文字のコード
 *	<  0 :エラー or EOF
 */
int RemoteNyaos::readchar() throw()
{
  for(;;){
    if( pointor >= left ){
      errCode=DosRead( handle , buffer , sizeof(buffer) , &left );
      pointor = 0;
      if( errCode != 0 )return -errCode;
      if( left <= 0 )	return -1;
    }
    if( buffer[ pointor ] != '\r' )
      break;
    ++pointor;
  } /* \r を無視するためのループ */
  return buffer[ pointor++ ] & 255;
}

/* 名前付きパイプより、1行読みとる。(\n は含まず)
 *	sbuf - 読みとる先のバッファ
 * return
 *	字数( < 0 : エラー or EOF );
 * throw
 *	CantExpand - バッファオーバーフロー
 */
int RemoteNyaos::readline( StrBuffer &sbuf ) throw( StrBuffer::MallocError )
{
  int ch;
  int count=0;
  
  if( (ch=readchar()) < 0 )
    return -1;

  if( ch == '\n' )
    return 0;
  
  do{
    sbuf << char(ch);
    ++count;
  }while( (ch=readchar()) >= 0 && ch != '\n' );
  
  return count;
}

int RemoteNyaos::close()
{
  if( handle != ~0u  &&  connectFlag  ){
    int rc=DosDisConnectNPipe( handle );
    handle = ~0u;
    return rc;
  }
  return 0;
}
