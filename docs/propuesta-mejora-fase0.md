# Propuesta de mejora para la implementación del Proyecto 2

## 1. Propósito

Este documento propone una mejora técnica para el planteamiento descrito en `fase0.md`. El objetivo es conservar el enfoque requerido por el proyecto, pero corregir los riesgos de la implementación inicial y utilizar estrategias de distribución que permitan comparar rendimiento de forma válida.

La propuesta se basa en cuatro versiones del programa:

1. Versión secuencial.
2. Versión paralela naive con bloques estáticos.
3. Versión paralela con distribución cíclica.
4. Versión paralela con distribución dinámica master-worker.

Estas versiones permiten comparar directamente el costo de paralelizar, el efecto de la posición de la llave y la capacidad de cada estrategia para balancear el trabajo.

## 2. Problemas del planteamiento original

### 2.1 Tipo de dato insuficiente

El espacio de búsqueda de DES contiene hasta `2^56` valores. Por tanto, no se puede utilizar `int` para recorrer las llaves. En la mayoría de sistemas, `int` solo permite representar valores hasta aproximadamente `2^31 - 1`.

La implementación debe utilizar un tipo de 64 bits, por ejemplo:

```c
uint64_t key;
```

También deben utilizarse constantes y operaciones compatibles con 64 bits para evitar desbordamientos silenciosos.

### 2.2 Comunicación MPI incompleta

El programa base publica una sola recepción no bloqueante por proceso y luego el proceso que encuentra la llave envía mensajes a todos los procesos. Esto puede dejar mensajes pendientes, generar bloqueos o finalizar con solicitudes MPI sin completar.

La comunicación debe tener un protocolo explícito para:

- Notificar que una llave fue encontrada.
- Elegir un único resultado si más de un proceso reporta una coincidencia.
- Detener todos los procesos.
- Completar o cancelar correctamente las operaciones no bloqueantes.

### 2.3 Uso incorrecto de la longitud del texto cifrado

El texto cifrado es información binaria. Por ello, no debe calcularse su longitud con `strlen`, porque un byte cero puede aparecer dentro del contenido cifrado.

La longitud debe transportarse como un valor separado y todas las funciones deben respetarla. Además, DES trabaja con bloques de 8 bytes, por lo que el texto debe rellenarse antes de cifrarlo y conservarse la longitud real del mensaje.

### 2.4 LCG propuesto en `fase0.md`

Un LCG de período completo con una semilla diferente por proceso no particiona automáticamente el espacio de llaves. En ese caso, todos los procesos podrían recorrer las mismas llaves en distinto orden.

Esto puede producir duplicación de trabajo y mediciones de rendimiento engañosas. Para evitarlo, se reemplaza esta alternativa por un esquema master-worker, cuya asignación de trabajo puede verificarse directamente.

## 3. Arquitectura común

Las cuatro versiones deben reutilizar una misma capa de utilidades. La búsqueda paralela no debería contener implementaciones diferentes de DES, lectura de archivos o validación del texto.

### 3.1 Componentes sugeridos

```text
common.h / common.c
    Lectura y validación de archivos
    Padding y longitud del mensaje
    encrypt_text()
    decrypt_text()
    try_key()
    Medición y manejo de errores

sequential.c
    Búsqueda secuencial

mpi_naive.c
    Bloques estáticos contiguos

mpi_cyclic.c
    Distribución cíclica

mpi_dynamic.c
    Distribución dinámica master-worker
```

### 3.2 Representación del resultado

No se debe utilizar `found == 0` como indicador de que todavía no existe resultado, porque la llave cero es válida. Se recomienda utilizar una estructura o una bandera separada:

```c
typedef struct {
    int found;
    uint64_t key;
} SearchResult;
```

### 3.3 Criterio de validación

Cada llave candidata debe:

