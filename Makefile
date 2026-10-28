# Compilador y banderas
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lrt -lpthread

# Nombres de los ejecutables
TARGETS = servidor clienteFifo clienteShm

# Archivos objeto
OBJS = servidor.o clienteFifo.o clienteShm.o

# Cabecera común como dependencia
DEPS = comun.h

# Regla principal: compila todos los programas
all: $(TARGETS)

# Compilación de los ejecutables vinculando las librerías necesarias
servidor: servidor.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clienteFifo: clienteFifo.o
	$(CC) $(CFLAGS) -o $@ $<

clienteShm: clienteShm.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Regla para transformar .c a .o considerando comun.h
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Limpieza de ejecutables y archivos objeto
clean:
	rm -f $(TARGETS) $(OBJS)

# Limpieza profunda de FIFOs y Semáforos en el sistema si quedan bloqueados
clean-ipc:
	rm -f /tmp/fifo_servidor /tmp/fifo_cliente
	rm -f /dev/shm/sem.sem_cliente_shm /dev/shm/sem.sem_servidor_shm

.PHONY: all clean clean-ipc