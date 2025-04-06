#include <stdio.h>
#include <stdlib.h>
// Funciones POSIX: fork, read, write, close...
#include <unistd.h>
// Funciones de manejo de señales: signal, SIGINT
#include <signal.h>
// Funciones de manejo de semáforos POSIX: sem_open, sem_wait, sem_post...
#include <semaphore.h>
// Control de archivos: open, O_RDONLY...
#include <fcntl.h>
// Constantes y estructuras para manipulación de archivos y FIFOs
#include <sys/stat.h>
#include <string.h>
// Manejo de errores: errno, perror
#include <errno.h>
// Hilos POSIX: pthread_create, pthread_detach...
#include <pthread.h>
// Manejo de fechas y horas: time, localtime, strftime
#include <time.h>
// Definiciones de tipos de datos del sistema: pid_t...
#include <sys/types.h>
// Definición de estructuras de comunicación entre procesos
#include "../include/mensaje.h"
// Lectura de configuración desde config.txt
#include "../include/config.h"
// Estructura de datos para las cuentas bancarias
#include "../include/cuenta.h"

// Número máximo de usuarios que pueden conectarse simultáneamente
#define MAX_USUARIOS 5
// Hemos definido una ruta común para la FIFO usada por el monitor para 
//enviar alertas al proceso principal del banco
#define ALERT_PIPE "/tmp/alertas"

// Semáforo para controlar el número de usuarios concurrentes
sem_t *semaforo;
int servidorCorriendo = 1;
static Config configuracion;

// Cierra el servidor de forma controlada: detiene el bucle principal, 
// cierra el semáforo, borra la FIFO de alertas y finaliza el proceso
void manejarSenal(int signo) {
    (void) signo;
    printf("\nSeñal recibida. Cerrando SecureBank...\n");
    servidorCorriendo = 0;
    sem_close(semaforo);
    sem_unlink(SEMAFORO_NOMBRE);
    unlink(ALERT_PIPE);
    exit(0);
}

// Creamos o abrimos un semáforo POSIX que limita la cantidad de usuarios
// activos concurrentes
void inicializarSemaforo() {
    semaforo = sem_open(SEMAFORO_NOMBRE, O_CREAT, 0644, MAX_USUARIOS);
    if (semaforo == SEM_FAILED) {
        perror("Error al crear semáforo.");
        exit(EXIT_FAILURE);
    }
}

// Se muestra por terminal la lista de usuarios disponibles para "iniciar
// sesión" o para salir
void mostrarMenuPrincipal() {
    printf("\n=====================================\n");
    printf("¡Bienvenid@ a SecureBank!\n");
    printf("Seleccione el usuario para iniciar sesión:\n");
    printf("  1. Usuario nº1\n");
    printf("  2. Usuario nº2\n");
    printf("  3. Usuario nº3\n");
    printf("  4. Usuario nº4\n");
    printf("O presione q para salir.\n");
    printf("=====================================\n");
    printf("Ingrese opción: ");
}

// Hilo que escucha la FIFO de alertas (/tmp/alertas) y muestra las 
// alertas en la terminal del monitor
void *escuchaAlerta(void *arg) {
    (void)arg;
    int fd_alerta = open(ALERT_PIPE, O_RDONLY);
    if (fd_alerta == -1) {
        perror("Error abriendo tubería de alertas.");
        pthread_exit(NULL);
    }
    // Lee continuamente mensajes desde la tubería. Si el monitor escribe 
    // algo, lo mostramos inmediatamente en pantalla
    char buffer[256];
    while (1) {
        ssize_t bytes = read(fd_alerta, buffer, sizeof(buffer)-1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("\n>>> ALERTA DEL MONITOR: %s\n", buffer);
            fflush(stdout);
        } else if (bytes == 0) {
            close(fd_alerta);
            fd_alerta = open(ALERT_PIPE, O_RDONLY);
            if (fd_alerta == -1) {
                perror("Error reabriendo tubería de alertas.");
                break;
            }
        } else {
            perror("Error leyendo tubería de alertas.");
        }
    }
    close(fd_alerta);
    pthread_exit(NULL);
}

