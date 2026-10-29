void buscar_en_disco_csv(int tipo, const char *termino, char *buffer_salida, size_t max_len) {
    FILE *file = fopen(ARCHIVO_CSV, "r");
    if (!file) {
        snprintf(buffer_salida, max_len, "[ERROR SERVIDOR] No se pudo abrir el archivo CSV (%s).\n", ARCHIVO_CSV);
        return;
    }

    char linea[256];
    int coincidencias = 0;
    buffer_salida[0] = '\0';

    char termino_limpio[50];
    strncpy(termino_limpio, termino, sizeof(termino_limpio) - 1);
    termino_limpio[sizeof(termino_limpio) - 1] = '\0';
    trim(termino_limpio);

    while (fgets(linea, sizeof(linea), file)) {
        if (linea[0] == '\n' || linea[0] == '\r' || linea[0] == '#') continue;

        char id_v[20] = {0}, tipo_v[50] = {0}, marca_v[50] = {0};
        char modelo_v[50] = {0}, anio_v[20] = {0}, hp_v[20] = {0};

        // Parseo de las 6 columnas separadas por coma
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

        // Limpieza de espacios extra
        trim(tipo_v);
        trim(marca_v);
        trim(modelo_v);

        // Omitir la línea del encabezado
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