#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include "comun.h"

int main(void) {
    // Asegurar que la tubería de respuesta del cliente existe
    unlink(FIFO_CLIENTE);
    if (mkfifo(FIFO_CLIENTE, 0666) == -1) {
        perror("[ERROR CLIENTE] No se pudo crear FIFO_CLIENTE");
    }

    while (1) {
        int opcion = 0;
        printf("\n=== CLIENTE FIFO ===\n");
        printf("1. Buscar por Tipo\n");
        printf("2. Buscar por Marca\n");
        printf("3. Salir\n");
        printf("Opción: ");
        fflush(stdout);

        if (scanf("%d", &opcion) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF); // Limpiar buffer
            continue;
        }

        // Limpiar completamente el buffer hasta el salto de línea
        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        if (opcion == 3) break;
        if (opcion != 1 && opcion != 2) {
            printf("Opción no válida. Intente de nuevo.\n");
            continue;
        }

        Solicitud sol;
        memset(&sol, 0, sizeof(Solicitud));
        sol.tipo = opcion;
        sol.cliente_pid = getpid();

        printf("Ingrese término a buscar: ");
        fflush(stdout);

        if (fgets(sol.termino, sizeof(sol.termino), stdin) == NULL) {
            continue;
        }
        
        // Quitar saltos de línea \n o \r al final
        sol.termino[strcspn(sol.termino, "\r\n")] = '\0';

        // Si la cadena quedó vacía por presionar Enter sin querer, reintentar
        if (strlen(sol.termino) == 0) {
            printf("El término buscado no puede estar vacío.\n");
            continue;
        }

        // 1. Enviar la solicitud al servidor
        int fd_serv = open(FIFO_SERVIDOR, O_WRONLY);
        if (fd_serv == -1) {
            perror("\n[ERROR CLIENTE] Servidor no disponible (FIFO_SERVIDOR)");
            printf("Asegúrate de que el servidor esté corriendo en otra terminal.\n");
            continue;
        }
        write(fd_serv, &sol, sizeof(Solicitud));
        close(fd_serv);

        // 2. Abrir tubería cliente para recibir respuesta
        int fd_cli = open(FIFO_CLIENTE, O_RDONLY);
        if (fd_cli != -1) {
            char respuesta[4096];
            memset(respuesta, 0, sizeof(respuesta));

            ssize_t bytes = read(fd_cli, respuesta, sizeof(respuesta) - 1);
            if (bytes > 0) {
                respuesta[bytes] = '\0';
                printf("\n--- RESPUESTA RECIBIDA VÍA FIFO ---\n%s\n", respuesta);
            } else {
                printf("\n[ERROR CLIENTE] No se recibieron datos del servidor.\n");
            }
            close(fd_cli);
        } else {
            perror("\n[ERROR CLIENTE] No se pudo abrir FIFO_CLIENTE para respuesta");
        }
    }

    unlink(FIFO_CLIENTE);
    return 0;
}