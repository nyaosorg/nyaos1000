CC = gcc -O2

.cc.o :
	$(CC) -c $<

NYAOS=nyaos.o edlin.o siminput.o escedlin.o complete.o eadir.o \
	shell.o foreach.o script.o alias.o params.o execute.o \
	commands.o prepro.o
nyaos.exe : $(NYAOS)
	$(CC) $(NYAOS) -o nyaos.out -lvideo
	emxbind nyaos.out
	del nyaos.out

nyaos.o : nyaos.cc edlin.h
siminput.o : siminput.cc edlin.h
escedlin.o : escedlin.cc edlin.h
complete.o : complete.cc complete.h
eadir.o : eadir.cc

shell.o : shell.cc edlin.h
foreach.o : foreach.cc
alias.o : alias.cc
script.o : script.cc
params.o : params.cc params.h
execute.o : execute.cc

commands.o : commands.cc
prepro.o : prepro.cc

clean :
	rm -f *.o *~
