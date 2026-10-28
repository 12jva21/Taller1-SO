#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define CLAVE_SHM 12345
#define TAM_MEMORIA 8192

#define BUSCAR_POR_TIPO 1
#define BUSCAR_POR_MARCA 2

void limpiar_buffer_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int mostrar_menu(void) {
    int opcion;
    printf("\n====================================\n");
    printf("   CLIENTE BÚSQUEDA VÍA MEMORIA (SHM)\n");
    printf("====================================\n");
    printf("1. Buscar por Tipo (ej. SUV, Sedan)\n");
    printf("2. Buscar por Marca (ej. Toyota, Honda)\n");
    printf("3. Salir\n");
    printf("Seleccione una opción: ");
    
    if (scanf("%d", &opcion) != 1) {
        limpiar_buffer_stdin();
        return -1;
    }
    limpiar_buffer_stdin();
    return opcion;
}

void buscar_en_memoria_compartida(int tipo_busqueda, const char *termino, const char *mem_ptr) {
    char copia_csv[TAM_MEMORIA];
    // Hacer una copia local del segmento de memoria compartida para tokenizar con seguridad
    strncpy(copia_csv, mem_ptr, sizeof(copia_csv) - 1);
    copia_csv[sizeof(copia_csv) - 1] = '\0';

    char *linea = strtok(copia_csv, "\n");
    int coincidencias = 0;

    printf("\n--- RESULTADOS (CONSULTA DIRECTA EN MEMORIA) ---\n");

    while (linea != NULL) {
        char tipo_veh[50], marca_veh[50], modelo_veh[50];
        
        // Parsear cada línea en formato CSV: Tipo,Marca,Modelo
        if (sscanf(linea, "%49[^,],%49[^,],%49[^\n]", tipo_veh, marca_veh, modelo_veh) == 3) {
            const char *campo_evaluar = (tipo_busqueda == BUSCAR_POR_TIPO) ? tipo_veh : marca_veh;

            if (strcasecmp(campo_evaluar, termino) == 0) {
                printf("Coincidencia: [%s] %s - %s\n", tipo_veh, marca_veh, modelo_veh);
                coincidencias++;
            }
        }
        linea = strtok(NULL, "\n");
    }

    if (coincidencias == 0) {
        printf("No se encontraron resultados para la búsqueda solicitada.\n");
    }
    printf("--------------------------------------------------\n");
}

int main(void) {
    int shmid;
    char *mem_compartida;

    // 1. Obtener el identificador del segmento de Memoria Compartida creado por el servidor
    shmid = shmget((key_t)CLAVE_SHM, TAM_MEMORIA, 0666);
    if (shmid == -1) {
        perror("[ERROR] No se pudo acceder a la Memoria Compartida (¿El servidor está ejecutándose?)");
        exit(EXIT_FAILURE);
    }

    // 2. Vincular (attach) el segmento de memoria al espacio de direcciones del cliente
    mem_compartida = (char *)shmat(shmid, NULL, 0);
    if (mem_compartida == (char *)-1) {
        perror("[ERROR] Fallo al vincular la Memoria Compartida");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int opcion = mostrar_menu();

        if (opcion == 3) {
            printf("\nDesconectando del segmento de memoria y saliendo...\n");
            break;
        }

        if (opcion != 1 && opcion != 2) {
            printf("\n[ERROR] Opción no válida. Ingrese 1, 2 o 3.\n");
            continue;
        }

        char termino[50];
        printf("Ingrese el término a buscar: ");
        if (fgets(termino, sizeof(termino), stdin) != NULL) {
            termino[strcspn(termino, "\n")] = '\0';
        }

        // Consultar la memoria compartida directamente
        buscar_en_memoria_compartida(opcion, termino, mem_compartida);
    }

    // 3. Desvincular la memoria compartida al finalizar la ejecución del cliente
    shmdt(mem_compartida);
    return 0;
}