/* -*- c++ -*- */
#ifndef PATHLIST_H
#define PATHLIST_H

class PathList {
  struct OnePath {
    OnePath *next;
    int len;
    char name[1];
  } *first;
  int sum_of_length;
  int appendOne(const char *top,int len );
public:
  PathList() : first(NULL) , sum_of_length(0) { }
  ~PathList();
  
  // int remove( const char *path );
  void append( const char *path , int dem=';' );
  int getSumOfLength() const { return sum_of_length; }
  void listing(char *buffer,int dem=';');
};
#endif
