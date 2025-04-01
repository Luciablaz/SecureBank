#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>  // mkfifo
#include <string.h>           // Para usar strstr, strcmp, etc. (por si lo necesitas)
#include <errno.h>            // Para usar errno (por si lo necesitas)
#include "../include/mensaje.h"  // Para CLAVE_COLA (al final eliminaremos la cola de mensajes)
#include "../include/banco.h"
#include "../include/config.h"

#define MAX_USUARIOS 5

sem_t *semaforo;
int servidor_corriendo = 1;

// ADAPTACIÓN TUBERÍA: descriptor global para leer la tubería
static int fd_fifo_lec = -1;

void inicializar_semaforo() {
    semaforo = sem_open(SEMAFORO_NOMBRE, O_CREAT, 0644, MAX_USUARIOS);
    if (semaforo == SEM_FAILED) {
        perror("Error al crear semáforo");
        exit(EXIT_FAILURE);
    }
}

void manejar_senal(int signo) {
    if (signo == SIGINT){
        printf("\nSeñal recibida. Cerrando SecureBank...\n");
    }
    servidor_corriendo = 0;
    sem_close(semaforo);
    sem_unlink(SEMAFORO_NOMBRE);
    // Eliminar la cola de mensajes (Paso 5)
    int cola_mensajes = msgget(CLAVE_COLA, 0666);
    if (cola_mensajes != -1) {
        msgctl(cola_mensajes, IPC_RMID, NULL);
    }
    // ADAPTACIÓN TUBERÍA: cerrar y eliminar la tubería
    if (fd_fifo_lec >= 0) {
        close(fd_fifo_lec);
    }
    unlink(PIPE_NOMBRE);
    exit(0);
}

int seleccionar_usuario() {
    int usuario_id;
    printf("\nSeleccione un usuario para iniciar sesión (1-5): ");
    while (scanf("%d", &usuario_id) != 1 || usuario_id < 1 || usuario_id > 5) {
        printf("Entrada inválida. Intente de nuevo: ");
        while (getchar() != '\n');
    }
    return usuario_id;
}

void manejar_usuario(int id) {
    char id_str[10];
    sprintf(id_str, "%d", id);
    execl("./bin/usuario", "./bin/usuario", id_str, NULL);
    perror("Error al ejecutar usuario");
    exit(EXIT_FAILURE);
}

int main() {
    // Paso 3: Leer config
    Config configuracion = leer_configuracion(CONFIG_FILE);
    printf("Límite retiro: %d\n", configuracion.limite_retiro);
    printf("Límite transferencia: %d\n", configuracion.limite_transferencia);
    printf("Archivo cuentas: %s\n", configuracion.archivo_cuentas);
    printf("Archivo log: %s\n", configuracion.archivo_log);

    signal(SIGINT, manejar_senal);
    signal(SIGTERM, manejar_senal);

    printf("Iniciando SecureBank...\n");
    inicializar_semaforo();

    // Crear FIFO (si ya existe, mkfifo devolverá EEXIST — no es error)
    if (mkfifo(PIPE_NOMBRE, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo fallo");
    }

    // Abrir FIFO para lectura en modo no‑bloqueante
    fd_fifo_lec = open(PIPE_NOMBRE, O_RDONLY | O_NONBLOCK);
    if (fd_fifo_lec < 0 && errno != ENOENT) {
        perror("Error abriendo tubería en modo lectura");
    }
    // Si errno == ENOENT, simplemente ignoramos (el monitor la creará luego)

    while (servidor_corriendo) {
        // ADAPTACIÓN TUBERÍA: leer posibles alertas del monitor
        char buffer[256];
        ssize_t bytes = read(fd_fifo_lec, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("[ALERTA DEL MONITOR] %s\n", buffer);
        }
        
        printf("Esperando un nuevo usuario...\n");
        int usuario_id = seleccionar_usuario();

        sem_wait(semaforo);
        pid_t pid = fork();
        if (pid == 0) {
            manejar_usuario(usuario_id);
        }

        int status;
        wait(&status);
        sem_post(semaforo);
        sleep(1);
    }

    // Si por alguna razón salimos del bucle sin señal, cerramos la tubería
    if (fd_fifo_lec >= 0) {
        close(fd_fifo_lec);
    }
    unlink(PIPE_NOMBRE);

    return 0;
}
