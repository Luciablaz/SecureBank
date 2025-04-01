#ifndef MENSAJE_H
#define MENSAJE_H

#include <sys/ipc.h>
#include <sys/msg.h>

#define CLAVE_COLA 1234  // Clave fija para la cola de mensajes

// Estructura para enviar mensajes (transacciones)
typedef struct {
    long tipo;
    char texto[100];
} MsgOperacion;

#endif
