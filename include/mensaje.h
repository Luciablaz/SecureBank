#ifndef MENSAJE_H
#define MENSAJE_H

// Estructura para comunicación por FIFO
typedef struct {
    int tipo;             // 1=depósito, 2=retiro, 3=transferencia, 4=consulta
    int numero_cuenta;
    int cuenta_destino;
    float monto;
    char respuesta[100];  // FIFO de respuesta para imprimir en la terminal del usuario
} DatosOperacion;

// Estructura para la cola de mensajes con "long" tipo obligatorio
typedef struct {
    long tipo;
    int operacion;
    int numero_cuenta;
    int cuenta_destino;
    float monto;
} MensajeOperacion;

#endif
