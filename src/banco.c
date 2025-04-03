#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>
#include "../include/mensaje.h"
#include "../include/banco.h"
#include "../include/config.h"
#include "../include/cuenta.h"

#define MAX_USUARIOS 5

sem_t *semaforo;
int servidor_corriendo = 1;
static int fd_fifo_lec = -1;  // Descriptor para el FIFO del monitor

// Configuración global leída desde el archivo
static Config configuracion;

// Estructura para la operación que envía el hijo
typedef struct {
    int tipo;
    int numero_cuenta;
    float monto;
    int cuenta_destino;
} DatosOperacion;

//
// Funciones auxiliares
//

// Busca una cuenta en el archivo binario
int buscar_cuenta(int numero_cuenta, FILE *archivo, Cuenta *cuenta) {
    rewind(archivo);
    while (fread(cuenta, sizeof(Cuenta), 1, archivo) == 1) {
        if (cuenta->numero_cuenta == numero_cuenta) {
            return ftell(archivo) - sizeof(Cuenta);
        }
    }
    return -1;
}

// Envía una transacción al monitor vía cola de mensajes
void enviar_transaccion_monitor(int tipo, int cuenta_origen, float monto, int cuenta_destino) {
    int cola_mensajes = msgget(CLAVE_COLA, 0666 | IPC_CREAT);
    if (cola_mensajes != -1) {
        MsgOperacion mensaje;
        mensaje.tipo = 1;
        if (tipo == 1)
            snprintf(mensaje.texto, sizeof(mensaje.texto), "DEPOSITO cuenta %d: %.2f", cuenta_origen, monto);
        else if (tipo == 2)
            snprintf(mensaje.texto, sizeof(mensaje.texto), "RETIRO cuenta %d: %.2f", cuenta_origen, monto);
        else if (tipo == 3)
            snprintf(mensaje.texto, sizeof(mensaje.texto), "TRANSFERENCIA de %d a %d: %.2f", cuenta_origen, cuenta_destino, monto);
        else if (tipo == 4)
            snprintf(mensaje.texto, sizeof(mensaje.texto), "CONSULTA saldo cuenta %d", cuenta_origen);
        msgsnd(cola_mensajes, &mensaje, sizeof(mensaje.texto), 0);
    }
}

// Procesa la operación recibida (actualiza cuentas.dat, envía alerta al monitor)
void procesar_operacion(DatosOperacion op) {
    FILE *archivo = fopen(configuracion.archivo_cuentas, "rb+");
    if (!archivo) {
        perror("Error al abrir archivo de cuentas");
        return;
    }
    Cuenta cuenta;
    int pos = buscar_cuenta(op.numero_cuenta, archivo, &cuenta);
    if (pos == -1) {
        printf("Cuenta %d no encontrada.\n", op.numero_cuenta);
        fclose(archivo);
        return;
    }

    switch (op.tipo) {
        case 1: // Depósito
            cuenta.saldo += op.monto;
            printf("Depósito en cuenta %d. Nuevo saldo: %.2f\n", op.numero_cuenta, cuenta.saldo);
            break;
        case 2: // Retiro
            if (op.monto > cuenta.saldo) {
                printf("Fondos insuficientes en cuenta %d.\n", op.numero_cuenta);
                fclose(archivo);
                return;
            }
            cuenta.saldo -= op.monto;
            printf("Retiro en cuenta %d. Nuevo saldo: %.2f\n", op.numero_cuenta, cuenta.saldo);
            break;
        case 3: { // Transferencia
            if (op.monto > cuenta.saldo) {
                printf("Fondos insuficientes en cuenta %d para transferencia.\n", op.numero_cuenta);
                fclose(archivo);
                return;
            }
            // Buscar cuenta destino
            Cuenta cuenta_dest;
            int pos_dest = buscar_cuenta(op.cuenta_destino, archivo, &cuenta_dest);
            if (pos_dest == -1) {
                printf("Cuenta destino %d no encontrada.\n", op.cuenta_destino);
                fclose(archivo);
                return;
            }
            cuenta.saldo -= op.monto;
            cuenta_dest.saldo += op.monto;
            // Actualizar cuenta destino
            fseek(archivo, pos_dest, SEEK_SET);
            fwrite(&cuenta_dest, sizeof(Cuenta), 1, archivo);
            printf("Transferencia de %.2f desde cuenta %d a cuenta %d realizada con éxito.\n",
                   op.monto, op.numero_cuenta, op.cuenta_destino);
            break;
        }
        case 4: // Consulta de saldo
            printf("Saldo actual de la cuenta %d: %.2f\n", op.numero_cuenta, cuenta.saldo);
            fclose(archivo);
            enviar_transaccion_monitor(op.tipo, op.numero_cuenta, op.monto, op.cuenta_destino);
            return;
        default:
            printf("Opción no reconocida.\n");
            fclose(archivo);
            return;
    }

    // Guardar cambios en la cuenta
    fseek(archivo, pos, SEEK_SET);
    fwrite(&cuenta, sizeof(Cuenta), 1, archivo);
    fclose(archivo);
    enviar_transaccion_monitor(op.tipo, op.numero_cuenta, op.monto, op.cuenta_destino);
}

//
// Manejo de señales
//

