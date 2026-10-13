# Casos literales del enunciado: mediciones nuevas

Fecha de ejecución: 13 de octubre de 2026. Estos datos se agregan a la campaña
histórica de `results/final-campaign/`; no la reemplazan.

## Entorno y protocolo

- Ubuntu 24.04.1 LTS en WSL 2, kernel `6.6.87.2-microsoft-standard-WSL2`.
- GCC 13.3.0, OpenSSL 3.0.13, Open MPI 4.1.6 y Python 3.12.3.
- Una computadora (`LAPTOP-ETV994DU`), cuatro procesos MPI (`-np 4`).
- Entrada `data/mensaje.txt` (34 bytes), frase `es una prueba de`.
- DES-ECB con padding PKCS#7; rango `[0, max-key)`.
- Cinco repeticiones medidas y un calentamiento por configuración.
- `--check-interval 4096`; en master-worker, `--chunk-size 4096`.
- Cada proceso externo de los casos de frontera tiene un límite de 30 segundos.
- Revisión Git `01c1151`, con `git_dirty=yes`: los cambios de este trabajo no
  estaban confirmados. No se atribuyen las mediciones a un commit limpio. El
  CSV de frontera registra además SHA-256 del binario, entrada y cifrado.

El tiempo **de búsqueda** es el que imprime cada programa. La versión MPI naive
lo mide con `MPI_Wtime` después de distribuir la entrada y sincronizar los
procesos; excluye arranque de `mpirun`, lectura inicial, difusión y presentación
final. `wall_seconds`, registrado por Python alrededor del subproceso de los
casos de frontera, incluye esos costos. No se mezclan estas dos métricas.

## Llave 123456

Comando de campaña:

```bash
python3 scripts/benchmark.py --skip-build --keys 123456 \
  --max-key 72057594037927936 --processes 4 --repetitions 5 --warmups 1 \
  --check-interval 4096 --chunk-size 4096 --seed 2026 \
  --output-dir results/required-123456
```

Cada ejecución encontró exactamente `123456`. Medianas verificadas desde las
20 filas de `raw.csv`:

| Estrategia | Procesos | Intentos medianos | Búsqueda mediana (s) | Speedup |
|---|---:|---:|---:|---:|
| Secuencial | 1 | 123457 | 0.065287287 | 1.000000 |
| Naive | 4 | 504385 | 0.099789442 | 0.654250 |
| Cíclica | 4 | 129169 | 0.018180595 | 3.591042 |
| Dinámica | 4 (3 workers) | 123457 | 0.024421049 | 2.673402 |

El rango fue el espacio completo de `2^56` candidatos, aunque todas las
búsquedas terminaron temprano al encontrar la llave. Archivos:
[`raw.csv`](../results/required-123456/raw.csv) y
[`summary.csv`](../results/required-123456/summary.csv).

## Búsqueda secuencial con distintas longitudes de llave

Para el inciso A.3 se cifró el mismo texto con llaves decimales de uno a seis
dígitos y se buscó secuencialmente en `[0,200000)`. Cada caso tuvo un
calentamiento y cinco mediciones, con límite externo de 30 segundos:

```bash
python3 scripts/benchmark_key_lengths.py
```

| Dígitos decimales | Llave | Intentos | Búsqueda mediana (s) |
|---:|---:|---:|---:|
| 1 | 9 | 10 | 0.000017744 |
| 2 | 99 | 100 | 0.000063639 |
| 3 | 999 | 1000 | 0.000522524 |
| 4 | 9999 | 10000 | 0.005212388 |
| 5 | 99999 | 100000 | 0.051894619 |
| 6 | 123456 | 123457 | 0.064412157 |

Son longitudes **decimales del valor de la llave**, no tamaños criptográficos
distintos: DES conserva 56 bits efectivos en todos los casos. La primera
medición dura solo microsegundos y es sensible al ruido del reloj y del
sistema; la tendencia responde sobre todo al número de candidatos probados.
Los tiempos totales del subproceso y cada repetición están en
[`raw.csv`](../results/key-lengths/raw.csv) y
[`summary.csv`](../results/key-lengths/summary.csv).

## Llaves alrededor de 2^56 / 4

`2^56 / 4 = 18014398509481984`. El enunciado pide las llaves
`18014398509481983` y `18014398509481984`.

Comando reproducible:

```bash
python3 scripts/required_boundary_cases.py \
  --output-dir results/required-boundary --prefix-max-key 1000000 \
  --repetitions 5 --warmups 1 --check-interval 4096 \
  --timeout-seconds 30
```

| Llave cifrada | Rango buscado | Estado real | Intentos medianos | Búsqueda mediana (s) | Total mediano (s) |
|---:|---:|---|---:|---:|---:|
| 18014398509481983 | `[0,1000000)` | Sin hallazgo **en ese prefijo** | 1000000 | 0.165618548 | 1.552298223 |
| 18014398509481984 | `[0,2^56)` | Encontrada | 12289 | 0.002423341 | 0.398726026 |

Para `18014398509481984`, el rank 1 comienza exactamente en la llave y la
encuentra en su primer intento. Con el intervalo de 4096, los otros tres
ranks completaron un lote antes de la reducción: `3 × 4096 + 1 = 12289`
intentos. Es una búsqueda real sobre el rango completo, terminada temprano.

Para `18014398509481983`, se cifró el texto con esa llave y **solo** se probó
el prefijo `[0,1000000)`. Al no pertenecer al prefijo, la ausencia de hallazgo
es el resultado esperado; no equivale a haber agotado `2^56`. Los cuatro ranks
probaron 250000 candidatos cada uno. El script acepta el código de salida no
cero del programa en ese caso y guarda la salida y el código de retorno.
Archivos: [`raw.csv`](../results/required-boundary/raw.csv),
[`summary.csv`](../results/required-boundary/summary.csv) y los registros
`*.log` de cada repetición. Los `.des` son auxiliares regenerables y están
excluidos de Git.

### Extrapolación, separada de las mediciones

Si el tiempo de búsqueda por lote y el rendimiento de cada rank se mantuvieran
constantes durante una búsqueda completa, el rank 0 tendría que examinar
`2^54 = 18014398509481984` candidatos para llegar a la llave anterior. En la
muestra examinó 250000. La extrapolación basada en la mediana medida es:

```text
factor = 18014398509481984 / 250000 = 72057594037.927936
tiempo hipotético = 0.165618548 s × factor
                   ≈ 11 934 074 097 s ≈ 378 años (365.25 días/año)
```

**378 años no es un tiempo observado ni una cota garantizada.** La muestra
corre solo un millón de candidatos, con segmentos distintos de los del rango
completo. Extrapolar unas once órdenes de magnitud ignora cambios de hardware,
temperatura, carga, comunicación y rendimiento sostenido. Su utilidad es
mostrar por qué se acotó el intento. Una medición completa requeriría un
entorno y tiempo de cómputo ajenos al equipo disponible.

## Verificación y procedencia

```bash
python3 scripts/verify_results.py
```

El verificador recalcula medianas, extremos, speedups y eficiencias de la
campaña histórica y de la llave 123456, y las medianas y estados de frontera
y de la serie secuencial de longitudes.
Las capturas de `evidence/` son **ejecuciones adicionales visibles** hechas con
los mismos comandos y parámetros. Sus tiempos individuales no se presentan
como si fueran una de las filas del CSV.
