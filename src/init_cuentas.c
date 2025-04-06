#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/cuenta.h"
#include "../include/config.h"

void crear_cuentas_iniciales() {
    // Leer la configuración desde config.txt usando CONFIG_FILE (definido en banco.h o config.h)
    Config config = leer_configuracion(CONFIG_FILE);
    
    if (access(config.archivo_cuentas, F_OK) == 0) {
        printf("El archivo de cuentas ya existe.\n");
        return;
    }

    Cuenta cuentas[] = {
        {1001, "John Doe", 5000.00, 0},
        {1002, "Jane Smith", 3000.00, 0},
        {1003, "Carlos Pérez", 10000.00, 0},
        {1004, "Ana López", 7500.00, 0}
    };

    FILE *archivo = fopen(config.archivo_cuentas, "wb");
    if (!archivo) {
        perror("Error al crear el archivo de cuentas.");
        exit(EXIT_FAILURE);
    }

    fwrite(cuentas, sizeof(Cuenta), 4, archivo);
    fclose(archivo);

    printf("Archivo de cuentas creado exitosamente.\n");
}

int main() {
    crear_cuentas_iniciales();
    return 0;
}