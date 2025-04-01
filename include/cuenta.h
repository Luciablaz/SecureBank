#ifndef CUENTA_H
#define CUENTA_H

typedef struct {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
} Cuenta;

#endif