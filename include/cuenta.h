#ifndef CUENTA_H
#define CUENTA_H

// Estructura de datos que representa una cuenta bancaria.
// Se guarda en el archivo cuentas.dat y se utiliza en todas 
// las operaciones bancarias: depósitos, retiros, transferencias, consultas...
typedef struct {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
} Cuenta;

#endif