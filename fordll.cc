#include <cstdio>
#include <exception>

void unexpected(void)
{
  fputs("nyaos: fatal error. unexpected exception occured.\n"
	"       Please mail to iya-hayamatta@ijk.com\n"
	, stderr );
}

#if 0
exception::exception()
{
  ;
}
#endif
