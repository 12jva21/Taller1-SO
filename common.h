#ifndef COMUN_H
#define COMUN_H

#include <sys/types.h>
#include <semaphore.h>

#define ARCHIVO_CSV "vehiculos.csv"
#define FIFO_SERVIDOR "/tmp/fifo_servidor"
#define FIFO_CLIENTE "/tmp/fifo_cliente"

#define SHM_KEY_PATH "/tmp"
#define SHM_PROJ_ID 'A'

#define SEM_CLIENTE_NAME "/sem_cliente_shm"
#define SEM_SERVIDOR_NAME "/sem_servidor_shm"

#define BUSCAR_POR_TIPO 1
#define BUSCAR_POR_MARCA 2
#define COMANDO_SALIR 3

// Estructura de Solicitud para FIFO
typedef struct {
    int tipo;
    char termino[50];
    pid_t cliente_pid;
} Solicitud;

// Estructura en Memoria Compartida (Buzón de IPC)
typedef struct {
    int tipo_busqueda;         // 1: Tipo, 2: Marca
    char termino[50];          // Término a buscar enviado por el cliente
    char respuesta[4096];      // Resultado procesado por el servidor desde el disco
    int listo_para_leer;       // Indicador de estado
} BufferSHM;

#endif