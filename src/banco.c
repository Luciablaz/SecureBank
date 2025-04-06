#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include "../include/mensaje.h"
#include "../include/config.h"
#include "../include/cuenta.h"

#define MAX_USUARIOS 5
#define ALERT_PIPE "/tmp/alertas"

sem_t *semaforo;
int servidor_corriendo = 1;
static Config configuracion;

void manejar_senal(int signo) {
    (void) signo;
    printf("\nSeñal recibida. Cerrando SecureBank...\n");
    servidor_corriendo = 0;
    sem_close(semaforo);
    sem_unlink(SEMAFORO_NOMBRE);
    unlink(ALERT_PIPE);
    exit(0);
}

void inicializar_semaforo_wrapper() {
    semaforo = sem_open(SEMAFORO_NOMBRE, O_CREAT, 0644, MAX_USUARIOS);
    if (semaforo == SEM_FAILED) {
        perror("Error al crear semáforo");
        exit(EXIT_FAILURE);
    }
}

void mostrar_menu_principal() {
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

// Hilo que escucha la tubería de alertas y muestra los mensajes en la terminal del padre
void *alert_listener(void *arg) {
    (void)arg;
    int fd_alert = open(ALERT_PIPE, O_RDONLY);
    if (fd_alert == -1) {
        perror("Error abriendo tubería de alertas.");
        pthread_exit(NULL);
    }
    char buffer[256];
    while (1) {
        ssize_t bytes = read(fd_alert, buffer, sizeof(buffer)-1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("\n>>> ALERTA DEL MONITOR: %s\n", buffer);
            fflush(stdout);
        } else if (bytes == 0) {
            close(fd_alert);
            fd_alert = open(ALERT_PIPE, O_RDONLY);
            if (fd_alert == -1) {
                perror("Error reabriendo tubería de alertas.");
                break;
            }
        } else {
            perror("Error leyendo tubería de alertas.");
        }
    }
    close(fd_alert);
    pthread_exit(NULL);
}

void *procesar_operacion(void *arg) {
    char *fifo_path = (char *)arg;
    DatosOperacion op;
    int fd = open(fifo_path, O_RDONLY);
    if (fd == -1) {
        perror("Error abriendo FIFO para lectura.");
        free(fifo_path);
        pthread_exit(NULL);
    }
    while (read(fd, &op, sizeof(op)) > 0) {
        sem_wait(semaforo);
        // Abrimos archivo de cuentas
        FILE *archivo = fopen(configuracion.archivo_cuentas, "rb+");
        if (!archivo) {
            perror("Error al abrir archivo de cuentas.");
            continue;
        }

        // Buscamos la cuenta origen
        Cuenta cuenta;
        int encontrado = 0;
        while (fread(&cuenta, sizeof(Cuenta), 1, archivo) == 1) {
            if (cuenta.numero_cuenta == op.numero_cuenta) {
                encontrado = 1;
                break;
            }
        }
        // Mensaje resultado para enviar al usuario
        char resultado[256] = "";

        if (!encontrado) {
            snprintf(resultado, sizeof(resultado),
                     "Cuenta %d no encontrada.\n", op.numero_cuenta);
            fclose(archivo);
        } else {
            switch (op.tipo) {
                case 1: { // Depósito
                    cuenta.saldo += op.monto;
                    snprintf(resultado, sizeof(resultado),
                             "Depósito realizado. Nuevo saldo de la cuenta %d: %.2f €\n",
                             cuenta.numero_cuenta, cuenta.saldo);
                    break;
                }
                case 2: { // Retiro
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
                }
                case 3: { // Transferencia
                    if (op.monto > cuenta.saldo) {
                        snprintf(resultado, sizeof(resultado),
                                 "No hay suficiente saldo para transferir desde la cuenta %d.\n",
                                 op.numero_cuenta);
                        break;
                    }
                    // Descontar monto en origen
                    cuenta.saldo -= op.monto;
                    fseek(archivo, -sizeof(Cuenta), SEEK_CUR);
                    fwrite(&cuenta, sizeof(Cuenta), 1, archivo);

                    // Buscar cuenta destino
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
                    // Sumar monto a la cuenta destino
                    cuentaDestino.saldo += op.monto;
                    fseek(archivo, -sizeof(Cuenta), SEEK_CUR);
                    fwrite(&cuentaDestino, sizeof(Cuenta), 1, archivo);

                    snprintf(resultado, sizeof(resultado),
                             "Transferencia realizada de %.2f € desde la cuenta %d a la cuenta %d\n",
                             op.monto, op.numero_cuenta, op.cuenta_destino);
                    break;
                }
                case 4: { // Consulta de saldo
                    snprintf(resultado, sizeof(resultado),
                             "Saldo actual de la cuenta %d: %.2f €\n",
                             cuenta.numero_cuenta, cuenta.saldo);
                    break;
                }
                default:
                    snprintf(resultado, sizeof(resultado),
                             "Operación no reconocida.\n");
                    break;
            }
            // Guardamos los cambios en la cuenta origen si no era consulta
            if (op.tipo != 3 && op.tipo != 4) {
                // Si no fue transferencia o consulta, estamos al final del registro de la cuenta
                fseek(archivo, -sizeof(Cuenta), SEEK_CUR);
                fwrite(&cuenta, sizeof(Cuenta), 1, archivo);
            }
            fclose(archivo);
            sem_post(semaforo);
        }

        // Registrar la operación en el log
        // Registrar la operación en el log
        FILE *logFile = fopen(configuracion.archivo_log, "a");
        if (logFile) {
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "[%d-%m-%Y %H:%M:%S]", t);

            switch (op.tipo) {
            case 1: // Depósito
                fprintf(logFile, "%s Depósito en la cuenta número %d de %.2f\n",
                    timeStr, op.numero_cuenta, op.monto);
                break;
            case 2: // Retiro
                fprintf(logFile, "%s Retiro en la cuenta número %d de %.2f\n",
                    timeStr, op.numero_cuenta, op.monto);
                break;
            case 3: // Transferencia
                fprintf(logFile, "%s Transferencia de %.2f desde la cuenta número %d a la número %d\n",
                    timeStr, op.monto, op.numero_cuenta, op.cuenta_destino);
                break;
            case 4: // Consulta
                fprintf(logFile, "%s Consulta de saldo en la cuenta número %d\n",
                    timeStr, op.numero_cuenta);
                break;
            default:
                break;
            }

        fclose(logFile);
        }

        // Enviar el resultado al usuario, si hay FIFO de respuesta
        if (strlen(op.respuesta) > 0) {
            int fd_resp = open(op.respuesta, O_WRONLY);
            if (fd_resp != -1) {
                write(fd_resp, resultado, strlen(resultado));
                close(fd_resp);
            }
        }
    }
    close(fd);
    free(fifo_path);
    pthread_exit(NULL);
}

