#
# Nihongo Yet Another Os/2 Shell
# (c) 1996,97,98 HAYAMA,Kaoru
#
# make clean & make depend & make

CC=gcc
CFLAGS=-O2 -DWITH_CANNA
LDFLAGS=-lvideo -lwrap -Zcrtdll -lsocket

.SUFFIXES : .cc .o .tbl .exe .cmd .doc .html

.tbl.cc : 
	mkbtable.cmd < $< >$@

.cc.o :
	$(CC) $(CFLAGS) -c $<

NYAOS_SRC= alias.cc bindkey.cc chdirs.cc complete.cc commands.cc \
	command2.cc dbcs.cc eadir.cc edlin.cc edlin2.cc execute.cc \
	finds.cc filelist.cc foreach.cc getkey.cc hash.cc nyaos.cc \
	open.cc parse.cc pathlist.cc prepro.cc prompt.cc script.cc \
	search.cc shell.cc source.cc suffix.cc wordseek.cc
NYAOS_TBL=bindfunc.tbl keynames.tbl eadirop.tbl
NYAOS_OBJ=$(NYAOS_SRC:.cc=.o)

nyaos.exe : $(NYAOS_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(NYAOS_OBJ) : %.o : %.cc
	$(CC) $(CFLAGS) -c $<

bindkey.o : bindkey.cc bindfunc.cc keynames.cc
eadir.o : eadir.cc eadirop.cc

tables : $(NYAOS_TBL:.tbl=.cc) depend
bindfunc.cc : bindfunc.tbl mkbtable.cmd
keynames.cc : keynames.tbl mkbtable.cmd
eadirop.cc : eadirop.tbl mkbtable.cmd


nyaos.doc : nyaosdoc.html
	nkf -e $< > tmp.html
	lynx -dump -euc tmp.html | nkf -s >$@
	rm -f tmp.html

clean :
	rm -f *.o *~ depend $(NYAOS_TBL:.tbl=.cc)
