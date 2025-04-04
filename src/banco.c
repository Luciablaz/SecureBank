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
#include "../include/mensaje.h"
#include "../include/banco.h"
#include "../include/config.h"
#include "../include/cuenta.h"

#define MAX_USUARIOS 5

sem_t *semaforo;
int servidor_corriendo = 1;
static Config configuracion;

void manejar_senal(int signo) {
    (void)signo;
    printf("\nSeñal recibida. Cerrando SecureBank...\n");
    servidor_corriendo = 0;
    sem_close(semaforo);
    sem_unlink(SEMAFORO_NOMBRE);
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
    printf("Bienvenido a SecureBank (Modo Concurrente con Hilos)\n");
    printf("Seleccione el usuario para iniciar sesión:\n");
    printf("  1. Usuario 1\n");
    printf("  2. Usuario 2\n");
    printf("  3. Usuario 3\n");
    printf("  4. Usuario 4\n");
    printf("  5. Usuario 5\n");
    printf("O presione Q para salir.\n");
    printf("=====================================\n");
    printf("Ingrese opción: ");
}

void *procesar_operacion(void *arg) {
    char *fifo_path = (char *)arg;
    DatosOperacion op;

    int fd = open(fifo_path, O_RDONLY);
    if (fd == -1) {
        perror("Error abriendo FIFO para lectura");
        free(fifo_path);
        pthread_exit(NULL);
    }

    int user_id = 0;
    sscanf(fifo_path, "/tmp/pipe_usuario_%d", &user_id);

    char fifo_respuesta[100];
    snprintf(fifo_respuesta, sizeof(fifo_respuesta), "/tmp/respuesta_usuario_%d", user_id);

    printf("[Banco] Esperando operaciones en %s...\n", fifo_path);
    int fd_respuesta = open(fifo_respuesta, O_WRONLY);
    if (fd_respuesta == -1) {
        perror("[Banco] Error abriendo FIFO de respuesta");
        close(fd);
        unlink(fifo_path);
        free(fifo_path);
        pthread_exit(NULL);
    }

    while (read(fd, &op, sizeof(op)) > 0) {
        FILE *archivo = fopen(configuracion.archivo_cuentas, "rb+");
        if (!archivo) {
            perror("[Banco] Error al abrir archivo de cuentas");
            continue;
        }

        Cuenta cuenta;
        int encontrado = 0;
        long pos_cuenta = 0;
        char respuesta[256] = "";

        while (fread(&cuenta, sizeof(Cuenta), 1, archivo) == 1) {
            if (cuenta.numero_cuenta == op.numero_cuenta) {
                encontrado = 1;
                pos_cuenta = ftell(archivo) - sizeof(Cuenta);
                break;
            }
        }

        if (!encontrado) {
            snprintf(respuesta, sizeof(respuesta), "Cuenta %d no encontrada.", op.numero_cuenta);
            write(fd_respuesta, respuesta, strlen(respuesta) + 1);
            fclose(archivo);
            continue;
        }

        int operacion_exitosa = 1;

        switch (op.tipo) {
            case 1:
                cuenta.saldo += op.monto;
                snprintf(respuesta, sizeof(respuesta), "Depósito realizado. Nuevo saldo: %.2f", cuenta.saldo);
                break;

            case 2:
                if (op.monto <= cuenta.saldo) {
                    cuenta.saldo -= op.monto;
                    snprintf(respuesta, sizeof(respuesta), "Retiro realizado. Nuevo saldo: %.2f", cuenta.saldo);
                } else {
                    snprintf(respuesta, sizeof(respuesta), "Fondos insuficientes. Saldo actual: %.2f", cuenta.saldo);
                    operacion_exitosa = 0;
                }
                break;

            case 3: {
                Cuenta cuenta_destino;
                int encontrado_destino = 0;

                rewind(archivo);
                while (fread(&cuenta_destino, sizeof(Cuenta), 1, archivo) == 1) {
                    if (cuenta_destino.numero_cuenta == op.cuenta_destino) {
                        encontrado_destino = 1;
                        break;
                    }
                }

                if (!encontrado_destino) {
                    snprintf(respuesta, sizeof(respuesta), "Cuenta destino %d no encontrada.", op.cuenta_destino);
                    operacion_exitosa = 0;
                    break;
                }

                if (op.monto > cuenta.saldo) {
                    snprintf(respuesta, sizeof(respuesta), "Fondos insuficientes para transferencia.");
                    operacion_exitosa = 0;
                    break;
                }

                cuenta.saldo -= op.monto;
                cuenta_destino.saldo += op.monto;

                fseek(archivo, pos_cuenta, SEEK_SET);
                fwrite(&cuenta, sizeof(Cuenta), 1, archivo);

                rewind(archivo);
                while (fread(&cuenta, sizeof(Cuenta), 1, archivo) == 1) {
                    if (cuenta.numero_cuenta == op.cuenta_destino) {
                        fseek(archivo, -sizeof(Cuenta), SEEK_CUR);
                        fwrite(&cuenta_destino, sizeof(Cuenta), 1, archivo);
                        break;
                    }
                }

                snprintf(respuesta, sizeof(respuesta),
                         "Transferencia completada. Nuevo saldo origen: %.2f", cuenta.saldo);
                fclose(archivo);
                write(fd_respuesta, respuesta, strlen(respuesta) + 1);
                continue;
            }

            case 4:
                snprintf(respuesta, sizeof(respuesta), "Saldo actual: %.2f", cuenta.saldo);
                break;

            default:
                snprintf(respuesta, sizeof(respuesta), "Operación inválida.");
                operacion_exitosa = 0;
                break;
        }

        if (operacion_exitosa && op.tipo != 3) {
            fseek(archivo, pos_cuenta, SEEK_SET);
            fwrite(&cuenta, sizeof(Cuenta), 1, archivo);
        }

        fclose(archivo);
        write(fd_respuesta, respuesta, strlen(respuesta) + 1);
    }

    close(fd);
    close(fd_respuesta);
    unlink(fifo_path);
    free(fifo_path);
    pthread_exit(NULL);
}

