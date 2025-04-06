#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "../include/config.h"
#include "../include/mensaje.h"

#define CUENTA_MIN 1001
#define CUENTA_MAX 1004
#define NUM_CUENTAS (CUENTA_MAX - CUENTA_MIN + 1)

int indice_cuenta(int cuenta) {
    return cuenta - CUENTA_MIN;
}

static int cuentasEnUso[NUM_CUENTAS] = {0};

#define MAX_TRANSFERENCIAS 100
typedef struct {
    int origen;
    int destino;
    int contador;
} TransferInfo;

static TransferInfo transferencias[MAX_TRANSFERENCIAS];
static int numTransferencias = 0;

#define ALERT_PIPE "/tmp/alertas"

int main() {
    Config config = leer_configuracion(CONFIG_FILE);

    key_t key = ftok("src/monitor.c", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("Error creando o accediendo a la cola de mensajes.");
        exit(1);
    }

    //printf("Monitor: Umbral retiros = %d, Umbral transferencias = %d\n", config.umbral_retiros, config.umbral_transferencias);
    printf("\nMonitor iniciado. Escuchando transacciones...\n");

    MensajeOperacion msg;
    int contador_retiros = 0;
    int ultima_cuenta = -1;

    while (1) {
        ssize_t res = msgrcv(msgid, &msg, sizeof(MensajeOperacion) - sizeof(long), 0, 0);
        if (res == -1) {
            perror("Error al recibir mensaje.");
            continue;
        }

        switch (msg.operacion) {
            case 1:
                printf("Se ha realizado un DEPÓSITO (1) de %.2f en la cuenta número %d\n",
                       msg.monto, msg.numero_cuenta);
                break;
            case 2:
                printf("Se ha realizado un RETIRO (2) de %.2f en la cuenta número %d\n",
                       msg.monto, msg.numero_cuenta);
                break;
            case 3:
                printf("Se ha realizado una TRANSFERENCIA (3) de %.2f desde la cuenta número %d a la cuenta número %d\n",
                       msg.monto, msg.numero_cuenta, msg.cuenta_destino);
                break;
            case 4:
                printf("Se ha realizado una CONSULTA (4) en la cuenta número %d\n",
                       msg.numero_cuenta);
                break;
        }

        // Retiros sospechosos
        if (msg.operacion == 2 && msg.monto > config.limite_retiro) {
            if (msg.numero_cuenta == ultima_cuenta) {
                contador_retiros++;
            } else {
                contador_retiros = 1;
                ultima_cuenta = msg.numero_cuenta;
            }
            if (contador_retiros >= config.umbral_retiros) {
                char alerta[128];
                snprintf(alerta, sizeof(alerta),
                         "Retiros sospechosos consecutivos en cuenta %d\n",
                         msg.numero_cuenta);
                printf("ALERTA: %s", alerta);
                int fd_alert = open(ALERT_PIPE, O_WRONLY);
                if (fd_alert != -1) {
                    write(fd_alert, alerta, strlen(alerta));
                    close(fd_alert);
                }
                contador_retiros = 0;
            }
        } else {
            contador_retiros = 0;
        }

        // Transferencias repetidas
        if (msg.operacion == 3) {
            int found = 0;
            for (int i = 0; i < numTransferencias; i++) {
                if (transferencias[i].origen == msg.numero_cuenta &&
                    transferencias[i].destino == msg.cuenta_destino) {
                    transferencias[i].contador++;
                    if (transferencias[i].contador >= config.umbral_transferencias) {
                        char alerta[128];
                        snprintf(alerta, sizeof(alerta),
                                 "Transferencias repetidas entre cuentas %d y %d\n",
                                 msg.numero_cuenta, msg.cuenta_destino);
                        printf("ALERTA: %s", alerta);
                        int fd_alert = open(ALERT_PIPE, O_WRONLY);
                        if (fd_alert != -1) {
                            write(fd_alert, alerta, strlen(alerta));
                            close(fd_alert);
                        }
                        transferencias[i].contador = 0;
                    }
                    found = 1;
                    break;
                }
            }
            if (!found && numTransferencias < MAX_TRANSFERENCIAS) {
                transferencias[numTransferencias].origen = msg.numero_cuenta;
                transferencias[numTransferencias].destino = msg.cuenta_destino;
                transferencias[numTransferencias].contador = 1;
                numTransferencias++;
            }
        }

        // Uso simultáneo simple
        if (msg.operacion == 1 || msg.operacion == 2 || msg.operacion == 3) {
            int idx = indice_cuenta(msg.numero_cuenta);
            if (idx >= 0 && idx < NUM_CUENTAS) {
                cuentasEnUso[idx]++;
                if (cuentasEnUso[idx] > 1) {
                    char alerta[128];
                    snprintf(alerta, sizeof(alerta),
                             "Uso simultáneo de la cuenta %d\n", msg.numero_cuenta);
                    printf("ALERTA: %s", alerta);
                    int fd_alert = open(ALERT_PIPE, O_WRONLY);
                    if (fd_alert != -1) {
                        write(fd_alert, alerta, strlen(alerta));
                        close(fd_alert);
                    }
                }
                cuentasEnUso[idx]--;
            }
        }
    }
    return 0;
}