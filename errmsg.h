/* -*- c++ -*- */

class ErrMsg{
public:
  enum MsgNo {
    ChangeDriveError,		/* 「x:」で、そのドライブへ移動できない */
    TooNearTerminateChar,	/* 「&」や「|」が近すぎる */
    InternalError,		/* 謎の内部エラー */
    CantRemoveFile,		/* ファイルを削除できない */
    CantMakeDir,		/* ディレクトリを作れない */

    CantWriteSubject,		/* .SUBJECT へ書き込めない */
    CantWriteComments,		/* .COMMENTS へ書き込めない */
    InvalidKeyName,		/* キーの名前がちゃうちゅーて */
    InvalidKeySetName,		/* キーセットの名前が違う */
    InvalidFuncName,		/* 機能名が違う */

    TooLongVarName,		/* 環境変数名が長すぎる */
    MustNotInputRedirect,	/* 内蔵コマンドでは入力リダイレクトできない */
    InvalidVarName,		/* 環境変数名が変 */
    CantOutputRedirect,		/* 出力リダイレクトできなかった */
    TooFewArguments,		/* 引数が足りない */

    TooManyNesting,		/* ネストさせすぎ(source) */
    NoSuchFile,			/* そんなファイル無いちゅーに */
    NoSuchAlias,		/* そんな別名始めからないでー */
    BadHomeDir,			/* %HOME% がおかしい */
    NoSuchDir,			/* そんなディレクトリ無いちゅーに */
				   
    UnknownOption,		/* そんなオプション知らんでー */
    DirStackEmpty,		/* スタックは空やっちゅーに */
    NoSuchFileOrDir,		/* ファイルもディレクトリも無いちゅーに */
    CantGetCurDir,		/* ここはどこ、私は誰 */
    TooLargeStackNo,		/* スタック番号が大きすぎ */
    
    Missing,			/* あるはずの文字がねーよぉ */
    NoSuchEnvVar,		/* そんな環境変数ねーよ */
    ErrorInForeach,		/* foreach 実行プログラムにエラー */
    NotAvailableInRexx,		/* REXX中では使えない */
    MemoryAllocationError,	/* malloc でミスッたのよ */

    NotSupportVDM,		/* もはや VDM はサポートしていない */
    DBCStableCantGet,		/* DBCS表をOSより取得できない */
    WhereIsCmdExe,		/* CMD.EXE が無いぞ */
    BadParameter,		/* とにかくパラメータが悪い(何のこっちゃ) */
    AmbiguousRedirect,		/* どっちにリダイレクトすんねん */

    EventNotFound,		/* そんなヒストリはねぇぇぇ */
    TooManyErrors,		/* エラーが多すぎる */
    FileExists,			/* ファイルを上書きしようとしている */
#ifdef VMAX
    BadCommandOrFileName	/* コマンドまたはファイル名が違います。*/
#endif
    };
  static void say(int x,...);
  static void mount(int x,const char *s);
};

extern int source_errmsg(const char *fname);