int main() {
    signal(SIGINT, manejar_senal);
    configuracion = leer_configuracion(CONFIG_FILE);
    inicializar_semaforo_wrapper();

    while (servidor_corriendo) {
        mostrar_menu_principal();

        char opcion_str[10];
        if (fgets(opcion_str, sizeof(opcion_str), stdin) == NULL) continue;
        if (opcion_str[0] == 'q' || opcion_str[0] == 'Q') break;

        int userId = atoi(opcion_str);
        if (userId < 1 || userId > 5) {
            printf("Opción inválida.\n");
            continue;
        }

        char fifo_path[100];
        snprintf(fifo_path, sizeof(fifo_path), "/tmp/pipe_usuario_%d", userId);
        mkfifo(fifo_path, 0666);

        char fifo_respuesta[100];
        snprintf(fifo_respuesta, sizeof(fifo_respuesta), "/tmp/respuesta_usuario_%d", userId);
        mkfifo(fifo_respuesta, 0666);

        pid_t pid = fork();
        if (pid == 0) {
            execlp("gnome-terminal", "gnome-terminal", "--", "./bin/usuario", fifo_path, NULL);
            perror("Error al abrir nueva terminal");
            exit(EXIT_FAILURE);
        }

        sem_wait(semaforo);

        pthread_t hilo;
        char *fifo_path_copia = strdup(fifo_path);
        if (pthread_create(&hilo, NULL, procesar_operacion, fifo_path_copia) != 0) {
            perror("Error creando hilo");
            sem_post(semaforo);
            continue;
        }

        pthread_detach(hilo);
        sem_post(semaforo);
    }

    return 0;
}
