/* -*- rexx -*-
 */

SAY "---- NYAOS 配布パッケージ製作ユーティリティー ----"

"lxlite nyaos.exe"

/* DLL を呼び出す */
CALL RxFuncAdd  SysFileTree, RexxUtil, SysFileTree
CALL RxFuncAdd  GetKey , RexxUtil , SysGetKey

/* readme.XXX の XXX を元に、バージョンナンバを調べる。
 * 複数ある中で、最も大きい XXX を採用する。
 */

IF ARG()<1 | ARG(1)=="" THEN DO
    CALL SysFileTree "readme.*","list","FO"
    ver = -1
    DO i=1 TO list.0
	PARSE VAR list.i . "." suffix
	IF DATATYPE(suffix,"N") & suffix > ver THEN
	    ver = suffix
    END
    IF ver = -1 THEN DO 
	SAY "バ－ジョンナンバを引数に必要です。"
	EXIT 1
    END
END
ELSE DO 
    ver = ARG(1)
    IF POS(".",ver)<>0 THEN
	ver = DELSTR(ver,POS(".",ver),1)
END


readme_1st = "readme."||ver
IF stream( readme_1st ,"C","query exists") == "" THEN DO
    readme_1st = "README.1ST"
    IF stream( readme_1st , "C" ,"query exists" )=="" THEN DO 
	SAY "README."ver"が存在しません。"
	EXIT 1
    END
END

major=LEFT(ver,1)
minor=SUBSTR(ver,2)

binpack = "nyaos"ver".lzh"
srcpack = "nyaos-"major"."minor".tar.bz2"

SAY "---- バイナリパッケージ作成 ----"
"lha a" binpack readme_1st "nyaos.doc nyaosdoc.html nyaos.exe" ,
    "nyaos.rc nyaos1.ico nyaos2.ico nyaos-fc.ico nyaos-fo.ico",
	"history install.cmd"

SAY "---- ソースパッケージ作成 ----"
"make README1ST="readme_1st "nyaos.tar"
"bzip2 --compress --repetitive-best < nyaos.tar >" srcpack
"del nyaos.tar"

webdir = "..\..\www\my\warp"
SAY "ウェブディレクトリ("webdir")へコピーしますか？"
IF GetKey() = 'y' THEN DO
    "copy" binpack webdir
    "copy" srcpack webdir
END
SAY

packdir = "..\package\nyaos"
SAY "パッケージディレクトリ("packdir")へコピーしますか？"
if GetKey() = 'y' THEN DO 
    "copy" binpack packdir
    "copy" srcpack packdir
END
SAY

exit 0