1. Copiar el texto cifrado a un búfer independiente.
2. Descifrar el búfer con la llave candidata.
3. Agregar terminación de cadena únicamente cuando el contenido sea texto válido.
4. Buscar la frase clave mediante `strstr` u otra función equivalente.

La frase clave debe ser suficientemente larga para reducir falsos positivos.

## 4. Versión secuencial

La versión secuencial es la referencia de rendimiento para calcular speedup. Debe recorrer el mismo espacio de búsqueda y utilizar exactamente las mismas funciones criptográficas que las versiones MPI.

Pseudocódigo:

```text
leer texto y frase clave
cifrar el texto con la llave conocida
iniciar temporizador

para key desde 0 hasta upper_bound - 1:
    si try_key(key) es verdadero:
        guardar key
        detener búsqueda

detener temporizador
imprimir llave, iteraciones y tiempo
```

La cantidad de iteraciones también debe registrarse. Esto permite distinguir entre el tiempo causado por el algoritmo y el tiempo causado por la implementación o la comunicación.

## 5. Versión MPI naive mejorada

La versión naive conserva la división del espacio en bloques contiguos. Su objetivo no es ser la mejor implementación, sino proporcionar una referencia paralela sencilla y demostrar la dependencia del rendimiento respecto a la posición de la llave.

Para `N` procesos, el rango semiabierto del proceso `rank` es:

```text
start = rank * total_keys / N
end   = (rank + 1) * total_keys / N
```

Cada proceso prueba las llaves `[start, end)`.

### Comunicación recomendada

Todos los procesos deben ejecutar la misma secuencia general:

1. El proceso raíz lee el archivo y distribuye texto, longitud y frase clave mediante `MPI_Bcast`.
2. Cada proceso busca dentro de su rango.
3. Si encuentra una llave, guarda su resultado local.
4. Todos los procesos ejecutan una operación colectiva para determinar si existe una solución.
5. La llave ganadora se distribuye mediante `MPI_Bcast`.
6. Todos finalizan de forma coordinada.

Una opción sencilla es utilizar una bandera local y después `MPI_Allreduce` con operación lógica OR. Si el costo de revisar la bandera cada iteración es alto, puede revisarse cada cierto número de candidatos, aceptando una pequeña cantidad de trabajo adicional después de encontrar la llave.

Este diseño es más seguro que enviar mensajes individualmente a todos los procesos, porque evita múltiples `MPI_Send` pendientes y hace explícita la sincronización final.

## 6. Alternativa 1: distribución cíclica

En lugar de asignar bloques contiguos, el proceso `rank` prueba:

```text
rank, rank + N, rank + 2N, rank + 3N, ...
```

El ciclo puede expresarse como:

```c
for (uint64_t key = rank; key < total_keys; key += size) {
    ...
}
```

### Por qué mejora el naive

El naive asigna a cada proceso una región completa. Si la llave está al inicio del bloque del proceso 1, ese proceso termina inmediatamente; si está al final del bloque del proceso 0, el programa puede tardar casi lo mismo que la versión secuencial.

La distribución cíclica hace que cada proceso explore llaves de todo el espacio desde el inicio. Esto reduce la relación entre la magnitud numérica de la llave y el proceso que la encuentra.

### Ventajas

- No repite llaves.
- Es sencilla de implementar.
- No necesita un coordinador central.
- Mantiene un bajo costo de comunicación.
- Produce tiempos más representativos para comparar diferentes posiciones de la llave.

### Limitación

La cantidad de candidatos probados antes de encontrar la llave sigue dependiendo del orden pseudo-secuencial de la búsqueda. Por ello, mejora la distribución entre procesos, pero no elimina completamente la variación de los tiempos.

## 7. Alternativa 2: distribución dinámica master-worker

En esta estrategia, el proceso 0 actúa como coordinador. Los demás procesos solicitan bloques de llaves y los procesan.

### Funcionamiento

