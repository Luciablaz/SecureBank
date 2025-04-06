#ifndef CONFIG_H
#define CONFIG_H
// Nombre POSIX del semáforo usado para controlar el número 
// de usuarios concurrentes conectados al banco
#define SEMAFORO_NOMBRE "/securebank_semaforo"

// Ruta relativa al archivo de configuración que contiene 
// los parámetros del sistema (límites, rutas...)
#define CONFIG_FILE "data/config.txt"

// Estructura que agrupa todos los parámetros configurables 
// del sistema, cargados desde el archivo config.txt
typedef struct {
    int limite_retiro;
    int limite_transferencia;
    int umbral_retiros;
    int umbral_transferencias;
    int num_hilos;
    char archivo_cuentas[100];
    char archivo_log[100];
} Config;

// Prototipo de la función que lee el archivo de configuración 
// y devuelve una estructura Config con los valores cargados
Config leerConfiguracion(const char *ruta);

#endif