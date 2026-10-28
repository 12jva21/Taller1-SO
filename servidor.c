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

// Función que lee directamente del DISCO (cumple sección 7 del PDF)
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
    printf("[SERVIDOR] Iniciando servidor con lectura en disco (CSV)...\n");

    // --- CONFIGURACIÓN DE MEMORIA COMPARTIDA Y SEMÁFOROS ---
    key_t key = ftok(SHM_KEY_PATH, SHM_PROJ_ID);
    int shmid = shmget(key, sizeof(BufferSHM), 0666 | IPC_CREAT);
    BufferSHM *shm_ptr = (BufferSHM *)shmat(shmid, NULL, 0);
    memset(shm_ptr, 0, sizeof(BufferSHM));

    sem_unlink(SEM_CLIENTE_NAME);
    sem_unlink(SEM_SERVIDOR_NAME);

    sem_t *sem_servidor = sem_open(SEM_SERVIDOR_NAME, O_CREAT, 0666, 0); // Servidor espera a 0
    sem_t *sem_cliente = sem_open(SEM_CLIENTE_NAME, O_CREAT, 0666, 0);   // Cliente espera a 0

    // --- CONFIGURACIÓN DE FIFO ---
    mkfifo(FIFO_SERVIDOR, 0666);

    printf("[SERVIDOR] Listo y escuchando solicitudes por FIFO y SHM (Semáforos)...\n");

    // Fork para atender SHM en segundo plano y FIFO en el hilo principal
    if (fork() == 0) {
        // PROCESO HIJO: Atiende solicitudes por Memoria Compartida (SHM)
        while (1) {
            sem_wait(sem_servidor); // Espera notificación del cliente SHM

            if (shm_ptr->tipo_busqueda == COMANDO_SALIR) break;

            printf("[SERVIDOR-SHM] Solicitud recibida en RAM -> Buscando en Disco...\n");
            buscar_en_disco_csv(shm_ptr->tipo_busqueda, shm_ptr->termino, shm_ptr->respuesta, sizeof(shm_ptr->respuesta));

            sem_post(sem_cliente); // Avisa al cliente SHM que la respuesta está lista
        }
        shmdt(shm_ptr);
        exit(0);
    }

    // PROCESO PADRE: Atiende solicitudes por FIFO
    while (1) {
        int fd_serv = open(FIFO_SERVIDOR, O_RDONLY);
        if (fd_serv == -1) break;

        Solicitud sol;
        ssize_t bytes = read(fd_serv, &sol, sizeof(Solicitud));
        close(fd_serv);

        if (bytes > 0) {
            printf("[SERVIDOR-FIFO] Solicitud recibida -> Buscando en Disco...\n");
            int fd_cli = open(FIFO_CLIENTE, O_WRONLY);
            if (fd_cli != -1) {
                char respuesta[4096];
                buscar_en_disco_csv(sol.tipo, sol.termino, respuesta, sizeof(respuesta));
                write(fd_cli, respuesta, strlen(respuesta));
                close(fd_cli);
            }
        }
    }

    // Limpieza
    sem_close(sem_servidor);
    sem_close(sem_cliente);
    sem_unlink(SEM_SERVIDOR_NAME);
    sem_unlink(SEM_CLIENTE_NAME);
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    unlink(FIFO_SERVIDOR);

    return 0;
}