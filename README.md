# SecureBank - Simulador Bancario

SecureBank es un simulador bancario desarrollado en C que permite realizar operaciones bancarias en tiempo real mediante múltiples procesos e hilos, con control de concurrencia, detección de anomalías y comunicación entre procesos (IPC).

---

##  Estructura del proyecto

SecureBank-develop/ 
├── src/ → Código fuente principal en C 
├── bin/ → Ejecutables compilados (banco, monitor, usuario, init_cuentas) 
├── include/ → Archivos .h con estructuras y funciones comunes 
├── data/ → Archivos de configuración y cuentas (cuentas.dat, config.txt) 
├── logs/ → Archivo securebank.log con operaciones registradas 
├── Makefile → Script de compilación 
└── README.md → Este documento

---

##  Requisitos

- Sistema operativo Linux 
- Compilador gcc 
- make instalado 
- Terminal gráfica compatible (gnome-terminal) 
- Librerías POSIX (-pthread, -lrt)

---

##  Compilación

Desde la raíz del proyecto, ejecutar:

make clean
make

Esto compilará todos los archivos fuente y generará los ejecutables dentro de la carpeta bin/.

A continuación, sigue estos pasos para ejecutar el sistema:

1. Inicializar las cuentas
(Solo la primera vez o cuando quieras reiniciar los datos)

./bin/init_cuentas

2. En la misma terminal, lanzar el banco

./bin/banco

3. En una nueva terminal, ejecutar el monitor

./bin/monitor

Selecciona un número de usuario (del 1 al 4). Se abrirá automáticamente una nueva terminal para ese usuario.