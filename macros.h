/* -*- c++ -*- */
#ifndef MACROS_H
#define MACROS_H

// #include <sys/nls.h>

/* dbcs.c */
extern char dbcs_table[128+256];
int dbcs_table_init();
extern char toupper_table[128+256];
extern char tolower_table[128+256];
#define is_kanji(x) (dbcs_table+128)[x]
// #define is_kanji(x) _nls_is_dbcs_lead((x) & 0xFF)

#define to_upper(x) (toupper_table+128)[x]
#define to_lower(x) (tolower_table+128)[x]
#define is_space(x) isspace((x)& 255)

#define is_digit(x) isdigit((x)& 255)
#define is_alpha(x) isalpha((x)& 255)
#define is_xdigit(x) isxdigit((x)& 255)
#define is_lower(x) islower((x)& 255)
#define is_upper(x) isupper((x)& 255)
#define is_alnum(x) isalnum((x)& 255)

#undef numof
#define numof(A) (sizeof(A)/sizeof((A)[0]))
#define tailof(A) ((A)+numof(A))

#define alloca_char(n) ((char*)alloca(n))

enum{
  NO_FILE = 0,           /* ファイルは存在しない        */
  EXE_FILE = 1,          /* バイナリ実行ファイル	*/
  CMD_FILE = 2,          /* OS/2 コマンドファイル	*/
  COM_FILE = 3,	         /* COM(SOS) ファイル		*/
  FILE_EXISTS = 4,       /* その他のファイル		*/
};

int SearchEnv(const char *fname,const char *envname,char *path);

void raw_mode(void);
void cocked_mode(void);
int get86key(void);
int getkey(void);
void ungetkey(int key);

#endif
