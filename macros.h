#ifndef MACROS_H
#define MACROS_H

extern char dbcstable[256];
int dbcs_table_init();

#define is_space(x) isspace((x)& 255)
#define is_kanji(x) dbcstable[(x)& 255]
#define is_digit(x) isdigit((x)& 255)
#define is_alpha(x) isalpha((x)& 255)
#define is_xdigit(x) isxdigit((x)& 255)
#define is_lower(x) islower((x)& 255)
#define is_upper(x) isupper((x)& 255)
#define is_alnum(x) isalnum((x)& 255)
#define to_upper(x) toupper((x)& 255)
#define to_lower(x) tolower((x)& 255)

#undef numof
#define numof(A) (sizeof(A)/sizeof((A)[0]))

#endif
