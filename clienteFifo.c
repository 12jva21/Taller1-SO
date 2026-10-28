#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FIFO_SERVIDOR "/tmp/fifo_servidor"
#define FIFO_CLIENTE "/tmp/fifo_cliente"

#define BUSCAR_POR_TIPO 1
#define BUSCAR_POR_MARCA 2

// Estructura idéntica a la del servidor para el envío de peticiones
typedef struct {
    int tipo;               // BUSCAR_POR_TIPO o BUSCAR_POR_MARCA
    char termino[50];       // Término ingresado por el usuario
    pid_t cliente_pid;      // PID del proceso cliente
} Solicitud;

void limpiar_buffer_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int mostrar_menu(void) {
    int opcion;
    printf("\n====================================\n");
    printf("     CLIENTE BÚSQUEDA VÍA FIFO      \n");
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

int main(void) {
    int fd_servidor, fd_cliente;
    Solicitud sol;
    char buffer_respuesta[1024];
    ssize_t bytes_leidos;

    // Crear la FIFO del cliente si no existe previamente
    mkfifo(FIFO_CLIENTE, 0666);

    while (1) {
        int opcion = mostrar_menu();

        if (opcion == 3) {
            printf("\nFinalizando el programa cliente...\n");
            break;
        }

        if (opcion != 1 && opcion != 2) {
            printf("\n[ERROR] Opción no válida. Ingrese 1, 2 o 3.\n");
            continue;
        }

        // Configurar la solicitud
        sol.tipo = (opcion == 1) ? BUSCAR_POR_TIPO : BUSCAR_POR_MARCA;
        sol.cliente_pid = getpid();

        printf("Ingrese el término a buscar: ");
        if (fgets(sol.termino, sizeof(sol.termino), stdin) != NULL) {
            // Eliminar el salto de línea '\n' capturado por fgets
            sol.termino[strcspn(sol.termino, "\n")] = '\0';
        }

        // 1. Abrir la FIFO del servidor para enviar la estructura con los datos
        fd_servidor = open(FIFO_SERVIDOR, O_WRONLY);
        if (fd_servidor == -1) {
            perror("[ERROR] No se pudo conectar con el servidor (Asegúrese de ejecutar ./servidor primero)");
            continue;
        }

        write(fd_servidor, &sol, sizeof(Solicitud));
        close(fd_servidor);

        // 2. Abrir la FIFO del cliente para leer la respuesta enviada por el servidor
        fd_cliente = open(FIFO_CLIENTE, O_RDONLY);
        if (fd_cliente == -1) {
            perror("[ERROR] Fallo al abrir la FIFO de respuesta del cliente");
            continue;
        }

        printf("\n--- RESULTADOS RECIBIDOS DEL SERVIDOR ---\n");
        while ((bytes_leidos = read(fd_cliente, buffer_respuesta, sizeof(buffer_respuesta) - 1)) > 0) {
            buffer_respuesta[bytes_leidos] = '\0';
            printf("%s", buffer_respuesta);
        }
        printf("-----------------------------------------\n");

        close(fd_cliente);
    }

    // Eliminar el nodo FIFO del cliente al salir limpiamente
    unlink(FIFO_CLIENTE);
    return 0;
}