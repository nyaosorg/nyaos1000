#ifndef KEYNAME_H
#define KEYNAME_H

struct KeyName { /* POD ç\ë¢ëÃ */
  const char *name;
  int code;

  static KeyName *find(int n);
  static KeyName *find(const char *name);
  static int compareWithTop(const void *key,const void *el);
};

#endif

