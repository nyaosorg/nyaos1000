# 
#  Nihongo Yet Another Os/2 Shell
#   (c) 1996,97 HAYAMA,Kaoru
#

CC = gcc -O2

.cc.o :
	$(CC) -c $<

NYAOS=	nyaos.o edlin.o edlin2.o complete.o eadir.o \
	shell.o foreach.o script.o alias.o parse.o execute.o \
	commands.o prepro.o bindkey.o open.o source.o search.o

nyaos.exe : $(NYAOS)
	$(CC) $(NYAOS) -o nyaos.out -lvideo -lwrap -Zcrtdll
	emxbind nyaos.out
	del nyaos.out

bindkey.o : bindkey.cc
nyaos.o : nyaos.cc edlin.h
siminput.o : siminput.cc edlin.h
edlin2.o : edlin2.cc edlin.h
complete.o : complete.cc complete.h
eadir.o : eadir.cc

source.o : source.cc
shell.o : shell.cc edlin.h
foreach.o : foreach.cc
alias.o : alias.cc
script.o : script.cc
parse.o : parse.cc parse.h
execute.o : execute.cc

commands.o : commands.cc
prepro.o : prepro.cc
open.o : open.cc
search.o : search.cc

clean :
	rm -f *.o *~
