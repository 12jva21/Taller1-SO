#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "comun.h"

int main(void) {
    mkfifo(FIFO_CLIENTE, 0666);

    while (1) {
        int opcion;
        printf("\n=== CLIENTE FIFO ===\n");
        printf("1. Buscar por Tipo\n2. Buscar por Marca\n3. Salir\nOpción: ");
        if (scanf("%d", &opcion) != 1) break;
        getchar();

        if (opcion == 3) break;
        if (opcion != 1 && opcion != 2) continue;

        Solicitud sol;
        sol.tipo = opcion;
        sol.cliente_pid = getpid();
        printf("Ingrese término a buscar: ");
        fgets(sol.termino, sizeof(sol.termino), stdin);
        sol.termino[strcspn(sol.termino, "\n")] = '\0';

        int fd_serv = open(FIFO_SERVIDOR, O_WRONLY);
        if (fd_serv == -1) {
            perror("[ERROR] Servidor no disponible");
            continue;
        }
        write(fd_serv, &sol, sizeof(Solicitud));
        close(fd_serv);

        int fd_cli = open(FIFO_CLIENTE, O_RDONLY);
        if (fd_cli != -1) {
            char respuesta[4096];
            ssize_t bytes = read(fd_cli, respuesta, sizeof(respuesta) - 1);
            if (bytes > 0) {
                respuesta[bytes] = '\0';
                printf("\n--- RESPUESTA RECIBIDA VÍA FIFO ---\n%s", respuesta);
            }
            close(fd_cli);
        }
    }

    unlink(FIFO_CLIENTE);
    return 0;
}