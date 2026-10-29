#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <ctype.h>
#include "comun.h"

int main(void) {
    key_t key = ftok(SHM_KEY_PATH, SHM_PROJ_ID);
    if (key == -1) {
        perror("[ERROR CLIENTE SHM] Error en ftok");
        exit(EXIT_FAILURE);
    }

    int shmid = shmget(key, sizeof(BufferSHM), 0666);
    if (shmid == -1) {
        perror("[ERROR CLIENTE SHM] No se encontró Memoria Compartida. ¿Servidor corriendo?");
        exit(EXIT_FAILURE);
    }

    BufferSHM *shm_ptr = (BufferSHM *)shmat(shmid, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("[ERROR CLIENTE SHM] Error en shmat");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_servidor = sem_open(SEM_SERVIDOR_NAME, 0);
    sem_t *sem_cliente = sem_open(SEM_CLIENTE_NAME, 0);

    if (sem_servidor == SEM_FAILED || sem_cliente == SEM_FAILED) {
        perror("[ERROR CLIENTE SHM] Error al abrir semáforos POSIX");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int opcion = 0;
        printf("\n=== CLIENTE MEMORIA COMPARTIDA ===\n");
        printf("1. Buscar por Tipo\n");
        printf("2. Buscar por Marca\n");
        printf("3. Salir\n");
        printf("Opción: ");
        fflush(stdout);

        if (scanf("%d", &opcion) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }

        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        if (opcion == 3) break;
        if (opcion != 1 && opcion != 2) {
            printf("Opción inválida. Intente de nuevo.\n");
            continue;
        }

        shm_ptr->tipo_busqueda = opcion;

        printf("Ingrese término a buscar: ");
        fflush(stdout);

        if (fgets(shm_ptr->termino, sizeof(shm_ptr->termino), stdin) == NULL) {
            continue;
        }
        shm_ptr->termino[strcspn(shm_ptr->termino, "\r\n")] = '\0';

        if (strlen(shm_ptr->termino) == 0) {
            printf("El término buscado no puede estar vacío.\n");
            continue;
        }

        // Sincronización mediante semáforos POSIX
        sem_post(sem_servidor); // Notifica al servidor
        sem_wait(sem_cliente);  // Espera la respuesta del servidor

        printf("\n--- RESPUESTA RECIBIDA POR SHM ---\n%s\n", shm_ptr->respuesta);
    }

    shmdt(shm_ptr);
    sem_close(sem_servidor);
    sem_close(sem_cliente);
    return 0;
}