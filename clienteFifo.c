#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "comun.h"

int main(void) {
    // Crear el FIFO exclusivo para las respuestas de este cliente
    mkfifo(FIFO_CLIENTE, 0666);

    while (1) {
        int opcion;
        printf("\n=== CLIENTE FIFO ===\n");
        printf("1. Buscar por Tipo\n2. Buscar por Marca\n3. Salir\nOpción: ");
        
        if (scanf("%d", &opcion) != 1) {
            // Limpiar entrada no válida si no ingresan números
            while (getchar() != '\n');
            continue;
        }

        // Limpiar el salto de línea '\n' dejado por scanf en el buffer de stdin
        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        if (opcion == 3) break;
        if (opcion != 1 && opcion != 2) {
            printf("Opción inválida. Intente de nuevo.\n");
            continue;
        }

        Solicitud sol;
        sol.tipo = opcion;
        sol.cliente_pid = getpid();

        printf("Ingrese término a buscar: ");
        if (fgets(sol.termino, sizeof(sol.termino), stdin) == NULL) {
            continue;
        }
        // Eliminar el salto de línea al final de la cadena
        sol.termino[strcspn(sol.termino, "\r\n")] = '\0';

        // 1. Enviar solicitud al servidor
        int fd_serv = open(FIFO_SERVIDOR, O_WRONLY);
        if (fd_serv == -1) {
            perror("\n[ERROR] Servidor no disponible (FIFO_SERVIDOR)");
            continue;
        }
        write(fd_serv, &sol, sizeof(Solicitud));
        close(fd_serv);

        // 2. Abrir FIFO cliente para recibir la respuesta (se bloquea hasta que el servidor escriba)
        int fd_cli = open(FIFO_CLIENTE, O_RDONLY);
        if (fd_cli != -1) {
            char respuesta[4096];
            memset(respuesta, 0, sizeof(respuesta));

            ssize_t bytes = read(fd_cli, respuesta, sizeof(respuesta) - 1);
            if (bytes > 0) {
                respuesta[bytes] = '\0';
                printf("\n--- RESPUESTA RECIBIDA VÍA FIFO ---\n%s", respuesta);
            } else {
                printf("\n[ERROR] No se recibieron datos del servidor.\n");
            }
            close(fd_cli);
        } else {
            perror("\n[ERROR] No se pudo abrir FIFO_CLIENTE para lectura");
        }
    }

    // Limpieza de tubería al salir del bucle
    unlink(FIFO_CLIENTE);
    return 0;
}