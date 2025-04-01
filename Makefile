CC = gcc
CFLAGS = -Iinclude -lpthread -lrt -Wall -Wextra

all: bin/init_cuentas bin/banco bin/monitor bin/usuario

bin/init_cuentas: src/init_cuentas.c src/config.c
	$(CC) -o $@ $^ $(CFLAGS)

bin/banco: src/banco.c src/config.c
	$(CC) -o $@ $^ $(CFLAGS)

bin/monitor: src/monitor.c src/config.c
	$(CC) -o $@ $^ $(CFLAGS)

bin/usuario: src/usuario.c src/config.c
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f bin/* data/cuentas.dat logs/securebank.log