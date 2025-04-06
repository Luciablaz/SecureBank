#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/config.h"

// Función que abre y lee el archivo config.txt
// y devuelve una estructura Config con los valores encontrados
Config leerConfiguracion(const char *ruta) {
    FILE *archivo = fopen(ruta, "r");
    if (!archivo) {
        perror("Error al abrir config.txt");
        exit(1);
    }
    Config config;
    char linea[100];
    // Inicialización de todos los campos de la estructura Config por si alguna 
    // línea no estuviera presente en el archivo
    config.limite_retiro = 0;
    config.limite_transferencia = 0;
    config.umbral_retiros = 0;
    config.umbral_transferencias = 0;
    config.num_hilos = 0;
    strcpy(config.archivo_cuentas, "");
    strcpy(config.archivo_log, "");
    // Bucle que lee el archivo línea por línea
    while (fgets(linea, sizeof(linea), archivo)) {
        // Se descartan las líneas vacías y los comentarios
        if (linea[0] == '#' || strlen(linea) < 3) continue;
        // Se extraen los distintos valores numéricos de configuración 
        // del banco relacionados con límites y umbrales
        if (strstr(linea, "LIMITE_RETIRO"))
            sscanf(linea, "LIMITE_RETIRO=%d", &config.limite_retiro);
        else if (strstr(linea, "LIMITE_TRANSFERENCIA"))
            sscanf(linea, "LIMITE_TRANSFERENCIA=%d", &config.limite_transferencia);
        else if (strstr(linea, "UMBRAL_RETIROS"))
            sscanf(linea, "UMBRAL_RETIROS=%d", &config.umbral_retiros);
        else if (strstr(linea, "UMBRAL_TRANSFERENCIAS"))
            sscanf(linea, "UMBRAL_TRANSFERENCIAS=%d", &config.umbral_transferencias);
        else if (strstr(linea, "NUM_HILOS"))
            sscanf(linea, "NUM_HILOS=%d", &config.num_hilos);
        else if (strstr(linea, "ARCHIVO_CUENTAS"))
            sscanf(linea, "ARCHIVO_CUENTAS=%s", config.archivo_cuentas);
        else if (strstr(linea, "ARCHIVO_LOG"))
            sscanf(linea, "ARCHIVO_LOG=%s", config.archivo_log);
    }
    // Cierra el archivo y devuelve la estructura Config ya cargada
    fclose(archivo);
    return config;
}
