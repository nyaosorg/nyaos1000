#! nyaos.exe -c

comment makefile	"メイクファイル"
comment macros.h	"macros.h    : 汎用的なマクロの定義"
comment complete.h	"complete.h  : ファイル名補完クラス Complete 宣言"
comment complete.cc	"complete.cc : ファイル名補完クラス Complete"

comment edlin.h		"Edlin (header)"
comment edlin.cc	"クラス Edlin: 汎用入力・編集"
comment escedlin.cc	"クラス EscEdlin : Edlin のエスケープシーケンス特化版"
comment shell.cc	"クラス ShellEdlin : EscEdlin のシェル特化版"
comment bindkey.cc	"クラス Shell : 機能とキーバインドを結びつける"

comment parse.h		"クラス Parse : 構文解析を行う(宣言)"
comment parse.cc	"クラス Parse : 構文解析を行う"

comment alias.cc	"alias.cc : (un)alias命令と置換処理そのものを行う"

comment execute.cc	"execute.cc : NYAOS版system関数である execute() 他"
comment commands.cc	"commands.cc : 内蔵コマンドの定義"

comment eadir.cc	"内蔵コマンド「ls」「eadir」"
comment foreach.cc	"内蔵コマンド「foreach」"

comment prepro.cc	"チルダ・環境変数の置換ル－チン"
comment script.cc	"スクリプト実行支援の置換ル－チン"

comment nyaos.h		"ソ－ス全体から参照される宣言"
comment nyaos.cc	"NYAOS メインル－チン"

exit
