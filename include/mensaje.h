#ifndef MENSAJE_H
#define MENSAJE_H

// Estructura para comunicación por FIFO
typedef struct {
    int tipo;
    int numero_cuenta;
    int cuenta_destino;
    float monto;
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
