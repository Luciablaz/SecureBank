#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include "../include/config.h"
#include "../include/mensaje.h"

int main() {
    Config config = leer_configuracion(CONFIG_FILE);

    key_t key = ftok("src/monitor.c", 65);  // Usa un archivo conocido como clave
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("Error creando o accediendo a la cola de mensajes");
        exit(1);
    }

    printf("Monitor: Umbral retiros = %d, Umbral transferencias = %d\n",
           config.umbral_retiros, config.umbral_transferencias);
    printf("Monitor iniciado. Escuchando transacciones...\n");

    MensajeOperacion msg;
    int contador_retiros = 0;
    int ultima_cuenta = -1;

    while (1) {
        ssize_t res = msgrcv(msgid, &msg, sizeof(MensajeOperacion) - sizeof(long), 0, 0);
        if (res == -1) {
            perror("Error al recibir mensaje");
            continue;
        }

        printf("Monitor recibió operación %d en cuenta %d por %.2f\n",
               msg.operacion, msg.numero_cuenta, msg.monto);

        // Detectar retiros sospechosos consecutivos
        if (msg.operacion == 2 && msg.monto > config.limite_retiro) {
            if (msg.numero_cuenta == ultima_cuenta) {
                contador_retiros++;
            } else {
                contador_retiros = 1;
                ultima_cuenta = msg.numero_cuenta;
            }

            if (contador_retiros >= config.umbral_retiros) {
                printf("ALERTA: Retiros sospechosos consecutivos en cuenta %d\n", msg.numero_cuenta);
                contador_retiros = 0;
            }
        } else {
            contador_retiros = 0;
        }

        // Aquí puedes añadir lógica adicional para transferencias repetidas, etc.
    }

    return 0;
}
