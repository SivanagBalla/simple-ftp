CC:=gcc
#CCFLAGS:=-Wall -Iinclude
CCFLAGS:=-Iinclude
CRYPT:=-lcrypt
BIN:=bin

all: siftp siftpd print_server

libs:=lib/utils.c lib/zlog.c lib/vars.c

siftpd:server.c ${libs} $(BIN)
	${CC} ${CCFLAGS} -o./${BIN}/$@ server.c ${libs} ${CRYPT}

siftp:client.c ${libs} $(BIN)
	${CC} ${CCFLAGS} -o./${BIN}/$@ client.c ${libs}

print_server:test/print_server.c ${libs} $(BIN)
	${CC} ${CCFLAGS} -o./${BIN}/$@ test/print_server.c ${libs}

$(BIN):
	if [ ! -d bin ]; then mkdir bin; fi

clean:
	rm ./${BIN}/*.o
	rm ./${BIN}/ftp*
	rm ./${BIN}/print_server
