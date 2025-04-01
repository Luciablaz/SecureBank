#ifndef BANCO_H
#define BANCO_H

#include <semaphore.h>

#define CONFIG_FILE "data/config.txt"
#define SEMAFORO_NOMBRE "/cuentas_sem"
#define PIPE_NOMBRE "/tmp/pipe_banco"

typedef struct {
    int limite_retiro;
    int limite_transferencia;
    int umbral_retiros;
    int umbral_transferencias;
    int num_hilos;
    char archivo_cuentas[50];
    char archivo_log[50];
} Config;

#endif
