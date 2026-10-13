# Matriz de trazabilidad

Esta matriz relaciona los requisitos del enunciado con la implementación, su verificación y su estado. Debe actualizarse cada vez que se agregue una estrategia o prueba.

| ID | Requisito | Implementación | Verificación | Estado |
|---|---|---|---|---|
| R01 | Código propio en C | `include/`, `src/`, `apps/` | `make` | Cumplido en el avance |
| R02 | Uso de Open MPI | `apps/bruteforce_mpi.c` | `make mpi`, `make demo-mpi` | Cumplido: naive |
| R03 | Versión secuencial | `apps/bruteforce_seq.c` | `make demo` | Cumplido |
| R04 | Acercamiento naive | Partición contigua en `bruteforce_mpi.c` | Ejecución con `-np 4` | Cumplido |
| R05 | Dos acercamientos alternativos | Cíclico y master-worker dinámico | `make test-mpi` | Cumplido |
| R06 | Texto cargado desde `.txt` | `src/file_io.c`, `des_tool` | `data/mensaje.txt` | Cumplido |
| R07 | Llave como parámetro | Opción `--key` de `des_tool` | `make demo` | Cumplido |
| R08 | Cifrado y descifrado | `src/des_crypto.c` | `tests/test_core.c`, comparación `cmp` | Cumplido |
| R09 | Frase clave configurable | Opción `--phrase` | Demostraciones secuencial y MPI | Cumplido |
| R10 | Mostrar llave, archivo y frase | Salida de las aplicaciones de búsqueda | Inspección de `make demo-mpi` | Cumplido |
| R11 | Medir tiempo | Reloj monotónico y `MPI_Wtime` | Salida `Tiempo`; región de búsqueda diferenciada de tiempo total en `docs/CASOS_LITERAL.md` | Cumplido para los casos medidos |
| R12 | Medir con distintas llaves | `scripts/benchmark.py` y `scripts/required_boundary_cases.py` | CSV históricos y nuevos; `docs/CASOS_LITERAL.md` | Cumplido en rango reducido y casos literales acotados |
| R13 | Calcular speedup | Resumen CSV por llave y estrategia | `python3 scripts/verify_results.py` | Cumplido para campañas con referencia secuencial; no se atribuye speedup a la muestra incompleta |
| R14 | Manejo adecuado de memoria | Liberación por aplicación; inicialización de resultado en `src/search.c` | `make test-all` y `make test-sanitize` del núcleo | Cumplido en rutas probadas; MPI no instrumentado con sanitizadores |
| R15 | Programación defensiva | Validación de rangos, archivos, padding y tamaños | `make test-all` | Cumplido en la base actual |
| R16 | README de ejecución | `README.md` | Repetición desde entorno limpio | Cumplido |
| R17 | Bitácora de pruebas | `results/final-campaign/`, `results/required-*/` y `results/key-lengths/` | 105 históricas, 20 de llave 123456, 10 de frontera y 30 secuenciales por longitud; verificador CSV | Cumplido para pruebas realizadas |
| R18 | Explicación de primitivas MPI | Anexo B de `papers/INFORME_FINAL.md` | Revisión documental | Cumplido |
| R19 | Catálogo de funciones y bibliotecas | Anexo A de `papers/INFORME_FINAL.md`, apartados A.4-A.6 | Nombre, tipo y descripción de parámetros y salidas; dependencias usadas en el código | Cumplido para rutinas y bibliotecas del proyecto |
| R20 | Capturas de evidencia | [Galería y alcance](EVIDENCIA.md), [captura de cifrado](../evidence/encrypt_decrypt_42.png), [pruebas](../evidence/tests.png), [sanitizadores](../evidence/sanitizers.png), [MPI naive](../evidence/mpi_naive.png), [MPI cíclico](../evidence/mpi_cyclic.png), [MPI dinámico](../evidence/mpi_dynamic.png), [llave 123456](../evidence/key_123456.png), [frontera rápida](../evidence/quarter_first.png), [prefijo acotado](../evidence/quarter_minus_one_prefix.png), [resúmenes](../evidence/summaries.png), [histórico](../evidence/historical_summary.png) | Capturas reales de terminal con comandos y resultados; `docs/EVIDENCIA.md` delimita cada una | Cumplido para ejecuciones disponibles; no se presenta extra de clúster |
| R21 | Llaves literales 123456, `2^56/4-1` y `2^56/4` | `scripts/benchmark.py`, `scripts/required_boundary_cases.py` | `docs/CASOS_LITERAL.md` y CSV nuevos | 123456 y `2^56/4` encontrados; `2^56/4-1` solo prefijo de 1 millón y estimación separada |
| R22 | Prueba DES independiente | Vector NIST SP 800-17 en `tests/test_core.c` | `make test` y `make test-sanitize` | Cumplido |
| R23 | Datos de integrantes, carátula y formato de informe | [Informe fuente](../papers/INFORME_FINAL.md), [PDF exportado](../papers/INFORME_FINAL.pdf), `scripts/build_report_pdf.ps1` | Nombres y carnés proporcionados por el equipo; portada centrada, índice con páginas verificadas, tamaño carta, folios, cuerpo y anexos revisados; DOCX de `EJEMPLOS/` usado solo como referencia visual | Cumplido para el formato y contenido verificables con los archivos disponibles |
| R24 | Tiempo secuencial con distintas longitudes de llave (A.3) | `scripts/benchmark_key_lengths.py` | `results/key-lengths/` y `docs/CASOS_LITERAL.md` | Cumplido para seis longitudes decimales medidas |

## Convenciones para mantener la trazabilidad

- Cada requisito nuevo debe recibir un ID estable.
- Toda funcionalidad debe enlazar una implementación y una forma de verificarla.
- Un requisito solo se marca como cumplido cuando existe evidencia reproducible.
- Los resultados de rendimiento deben incluir entrada, llave, procesos, versión del programa y parámetros.
- Los archivos generados en `build/` y `bin/` no se consideran evidencia durable; comandos, CSV, registros y capturas relevantes están en `docs/`, `results/` y `evidence/`.

## Correspondencia con la rúbrica del informe

| Criterio de la guía | Ubicación comprobada | Alcance |
|---|---|---|
| Antecedentes DES, cifrado, fuerza bruta y diagrama | Informe §§4.1-4.3, figuras y Anexo A.3 | Incluye flujo DES y equivalencias de rutinas del programa base. |
| Carátula, índice, introducción, cuerpo, citas, conclusiones y apéndices | PDF pp. 1-3, §§2-15 y anexos A-D | Portada e índice siguen el diseño de referencia; cinco fuentes bibliográficas y una cita textual breve verificada contra MPI Forum. |
| Retos, aprendizajes, soluciones y dos alternativas MPI | Informe §§6, 9-11 | Compara naive, cíclico y dinámico; limita conclusiones al entorno medido. |
| Tres posiciones de llave y speedups frente a secuencial y naive | Informe §§7-8; `results/final-campaign/` | 105 ejecuciones históricas preservadas y verificadas. |
| Anexo 1: funciones y bibliotecas | Informe Anexo A.4-A.6 | Nombres, tipos, uso, entradas y salidas de rutinas; bibliotecas reales del código. |
| Anexo 2: bitácora, speedups y capturas | Informe Anexos C-D; `results/` y `evidence/` | Once capturas reales y CSV. La llave 18014398509481983 tiene solo un intento acotado y una extrapolación separada. |