// Hilo que procesa las operaciones de los usuarios. Cada hilo se encarga
// de leer la FIFO del usuario correspondiente y procesar la operación
void *procesarOperacion(void *arg) {
    char *fifo_path = (char *)arg;
    DatosOperacion op;
    int fd = open(fifo_path, O_RDONLY);
    if (fd == -1) {
        perror("Error abriendo FIFO para lectura.");
        free(fifo_path);
        pthread_exit(NULL);
    }
    // Lee estructura DatosOperacion y accede al archivo de cuentas protegido por semáforo
    while (read(fd, &op, sizeof(op)) > 0) {
        sem_wait(semaforo);
        // Abre el archivo de cuentas para modificar datos de la cuenta origen 
        // (y destino si es transferencia)
        FILE *archivo = fopen(configuracion.archivo_cuentas, "rb+");
        if (!archivo) {
            perror("Error al abrir archivo de cuentas.");
            continue;
        }

        Cuenta cuenta;
        int encontrado = 0;
        while (fread(&cuenta, sizeof(Cuenta), 1, archivo) == 1) {
            if (cuenta.numero_cuenta == op.numero_cuenta) {
                encontrado = 1;
                break;
            }
        }

        char resultado[256] = "";

        if (!encontrado) {
            snprintf(resultado, sizeof(resultado),
                     "Cuenta %d no encontrada.\n", op.numero_cuenta);
            fclose(archivo);
        } else {
            switch (op.tipo) {
                // Caso 1: Depósito. Se incrementa el saldo de la cuenta y se genera 
                // un mensaje de confirmación
                case 1:
                    cuenta.saldo += op.monto;
                    snprintf(resultado, sizeof(resultado),
                             "Depósito realizado. Nuevo saldo de la cuenta %d: %.2f €\n",
                             cuenta.numero_cuenta, cuenta.saldo);
                    break;
                // Caso 2: Retiro. Se decrementa el saldo de la cuenta y se genera
                // un mensaje de confirmación. Si el saldo es insuficiente, se genera
                // un mensaje de error
                case 2:
                    if (op.monto <= cuenta.saldo) {
                        cuenta.saldo -= op.monto;
                        snprintf(resultado, sizeof(resultado),
                                 "Retiro realizado. Nuevo saldo de la cuenta %d: %.2f €\n",
                                 cuenta.numero_cuenta, cuenta.saldo);
                    } else {
                        snprintf(resultado, sizeof(resultado),
                                 "No hay suficiente saldo en la cuenta %d.\n",
                                 cuenta.numero_cuenta);
                    }
                    break;
                // Caso 3: Transferencia. Se decrementa el saldo de la cuenta origen y
                // se incrementa el saldo de la cuenta destino. Se genera un mensaje de
                // confirmación. Si el saldo es insuficiente, se genera un mensaje de error
                case 3:
                    if (op.monto > cuenta.saldo) {
                        snprintf(resultado, sizeof(resultado),
                                 "No hay suficiente saldo para transferir desde la cuenta %d.\n",
                                 op.numero_cuenta);
                        break;
                    }
                    cuenta.saldo -= op.monto;
                    fseek(archivo, -sizeof(Cuenta), SEEK_CUR);
                    fwrite(&cuenta, sizeof(Cuenta), 1, archivo);

                    int destinoEncontrado = 0;
                    Cuenta cuentaDestino;
                    rewind(archivo);
                    while (fread(&cuentaDestino, sizeof(Cuenta), 1, archivo) == 1) {
                        if (cuentaDestino.numero_cuenta == op.cuenta_destino) {
                            destinoEncontrado = 1;
                            break;
                        }
                    }
                    if (!destinoEncontrado) {
                        snprintf(resultado, sizeof(resultado),
                                 "Cuenta destino %d no encontrada.\n", op.cuenta_destino);
                        break;
                    }
                    cuentaDestino.saldo += op.monto;
                    fseek(archivo, -sizeof(Cuenta), SEEK_CUR);
                    fwrite(&cuentaDestino, sizeof(Cuenta), 1, archivo);

                    snprintf(resultado, sizeof(resultado),
                             "Transferencia realizada de %.2f € desde la cuenta %d a la cuenta %d\n",
                             op.monto, op.numero_cuenta, op.cuenta_destino);
                    break;
                // Caso 4: Consulta de saldo. Se genera un mensaje con el saldo actual
                // de la cuenta
                case 4:
                    snprintf(resultado, sizeof(resultado),
                             "Saldo actual de la cuenta %d: %.2f €\n",
                             cuenta.numero_cuenta, cuenta.saldo);
                    break;
                default:
                    snprintf(resultado, sizeof(resultado),
                             "Operación no reconocida.\n");
                    break;
            }
            // Si la operación no fue transferencia (ya se guardó antes) ni consulta 
            // (solo lectura), se guarda el nuevo estado de la cuenta origen
            if (op.tipo != 3 && op.tipo != 4) {
                fseek(archivo, -sizeof(Cuenta), SEEK_CUR);
                fwrite(&cuenta, sizeof(Cuenta), 1, archivo);
            }

            fclose(archivo);
            sem_post(semaforo);
        }

        // Apertura del log de transacciones. Se registra cada operación con timestamp
        FILE *logFile = fopen(configuracion.archivo_log, "a");
        if (logFile) {
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "[%d-%m-%Y %H:%M:%S]", t);

            switch (op.tipo) {
                case 1:
                    fprintf(logFile, "%s Depósito en la cuenta número %d de %.2f\n",
                            timeStr, op.numero_cuenta, op.monto);
                    break;
                case 2:
                    fprintf(logFile, "%s Retiro en la cuenta número %d de %.2f\n",
                            timeStr, op.numero_cuenta, op.monto);
                    break;
                case 3:
                    fprintf(logFile, "%s Transferencia de %.2f desde la cuenta número %d a la número %d\n",
                            timeStr, op.monto, op.numero_cuenta, op.cuenta_destino);
                    break;
                case 4:
                    fprintf(logFile, "%s Consulta de saldo en la cuenta número %d\n",
                            timeStr, op.numero_cuenta);
                    break;
                default:
                    break;
            }
            fclose(logFile);
        }

        if (strlen(op.respuesta) > 0) {
            int fd_resp = open(op.respuesta, O_WRONLY);
            if (fd_resp != -1) {
                write(fd_resp, resultado, strlen(resultado));
                close(fd_resp);
            }
        }
    }
    // Al finalizar: se cierra la FIFO, se libera la memoria reservada y se termina el hilo
    close(fd);
    free(fifo_path);
    pthread_exit(NULL);
}

