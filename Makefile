CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lpthread -lrt

TARGETS = servidor clienteFifo clienteShm

all: $(TARGETS)

servidor: servidor.c comun.h
	$(CC) $(CFLAGS) -o servidor servidor.c $(LDFLAGS)

clienteFifo: clienteFifo.c comun.h
	$(CC) $(CFLAGS) -o clienteFifo clienteFifo.c $(LDFLAGS)

clienteShm: clienteShm.c comun.h
	$(CC) $(CFLAGS) -o clienteShm clienteShm.c $(LDFLAGS)

clean:
	rm -f $(TARGETS) /tmp/fifo_*
	rm -rf /dev/shm/* 2>/dev/null || true