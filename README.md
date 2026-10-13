# Proyecto 2 — Computación Paralela y Distribuida

Implementación modular de una búsqueda de llaves DES por fuerza bruta. Incluye cifrado y descifrado de archivos, búsqueda secuencial y tres estrategias con Open MPI: naive, cíclica y dinámica master-worker. Repositorio: <https://github.com/DavidDominguez-11/Paralela-Proyecto2>.

DES se utiliza porque es un requisito académico del proyecto. No debe utilizarse para proteger información real.

## Estado verificado

- Cifrado y descifrado DES de archivos: funcional.
- Padding PKCS#7 y manejo binario de longitudes: funcional.
- Búsqueda secuencial por rango: funcional.
- Búsqueda MPI naive con bloques contiguos: funcional.
- Búsqueda MPI con distribución cíclica: funcional.
- Búsqueda MPI dinámica master-worker: funcional.
- Pruebas de núcleo, CLI, MPI y benchmark: ejecutadas con éxito en WSL 2.
- Campaña histórica de 105 mediciones: disponible y verificada.
- Casos literales del enunciado: llave 123456 y llave 18014398509481984
  medidas; para 18014398509481983 existe una muestra acotada y una estimación
  explícita, no una búsqueda completa.
- Serie secuencial de llaves de uno a seis dígitos decimales: 30 mediciones
  disponibles en `results/key-lengths/`.
- Capturas de ejecuciones reales: disponibles en `evidence/`.
- Integrantes: David Domínguez (23712), Jonathan Diaz (23873) y Luis Padilla
  (23663), según información proporcionada por el equipo.
- [Informe final en Markdown](papers/INFORME_FINAL.md) y [PDF](papers/INFORME_FINAL.pdf)
  exportado desde el Markdown. El PDF usa el diseño visual del DOCX de
  `EJEMPLOS/` y conserva únicamente el contenido de este proyecto.

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
scripts/benchmark.py    Campaña reproducible y resumen estadístico
data/mensaje.txt        Entrada reproducible de demostración
docs/                   Enunciado, decisiones y trazabilidad
```

La criptografía, entrada/salida y búsqueda están separadas de las aplicaciones. Las tres estrategias paralelas reutilizan el mismo núcleo sin duplicar DES ni la validación de candidatos.

## Dependencias

En Ubuntu 24.04 o WSL con Ubuntu 24.04:

```bash
sudo apt update
sudo apt install build-essential libssl-dev openmpi-bin libopenmpi-dev
```

La implementación usa la API DES de OpenSSL 3. Esta API está marcada como obsoleta por OpenSSL, pero se conserva detrás de `des_crypto.h` para poder reemplazarla sin afectar las aplicaciones.

Las mediciones nuevas se hicieron en Ubuntu 24.04.1 sobre WSL 2, con GCC
13.3.0, OpenSSL 3.0.13, Open MPI 4.1.6 y Python 3.12.3. El comando
`bash scripts/environment.sh` muestra las versiones del entorno actual.

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

El núcleo también puede comprobarse con AddressSanitizer y
UndefinedBehaviorSanitizer:

```bash
make test-sanitize
```

Este objetivo ejecuta `test_core` instrumentado; no cubre por sí solo los
procesos MPI. Los resultados registrados están en
[Evidencia visual](docs/EVIDENCIA.md).

También pueden ejecutarse por separado las pruebas de integración:

```bash
make test-cli
make test-mpi
make test-benchmark
```

## Mediciones y speedup

La campaña predeterminada se ejecuta con:

```bash
make benchmark
```

La campaña final reproducible, con las posiciones relativas de llave solicitadas por el enunciado, se ejecuta con:

```bash
make benchmark-final
```

Ese objetivo vuelve a escribir `results/final-campaign/raw.csv` y
`summary.csv`. Para conservar los datos históricos, ejecute el comando
equivalente de `scripts/benchmark.py` con otro `--output-dir`; las campañas
nuevas de este repositorio ya usan directorios separados.

Una campaña personalizada puede indicar llaves, rango, procesos, repeticiones y parámetros de sincronización:

```bash
python3 scripts/benchmark.py \
  --keys 1000,10000,50000 \
  --max-key 100000 \
  --processes 2,4 \
  --repetitions 5 \
  --warmups 1 \
  --check-interval 4096 \
  --chunk-size 4096 \
  --output-dir build/benchmarks
```

La herramienta valida cada llave recuperada y produce:

- `raw.csv`: una fila por ejecución, con metadatos, intentos y tiempo.
- `summary.csv`: mediana, mínimo, máximo, speedup y eficiencia por estrategia.

El speedup se calcula contra la mediana secuencial de la misma llave. `efficiency_total` divide entre todos los procesos MPI; `efficiency_workers` divide entre los procesos que efectivamente buscan, útil para interpretar master-worker.

Para la llave literal 123456 y los dos casos cercanos a `2^56/4`, consulte
[Casos literales](docs/CASOS_LITERAL.md). Las campañas se reproducen con:

```bash
python3 scripts/benchmark.py --skip-build --keys 123456 \
  --max-key 72057594037927936 --processes 4 --repetitions 5 --warmups 1 \
  --check-interval 4096 --chunk-size 4096 --seed 2026 \
  --output-dir results/required-123456
python3 scripts/required_boundary_cases.py \
  --output-dir results/required-boundary --prefix-max-key 1000000 \
  --repetitions 5 --warmups 1 --check-interval 4096 \
  --timeout-seconds 30
python3 scripts/benchmark_key_lengths.py
python3 scripts/verify_results.py
```

`required_boundary_cases.py` realiza cinco búsquedas completas y rápidas de
la llave al inicio del segundo bloque y cinco búsquedas **limitadas al prefijo
de un millón** para la llave al final del primero. Cada subproceso tiene un
límite de 30 segundos. Los tiempos de búsqueda excluyen arranque y preparación;
el CSV de frontera añade el tiempo total observado del subproceso.

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

- [Informe final](papers/INFORME_FINAL.md)
- [Informe final en PDF con capturas incrustadas](papers/INFORME_FINAL.pdf)
- [Informe del avance](docs/AVANCE.md)
- [Campaña experimental](docs/CAMPANA.md)
- [Casos literales y estimación acotada](docs/CASOS_LITERAL.md)
- [Capturas de evidencia](docs/EVIDENCIA.md)
- [Matriz de trazabilidad](docs/TRAZABILIDAD.md)
- [Propuesta de mejora](docs/propuesta-mejora-fase0.md)

Para repetir las capturas en un escritorio Windows interactivo se necesita
Python con Pillow (`python -m pip install Pillow`) y WSL Ubuntu 24.04 con las
dependencias anteriores. Ejecute `python scripts/capture_evidence.py`; las
imágenes se guardan en `evidence/`. Las capturas son ejecuciones adicionales,
por lo que sus tiempos individuales pueden diferir de las medianas CSV.

Para actualizar el PDF en Windows, instale `markdown-it-py` en Python y
Graphviz (`dot`), y ejecute `./scripts/build_report_pdf.ps1` con Chrome
instalado. El script genera el HTML desde el Markdown, imprime en tamaño
**carta**, calcula los números reales del índice, verifica el PDF y añade los
folios. El PDF contiene las once capturas del Anexo D. El DOCX de `EJEMPLOS/`
se usó solamente como referencia de diseño; sus datos y su contenido no se
incorporaron al informe.
