/* Nihongo Yet Another Os/2 Shell installing script */

call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs

SAY "    //  // //  //  ////   ////   /////    Free Software"
SAY "   /// // //  // //  // //  // //    / Nihongo Yet Another"
SAY "  //////  ////  ////// //  //   ///        Os/2 Shell"
SAY " // ///   //   //  // //  // /    //    (C) 1996,97,98"
SAY "//  //   //   //  //  ////   /////    HAYAMA,Kaoru(葉山薫)"
SAY
SAY "----- Hello, Nyaosick people. Welcome to NYAOS World! -----"
SAY 

/* -------------
 * ＥＭＸ check
 * ------------- */

config_sys = LEFT(VALUE("SYSTEM_INI",,"OS2ENVIRONMENT"),2) || "\CONFIG.SYS"
flag = 0

/* 必要なリビジョン */
emxdll_need_revision   = 60
emxlibcs_need_revision = 62
emxwrap_need_revision  = 60

CALL SysFileSearch "LIBPATH",config_sys,"libpath"
IF libpath.0 <> 0 THEN DO i=1 TO libpath.0
    PARSE UPPER VAR libpath.i cmdname "=" dir ";" rest
    IF cmdname <> "LIBPATH" THEN
	ITERATE
    
    DO WHILE dir <> ""
	emxdll   = stream(dir || "\emx.dll","C","query EXISTS")
	IF emxdll <> "" THEN DO
	    rev= revision(emxdll)
	    IF rev >= emxdll_need_revision THEN DO 
		SAY "emx.dll -------->" emxdll "     (revision:"rev">=",
		    || emxdll_need_revision || ":Ok) "
		flag = flag + 1
	    END
	    ELSE
		SAY "emx.dll -------->" emxdll "     (revision:"rev"< ",
		    || emxdll_need_revision || ":Ng)"
	END
	emxlibcs = stream(dir || "\emxlibcs.dll","C","query EXISTS")
	IF emxlibcs <> "" THEN DO
	    rev = revision(emxlibcs)
	    IF rev >= emxlibcs_need_revision THEN DO 
		SAY "emxlibcs.dll --->" emxlibcs "(revision:"rev">=",
		    || emxlibcs_need_revision || ":Ok) "
		flag = flag + 2
	    END
	    ELSE
		SAY "emxlibcs.dll --->" emxlibcs "(revision:"rev"< ",
		    || emxlibcs_need_revision || ":Ng)"
	END
	emxwrap = stream(dir || "\emxwrap.dll","C","query EXISTS")
	IF emxwrap <> "" THEN DO
	    rev = revision(emxwrap)
	    IF rev >= emxwrap_need_revision THEN DO 
		SAY "emxwrap.dll ---->" emxwrap " (revision:"rev">=",
		    || emxwrap_need_revision || ":Ok)"
		flag = flag + 4
	    END
	    ELSE
		SAY "emxwrap.dll ---->" emxwrap	" (revision:"rev"< ",
		    || emxwrap_need_revision || ":Ng)"
	END
	PARSE VAR rest dir ";" rest
    END
END

IF flag <> 7 THEN DO
    SAY "07"x
    SAY "Warning !!!"
    SAY "-----------"
    SAY "(English)"
    SAY "  EMX.DLL , EMXLIBCS.DLL , EMXWRAP.DLL whose version is later"
    SAY "  than 0.9d FIX02  is not found on LIBPATH. Please install them"
    SAY "  before installing `Nihongo Yet Another Os/2 Shell'."
    SAY 
    SAY "  Are you sure to continue to install without EMX*.DLL ? (Yes/No)"
    SAY
    SAY "(Japanese)"
    SAY "  LIBPATH 上に、必要なバージョン以上の EMX のダイナミックリンク"
    SAY "  リンクライブラリが見付かりませんでした。"
    SAY 
    SAY "  NYAOSのインストールの前に、0.9d FIX 02 以降のバージョンの"
    SAY "  emxrt.zip (0.9d FIX02 以上)をインストールしてください。"
    SAY
    SAY "  NYAOS のインストールを続行しますか? (Yes/No)"

    key = SysGetKey()
    IF key <> "y" & key <> "Y" THEN DO
	SAY ":No --> NYAOS のインストールを中断します。"
	EXIT 1
    END
    SAY
