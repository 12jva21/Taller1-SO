#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include "comun.h"

void buscar_en_disco_csv(int tipo, const char *termino, char *buffer_salida, size_t max_len) {
    FILE *file = fopen(ARCHIVO_CSV, "r");
    if (!file) {
        snprintf(buffer_salida, max_len, "[ERROR SERVIDOR] No se pudo abrir el archivo CSV en disco.\n");
        return;
    }

    char linea[256];
    int coincidencias = 0;
    buffer_salida[0] = '\0';

    while (fgets(linea, sizeof(linea), file)) {
        char tipo_v[50], marca_v[50], modelo_v[50];
        if (sscanf(linea, "%49[^,],%49[^,],%49[^\n]", tipo_v, marca_v, modelo_v) == 3) {
            const char *evaluar = (tipo == BUSCAR_POR_TIPO) ? tipo_v : marca_v;
            if (strcasecmp(evaluar, termino) == 0) {
                char coincidencia[256];
                snprintf(coincidencia, sizeof(coincidencia), "Coincidencia: [%s] %s - %s\n", tipo_v, marca_v, modelo_v);
                strncat(buffer_salida, coincidencia, max_len - strlen(buffer_salida) - 1);
                coincidencias++;
            }
        }
    }
    fclose(file);

    if (coincidencias == 0) {
        snprintf(buffer_salida, max_len, "No se encontraron resultados en el disco.\n");
    }
}

int main(void) {
    printf("[SERVIDOR] Iniciando servidor único (sin fork) con acceso directo a disco...\n");

    // 1. Configuración SHM
    key_t key = ftok(SHM_KEY_PATH, SHM_PROJ_ID);
    int shmid = shmget(key, sizeof(BufferSHM), 0666 | IPC_CREAT);
    BufferSHM *shm_ptr = (BufferSHM *)shmat(shmid, NULL, 0);
    memset(shm_ptr, 0, sizeof(BufferSHM));

    // 2. Configuración Semáforos POSIX
    sem_unlink(SEM_CLIENTE_NAME);
    sem_unlink(SEM_SERVIDOR_NAME);
    sem_t *sem_servidor = sem_open(SEM_SERVIDOR_NAME, O_CREAT, 0666, 0);
    sem_t *sem_cliente = sem_open(SEM_CLIENTE_NAME, O_CREAT, 0666, 0);

    // 3. Configuración FIFO
    mkfifo(FIFO_SERVIDOR, 0666);

    printf("[SERVIDOR] Listo. Atendiendo peticiones por FIFO y SHM en un solo proceso central...\n");

    while (1) {
        // --- A. REVISAR MEMORIA COMPARTIDA (SHM) ---
        // sem_trywait no se bloquea si no hay cliente SHM esperando
        if (sem_trywait(sem_servidor) == 0) {
            if (shm_ptr->tipo_busqueda == COMANDO_SALIR) break;

            printf("[SERVIDOR] Consulta recibida por SHM -> Buscando en Disco...\n");
            buscar_en_disco_csv(shm_ptr->tipo_busqueda, shm_ptr->termino, shm_ptr->respuesta, sizeof(shm_ptr->respuesta));

            sem_post(sem_cliente); // Avisa al cliente SHM que la respuesta está lista
        }

        // --- B. REVISAR FIFO (Modo No Bloqueante) ---
        int fd_serv = open(FIFO_SERVIDOR, O_RDONLY | O_NONBLOCK);
        if (fd_serv != -1) {
            Solicitud sol;
            ssize_t bytes = read(fd_serv, &sol, sizeof(Solicitud));
            close(fd_serv);

            if (bytes > 0) {
                printf("[SERVIDOR] Consulta recibida por FIFO -> Buscando en Disco...\n");
                int fd_cli = open(FIFO_CLIENTE, O_WRONLY);
                if (fd_cli != -1) {
                    char respuesta[4096];
                    buscar_en_disco_csv(sol.tipo, sol.termino, respuesta, sizeof(respuesta));
                    write(fd_cli, respuesta, strlen(respuesta));
                    close(fd_cli);
                }
            }
        }

        usleep(50000); // Pausa de 50ms para evitar consumo excesivo de CPU
    }

    // Limpieza de recursos IPC
    sem_close(sem_servidor);
    sem_close(sem_cliente);
    sem_unlink(SEM_SERVIDOR_NAME);
    sem_unlink(SEM_CLIENTE_NAME);
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    unlink(FIFO_SERVIDOR);

    return 0;
}