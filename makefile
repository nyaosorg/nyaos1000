#
# Makefile for GNU Make.
#
# Free Software : Nihongo Yet Another Os/2 Shell
# (c) 1996,97,98,99 HAYAMA,Kaoru
#
# If you have canna.a, please add '-DCANNA=0' to CFLAGS.
#

CFLAGS=-Wall -O2 -DNDEBUG
#CFLAGS=-Wall -g
#CFLAGS=-Wall -O2 -DCANNA=0

LDFLAGS=-lvideo -lsocket -lwrap -Zcrtdll
CC=gcc

all : nyaos.exe nyaos.doc

# -------------- 自動生成ルール ----------------

.SUFFIXES : .cc .o .tbl .exe .cmd .doc .html

.tbl.cc : 
	mkbtable.cmd < $< >$@

.cc.o :
	$(CC) $(CFLAGS) -c $<

# -------------- ファイルリスト -----------------

NYAOS_HDR=\
	complete.h edlin.h finds.h hash.h macros.h nyaos.h substr.h \
	parse.h pathlist.h smartptr.h strtok.h keyname.h strbuffer.h \
	quoteflag.h autofileptr.h autofreeptr.h prompt.h errmsg.h
NYAOS_SRC=\
	alias.cc bindkey.cc chdirs.cc complete.cc command1.cc \
	command2.cc dbcs.cc eadir2.cc edlin.cc edlin2.cc execute.cc \
	finds.cc filelist.cc foreach2.cc getkey.cc hash.cc nyaos.cc \
	open.cc parse.cc pathlist.cc prepro2.cc prompt3.cc script2.cc \
	search.cc shell.cc source.cc vzhistory.cc strtok.cc keynameseek.cc \
	strbuffer.cc debugger.cc let.cc errmsg.cc
# suffix.cc 

NYAOS_TBL=\
	bindfunc.tbl keynames.tbl eadirop.tbl
NYAOS_OBJ=$(NYAOS_SRC:.cc=.o)

# ------------- パッケージ作成 -----------------

# pknyaos.cmd から呼び出される。
# 「make README1ST=readme.XXX nyaos.tar」と呼び出す必要がある。

nyaos.tar :
	cd .. && tar cvf nyaos/$@ $(foreach A,\
		Makefile pknyaos.cmd $(NYAOS_HDR) $(NYAOS_SRC) \
		mkbtable.cmd $(README1ST) $(NYAOS_TBL),nyaos/$(A))

# ------------- 実行ファイル作成 ----------------

nyaos.exe : $(NYAOS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(NYAOS_OBJ) : %.o : %.cc
	$(CC) $(CFLAGS) -c $<

keynameseek.o : keynameseek.cc keynames.cc
bindkey.o : bindkey.cc bindfunc.cc
eadir2.o : eadir2.cc eadirop.cc

tables : $(NYAOS_TBL:.tbl=.cc)
bindfunc.cc : bindfunc.tbl mkbtable.cmd
keynames.cc : keynames.tbl mkbtable.cmd
eadirop.cc : eadirop.tbl mkbtable.cmd

# ------------- ドキュメント作成 -----------------

nyaos.doc : nyaosdoc.html
	nkf -e $< > tmp.html
	lynx -dump -euc tmp.html | nkf -s >$@
	rm -f tmp.html

nyaos.eng : nyaoseng.xx
	xtr -e $< > $@

# ------------- お掃除 -------------

clean :
	rm -f *.o *~ $(NYAOS_TBL:.tbl=.cc)
