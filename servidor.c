#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <ctype.h>
#include "comun.h"

// Función auxiliar para eliminar espacios al inicio y al final de una cadena
void trim(char *str) {
    if (!str) return;
    
    // Quitar espacios/saltos al final
    int len = strlen(str);
    while (len > 0 && (isspace((unsigned char)str[len - 1]) || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[--len] = '\0';
    }

    // Quitar espacios al inicio
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

void buscar_en_disco_csv(int tipo, const char *termino, char *buffer_salida, size_t max_len) {
    FILE *file = fopen(ARCHIVO_CSV, "r");
    if (!file) {
        snprintf(buffer_salida, max_len, "[ERROR SERVIDOR] No se pudo abrir el archivo CSV (%s).\n", ARCHIVO_CSV);
        return;
    }

    char linea[256];
    int coincidencias = 0;
    buffer_salida[0] = '\0';

    // Copia del término buscado y limpio de espacios
    char termino_limpio[50];
    strncpy(termino_limpio, termino, sizeof(termino_limpio) - 1);
    termino_limpio[sizeof(termino_limpio) - 1] = '\0';
    trim(termino_limpio);

    while (fgets(linea, sizeof(linea), file)) {
        // Ignorar líneas vacías o comentarios
        if (linea[0] == '\n' || linea[0] == '\r' || linea[0] == '#') continue;

        char tipo_v[50] = {0}, marca_v[50] = {0}, modelo_v[50] = {0};

        // Parseo seguro usando strtok para separar por comas
        char *token = strtok(linea, ",");
        if (token) strncpy(tipo_v, token, sizeof(tipo_v) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(marca_v, token, sizeof(marca_v) - 1);

        token = strtok(NULL, ",\r\n");
        if (token) strncpy(modelo_v, token, sizeof(modelo_v) - 1);

        // Limpiar espacios extra alrededor de las variables extraídas
        trim(tipo_v);
        trim(marca_v);
        trim(modelo_v);

        // Omitir fila de encabezado si existe en el CSV
        if (strcasecmp(tipo_v, "tipo") == 0 && strcasecmp(marca_v, "marca") == 0) continue;

        int coincide = 0;
        if (tipo == BUSCAR_POR_TIPO && strcasecmp(tipo_v, termino_limpio) == 0) {
            coincide = 1;
        } else if (tipo == BUSCAR_POR_MARCA && strcasecmp(marca_v, termino_limpio) == 0) {
            coincide = 1;
        }

        if (coincide) {
            char temp[256];
            snprintf(temp, sizeof(temp), "Tipo: %s | Marca: %s | Modelo: %s\n", tipo_v, marca_v, modelo_v);
            if (strlen(buffer_salida) + strlen(temp) < max_len - 1) {
                strcat(buffer_salida, temp);
                coincidencias++;
            }
        }
    }

    fclose(file);

    if (coincidencias == 0) {
        snprintf(buffer_salida, max_len, "No se encontraron coincidencias para '%s'.\n", termino_limpio);
    }
}

int main(void) {
    printf("[SERVIDOR] Iniciando servidor único (sin fork) con acceso directo a disco...\n");
    fflush(stdout);

    // 1. Configuración Memoria Compartida (SHM)
    key_t key = ftok(SHM_KEY_PATH, SHM_PROJ_ID);
    if (key == -1) {
        perror("[ERROR SERVIDOR] ftok falló");
        exit(1);
    }

    int shmid = shmget(key, sizeof(BufferSHM), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("[ERROR SERVIDOR] shmget falló");
        exit(1);
    }

    BufferSHM *shm_ptr = (BufferSHM *)shmat(shmid, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("[ERROR SERVIDOR] shmat falló");
        exit(1);
    }
    memset(shm_ptr, 0, sizeof(BufferSHM));

    // 2. Configuración Semáforos POSIX
    sem_unlink(SEM_CLIENTE_NAME);
    sem_unlink(SEM_SERVIDOR_NAME);
    sem_t *sem_servidor = sem_open(SEM_SERVIDOR_NAME, O_CREAT, 0666, 0);
    sem_t *sem_cliente  = sem_open(SEM_CLIENTE_NAME, O_CREAT, 0666, 0);

    if (sem_servidor == SEM_FAILED || sem_cliente == SEM_FAILED) {
        perror("[ERROR SERVIDOR] Error creando semáforos POSIX");
        exit(1);
    }

    // 3. Configuración FIFO Servidor
    unlink(FIFO_SERVIDOR);
    if (mkfifo(FIFO_SERVIDOR, 0666) == -1) {
        perror("[ERROR SERVIDOR] Error al crear FIFO servidor");
    }

    printf("[SERVIDOR] Listo. Atendiendo peticiones por FIFO y SHM...\n");
    fflush(stdout);

    while (1) {
        // --- A. REVISAR MEMORIA COMPARTIDA (SHM) ---
        if (sem_trywait(sem_servidor) == 0) {
            if (shm_ptr->tipo_busqueda == COMANDO_SALIR) break;

            printf("[SERVIDOR] Consulta recibida por SHM -> Buscando en Disco...\n");
            fflush(stdout);
            buscar_en_disco_csv(shm_ptr->tipo_busqueda, shm_ptr->termino, shm_ptr->respuesta, sizeof(shm_ptr->respuesta));

            sem_post(sem_cliente); // Avisa al cliente SHM que la respuesta está lista
        }

        // --- B. REVISAR FIFO (Modo No Bloqueante) ---
        int fd_serv = open(FIFO_SERVIDOR, O_RDONLY | O_NONBLOCK);
        if (fd_serv != -1) {
            Solicitud sol;
            ssize_t bytes = read(fd_serv, &sol, sizeof(Solicitud));
            close(fd_serv);

            if (bytes == sizeof(Solicitud)) {
                printf("[SERVIDOR] Consulta recibida por FIFO desde PID %d -> Buscando en Disco...\n", sol.cliente_pid);
                fflush(stdout);

                // Esperar a abrir la tubería del cliente
                int fd_cli = open(FIFO_CLIENTE, O_WRONLY);
                if (fd_cli != -1) {
                    char respuesta[4096];
                    buscar_en_disco_csv(sol.tipo, sol.termino, respuesta, sizeof(respuesta));
                    
                    // Enviar la respuesta incluyendo el terminador nulo '\0'
                    write(fd_cli, respuesta, strlen(respuesta) + 1);
                    close(fd_cli);
                } else {
                    perror("[ERROR SERVIDOR] No se pudo abrir FIFO_CLIENTE para respuesta");
                }
            }
        }

        usleep(50000); // Pausa de 50ms para evitar alto uso de CPU
    }

    // Limpieza de recursos al finalizar
    sem_close(sem_servidor);
    sem_close(sem_cliente);
    sem_unlink(SEM_SERVIDOR_NAME);
    sem_unlink(SEM_CLIENTE_NAME);
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    unlink(FIFO_SERVIDOR);

    return 0;
}