#
# Nihongo Yet Another Os/2 Shell
# (c) 1996,97,98 HAYAMA,Kaoru
#

CC = gcc -O2

.cc.o :
	$(CC) -c $<

NYAOS=	nyaos.o edlin.o complete.o eadir.o shell.o foreach.o script.o \
	alias.o parse.o execute.o chdirs.o commands.o prepro.o bindkey.o \
	open.o source.o search.o finds.o getkey.o dbcs.o hash.o command2.o \
	prompt.o edlin2.o wordseek.o suffix.o filelist.o
# wstitle93.a

nyaos.exe : $(NYAOS)
	$(CC) $(NYAOS) -o nyaos.exe -lvideo -lwrap -Zcrtdll -lsocket

RMCLONE=rmclone.o finds.o dbcs.o getkey.o
rmclone.exe : $(RMCLONE)
	gcc $(RMCLONE)

rmclone.o : rmclone.cc

edlin2.o : edlin2.cc edlin.h
	$(CC) -DWITH_CANNA -c $< -o edlin2.o

# wstitle93.a : wstitle.imp
#	emximp -o wstitle93.a wstitle.imp

filelist.o : filelist.cc finds.h
suffix.o : suffix.cc parse.h hash.h
wordseek.o : wordseek.cc edlin.h
nyaos.o : nyaos.cc edlin.h
prompt.o : prompt.cc
hash.o : hash.cc hash.h
dbcs.o : dbcs.cc
getkey.o : getkey.cc
finds.o : finds.cc
chdirs.o : chdirs.cc

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
	lynx -dump nyaosdoc.html >nyaos.doc

clean :
	rm -f *.o *~
