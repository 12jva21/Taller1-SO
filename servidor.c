#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define ARCHIVO_CSV "vehiculos.csv"
#define FIFO_SERVIDOR "/tmp/fifo_servidor"
#define FIFO_CLIENTE "/tmp/fifo_cliente"

#define CLAVE_SHM 12345
#define TAM_MEMORIA 8192 // 8 KB para la base de datos en RAM

#define BUSCAR_POR_TIPO 1
#define BUSCAR_POR_MARCA 2

// Estructura de solicitud enviada desde clienteFifo.c
typedef struct {
    int tipo;               // BUSCAR_POR_TIPO o BUSCAR_POR_MARCA
    char termino[50];       // Término a buscar
    pid_t cliente_pid;      // PID del proceso cliente
} Solicitud;

// Función para cargar el archivo CSV a la Memoria Compartida
int cargar_csv_en_memoria_compartida(char *mem_ptr) {
    FILE *file = fopen(ARCHIVO_CSV, "r");
    if (!file) {
        perror("[ERROR SERVIDOR] No se pudo abrir el archivo vehiculos.csv");
        return -1;
    }

    memset(mem_ptr, 0, TAM_MEMORIA);
    size_t bytes_leidos = fread(mem_ptr, 1, TAM_MEMORIA - 1, file);
    fclose(file);

    printf("[SERVIDOR] Base de datos 'vehiculos.csv' cargada en Memoria Compartida (%2zd bytes).\n", bytes_leidos);
    return 0;
}

// Función para procesar la búsqueda en el CSV y responder por la FIFO
void procesar_busqueda_fifo(const Solicitud *sol, int fd_respuesta, const char *mem_ptr) {
    char copia_csv[TAM_MEMORIA];
    strncpy(copia_csv, mem_ptr, sizeof(copia_csv));

    char *linea = strtok(copia_csv, "\n");
    int coincidencias = 0;
    char linea_resultado[256];

    while (linea != NULL) {
        char tipo_veh[50], marca_veh[50], modelo_veh[50];
        
        // Parsear formato CSV esperado: Tipo,Marca,Modelo (o separado por comas/espacios)
        if (sscanf(linea, "%49[^,],%49[^,],%49[^\n]", tipo_veh, marca_veh, modelo_veh) == 3) {
            const char *campo_evaluar = (sol->tipo == BUSCAR_POR_TIPO) ? tipo_veh : marca_veh;

            if (strcasecmp(campo_evaluar, sol->termino) == 0) {
                snprintf(linea_resultado, sizeof(linea_resultado),
                         "Coincidencia: [%s] %s - %s\n", tipo_veh, marca_veh, modelo_veh);
                write(fd_respuesta, linea_resultado, strlen(linea_resultado));
                coincidencias++;
            }
        }
        linea = strtok(NULL, "\n");
    }

    if (coincidencias == 0) {
        const char *msg_vacio = "No se encontraron resultados para la búsqueda solicitada.\n";
        write(fd_respuesta, msg_vacio, strlen(msg_vacio));
    }
}

int main(void) {
    int shmid;
    char *mem_compartida;
    int fd_servidor, fd_cliente;
    Solicitud sol;

    printf("==================================================\n");
    printf("   SERVIDOR CENTRAL DE BÚSQUEDA (FIFO + SHM)      \n");
    printf("==================================================\n");

    // 1. Inicializar la Memoria Compartida (SHM)
    shmid = shmget((key_t)CLAVE_SHM, TAM_MEMORIA, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("[ERROR SERVIDOR] Fallo al crear el segmento de Memoria Compartida");
        exit(EXIT_FAILURE);
    }

    mem_compartida = (char *)shmat(shmid, NULL, 0);
    if (mem_compartida == (char *)-1) {
        perror("[ERROR SERVIDOR] Fallo al adjuntar la Memoria Compartida");
        exit(EXIT_FAILURE);
    }

    // 2. Cargar datos del CSV a la memoria en RAM
    if (cargar_csv_en_memoria_compartida(mem_compartida) == -1) {
        printf("[ADVERTENCIA] El servidor iniciará pero la memoria está vacía.\n");
    }

    // 3. Crear el canal de FIFO principal para el servidor
    mkfifo(FIFO_SERVIDOR, 0666);
    printf("[SERVIDOR] Escuchando peticiones en la FIFO: %s\n", FIFO_SERVIDOR);
    printf("[SERVIDOR] Memoria compartida lista con ID: %d (Clave: %d)\n", shmid, CLAVE_SHM);
    printf("--------------------------------------------------\n");

    // 4. Bucle principal de atención de peticiones
    while (1) {
        // Abrir la FIFO en modo lectura (se bloquea hasta que un cliente se conecta)
        fd_servidor = open(FIFO_SERVIDOR, O_RDONLY);
        if (fd_servidor == -1) {
            perror("[ERROR SERVIDOR] Error al abrir FIFO_SERVIDOR");
            break;
        }

        ssize_t bytes = read(fd_servidor, &sol, sizeof(Solicitud));
        close(fd_servidor); // Cerrar canal de recepción para la siguiente lectura

        if (bytes > 0) {
            printf("[PETICIÓN FIFO RECIBIDA] Cliente PID: %d | Tipo: %d | Término: \"%s\"\n",
                   sol.cliente_pid, sol.tipo, sol.termino);

            // Responder al cliente abriendo la FIFO que creó el cliente
            fd_cliente = open(FIFO_CLIENTE, O_WRONLY);
            if (fd_cliente != -1) {
                procesar_busqueda_fifo(&sol, fd_cliente, mem_compartida);
                close(fd_cliente);
            } else {
                perror("[ERROR SERVIDOR] No se pudo conectar a la FIFO del cliente");
            }
        }
    }

    // Limpieza de recursos al salir
    shmdt(mem_compartida);
    shmctl(shmid, IPC_RMID, NULL);
    unlink(FIFO_SERVIDOR);

    return 0;
}