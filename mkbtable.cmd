/* mkbtable.cmd 二分検索の為のテーブルを作る為の REXX Script
 *   
 * 「A B…」形式の行を A をキーとしてソートし、
 * 「  { "A",B… } 」の形で出力する。
 */

n=0
DO WHILE LINES() > 0
    /* タブを空白に変換しつつ読み込む */
    line = TRANSLATE(linein(),' ','09'x)
    IF LEFT(line,1) = "#" | WORDS(line) < 2 THEN
	iterate
    n = n+1
    PARSE var line key.n dat.n
END
DO i=1 TO n-1
    DO j=i+1 TO n
	IF key.j < key.i THEN DO 
	    temp = key.i ; key.i = key.j ; key.j = temp
	    temp = dat.i ; dat.i = dat.j ; dat.j = temp
	END
    END
END

SAY "/* このファイルは mkbtable.cmd で作られたものです。*/"
SAY "/* 本内容に変更を行う場合はソース(*.tbl)の方に変更が必要です。*/"
SAY ""
DO i=1 TO n
    SAY '  { "' || key.i || '",' dat.i '},'
END



