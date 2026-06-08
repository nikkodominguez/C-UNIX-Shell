# C UNIX Shell

Este repositorio contiene la implementación de un **Shell interactivo (intérprete de comandos)** para sistemas UNIX, desarrollado en C. Este proyecto es el resultado de las prácticas de la asignatura de Sistemas Operativos, y su objetivo es comprender a bajo nivel cómo interactúa el software de nivel de usuario con el kernel del Sistema Operativo a través de llamadas al sistema.

## Características principales

El shell no solo permite la ejecución de comandos externos, sino que incluye un amplio conjunto de comandos internos diseñados para gestionar diferentes recursos del sistema operativo de forma manual:

* **Gestión del Historial:** Registro y repetición de comandos ejecutados anteriormente.
* **Gestión de Ficheros y E/S:** Apertura, cierre y duplicación de descriptores de ficheros, así como el listado de ficheros abiertos por el shell.
* **Gestión de Memoria:** Asignación y desasignación de memoria dinámica (malloc), memoria compartida (shared) y ficheros mapeados en memoria (mmap). Incluye comandos para volcar (dump) y llenar (fill) zonas de memoria.
* **Gestión de Procesos:** Ejecución de procesos en primer plano (foreground) y en segundo plano (background). Listado de procesos activos, finalizados y suspendidos, junto con la gestión de señales.

## Estructura del Código

El proyecto está modularizado para mantener un código limpio y escalable. Se divide en los siguientes módulos principales:

* `shell.c`: Contiene el bucle principal, que lee la entrada del usuario, la parsea y delega la ejecución.
* `commands.c` / `.h`: Implementación de la lógica de los comandos internos soportados por el shell.
* `command_list.c` / `.h`: Implementación de una lista enlazada para gestionar el historial de comandos ejecutados.
* `file_list.c` / `.h`: Estructura de datos para realizar el seguimiento de los ficheros y descriptores abiertos por la sesión del shell.
* `memory_list.c` / `.h`: Módulo encargado de llevar el registro de las zonas de memoria asignadas por el usuario.
* `process_list.c` / `.h`: Gestión de la tabla de procesos en segundo plano, controlando sus PIDs, estados y líneas de comando.

## Compilación y Ejecución

El proyecto incluye un `Makefile` para facilitar la compilación. 

Para compilar el proyecto, simplemente clona el repositorio y ejecuta el siguiente comando en la raíz:

```bash
make