END

/* NYAOS.EXE , NYAOS.ICO がカレントディレクトリにあるかをチェックする。*/

install:

curdir = directory()

nyaos_exe    = stream("NYAOS.EXE"   ,"C","query EXISTS")
nyaos1_ico   = stream("NYAOS1.ICO"  ,"C","query EXISTS")
nyaos2_ico   = stream("NYAOS2.ICO"  ,"C","query EXISTS")
nyaos_fc_ico = stream("NYAOS-FC.ICO","C","query EXISTS")
nyaos_fo_ico = stream("NYAOS-FO.ICO","C","query EXISTS")

IF nyaos_exe == "" | nyaos_ico1 == "" | nyaos_ico2 == "" THEN DO
    SAY "07"x
    SAY "Error"
    SAY "-----"
    SAY "(English)"
    SAY "  NYAOS.EXE is not found in the current directory."
    SAY "  Please execute INSTALL.CMD on the directory where"
    SAY "  NYAOS.EXE exists."
    SAY
    SAY "(Japanese)"
    SAY "  カレントディクトリに NYAOS.EXE が存在していません。"
    SAY "  NYAOSパッケージを展開したディレクトリで本インストー"
    SAY "  ラーを実行してください。"
    EXIT 1
END

/* -----------------------
 * デスクトップに登録する 
 * ----------------------- */

rc0=SysCreateObject("WPFolder","NYAOS","<WP_DESKTOP>",
    ,"OBJECTID=<NYAOSFD>;ICONFILE="nyaos_fc_ico";ICONNFILE=1,"nyaos_fo_ico,
	,"replace")

parameter = "EXENAME="nyaos_exe";STARTUPDIR="curdir";CCVIEW=YES;ICONFILE="
title = "NYAOS"||'0D0A'x

rc3=SysCreateObject("WPProgram",title||"全画面","<NYAOSFD>",,
    parameter||nyaos2_ico";PROGTYPE=FULLSCREEN","replace" )

rc2=SysCreateObject("WPProgram",title||"100x40","<NYAOSFD>",,
    parameter||nyaos1_ico";PARAMETERS=-g 100x40;MAXIMIZED=YES","replace" )

rc1=SysCreateObject("WPProgram",title||"80x25","<NYAOSFD>",,
    parameter||nyaos1_ico,"replace" )

IF rc0=0 | rc1=0 | rc2=0 | rc3=0 THEN DO
    SAY "プログラムオブジェクトの登録に失敗しました。"
    EXIT 1
END

/* nyaos.rc を ホームディレクトリにコピーする */

IF stream("nyaos.rc","C","QUERY EXISTS") = "" THEN
    EXIT 0

home = VALUE("HOME",,"OS2ENVIRONMENT")
IF home = "" THEN
    EXIT 0

home = TRANSLATE(home,"\","/")

IF  stream(home ||"\nyaos.rc","C","QUERY EXISTS")="" & ,
    stream(home ||"\.nyaos"  ,"C","QUERY EXISTS")="" THEN DO
	SAY "環境変数HOMEが定義されています。"
	SAY "nyaos.rc をディレクトリ" home "にコピーしますか？(Y/N)"
	key = SysGetKey()
	IF key = "y" | key = "Y" THEN 
	    "@copy nyaos.rc" home"\."
    END

EXIT 0


revision: PROCEDURE

ARG pathname
CALL RxFuncAdd 'emx_revision',pathname,'emx_revision'
SIGNAL ON syntax name error
tmp = emx_revision()
SIGNAL OFF syntax
CALL RxFuncDrop 'emx_revision'
RETURN tmp

ERROR:
RETURN -1
