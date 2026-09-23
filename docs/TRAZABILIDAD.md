# Matriz de trazabilidad

Esta matriz relaciona los requisitos del enunciado con la implementación, su verificación y su estado. Debe actualizarse cada vez que se agregue una estrategia o prueba.

| ID | Requisito | Implementación | Verificación | Estado |
|---|---|---|---|---|
| R01 | Código propio en C | `include/`, `src/`, `apps/` | `make` | Cumplido en el avance |
| R02 | Uso de Open MPI | `apps/bruteforce_mpi.c` | `make mpi`, `make demo-mpi` | Cumplido: naive |
| R03 | Versión secuencial | `apps/bruteforce_seq.c` | `make demo` | Cumplido |
| R04 | Acercamiento naive | Partición contigua en `bruteforce_mpi.c` | Ejecución con `-np 4` | Cumplido |
| R05 | Dos acercamientos alternativos | Diseño en `propuesta-mejora-fase0.md` | Pendiente | No iniciado |
| R06 | Texto cargado desde `.txt` | `src/file_io.c`, `des_tool` | `data/mensaje.txt` | Cumplido |
| R07 | Llave como parámetro | Opción `--key` de `des_tool` | `make demo` | Cumplido |
| R08 | Cifrado y descifrado | `src/des_crypto.c` | `tests/test_core.c`, comparación `cmp` | Cumplido |
| R09 | Frase clave configurable | Opción `--phrase` | Demostraciones secuencial y MPI | Cumplido |
| R10 | Mostrar llave, archivo y frase | Salida de las aplicaciones de búsqueda | Inspección de `make demo-mpi` | Cumplido |
| R11 | Medir tiempo | Reloj monotónico y `MPI_Wtime` | Salida `Tiempo` | Cumplido inicialmente |
| R12 | Medir con distintas llaves | Infraestructura por argumentos | Campaña de pruebas | Pendiente |
| R13 | Calcular speedup | No implementado todavía | Tabla comparativa futura | Pendiente |
| R14 | Manejo adecuado de memoria | Liberación centralizada por aplicación | Pruebas y revisión | Cumplido en rutas probadas |
| R15 | Programación defensiva | Validación de rangos, archivos y tamaños | Casos de prueba y códigos de salida | Parcial; ampliar pruebas |
| R16 | README de ejecución | `README.md` | Repetición desde entorno limpio | Cumplido |
| R17 | Bitácora de pruebas | Resultados iniciales en `AVANCE.md` | Evidencia futura | Parcial |
| R18 | Explicación de primitivas MPI | Informe final | Revisión documental | Pendiente |
| R19 | Catálogo de funciones | Encabezados públicos; anexo futuro | Revisión documental | Parcial |
| R20 | Capturas de evidencia | No se almacenan todavía | Anexo del informe | Pendiente |

## Convenciones para mantener la trazabilidad

- Cada requisito nuevo debe recibir un ID estable.
- Toda funcionalidad debe enlazar una implementación y una forma de verificarla.
- Un requisito solo se marca como cumplido cuando existe evidencia reproducible.
- Los resultados de rendimiento deben incluir entrada, llave, procesos, versión del programa y parámetros.
- Los archivos generados en `build/` y `bin/` no se consideran evidencia durable; los comandos y resultados relevantes deben registrarse en `docs/` o en la futura bitácora.

