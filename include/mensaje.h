#ifndef MENSAJE_H
#define MENSAJE_H

// Estructura que se utiliza para enviar una operación desde 
// el proceso del usuario hacia el servidor (proceso banco) mediante FIFO:
// >> Permite identificar qué tipo de operación se quiere realizar y cómo responderla
// >> El campo respuesta permite que el servidor devuelva el resultado de 
//    la operación a la FIFO del usuario
typedef struct {
    int tipo;             
    int numero_cuenta;
    int cuenta_destino;
    float monto;
    char respuesta[100];
} DatosOperacion;

// Estructura usada para enviar operaciones al monitor a través de una cola 
// de mensajes SysV
typedef struct {
    long tipo;
    int operacion;
    int numero_cuenta;
    int cuenta_destino;
    float monto;
} MensajeOperacion;

#endif