void manejar_senal(int signo) {
    if (signo == SIGINT) {
        printf("\nSeñal recibida. Cerrando SecureBank...\n");
    }
    servidor_corriendo = 0;
    sem_close(semaforo);
    sem_unlink(SEMAFORO_NOMBRE);

    int cola_mensajes = msgget(CLAVE_COLA, 0666);
    if (cola_mensajes != -1) {
        msgctl(cola_mensajes, IPC_RMID, NULL);
    }
    if (fd_fifo_lec >= 0) {
        close(fd_fifo_lec);
    }
    unlink(PIPE_NOMBRE);
    exit(0);
}

void inicializar_semaforo_wrapper() {
    semaforo = sem_open(SEMAFORO_NOMBRE, O_CREAT, 0644, MAX_USUARIOS);
    if (semaforo == SEM_FAILED) {
        perror("Error al crear semáforo");
        exit(EXIT_FAILURE);
    }
}

// Muestra un menú principal más natural para el padre
void mostrar_menu_principal() {
    printf("\n=====================================\n");
    printf("Bienvenido a SecureBank (Modo Secuencial)\n");
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

//
// Función principal del proceso padre
//
int main() {
    // Configurar señales
    signal(SIGINT, manejar_senal);
    signal(SIGTERM, manejar_senal);

    // Leer configuración
    configuracion = leer_configuracion(CONFIG_FILE);
    printf("Límite retiro: %d\n", configuracion.limite_retiro);
    printf("Límite transferencia: %d\n", configuracion.limite_transferencia);
    printf("Archivo cuentas: %s\n", configuracion.archivo_cuentas);
    printf("Archivo log: %s\n", configuracion.archivo_log);

    printf("Iniciando SecureBank...\n");
    inicializar_semaforo_wrapper();

    // Crear FIFO para alertas del monitor
    if (mkfifo(PIPE_NOMBRE, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo fallo");
    }
    fd_fifo_lec = open(PIPE_NOMBRE, O_RDONLY | O_NONBLOCK);
    if (fd_fifo_lec < 0 && errno != ENOENT) {
        perror("Error abriendo tubería en modo lectura");
    }

    while (servidor_corriendo) {
        // Leer posibles alertas del monitor (opcional, en modo no-bloqueante)
        char monitor_buf[256];
        ssize_t bytes = read(fd_fifo_lec, monitor_buf, sizeof(monitor_buf) - 1);
        if (bytes > 0) {
            monitor_buf[bytes] = '\0';
            printf("[ALERTA DEL MONITOR] %s\n", monitor_buf);
        }

        // Mostrar menú del padre
        mostrar_menu_principal();

        char opcion_str[10];
        if (scanf("%s", opcion_str) != 1) {
            continue;
        }
        if (strcmp(opcion_str, "q") == 0 || strcmp(opcion_str, "Q") == 0) {
            // Salir del sistema
            break;
        }

        int userId = atoi(opcion_str);
        if (userId < 1 || userId > 5) {
            printf("Opción no válida. Por favor ingrese un número entre 1 y 5 o Q para salir.\n");
            continue;
        }

        // Crear tubería para comunicarnos con el usuario
        int pipe_fd[2];
        if (pipe(pipe_fd) == -1) {
            perror("Error creando la tubería entre padre e hijo");
            continue;
        }

        sem_wait(semaforo);
        pid_t pid = fork();
        if (pid == 0) {
            // Hijo: cierra extremo de lectura y ejecuta usuario.c
            close(pipe_fd[0]);
            char pipe_fd_str[10], userId_str[10];
            sprintf(pipe_fd_str, "%d", pipe_fd[1]);
            sprintf(userId_str, "%d", userId);

            execl("./bin/usuario", "./bin/usuario", userId_str, pipe_fd_str, NULL);
            perror("Error al ejecutar usuario");
            exit(EXIT_FAILURE);
        } else {
            // Padre: cierra extremo de escritura y atiende al hijo
            close(pipe_fd[1]);

            // Leer y procesar operaciones mientras el hijo no termine
            while (1) {
                // Revisar si el hijo ha muerto
                int status;
                pid_t w = waitpid(pid, &status, WNOHANG);
                if (w == pid) {
                    // El hijo ha terminado
                    break;
                }
                // Revisar si hay operaciones en la tubería
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(pipe_fd[0], &readfds);
                struct timeval tv;
                tv.tv_sec = 1;
                tv.tv_usec = 0;

                int ready = select(pipe_fd[0] + 1, &readfds, NULL, NULL, &tv);
                if (ready > 0 && FD_ISSET(pipe_fd[0], &readfds)) {
                    DatosOperacion op;
                    int r = read(pipe_fd[0], &op, sizeof(op));
                    if (r > 0) {
                        procesar_operacion(op);
                    } else if (r == 0) {
                        // Fin de la tubería: el hijo cerró el pipe
                        break;
                    }
                }
            }
            // Cerrar la tubería de lectura y liberar el semáforo
            close(pipe_fd[0]);
            sem_post(semaforo);
        }
    }

    // Cerrar y limpiar
    if (fd_fifo_lec >= 0) {
        close(fd_fifo_lec);
    }
    unlink(PIPE_NOMBRE);
    return 0;
}
