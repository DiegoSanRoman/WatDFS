# WatDFS

Este proyecto implementa un sistema de archivos distribuido sencillo llamado **WatDFS**. Fue desarrollado para la asignatura de **Distributed Systems** en el último curso de Computer Science en la *University of Waterloo*, Canadá.

## Descripción

WatDFS se basa en [FUSE](https://github.com/libfuse/libfuse) para montar el sistema de archivos en espacio de usuario y utiliza una biblioteca de *Remote Procedure Call* (RPC) para la comunicación entre un cliente y un servidor. El servidor gestiona las operaciones reales sobre el sistema de archivos mientras que el cliente expone la interfaz de FUSE.

Entre los componentes principales se incluye una implementación de bloqueo lector/escritor (`rw_lock`) para gestionar la concurrencia en el servidor.

## Instalación

1. Clona este repositorio:
   ```bash
   git clone <repo-url>
   cd CS-454
   ```
2. Asegúrate de tener instalado **g++** y las bibliotecas de **FUSE**.
   En sistemas basados en Debian esto puede hacerse con:
   ```bash
   sudo apt-get install build-essential libfuse-dev
   ```
3. Compila los binarios ejecutando:
   ```bash
   make
   ```
   El comando genera `libwatdfs.a`, `watdfs_server` y `watdfs_client`.

## Uso

1. Ejecuta el servidor indicando el directorio donde persistirá la información:
   ```bash
   ./watdfs_server /ruta/al/directorio
   ```
   Al iniciarse, el servidor imprimirá dos variables de entorno que debes exportar en la terminal donde ejecutes el cliente:
   ```bash
   export SERVER_ADDRESS=<direccion>
   export SERVER_PORT=<puerto>
   ```
2. En otra terminal, lanza el cliente indicando el punto de montaje FUSE:
   ```bash
   ./watdfs_client /ruta/punto_montaje
   ```
   Una vez montado, podrás usar `WatDFS` como un sistema de archivos normal.

## Estructura del proyecto

- `Makefile` – reglas de compilación de cliente y servidor.
- `watdfs_server.cpp` – implementación del servidor RPC.
- `watdfs_client.cpp` y `watdfs_client.h` – cliente basado en FUSE.
- `rw_lock.cpp` y `rw_lock.h` – bloqueo lector/escritor utilizado por el servidor.
- `rpc.h` y `librpc.a` – biblioteca RPC utilizada para la comunicación.
- `libwatdfsmain.a` – contiene la función `main` del cliente.
- `debug.h` – macros de depuración.
- `DOC` – breve descripción del código base y cómo encaja cada fichero.
- `watdfs.zip` – copia comprimida del proyecto original.

## ¿Para qué sirve?

WatDFS es un ejercicio académico que muestra cómo construir un sistema de archivos distribuido de forma sencilla. Sirve como base para experimentar con RPC, sincronización y FUSE, y puede ampliarse para soportar más operaciones o políticas de caché.

## Colaboradores

- Diego San Román Posada
