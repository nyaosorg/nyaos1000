/* -*- c++ -*- */
#ifndef PROMPT_H
#define PROMPT_H

#include <stdlib.h>

class Prompt {
  bool used_topline;
  char *promptstr;
public:
  int parse( const char *promptmacros );
  const char *get()  const { return promptstr; }
  const char *get2() const { return promptstr ? promptstr : ""; }
  bool isTopUsed() const { return used_topline; }
  
  Prompt() : used_topline(false) , promptstr(0)
    { }
  explicit Prompt(const char *s) : used_topline(false),promptstr(0)
    { parse(s); }
  ~Prompt(){ free(promptstr); }
};

#endif
