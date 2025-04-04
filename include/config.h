#ifndef CONFIG_H
#define CONFIG_H
#define SEMAFORO_NOMBRE "/securebank_semaforo"

#define CONFIG_FILE "data/config.txt"

typedef struct {
    int limite_retiro;
    int limite_transferencia;
    int umbral_retiros;
    int umbral_transferencias;
    int num_hilos;
    char archivo_cuentas[100];
    char archivo_log[100];
} Config;

Config leer_configuracion(const char *ruta);

#endif