```text
Proceso master:
    mantener next_key
    mientras queden bloques y no exista solución:
        recibir solicitud de un worker
        enviarle un bloque [start, end)
    cuando se encuentre la solución:
        enviar señal STOP a todos los workers

Proceso worker:
    solicitar un bloque
    recibir [start, end) o STOP
    probar las llaves del bloque
    si encuentra la llave:
        informar resultado al master
    si no:
        solicitar otro bloque
```

### Tamaño de bloque

El tamaño del bloque debe ser configurable. Un bloque muy grande reduce la comunicación, pero deja más trabajo inútil después de encontrar la llave. Un bloque muy pequeño mejora la capacidad de reacción, pero aumenta el overhead de MPI.

Se recomienda probar varios tamaños y documentar el resultado. Por ejemplo:

```text
1 000 candidatos
10 000 candidatos
100 000 candidatos
```

### Por qué mejora el naive

El master-worker asigna nuevo trabajo conforme los procesos quedan disponibles. Esto evita que un proceso termine rápidamente y permanezca ocioso mientras otros continúan con rangos grandes.

Es particularmente útil cuando:

- El costo de probar candidatos no es uniforme.
- La terminación depende de una llave ubicada en una posición impredecible.
- Se desea controlar el tamaño del trabajo descartado después de encontrar la solución.

### Riesgo principal

El proceso master puede convertirse en un cuello de botella si los bloques son demasiado pequeños. Por eso el tamaño de bloque debe medirse y no elegirse arbitrariamente.

## 8. Comparación de las estrategias

| Estrategia | Distribución | Comunicación | Balance | Riesgo principal |
|---|---|---:|---:|---|
| Secuencial | Un solo proceso | Ninguna | No aplica | Tiempo elevado |
| Naive | Bloques contiguos | Baja | Sensible a la llave | Speedup inconsistente |
| Cíclica | Llaves intercaladas | Baja | Mejor que naive | Variación aún existente |
| Master-worker | Bloques dinámicos | Media/alta | Adaptativo | Overhead del coordinador |

La comparación debe hacerse con las mismas entradas, la misma librería criptográfica y el mismo criterio de validación.

## 9. Mediciones recomendadas

Para cada estrategia se deben registrar:

- Número de procesos.
- Llave utilizada para cifrar.
- Tamaño del texto.
- Frase clave.
- Tiempo total.
- Número de iteraciones o candidatos probados.
- Tamaño de bloque, cuando aplique.
- Llave encontrada.
- Speedup.
- Eficiencia.

Las métricas principales son:

```text
speedup = T_secuencial / T_paralelo
```

```text
eficiencia = speedup / número_de_procesos
```

El speedup debe calcularse utilizando ejecuciones comparables. No es válido comparar una versión que procesa menos candidatos con otra que procesa todo el rango sin documentar esa diferencia.

## 10. Resultado esperado de la mejora

La propuesta mejorada debe producir los siguientes resultados:

- La versión secuencial sirve como referencia confiable.
- El naive demuestra claramente la variabilidad causada por la posición de la llave.
- La distribución cíclica reduce la dependencia entre el valor de la llave y el proceso favorecido.
- El master-worker permite balancear el trabajo dinámicamente y controlar el trabajo desperdiciado.
- Todas las versiones utilizan una implementación común de DES y validación.
- La comunicación MPI termina de forma coordinada y sin solicitudes pendientes.
- Las mediciones reflejan diferencias reales entre estrategias y no errores de sincronización.

## 11. Conclusión

La implementación propuesta en `fase0.md` es una buena base conceptual, pero requiere corregir el manejo de datos binarios, los tipos numéricos y la sincronización MPI. La distribución cíclica es una alternativa simple y correcta al naive. Para la segunda alternativa se recomienda utilizar master-worker dinámico en lugar de un LCG con semillas independientes.

Esta modificación es mejor porque mantiene el objetivo académico del proyecto, evita duplicar trabajo, permite verificar la cobertura del espacio de búsqueda y proporciona una comparación de rendimiento más justa entre métodos.
