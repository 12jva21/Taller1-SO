#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "comun.h"

int main(void) {
    // 1. Asegurar que la tubería de respuesta del cliente existe
    unlink(FIFO_CLIENTE);
    if (mkfifo(FIFO_CLIENTE, 0666) == -1) {
        perror("[ERROR CLIENTE] No se pudo crear FIFO_CLIENTE");
    }

    while (1) {
        int opcion;
        printf("\n=== CLIENTE FIFO ===\n");
        printf("1. Buscar por Tipo\n");
        printf("2. Buscar por Marca\n");
        printf("3. Salir\n");
        printf("Opción: ");

        if (scanf("%d", &opcion) != 1) {
            while (getchar() != '\n'); // Limpiar entrada inválida
            continue;
        }

        // Limpiar el salto de línea residual de scanf
        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        if (opcion == 3) break;
        if (opcion != 1 && opcion != 2) {
            printf("Opción no válida. Intente de nuevo.\n");
            continue;
        }

        Solicitud sol;
        sol.tipo = opcion;
        sol.cliente_pid = getpid();

        printf("Ingrese término a buscar: ");
        if (fgets(sol.termino, sizeof(sol.termino), stdin) == NULL) {
            continue;
        }
        sol.termino[strcspn(sol.termino, "\r\n")] = '\0';

        // 2. Enviar la solicitud al servidor
        int fd_serv = open(FIFO_SERVIDOR, O_WRONLY);
        if (fd_serv == -1) {
            perror("\n[ERROR CLIENTE] Servidor no disponible (FIFO_SERVIDOR)");
            printf("Asegúrate de que el servidor esté corriendo en otra terminal.\n");
            continue;
        }
        write(fd_serv, &sol, sizeof(Solicitud));
        close(fd_serv);

        // 3. Abrir la tubería cliente en modo lectura (se bloquea hasta que el servidor abra su extremo para escribir)
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
            perror("\n[ERROR CLIENTE] No se pudo abrir FIFO_CLIENTE para recibir la respuesta");
        }
    }

    // Limpieza final
    unlink(FIFO_CLIENTE);
    return 0;
}