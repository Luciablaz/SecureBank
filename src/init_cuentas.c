#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/cuenta.h"
#include "../include/config.h"

void crearCuentasIniciales() {
    // Se obtiene la configuración actual del sistema, incluyendo la 
    // ruta donde debe guardarse el archivo de cuentas
    Config config = leerConfiguracion(CONFIG_FILE);
    
    // Si el archivo ya existe, no se sobrescribe para evitar pérdida de información
    if (access(config.archivo_cuentas, F_OK) == 0) {
        printf("El archivo de cuentas ya existe.\n");
        return;
    }

    // Se inicializa un array de cuentas predeterminadas con datos de prueba
    Cuenta cuentas[] = {
        {1001, "John Doe", 5000.00, 0},
        {1002, "Jane Smith", 3000.00, 0},
        {1003, "Carlos Pérez", 10000.00, 0},
        {1004, "Ana López", 7500.00, 0}
    };

    // Se abre el archivo de cuentas en modo escritura binaria (wb)
    FILE *archivo = fopen(config.archivo_cuentas, "wb");
    if (!archivo) {
        perror("Error al crear el archivo de cuentas.");
        exit(EXIT_FAILURE);
    }

    // Se escriben las 4 cuentas al archivo y se cierra el archivo correctamente
    fwrite(cuentas, sizeof(Cuenta), 4, archivo);
    fclose(archivo);

    printf("¡Archivo de cuentas creado con éxito!\n");
}

// Punto de entrada del programa. Simplemente llama a la función que genera el archivo de cuentas
int main() {
    crearCuentasIniciales();
    return 0;
}