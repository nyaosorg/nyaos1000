#
# Nihongo Yet Another Os/2 Shell
# (c) 1996,97,98,99 HAYAMA,Kaoru
#
# make clean & make depend & make

CC=gcc
CFLAGS=-DWITH_CANNA -Wall -O2
LDFLAGS=-lvideo -lwrap -Zcrtdll -lsocket

all : nyaos.exe nyaos.doc

# -------------- 自動生成ルール ----------------

.SUFFIXES : .cc .o .tbl .exe .cmd .doc .html

.tbl.cc : 
	mkbtable.cmd < $< >$@

.cc.o :
	$(CC) $(CFLAGS) -c $<

# -------------- ファイルリスト -----------------

NYAOS_HDR=\
	complete.h edlin.h finds.h hash.h macros.h nyaos.h \
	parse.h pathlist.h smartptr.h strtok.h
NYAOS_SRC=\
	alias.cc bindkey.cc chdirs.cc complete.cc command1.cc \
	command2.cc dbcs.cc eadir.cc edlin.cc edlin2.cc execute.cc \
	finds.cc filelist.cc foreach.cc getkey.cc hash.cc nyaos.cc \
	open.cc parse.cc pathlist.cc prepro.cc prompt.cc script.cc \
	search.cc shell.cc source.cc suffix.cc wordseek.cc strtok.cc
NYAOS_TBL=\
	bindfunc.tbl keynames.tbl eadirop.tbl
NYAOS_OBJ=$(NYAOS_SRC:.cc=.o)

# ------------- パッケージ作成 -----------------

# pknyaos.cmd から呼び出される。
# 「make README1ST=readme.XXX nyaos.tar」と呼び出す必要がある。
nyaos.tar :
	tar cvf $@ readme.src Makefile $(NYAOS_HDR) $(NYAOS_SRC) \
		mkbtable.cmd $(README1ST) $(NYAOS_TBL)


# ------------- 実行ファイル作成 ----------------

nyaos.exe : $(NYAOS_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(NYAOS_OBJ) : %.o : %.cc
	$(CC) $(CFLAGS) -c $<

bindkey.o : bindkey.cc bindfunc.cc keynames.cc
eadir.o : eadir.cc eadirop.cc

tables : $(NYAOS_TBL:.tbl=.cc)
bindfunc.cc : bindfunc.tbl mkbtable.cmd
keynames.cc : keynames.tbl mkbtable.cmd
eadirop.cc : eadirop.tbl mkbtable.cmd


# ------------- ドキュメント作成 -----------------

nyaos.doc : nyaosdoc.html
	nkf -e $< > tmp.html
	lynx -dump -euc tmp.html | nkf -s >$@
	rm -f tmp.html

# ------------- お掃除 -------------

clean :
	rm -f *.o *~ $(NYAOS_TBL:.tbl=.cc)
