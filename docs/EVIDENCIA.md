# Evidencia visual y alcance

El enunciado pide explícitamente una captura del cifrado y descifrado de una
oración con llave simple (Parte B.1), evidencia de las pruebas con llaves
específicas y de las alternativas (Parte B.2 y B.5), y capturas en el Anexo 2
de la bitácora de pruebas y speedups. El extra de clúster de dos máquinas solo
requiere evidencia si se presenta ese extra; aquí no se presenta.

Las imágenes siguientes son capturas de una consola Windows visible obtenidas
durante ejecuciones reales con `python scripts/capture_evidence.py`. El script
imprime los argumentos que ejecuta y captura la pantalla tras finalizar el
comando. No se dibujaron resultados en las imágenes. Las capturas muestran una
ejecución demostrativa; las medianas provienen exclusivamente de los CSV.

| Evidencia | Qué muestra | Estado |
|---|---|---|
| [Cifrado, descifrado y comparación con llave 42](../evidence/encrypt_decrypt_42.png) | Entrada, salida, llave, `cmp` exitoso y búsqueda secuencial | Obtenida |
| [Suite de pruebas](../evidence/tests.png) | `make test-all` y sus cuatro etapas | Obtenida |
| [ASan y UBSan](../evidence/sanitizers.png) | `make test-sanitize` del núcleo | Obtenida |
| [MPI naive](../evidence/mpi_naive.png) | Cuatro procesos, llave 42, intentos y tiempo | Obtenida |
| [MPI cíclica](../evidence/mpi_cyclic.png) | Cuatro procesos, llave 42, intentos y tiempo | Obtenida |
| [MPI dinámica](../evidence/mpi_dynamic.png) | Cuatro procesos, tres workers, llave 42 | Obtenida |
| [Llave literal 123456](../evidence/key_123456.png) | Naive, `-np 4`, rango completo y solución | Obtenida |
| [Inicio del segundo bloque](../evidence/quarter_first.png) | Llave 18014398509481984 encontrada en rango completo | Obtenida |
| [Prefijo acotado del caso lento](../evidence/quarter_minus_one_prefix.png) | Llave 18014398509481983, rango limitado a un millón, sin hallazgo en ese prefijo | Obtenida, **no prueba búsqueda completa** |
| [Tablas medidas](../evidence/summaries.png) | Lectura de los CSV nuevos, incluidos seis tamaños decimales de llave, mediante `show_results.py` | Obtenida |
| [Comparación histórica](../evidence/historical_summary.png) | Speedups naive, cíclico y dinámico con cuatro procesos para las tres posiciones elegidas del rango reducido | Obtenida |

Los nombres y carnés del informe fueron proporcionados por el equipo el 13 de
octubre de 2026. No hay captura ni medición de
un clúster físico de dos máquinas ni de OpenMP, pues son extras opcionales que
no se ejecutaron.

El informe final está disponible como [Markdown](../papers/INFORME_FINAL.md) y
[PDF](../papers/INFORME_FINAL.pdf). Para regenerar el PDF después de editar el
Markdown, ejecute `./scripts/build_report_pdf.ps1` en Windows con Python,
`markdown-it-py`, Graphviz y Chrome. El script verifica los números del índice
y la copia final. La portada y el cuerpo siguen visualmente el DOCX de
`EJEMPLOS/`; ese archivo no se utilizó como fuente de contenido.
