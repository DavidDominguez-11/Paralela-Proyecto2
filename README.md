# Proyecto 2 — Computación Paralela y Distribuida

Implementación modular de una búsqueda de llaves DES por fuerza bruta. Incluye cifrado y descifrado de archivos, búsqueda secuencial y tres estrategias con Open MPI: naive, cíclica y dinámica master-worker.

DES se utiliza porque es un requisito académico del proyecto. No debe utilizarse para proteger información real.

## Estado del avance

- Cifrado y descifrado DES de archivos: funcional.
- Padding PKCS#7 y manejo binario de longitudes: funcional.
- Búsqueda secuencial por rango: funcional.
- Búsqueda MPI naive con bloques contiguos: funcional.
- Búsqueda MPI con distribución cíclica: funcional.
- Búsqueda MPI dinámica master-worker: funcional.
- Prueba automatizada del núcleo: funcional.
- Campaña completa de mediciones y speedup: pendiente.

El archivo `bruteforce.c` de la raíz se conserva únicamente como referencia proporcionada por el curso. No forma parte de la compilación.

## Arquitectura

```text
include/project2/       Interfaces públicas
src/                    Implementación reutilizable
apps/des_tool.c         Cifrado y descifrado de archivos
apps/bruteforce_seq.c   Búsqueda secuencial
apps/bruteforce_mpi.c           Búsquedas MPI naive y cíclica
apps/bruteforce_mpi_dynamic.c   Búsqueda dinámica master-worker
tests/test_core.c       Pruebas del núcleo
data/mensaje.txt        Entrada reproducible de demostración
docs/                   Enunciado, decisiones y trazabilidad
```

La criptografía, entrada/salida y búsqueda están separadas de las aplicaciones. Las siguientes estrategias paralelas podrán reutilizar el mismo núcleo sin duplicar DES ni la validación de candidatos.

## Dependencias

En Ubuntu 24.04 o WSL con Ubuntu 24.04:

```bash
sudo apt update
sudo apt install build-essential libssl-dev openmpi-bin libopenmpi-dev
```

La implementación usa la API DES de OpenSSL 3. Esta API está marcada como obsoleta por OpenSSL, pero se conserva detrás de `des_crypto.h` para poder reemplazarla sin afectar las aplicaciones.

## Compilación

```bash
make
```

Los ejecutables se generan en `bin/`:

- `des_tool`
- `bruteforce_seq`
- `bruteforce_mpi`
- `bruteforce_mpi_cyclic`
- `bruteforce_mpi_dynamic`
- `test_core`, después de ejecutar `make test`

## Pruebas

Prueba automatizada del núcleo:

```bash
make test
```

Suite completa del núcleo, aplicaciones de línea de comandos y MPI:

```bash
make test-all
```

También pueden ejecutarse por separado las pruebas de integración:

```bash
make test-cli
make test-mpi
```

Demostración secuencial completa:

```bash
make demo
```

Demostración secuencial y MPI con cuatro procesos:

```bash
make demo-mpi
```

## Uso manual

Cifrar un archivo con la llave 42:

```bash
./bin/des_tool encrypt \
  --input data/mensaje.txt \
  --output build/mensaje.des \
  --key 42
```

Descifrarlo:

```bash
./bin/des_tool decrypt \
  --input build/mensaje.des \
  --output build/mensaje.dec.txt \
  --key 42
```

Buscar la llave secuencialmente en el rango `[0, 1000)`:

```bash
./bin/bruteforce_seq \
  --input build/mensaje.des \
  --phrase "es una prueba de" \
  --max-key 1000
```

Buscarla con cuatro procesos MPI:

```bash
mpirun -np 4 ./bin/bruteforce_mpi \
  --input build/mensaje.des \
  --phrase "es una prueba de" \
  --max-key 1000 \
  --check-interval 16
```

Buscarla con distribución cíclica:

```bash
mpirun -np 4 ./bin/bruteforce_mpi_cyclic \
  --input build/mensaje.des \
  --phrase "es una prueba de" \
  --max-key 1000 \
  --check-interval 16
```

Buscarla con asignación dinámica de bloques:

```bash
mpirun -np 4 ./bin/bruteforce_mpi_dynamic \
  --input build/mensaje.des \
  --phrase "es una prueba de" \
  --max-key 1000 \
  --chunk-size 16
```

`--max-key` es exclusivo. El ejemplo anterior recorre desde 0 hasta 999. La opción `--check-interval` controla cuántas llaves procesa cada proceso entre sincronizaciones colectivas.
En master-worker, `--chunk-size` controla cuántas llaves recibe un worker por solicitud. El proceso 0 coordina y los procesos restantes ejecutan la búsqueda.

## Documentación del avance

- [Informe del avance](docs/AVANCE.md)
- [Matriz de trazabilidad](docs/TRAZABILIDAD.md)
- [Propuesta de mejora](docs/propuesta-mejora-fase0.md)
