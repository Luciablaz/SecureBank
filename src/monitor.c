#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>              // ADAPTACIÓN TUBERÍA: incluir para open, etc.
#include "../include/mensaje.h"  // Si usas el encabezado común
#include "../include/config.h"    // Para leer la configuración (CONFIG_FILE y la estructura Config)
#include "../include/banco.h"     // (Opcional, si necesitas CONFIG_FILE definido allí)

#define LOG_FILE "logs/securebank.log"

void escribir_log(const char *mensaje) {
    system("mkdir -p logs");
    
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Error al abrir archivo de log");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    fprintf(log_file, "[%s] %s\n", timestamp, mensaje);
    fclose(log_file);
}

int main() {
    // Unificar la clave de la cola
    int cola_mensajes = msgget(CLAVE_COLA, 0666 | IPC_CREAT);

    if (cola_mensajes == -1) {
        perror("Error creando la cola de mensajes");
        exit(1);
    }

    // Leer la configuración para obtener umbrales y otros parámetros
    Config config = leer_configuracion(CONFIG_FILE);
    printf("Monitor: Umbral retiros = %d, Umbral transferencias = %d\n", 
           config.umbral_retiros, config.umbral_transferencias);
    
    escribir_log("Monitor iniciado. Escuchando transacciones...");
    printf("Monitor iniciado. Escuchando transacciones...\n");

    // ADAPTACIÓN TUBERÍA: Abrir la tubería para alertas en modo escritura
    int fd_fifo = open(PIPE_NOMBRE, O_WRONLY);
    if (fd_fifo < 0) {
        perror("Error abriendo tubería para alertas en monitor");
        // Se continúa sin alertas, si lo prefieres.
    }

    // Bucle para recibir mensajes
    MsgOperacion mensaje;
    while (1) {
        // msgrcv espera el tamaño de mtext solamente
        if (msgrcv(cola_mensajes, &mensaje, sizeof(mensaje.texto), 0, 0) == -1) {
            perror("Error recibiendo mensaje");
            continue;
        }

        // Escribir la transacción recibida en el log
        char log_msg[150];
        snprintf(log_msg, sizeof(log_msg), "Transacción recibida: %s", mensaje.texto);
        escribir_log(log_msg);
        printf("%s\n", log_msg);

        // Ejemplo básico de detección de anomalías en retiros
        if (strstr(mensaje.texto, "RETIRO") != NULL) {
            int cuenta;
            float monto;
            char operacion[20];
            if (sscanf(mensaje.texto, "%s cuenta %d: %f", operacion, &cuenta, &monto) == 3) {
                if (monto >= (float)config.limite_retiro) {
                    char alerta_msg[150];
                    snprintf(alerta_msg, sizeof(alerta_msg), "ALERTA: Retiro alto detectado en cuenta %d: %.2f", cuenta, monto);
                    escribir_log(alerta_msg);
                    printf("%s\n", alerta_msg);
                    if (fd_fifo >= 0) {
                        write(fd_fifo, alerta_msg, strlen(alerta_msg));
                    }
                }
            }
        }
    }
    // Si por alguna razón salimos del bucle, cerrar la tubería
    close(fd_fifo);
    return 0;
}