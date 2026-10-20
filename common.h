#ifndef COMMON_H
#define COMMON_H

#define FIFO_REQ "/tmp/fifo_request"
#define FIFO_RESP "/tmp/fifo_response"
#define BUFFER_SIZE 256

// Tipos de búsqueda adaptados al dataset
typedef enum {
    BUSCAR_POR_TIPO = 1,   // Auto o Moto
    BUSCAR_POR_MARCA = 2,  // Ej: Ferrari, Ducati, Porsche
    SALIR = 3
} TipoBusqueda;

typedef struct {
    TipoBusqueda tipo;
    char termino[BUFFER_SIZE];
} Solicitud;

typedef struct {
    char resultado[BUFFER_SIZE];
    int fin; // 1 si no hay más resultados
} Respuesta;

#endif