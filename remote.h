#include <cstdlib>
#include "strbuffer.h"

class RemoteNyaos {
  char *name;
  unsigned long handle;
  char buffer[ 256 ];
  unsigned long pointor , left;
  int errCode;
  bool connectFlag;
public:
  RemoteNyaos() : name(0) , handle(~0u) , pointor(0) , left(0)
    , errCode(0) , connectFlag(false)  {}
 
  const char *getName() const { return name; }
  bool ok() const { return handle != NULL && errCode == 0 ; }
  int operator !() const { return errCode; }
  bool isConnected() const { return connectFlag; }
  
  int create( const char *baseName );
  int connect();
  int disconnect();
  
  int readchar() throw();
  int readline( StrBuffer & ) throw( StrBuffer::MallocError );
  
  int close();
  ~RemoteNyaos(){ if( ok() ) close(); free(name); }
};
