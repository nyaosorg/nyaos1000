#ifndef MACROS_H
#define MACROS_H

extern char dbcs_table[128+256];
int dbcs_table_init();
extern char toupper_table[128+256];
extern char tolower_table[128+256];
#define is_kanji(x) (dbcs_table+128)[x]
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

enum{
  NO_FILE = 0,           /* ファイルは存在しない        */
  EXE_FILE = 1,          /* バイナリ実行ファイル	*/
  CMD_FILE = 2,          /* OS/2 コマンドファイル	*/
  COM_FILE = 3,	         /* COM(SOS) ファイル		*/
  FILE_EXISTS = 4,       /* その他のファイル		*/
};

int SearchEnv(const char *fname,const char *envname,char *path);

#endif
