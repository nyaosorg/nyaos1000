# 
#  Nihongo Yet Another Os/2 Shell
#   (c) 1996,97,98 HAYAMA,Kaoru
#

CC = gcc -O2

.cc.o :
	$(CC) -c $<

COMMON=	edlin.o complete.o eadir.o shell.o foreach.o script.o \
	alias.o parse.o execute.o chdirs.o commands.o prepro.o bindkey.o \
	open.o source.o search.o finds.o getkey.o dbcs.o hash.o command2.o \
	prompt.o

NYAOS= $(COMMON) edlin2.o nyaos.o 
CANNYAOS= $(COMMON) edlin2c.o cannyaos.o

mkcannya.com : cannyaos.exe nyaos.exe
	wsp.com -W nyaos.exe cannyaos.exe mkcannya.com

cannyaos.exe : $(CANNYAOS)
	$(CC) $(CANNYAOS) -o cannyaos.out \
		-lvideo -lwrap -Zcrtdll -lcanna -lsocket
	emxbind cannyaos.out
	del cannyaos.out

nyaos.exe : $(NYAOS)
	$(CC) $(NYAOS) -o nyaos.out \
		-lvideo -lwrap -Zcrtdll
	emxbind nyaos.out
	del nyaos.out

edlin2.o : edlin2.cc edlin.h
edlin2c.o : edlin2.cc edlin.h
	$(CC) -DWITH_CANNA -c $< -o edlin2c.o

nyaos.o : nyaos.cc edlin.h
cannyaos.o : nyaos.cc edlin.h
	$(CC) -DWITH_CANNA -c $< -o cannyaos.o

prompt.o : prompt.cc
hash.o : hash.cc hash.h
dbcs.o : dbcs.cc
getkey.o : getkey.cc
finds.o : finds.cc
chdirs.o : chdirs.cc
bindkey.o : bindkey.cc

edlin.o : edlin.cc edlin.h

complete.o : complete.cc complete.h finds.h
eadir.o : eadir.cc finds.h

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

clean :
	rm -f *.o *~
