# Compilador y banderas
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lrt -lpthread

# Nombres de los ejecutables
TARGETS = servidor clienteFifo clienteShm

# Archivos objeto
OBJS = servidor.o clienteFifo.o clienteShm.o

# Regla principal: compila todos los programas
all: $(TARGETS)

# Reglas de compilación para cada ejecutable
servidor: servidor.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clienteFifo: clienteFifo.o
	$(CC) $(CFLAGS) -o $@ $<

clienteShm: clienteShm.o
	$(CC) $(CFLAGS) -o $@ $<

# Regla implícita para transformar archivos .c a .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Elimina archivos binarios y temporales creados por la compilación
clean:
	rm -f $(TARGETS) $(OBJS)

# Limpieza de FIFOs o archivos temporales de IPC en /tmp si fuera necesario
clean-ipc:
	rm -f /tmp/fifo_servidor /tmp/fifo_cliente

# Reglas phony para evitar conflictos con archivos que tengan el mismo nombre
.PHONY: all clean clean-ipc