int main() {
    signal(SIGINT, manejar_senal);
    configuracion = leer_configuracion(CONFIG_FILE);
    inicializar_semaforo_wrapper();

    mkfifo(ALERT_PIPE, 0666); // Tubería de alertas
    pthread_t hilo_alertas;
    if (pthread_create(&hilo_alertas, NULL, alert_listener, NULL) != 0) {
        perror("Error creando hilo de alertas.");
    }
    pthread_detach(hilo_alertas);

    while (servidor_corriendo) {
        mostrar_menu_principal();

        char opcion_str[10];
        if (fgets(opcion_str, sizeof(opcion_str), stdin) == NULL)
            continue;
        if (opcion_str[0] == 'q' || opcion_str[0] == 'Q')
            break;

        int userId = atoi(opcion_str);
        if (userId < 1 || userId > 5) {
            printf("Opción inválida.\n");
            continue;
        }

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

        // Abrir terminal para el usuario
        pid_t pid = fork();
        if (pid == 0) {
            execlp("gnome-terminal", "gnome-terminal", "--", "./bin/usuario", fifo_path, NULL);
            //execlp("dbus-launch", "dbus-launch", "gnome-terminal", "--", "./bin/usuario", fifo_path, NULL);
            perror("Error al abrir nueva terminal.");
            exit(EXIT_FAILURE);
        }

        // Crear hilo para procesar la FIFO del usuario
        sem_wait(semaforo);
        pthread_t hilo;
        char *fifo_path_copia = strdup(fifo_path);
        if (pthread_create(&hilo, NULL, procesar_operacion, fifo_path_copia) != 0) {
            perror("Error creando hilo.");
            sem_post(semaforo);
            continue;
        }
        pthread_detach(hilo);
        sem_post(semaforo);
    }
    return 0;
}
