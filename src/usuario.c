#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "../include/mensaje.h"

void mostrar_menu_usuario() {
    printf("\n--- Menú Bancario ---\n");
    printf("1. Depósito\n");
    printf("2. Retiro\n");
    printf("3. Transferencia\n");
    printf("4. Consultar saldo\n");
    printf("5. Salir\n");
    printf("Seleccione una opción: ");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <fifo_path>\n", argv[0]);
        return 1;
    }

    const char *fifo_envio = argv[1];

    // Obtener el ID del usuario desde el nombre del FIFO
    int user_id = 0;
    sscanf(fifo_envio, "/tmp/pipe_usuario_%d", &user_id);

    char fifo_respuesta[100];
    snprintf(fifo_respuesta, sizeof(fifo_respuesta), "/tmp/respuesta_usuario_%d", user_id);

    mkfifo(fifo_respuesta, 0666);

    printf("Conectado al banco en %s...\n", fifo_envio);

    int fd_envio = open(fifo_envio, O_WRONLY);
    int fd_respuesta = open(fifo_respuesta, O_RDONLY);
    if (fd_envio == -1 || fd_respuesta == -1) {
        perror("Error al abrir FIFO");
        return 1;
    }

    int opcion;
    DatosOperacion op;
    memset(&op, 0, sizeof(DatosOperacion));

    char respuesta[256];

    while (1) {
        mostrar_menu_usuario();
        if (scanf("%d", &opcion) != 1) break;

        if (opcion == 5) break;
        op.tipo = opcion;

        printf("Ingrese número de cuenta: ");
        scanf("%d", &op.numero_cuenta);

        if (opcion == 3) {
            printf("Ingrese cuenta destino: ");
            scanf("%d", &op.cuenta_destino);
        }

        if (opcion != 4) {
            printf("Ingrese monto: ");
            scanf("%f", &op.monto);
        }

        write(fd_envio, &op, sizeof(DatosOperacion));

        // Leer respuesta del banco
        if (read(fd_respuesta, respuesta, sizeof(respuesta)) > 0) {
            printf("%s\n", respuesta);
        }
    }

    close(fd_envio);
    close(fd_respuesta);
    unlink(fifo_envio);
    unlink(fifo_respuesta);
    return 0;
}