int main() {
    signal(SIGINT, manejarSenal);
    // Carga los parámetros desde el archivo config.txt, incluyendo rutas, límites 
    // y número de hilos
    configuracion = leerConfiguracion(CONFIG_FILE);
    inicializarSemaforo();

    // Elimina cualquier FIFO antigua de usuario que haya quedado de ejecuciones anteriores,
    // evitando errores como "usuario ya conectado"
    for (int i = 1; i <= MAX_USUARIOS; i++) {
        char path[100];
        snprintf(path, sizeof(path), "/tmp/pipe_usuario_%d", i);
        unlink(path);
    }

    mkfifo(ALERT_PIPE, 0666);
    pthread_t hiloAlertas;
    if (pthread_create(&hiloAlertas, NULL, escuchaAlerta, NULL) != 0) {
        perror("Error creando hilo de alertas.");
    }
    pthread_detach(hiloAlertas);

    while (servidorCorriendo) {
        mostrarMenuPrincipal();

        char opcion_str[10];
        if (fgets(opcion_str, sizeof(opcion_str), stdin) == NULL)
            continue;
        if (opcion_str[0] == 'q' || opcion_str[0] == 'Q')
            break;
        // Valida que la opción seleccionada sea un número de usuario válido.
        // Si no lo es, muestra un mensaje y vuelve a mostrar el menú
        int userId = atoi(opcion_str);
        if (userId < 1 || userId > MAX_USUARIOS) {
            printf("Opción inválida.\n");
            continue;
        }
        // Genera el nombre de la FIFO asociada al usuario seleccionado.
        // Si ya existe, significa que el usuario está en sesión, por lo que 
        // se impide una segunda conexión.
        char fifo_path[100];
        snprintf(fifo_path, sizeof(fifo_path), "/tmp/pipe_usuario_%d", userId);
        if (access(fifo_path, F_OK) == 0) {
            printf("El usuario %d ya está conectado.\n", userId);
            continue;
        }

        if (mkfifo(fifo_path, 0666) != 0) {
            perror("Error al crear FIFO.");
            continue;
        }
        // Crea un nuevo proceso hijo que abre una terminal (gnome-terminal) y 
        // ejecuta el programa usuario, pasándole la ruta de la FIFO como argumento.
        // Esto abre automáticamente la sesión del usuario en una nueva terminal
        pid_t pid = fork();
        if (pid == 0) {
            execlp("gnome-terminal", "gnome-terminal", "--", "./bin/usuario", fifo_path, NULL);
            perror("Error al abrir nueva terminal");
            exit(EXIT_FAILURE);
        }

        sem_wait(semaforo);
        pthread_t hilo;
        char *fifo_path_copia = strdup(fifo_path);
        // Crea un hilo para procesar las operaciones del usuario conectado.
        // El hilo se desvincula (detach) y no se espera a que finalice.
        // Se libera el semáforo justo después para que otros usuarios puedan entrar.
        if (pthread_create(&hilo, NULL, procesarOperacion, fifo_path_copia) != 0) {
            perror("Error creando hilo.");
            sem_post(semaforo);
            continue;
        }
        pthread_detach(hilo);
        sem_post(semaforo);
    }

    return 0;
}
