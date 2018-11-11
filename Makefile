CC = gcc
CFLAGS = 
#Descomentar la siguiente linea para olivo
LIBS = -lsocket -lnsl
#Descomentar la siguiente linea para linux
#LIBS =

PROGS = cliente servidor

all: ${PROGS}

servidor: servidor.o
	${CC} ${CFLAGS} -o $@ servidor.o ${LIBS}
	
cliente: cliente.o
	${CC} ${CFLAGS} -o $@ cliente.o ${LIBS}

clean:
	rm *.o ${PROGS}
