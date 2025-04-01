#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include "../include/cuenta.h"
#include "../include/mensaje.h"
#include "../include/config.h"   // Para leer_configuracion

#define SEMAFORO_NOMBRE "/cuentas_sem"

sem_t *semaforo;
static Config configuracion; // <-- Paso 3: variable global para almacenar la config

typedef struct {
    int tipo;
    int numero_cuenta;
    float monto;
    int cuenta_destino;
} DatosOperacion;

int buscar_cuenta(int numero_cuenta, FILE *archivo, Cuenta *cuenta) {
    rewind(archivo);
    while (fread(cuenta, sizeof(Cuenta), 1, archivo) == 1) {
        if (cuenta->numero_cuenta == numero_cuenta) {
            return ftell(archivo) - sizeof(Cuenta);
        }
    }
    return -1;
}

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

void realizar_operacion(int tipo, int numero_cuenta, float monto, int cuenta_destino) {
    sem_wait(semaforo);
    FILE *archivo = fopen(configuracion.archivo_cuentas, "rb+");
    if (!archivo) {
        perror("Error al abrir archivo de cuentas");
        sem_post(semaforo);
        return;
    }

    Cuenta cuenta = {0};
    int pos = buscar_cuenta(numero_cuenta, archivo, &cuenta);
    if (pos == -1) {
        printf("Cuenta no encontrada.\n");
        fclose(archivo);
        sem_post(semaforo);
        return;
    }

    switch (tipo) {
        case 1:
            cuenta.saldo += monto;
            printf("Depósito realizado. Nuevo saldo: %.2f\n", cuenta.saldo);
            break;
        case 2:
            if (monto > cuenta.saldo) {
                printf("Fondos insuficientes.\n");
                fclose(archivo);
                sem_post(semaforo);
                return;
            }
            cuenta.saldo -= monto;
            printf("Retiro realizado. Nuevo saldo: %.2f\n", cuenta.saldo);
            break;
        case 3: {
            if (monto > cuenta.saldo) {
                printf("Fondos insuficientes para transferencia.\n");
                fclose(archivo);
                sem_post(semaforo);
                return;
            }

            Cuenta cuenta_dest = {0};
            int pos_dest = buscar_cuenta(cuenta_destino, archivo, &cuenta_dest);
            if (pos_dest == -1) {
                printf("Cuenta destino no encontrada.\n");
                fclose(archivo);
                sem_post(semaforo);
                return;
            }

            cuenta.saldo -= monto;
            cuenta_dest.saldo += monto;

            fseek(archivo, pos, SEEK_SET);
            fwrite(&cuenta, sizeof(Cuenta), 1, archivo);

            fseek(archivo, pos_dest, SEEK_SET);
            fwrite(&cuenta_dest, sizeof(Cuenta), 1, archivo);

            printf("Transferencia realizada con éxito.\n");
            fclose(archivo);
            sem_post(semaforo);
            enviar_transaccion_monitor(tipo, numero_cuenta, monto, cuenta_destino);
            return;
        }
        case 4:
            printf("Saldo actual de la cuenta %d: %.2f\n", numero_cuenta, cuenta.saldo);
            fclose(archivo);
            sem_post(semaforo);
            enviar_transaccion_monitor(tipo, numero_cuenta, monto, cuenta_destino);
            return;
    }

    fseek(archivo, pos, SEEK_SET);
    fwrite(&cuenta, sizeof(Cuenta), 1, archivo);
    fclose(archivo);
    sem_post(semaforo);

    enviar_transaccion_monitor(tipo, numero_cuenta, monto, cuenta_destino);
}

// Función que ejecutará la operación bancaria en el hilo
void* ejecutar_operacion(void* arg) {
    // Recibimos los datos de la operación
    DatosOperacion* datos = (DatosOperacion*) arg;
    realizar_operacion(datos->tipo, datos->numero_cuenta, datos->monto, datos->cuenta_destino);
    return NULL;
}

void mostrar_menu() {

    semaforo = sem_open(SEMAFORO_NOMBRE, 0);
    if (semaforo == SEM_FAILED) {
        perror("Error al abrir semáforo");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int opcion, cuenta, cuenta_destino;
        float monto;
        printf("\n--- Menú Bancario ---\n");
        printf("1. Depósito\n2. Retiro\n3. Transferencia\n4. Consultar saldo\n5. Salir\n");
        printf("Seleccione una opción: ");
        if (scanf("%d", &opcion) != 1) {
            while (getchar() != '\n');
            continue;
        }
        if (opcion == 5) break;

        printf("Ingrese número de cuenta: ");
        if (scanf("%d", &cuenta) != 1) {
            while (getchar() != '\n');
            continue;
        }

        if (opcion == 3) {
            printf("Ingrese cuenta destino: ");
            if (scanf("%d", &cuenta_destino) != 1) {
                while (getchar() != '\n');
                continue;
            }
        } else {
            cuenta_destino = 0;
        }

        if (opcion != 4) {
            printf("Ingrese monto: ");
            if (scanf("%f", &monto) != 1) {
                while (getchar() != '\n');
                continue;
            }
        } else {
            monto = 0;
        }

        // Crear un hilo para ejecutar la operación
        pthread_t hilo;
        DatosOperacion *datos = malloc(sizeof(DatosOperacion));
        if (datos == NULL) {
            perror("Error al asignar memoria");
            exit(EXIT_FAILURE);
        }
        datos->tipo = opcion;
        datos->numero_cuenta = cuenta;
        datos->monto = monto;
        datos->cuenta_destino = cuenta_destino;
        
        pthread_create(&hilo, NULL, ejecutar_operacion, datos);
        pthread_join(hilo, NULL);
        free(datos);
    }
    sem_close(semaforo);
}

int main() {
    // Leer la configuración para usar la ruta de ARCHIVO_CUENTAS
    configuracion = leer_configuracion(CONFIG_FILE);
    printf("Usuario conectado. Accediendo al banco...\n");
    sleep(1);
    mostrar_menu();
    return 0;
}
