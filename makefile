#
# Nihongo Yet Another Os/2 Shell
# (c) 1996,97,98 HAYAMA,Kaoru
#

CC=gcc
CFLAGS=-O2 -DWITH_CANNA
LDFLAGS=-lvideo -lwrap -Zcrtdll -lsocket

.cc.o :
	$(CC) $(CFLAGS) -c $<

NYAOS=	nyaos.o edlin.o complete.o eadir.o shell.o foreach.o script.o \
	alias.o parse.o execute.o chdirs.o commands.o prepro.o bindkey.o \
	open.o source.o search.o finds.o getkey.o dbcs.o hash.o command2.o \
	prompt.o edlin2.o wordseek.o suffix.o filelist.o pathlist.o

nyaos.exe : $(NYAOS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(NYAOS) : %.o : %.cc
	$(CC) $(CFLAGS) -c $<

RMCLONE=rmclone.o finds.o dbcs.o getkey.o
rmclone.exe : $(RMCLONE)
	gcc $(RMCLONE)

rmclone.o : rmclone.cc

edlin2.o : edlin2.cc edlin.h
filelist.o : filelist.cc finds.h
suffix.o : suffix.cc parse.h hash.h
wordseek.o : wordseek.cc edlin.h
nyaos.o : nyaos.cc edlin.h
hash.o : hash.cc hash.h
pathlist.o : pathlist.cc pathlist.h

bindkey.o : bindkey.cc bindfunc.cc keynames.cc
bindfunc.cc : bindfunc.tbl mkbtable.cmd
	mkbtable.cmd <$< >$@
keynames.cc : keynames.tbl mkbtable.cmd
	mkbtable.cmd <$< >$@

edlin.o : edlin.cc edlin.h
complete.o : complete.cc complete.h finds.h
eadir.o : eadir.cc finds.h eadirop.cc
eadirop.cc : eadirop.tbl mkbtable.cmd
	mkbtable.cmd <$< >$@

source.o : source.cc
shell.o : shell.cc edlin.h
foreach.o : foreach.cc finds.h
alias.o : alias.cc hash.h
script.o : script.cc
parse.o : parse.cc parse.h
execute.o : execute.cc hash.h

commands.o : commands.cc
command2.o : command2.cc
prepro.o : prepro.cc
open.o : open.cc
search.o : search.cc

nyaos.doc : nyaosdoc.html
	nkf -e $< > tmp.html
	lynx -dump -euc tmp.html | nkf -s >$@
	rm -f tmp.html

clean :
	rm -f *.o *~

package :
	tar cvf package.tar $(NYAOS:.o=.cc)
