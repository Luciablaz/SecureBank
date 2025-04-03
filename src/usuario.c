#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

// Estructura para enviar la operación al padre
typedef struct {
    int tipo;
    int numero_cuenta;
    float monto;
    int cuenta_destino;
} DatosOperacion;

void mostrar_menu() {
    printf("\n--- Menú Bancario ---\n");
    printf("1. Depósito\n");
    printf("2. Retiro\n");
    printf("3. Transferencia\n");
    printf("4. Consultar saldo\n");
    printf("5. Salir\n");
    printf("Seleccione una opción: ");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <usuario_id> <pipe_fd>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int usuario_id = atoi(argv[1]);
    int pipe_fd = atoi(argv[2]);
    
    printf("Usuario %d conectado. Accediendo al banco...\n", usuario_id);
    sleep(1);
    
    DatosOperacion op;
    int opcion, cuenta, cuenta_destino;
    float monto;
    
    while (1) {
        mostrar_menu();
        if (scanf("%d", &opcion) != 1) {
            while(getchar() != '\n');  // Limpiar entrada
            continue;
        }
        if (opcion == 5) {
            break;
        }
        op.tipo = opcion;
        
        printf("Ingrese número de cuenta: ");
        if (scanf("%d", &cuenta) != 1) {
            while(getchar() != '\n');
            continue;
        }
        op.numero_cuenta = cuenta;
        
        if (opcion == 3) {
            printf("Ingrese cuenta destino: ");
            if (scanf("%d", &cuenta_destino) != 1) {
                while(getchar() != '\n');
                continue;
            }
            op.cuenta_destino = cuenta_destino;
        } else {
            op.cuenta_destino = 0;
        }
        
        if (opcion != 4) {
            printf("Ingrese monto: ");
            if (scanf("%f", &monto) != 1) {
                while(getchar() != '\n');
                continue;
            }
            op.monto = monto;
        } else {
            op.monto = 0;
        }
        
        int bytes_escritos = write(pipe_fd, &op, sizeof(op));
        if (bytes_escritos != sizeof(op)) {
            perror("Error escribiendo en el pipe");
        } else {
            // Solo mostramos "Operación enviada" si NO es una consulta (opción 4)
            if (opcion != 4) {
                printf("Operación enviada.\n");
            }
        }
        sleep(1);  // Simular tiempo de procesamiento
    }
    
    close(pipe_fd);
    return 0;
}
