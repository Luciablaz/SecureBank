#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <semaphore.h>
#include "../include/mensaje.h"
#include "../include/config.h"

// Estructura para pasar datos al hilo
typedef struct {
    char fifo_path[100];
    DatosOperacion op;
    char fifo_resp[100];
} OperacionThreadData;

static pthread_mutex_t mutex_operacion = PTHREAD_MUTEX_INITIALIZER;
int global_fd_fifo;
sem_t sem_hilos;

// Prototipo del hilo
void *procesar_operacion_usuario(void *arg);

// Función para mostrar el menú
void mostrar_menu_usuario() {
    printf("\n--- Menú de operaciones bancarias ---\n");
    printf("1. Depósito\n");
    printf("2. Retiro\n");
    printf("3. Transferencia\n");
    printf("4. Consultar saldo\n");
    printf("5. Salir\n");
    printf("Seleccione una opción: ");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <fifo_path>\n", argv[0]);
        return 1;
    }
    const char *fifo_path = argv[1];
    // Extraer el número de usuario desde el path FIFO
    int user_id = 0;
    sscanf(fifo_path, "/tmp/pipe_usuario_%d", &user_id);

    printf("\nBienvenido, usuario nº %d. Has accedido correctamente a SecureBank.\n", user_id);

    global_fd_fifo = open(fifo_path, O_WRONLY);
    if (global_fd_fifo == -1) {
        perror("Error al abrir FIFO en usuario.");
        return 1;
    }

    Config config = leer_configuracion(CONFIG_FILE);
    sem_init(&sem_hilos, 0, config.num_hilos);

    int opcion;
    while (1) {
        mostrar_menu_usuario();  
        if (scanf("%d", &opcion) != 1)
            break;
        if (opcion == 5)
            break;

        DatosOperacion op;
        memset(&op, 0, sizeof(op));
        op.tipo = opcion;

        printf("Ingrese número de cuenta: ");
        scanf("%d", &op.numero_cuenta);

        if (opcion == 3) {
            printf("Ingrese cuenta destino: ");
            scanf("%d", &op.cuenta_destino);
        }

        if (opcion != 4) {
            printf("Ingrese monto: ");
            scanf("%f", &op.monto);
        }

        char fifo_resp[100];
        snprintf(fifo_resp, sizeof(fifo_resp), "/tmp/respuesta_%d", getpid());
        if (mkfifo(fifo_resp, 0666) != 0) {
            perror("Error al crear FIFO de respuesta");
        }
        strncpy(op.respuesta, fifo_resp, sizeof(op.respuesta));

        OperacionThreadData *data = malloc(sizeof(OperacionThreadData));
        if (!data) {
            perror("Error al asignar memoria para la operación.");
            continue;
        }
        strncpy(data->fifo_path, fifo_path, sizeof(data->fifo_path));
        data->op = op;
        strncpy(data->fifo_resp, fifo_resp, sizeof(data->fifo_resp));

        sem_wait(&sem_hilos);

        pthread_t hilo;
        if (pthread_create(&hilo, NULL, procesar_operacion_usuario, data) != 0) {
            perror("Error creando hilo para operación.");
            sem_post(&sem_hilos);
            free(data);
            continue;
        }

        pthread_join(hilo, NULL);  // Espera al hilo antes de volver a mostrar el menú
    }

    sem_destroy(&sem_hilos);
    close(global_fd_fifo);
    unlink(fifo_path);
    return 0;
}

void *procesar_operacion_usuario(void *arg) {
    OperacionThreadData *data = (OperacionThreadData *)arg;

    pthread_mutex_lock(&mutex_operacion);
    if (write(global_fd_fifo, &data->op, sizeof(DatosOperacion)) == -1) {
        perror("Error al escribir en FIFO");
    }
    pthread_mutex_unlock(&mutex_operacion);

    key_t key = ftok("src/monitor.c", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid != -1) {
        MensajeOperacion m;
        m.tipo = 1;
        m.operacion = data->op.tipo;
        m.numero_cuenta = data->op.numero_cuenta;
        m.cuenta_destino = data->op.cuenta_destino;
        m.monto = data->op.monto;
        if (msgsnd(msgid, &m, sizeof(MensajeOperacion) - sizeof(long), 0) == -1) {
            perror("Error al enviar mensaje al monitor.");
        }
    }

    int fd_resp = open(data->fifo_resp, O_RDONLY);
    if (fd_resp == -1) {
        perror("Error al abrir FIFO de respuesta en hilo.");
    } else {
        char buffer[256];
        ssize_t n = read(fd_resp, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("\n>>> %s\n", buffer);  
            fflush(stdout);
        } else {
            perror("Error leyendo FIFO de respuesta.");
        }
        close(fd_resp);
    }

    unlink(data->fifo_resp);
    free(data);
    sem_post(&sem_hilos);
    pthread_exit(NULL);
}
