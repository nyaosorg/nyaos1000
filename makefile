CC = gcc -O2

.cc.o :
	$(CC) -c $<

.cc.obj :
	$(CC) -Zomf -Zsys -static -c $<

NYAOS=nyaos.o edlin.o escedlin.o complete.o eadir.o \
	shell.o foreach.o script.o alias.o parse.o execute.o \
	commands.o prepro.o bindkey.o
nyaos.exe : $(NYAOS)
	$(CC) $(NYAOS) -o nyaos.out -lvideo
	emxbind nyaos.out

# siminput.o
bindkey.o : bindkey.cc

NYAOS_S=nyaos.obj edlin.obj siminput.obj escedlin.obj complete.obj eadir.obj \
	shell.obj foreach.obj script.obj alias.obj parse.obj execute.obj \
	commands.obj prepro.obj
nyaos-s.exe : $(NYAOS_S)
	$(CC) -Zomf -Zsys -static $(NYAOS) -o nyaos.exe -lvideo

nyaos.obj : nyaos.cc edlin.h
siminput.obj : siminput.cc edlin.h
escedlin.obj : escedlin.cc edlin.h
complete.obj : complete.cc complete.h
eadir.obj : eadir.cc

shell.obj : shell.cc edlin.h
foreach.obj : foreach.cc
alias.obj : alias.cc
script.obj : script.cc
parse.obj : parse.cc parse.h
execute.obj : execute.cc

commands.obj : commands.cc
prepro.obj : prepro.cc

nyaos.o : nyaos.cc edlin.h
siminput.o : siminput.cc edlin.h
escedlin.o : escedlin.cc edlin.h
complete.o : complete.cc complete.h
eadir.o : eadir.cc

shell.o : shell.cc edlin.h
foreach.o : foreach.cc
alias.o : alias.cc
script.o : script.cc
parse.o : parse.cc parse.h
execute.o : execute.cc

commands.o : commands.cc
prepro.o : prepro.cc

clean :
	rm -f *.o *~
