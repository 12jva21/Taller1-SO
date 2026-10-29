#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include "comun.h"

// Función auxiliar para eliminar espacios al inicio y final de una cadena
void trim(char *str) {
    if (!str) return;
    
    int len = strlen(str);
    while (len > 0 && (isspace((unsigned char)str[len - 1]) || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[--len] = '\0';
    }

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

    // Reserva dinámica de memoria para acumular las coincidencias del dataset[cite: 1]
    size_t cap_acumulada = 1024;
    char *resultado_dinamico = (char *)malloc(cap_acumulada);
    if (!resultado_dinamico) {
        fclose(file);
        snprintf(buffer_salida, max_len, "[ERROR SERVIDOR] Fallo de asignación de memoria dinámica.\n");
        return;
    }
    resultado_dinamico[0] = '\0';

    char termino_limpio[50];
    strncpy(termino_limpio, termino, sizeof(termino_limpio) - 1);
    termino_limpio[sizeof(termino_limpio) - 1] = '\0';
    trim(termino_limpio);

    while (fgets(linea, sizeof(linea), file)) {
        if (linea[0] == '\n' || linea[0] == '\r' || linea[0] == '#') continue;

        char id_v[20] = {0}, tipo_v[50] = {0}, marca_v[50] = {0};
        char modelo_v[50] = {0}, anio_v[20] = {0}, hp_v[20] = {0};

        char *token = strtok(linea, ",");
        if (token) strncpy(id_v, token, sizeof(id_v) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(tipo_v, token, sizeof(tipo_v) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(marca_v, token, sizeof(marca_v) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(modelo_v, token, sizeof(modelo_v) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(anio_v, token, sizeof(anio_v) - 1);

        token = strtok(NULL, ",\r\n");
        if (token) strncpy(hp_v, token, sizeof(hp_v) - 1);

        trim(tipo_v);
        trim(marca_v);
        trim(modelo_v);

        if (strcasecmp(tipo_v, "Tipo") == 0 && strcasecmp(marca_v, "Marca") == 0) continue;

        int coincide = 0;
        if (tipo == BUSCAR_POR_TIPO && strcasecmp(tipo_v, termino_limpio) == 0) {
            coincide = 1;
        } else if (tipo == BUSCAR_POR_MARCA && strcasecmp(marca_v, termino_limpio) == 0) {
            coincide = 1;
        }

        if (coincide) {
            char temp[256];
            snprintf(temp, sizeof(temp), "[ID: %s] Tipo: %s | Marca: %s | Modelo: %s | Año: %s | HP: %s\n", 
                     id_v, tipo_v, marca_v, modelo_v, anio_v, hp_v);

            // Reasignación dinámica si excede la capacidad actual[cite: 1]
            if (strlen(resultado_dinamico) + strlen(temp) + 1 > cap_acumulada) {
                cap_acumulada *= 2;
                char *nuevo_ptr = (char *)realloc(resultado_dinamico, cap_acumulada);
                if (nuevo_ptr) {
                    resultado_dinamico = nuevo_ptr;
                } else {
                    break;
                }
            }
            strcat(resultado_dinamico, temp);
            coincidencias++;
        }
    }

    fclose(file);

    if (coincidencias == 0) {
        snprintf(buffer_salida, max_len, "No se encontraron coincidencias para '%s'.\n", termino_limpio);
    } else {
        strncpy(buffer_salida, resultado_dinamico, max_len - 1);
        buffer_salida[max_len - 1] = '\0';
    }

    // Liberación explícita de memoria dinámica[cite: 1]
    free(resultado_dinamico);
}

int main(void) {
    printf("[SERVIDOR] Iniciando servidor único con acceso directo a disco...\n");
    fflush(stdout);

    // 1. Configuración de Memoria Compartida (SHM)
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

    // 2. Semáforos POSIX[cite: 5]
    sem_unlink(SEM_CLIENTE_NAME);
    sem_unlink(SEM_SERVIDOR_NAME);
    sem_t *sem_servidor = sem_open(SEM_SERVIDOR_NAME, O_CREAT, 0666, 0);
    sem_t *sem_cliente  = sem_open(SEM_CLIENTE_NAME, O_CREAT, 0666, 0);

    if (sem_servidor == SEM_FAILED || sem_cliente == SEM_FAILED) {
        perror("[ERROR SERVIDOR] Error creando semáforos POSIX");
        exit(1);
    }

    // 3. FIFO Servidor
    unlink(FIFO_SERVIDOR);
    if (mkfifo(FIFO_SERVIDOR, 0666) == -1) {
        perror("[ERROR SERVIDOR] Error al crear FIFO servidor");
    }

    // Apertura única en modo O_RDWR | O_NONBLOCK para evitar pérdidas de paquetes en el FIFO
    int fd_serv = open(FIFO_SERVIDOR, O_RDWR | O_NONBLOCK);
    if (fd_serv == -1) {
        perror("[ERROR SERVIDOR] No se pudo abrir FIFO_SERVIDOR");
        exit(1);
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

            sem_post(sem_cliente);
        }

        // --- B. REVISAR FIFO ---
        Solicitud sol;
        ssize_t bytes = read(fd_serv, &sol, sizeof(Solicitud));
        if (bytes == sizeof(Solicitud)) {
            printf("[SERVIDOR] Consulta recibida por FIFO desde PID %d -> Buscando en Disco...\n", sol.cliente_pid);
            fflush(stdout);

            int fd_cli = open(FIFO_CLIENTE, O_WRONLY);
            if (fd_cli != -1) {
                char respuesta[4096];
                buscar_en_disco_csv(sol.tipo, sol.termino, respuesta, sizeof(respuesta));
                write(fd_cli, respuesta, strlen(respuesta) + 1);
                close(fd_cli);
            }
        }

        usleep(10000);
    }

    // Liberación y limpieza de recursos
    close(fd_serv);
    sem_close(sem_servidor);
    sem_close(sem_cliente);
    sem_unlink(SEM_SERVIDOR_NAME);
    sem_unlink(SEM_CLIENTE_NAME);
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    unlink(FIFO_SERVIDOR);

    return 0;
}