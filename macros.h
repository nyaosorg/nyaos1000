/* -*- c++ -*- */
#ifndef MACROS_H
#define MACROS_H

/* dbcs.c */

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
#define tailof(A) ((A)+numof(A))

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


/* dup2alloca(var,src,len);
 *
 * src[0]…src[len-1]までの領域を スタック領域へ dup する。
 * 末尾(src[len])は '\0' が設定される。
 * dup された領域は、第一引数のポインタの初期値とされる。
 * つまり、マクロ内で、そんポインタが宣言されるので、
 * すでに宣言されたポインタ変数だったら駄目。
 * あぁ、説明が長い… (^^;  以下の本体を見た方が速いぢゃろうて…
 *
 * マクロのくせに、C++ でないと実用にならないという難儀な代物でもある。
 */
#define dup2alloc(var,src,len) \
  char *var=(char*)alloca((len)+1);(memcpy(var,src,len),var[size]='\0')


#ifndef MALLOC_ERROR
#define MALLOC_ERROR
class MallocError{};
#endif

#endif
