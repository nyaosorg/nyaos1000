#ifndef QUOTEFLAG_H
#define QUOTEFLAG_H

/* 文字列を最初から読んでゆく際、現在の位置が 引用符の中か否かを
 * 知りたい時がある。そういう時に使うクラス。
 *
 * QuoteFlag qf;
 * for(const char *sp=src ; *sp != '\0' ; sp++ ){
 *	;  色々な処理
 *    qf.eval(*sp);
 * }
 * てな感じで使う。「色々な処理」の部分で、現在、どういう状態にあるかは
 *	qf.isInSingleQuote() … シングルクォートで囲まれている。
 *	qf.isInDoubleQuote() … ダブルクォートで囲まれている。
 *	qf.isInQuote() … 任意のクォートで囲まれている。
 * などで知ることができる。
 */

extern int option_single_quote;

class QuoteFlag {
  enum{ SINGLE=1 , DOUBLE=2 };
  unsigned flag;
public:
  void eval(int ch){
    if( ch == '"'  &&  (flag & SINGLE) == 0 )
      flag ^= DOUBLE;
    else if( ch == '\''  &&  (flag & DOUBLE) == 0  &&  option_single_quote )
      flag ^= SINGLE;
  }
  bool isInSingleQuote() const { return (flag & SINGLE) != 0; }
  bool isInDoubleQuote() const { return (flag & DOUBLE) != 0; }
  bool isInQuote()       const { return flag != 0; }
  QuoteFlag() : flag(0){ }
};

#endif
