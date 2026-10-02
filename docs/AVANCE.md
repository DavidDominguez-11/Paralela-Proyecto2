# Avance de implementación

Fecha de validación: 2 de octubre de 2026.

## 1. Alcance alcanzado

Este avance implementa una ruta completa y demostrable del proyecto:

```text
archivo de texto
    -> cifrado DES con llave conocida
    -> archivo binario cifrado
    -> búsqueda secuencial o MPI
    -> llave encontrada
    -> recuperación del texto original
```

Se generan cuatro ejecutables:

- `des_tool`: cifra y descifra archivos.
- `bruteforce_seq`: busca una llave dentro de un rango secuencial.
- `bruteforce_mpi`: divide el rango en bloques contiguos y lo busca con Open MPI.
- `bruteforce_mpi_cyclic`: intercala las llaves entre los procesos MPI.

## 2. Decisiones técnicas

### Núcleo reutilizable

Las funciones de DES, archivos, validación y búsqueda se encuentran en módulos independientes. Esto evita copiar código entre las versiones secuencial y MPI, y permite comparar algoritmos usando exactamente el mismo cifrado.

### Llaves de 64 bits

Las llaves se representan con `uint64_t`, aunque únicamente se aceptan valores menores que `2^56`. Esto evita el desbordamiento del `int` utilizado por el código base.

### Conversión de 56 a 64 bits DES

Los 56 bits útiles se distribuyen en ocho grupos de siete bits. Cada grupo se convierte en un byte DES y OpenSSL calcula el bit de paridad impar correspondiente. De esta forma, cada valor del espacio de 56 bits tiene una representación determinista.

### Datos binarios y padding

La longitud del cifrado se conserva explícitamente; no se utiliza `strlen` sobre el contenido binario. Se aplica padding PKCS#7 para procesar textos cuya longitud no es múltiplo de los ocho bytes del bloque DES.

### Rango semiabierto

Todos los rangos se expresan como `[inicio, fin)`. Esta convención evita excluir o repetir accidentalmente la última llave de un bloque.

### Terminación MPI coordinada

Las versiones naive y cíclica comparten el mismo protocolo de terminación. Cada proceso revisa un lote de candidatos y después participa en `MPI_Allreduce`:

- Una reducción obtiene la menor llave candidata reportada.
- Otra reducción detecta si todavía existen procesos activos.
- Los procesos conservan el mismo orden de operaciones colectivas, evitando recepciones pendientes y bloqueos al finalizar.

El intervalo de sincronización es configurable. Un intervalo pequeño reacciona antes al hallazgo, pero aumenta la comunicación; uno grande reduce comunicación, pero puede ejecutar más intentos innecesarios.

## 3. Evidencia reproducible

Entorno utilizado:

- Ubuntu 24.04 sobre WSL 2.
- GCC.
- OpenSSL 3.0.13.
- Open MPI 4.1.6.

La suite completa se ejecuta con:

```bash
make test-all
```

Resultado esperado:

```text
OK: parser, archivos, DES, padding y busqueda secuencial.
OK: comandos, llaves limite y resultados de error.
OK: particiones, intervalos, llave cero y errores MPI.
```

La demostración completa se ejecuta con:

```bash
make demo-mpi
```

En la validación del avance se utilizó:

- Texto: `Esta es una prueba de proyecto 2`.
- Llave: `42`.
- Frase conocida: `es una prueba de`.
- Rango: `[0, 1000)`.
- Procesos MPI: 4.
- Intervalo de sincronización: 16 candidatos.

Resultados observados:

| Versión | Resultado | Llave | Intentos | Tiempo observado |
|---|---|---:|---:|---:|
| Secuencial | Encontrada | 42 | 43 | 0.000034 s |
| MPI naive | Encontrada | 42 | 187 totales | 0.000092 s |
| MPI cíclico | Encontrada | 42 | 59 totales | 0.000043 s |

Estos tiempos no constituyen todavía un benchmark. La entrada es deliberadamente pequeña para demostrar corrección. En este caso MPI es más lento porque el costo de sincronización es mayor que el trabajo criptográfico realizado.

## 4. Qué demuestra este avance

- El texto puede cargarse desde un archivo y cifrarse con una llave arbitraria.
- El cifrado puede almacenarse y leerse sin confundir datos binarios con cadenas.
- La misma llave recupera exactamente el archivo original.
- La búsqueda secuencial encuentra una llave conocida y registra intentos y tiempo.
- Cuatro procesos MPI pueden compartir la entrada, buscar en rangos diferentes y terminar coordinadamente.
- La distribución cíclica cubre el rango sin duplicar candidatos y reduce la dependencia entre el valor de la llave y un bloque contiguo.
- Los errores de argumentos y archivos producen una terminación controlada.

## 5. Limitaciones conocidas

- DES y ECB son inseguros para aplicaciones reales; se usan únicamente por requerimiento académico.
- La API DES de OpenSSL 3 está obsoleta. El aislamiento en `des_crypto.c` permite reemplazarla posteriormente.
- El enfoque dinámico master-worker todavía no está implementado.
- Las mediciones pequeñas reflejan principalmente overhead y no deben interpretarse como speedup definitivo.
- Todavía no se ha ejecutado el espacio completo de `2^56`, porque no es viable como prueba de avance.

## 6. Próxima fase

1. Implementar master-worker con bloques dinámicos.
2. Agregar una bitácora automatizada en CSV para cada ejecución.
3. Ejecutar repeticiones controladas y calcular mediana, speedup y eficiencia.
4. Incorporar las llaves fáciles, medianas y difíciles del enunciado con rangos de prueba viables y claramente documentados.
