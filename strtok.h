#ifndef STRTOK_H
#define STRTOK_H

class Strtok {
  char *p;
public:
  Strtok(char *s) : p(s) { }
  char *cut_with(const char *dem=" \t\n\r\f");
};

#endif

