#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include "comun.h"

int main(void) {
    key_t key = ftok(SHM_KEY_PATH, SHM_PROJ_ID);
    int shmid = shmget(key, sizeof(BufferSHM), 0666);
    if (shmid == -1) {
        perror("[ERROR] No se pudo encontrar la Memoria Compartida. ¿Inició el servidor?");
        exit(EXIT_FAILURE);
    }

    BufferSHM *shm_ptr = (BufferSHM *)shmat(shmid, NULL, 0);

    sem_t *sem_servidor = sem_open(SEM_SERVIDOR_NAME, 0);
    sem_t *sem_cliente = sem_open(SEM_CLIENTE_NAME, 0);

    while (1) {
        int opcion;
        printf("\n=== CLIENTE MEMORIA COMPARTIDA (CON SEMÁFOROS) ===\n");
        printf("1. Buscar por Tipo\n2. Buscar por Marca\n3. Salir\nOpción: ");
        if (scanf("%d", &opcion) != 1) break;
        getchar(); // Limpiar salto de línea

        if (opcion == 3) break;
        if (opcion != 1 && opcion != 2) continue;

        shm_ptr->tipo_busqueda = opcion;
        printf("Ingrese término a buscar: ");
        fgets(shm_ptr->termino, sizeof(shm_ptr->termino), stdin);
        shm_ptr->termino[strcspn(shm_ptr->termino, "\n")] = '\0';

        // --- SINCRONIZACIÓN CON SEMÁFOROS (Sección 6.1 del PDF) ---
        sem_post(sem_servidor); // 1. Despierta al servidor para que lea la consulta
        sem_wait(sem_cliente);  // 2. Se bloquea esperando a que el servidor responda

        printf("\n--- RESPUESTA RECIBIDA POR SHM ---\n%s", shm_ptr->respuesta);
    }

    shmdt(shm_ptr);
    sem_close(sem_servidor);
    sem_close(sem_cliente);
    return 0;
}