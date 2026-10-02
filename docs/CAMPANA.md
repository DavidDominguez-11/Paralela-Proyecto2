# Campaña experimental

## Diseño

El espacio DES completo contiene `2^56` candidatos y no es viable para una campaña académica repetida en el hardware disponible. Para conservar el fenómeno descrito en el enunciado se utiliza un rango reducido de tamaño:

```text
M = 1 000 000
```

Las llaves mantienen las mismas posiciones relativas propuestas para el espacio completo:

| Clase | Fórmula reducida | Llave |
|---|---:|---:|
| Fácil | `M / 2 + 1` | 500001 |
| Media | `M / 2 + M / 8` | 625000 |
| Difícil | `ceil(M / 7 + M / 13)` | 219781 |

La clasificación describe el comportamiento esperado del reparto naive con cuatro procesos, no solamente la magnitud numérica de la llave. La llave fácil cae al inicio de un bloque; la media cae dentro de otro bloque; la difícil queda cerca del final del primer bloque.

## Configuración

- Entrada: `data/mensaje.txt`.
- Frase conocida: `es una prueba de`.
- Procesos MPI: 2 y 4.
- Estrategias: secuencial, naive, cíclica y master-worker.
- Repeticiones medidas: 5.
- Warmups: 1 por combinación.
- Intervalo colectivo: 4096 candidatos.
- Bloque master-worker: 4096 candidatos.
- Semilla para ordenar ejecuciones: 2026.

La campaña se reproduce con:

```bash
make benchmark-final
```

## Artefactos

- `results/final-campaign/raw.csv`: bitácora de cada ejecución.
- `results/final-campaign/summary.csv`: medianas, extremos, speedup y eficiencia.

Los archivos cifrados auxiliares tienen extensión `.des` y no se versionan.

## Interpretación

El speedup utiliza como referencia la mediana secuencial de la misma llave:

```text
speedup = mediana_secuencial / mediana_paralela
```

La eficiencia principal divide el speedup entre todos los procesos MPI. También se informa eficiencia por worker, porque el proceso 0 de master-worker se dedica exclusivamente a coordinar.

Los resultados deben interpretarse junto con el número de intentos. Un speedup alto puede provenir de que una distribución encontró la llave tras explorar menos candidatos, no necesariamente de una mayor tasa criptográfica.

## Resultados

La campaña produjo 105 ejecuciones medidas: 3 llaves, 7 configuraciones por llave y 5 repeticiones por configuración. Cada grupo del resumen contiene exactamente cinco muestras.

### Speedup por clase de llave

| Clase | Llave | Naive 2 | Naive 4 | Cíclico 2 | Cíclico 4 | Dinámico 2 | Dinámico 4 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Fácil | 500001 | 120.907 | 117.104 | 1.853 | 2.922 | 0.982 | 2.388 |
| Media | 625000 | 4.730 | 3.214 | 1.843 | 2.686 | 0.958 | 2.487 |
| Difícil | 219781 | 0.951 | 0.734 | 1.873 | 2.830 | 0.975 | 2.425 |

### Medianas con cuatro procesos

| Clase | Estrategia | Intentos | Tiempo (s) | Speedup | Eficiencia total |
|---|---|---:|---:|---:|---:|
| Fácil | Naive | 12290 | 0.002240 | 117.104 | 29.276 |
| Fácil | Cíclica | 505929 | 0.089765 | 2.922 | 0.731 |
| Fácil | Dinámica | 500002 | 0.109849 | 2.388 | 0.597 |
| Media | Naive | 505929 | 0.101022 | 3.214 | 0.804 |
| Media | Cíclica | 635483 | 0.120869 | 2.686 | 0.672 |
| Media | Dinámica | 625001 | 0.130531 | 2.487 | 0.622 |
| Difícil | Naive | 883334 | 0.158445 | 0.734 | 0.183 |
| Difícil | Cíclica | 226978 | 0.041087 | 2.830 | 0.707 |
| Difícil | Dinámica | 227974 | 0.047933 | 2.425 | 0.606 |

## Análisis

El naive confirma la advertencia central del enunciado. Para la llave fácil, un proceso empieza prácticamente sobre la solución: con dos procesos se realizaron solamente 4098 intentos y con cuatro, 12290. Esto produce speedups aparentes superiores a 117 y eficiencias mayores que uno. No representan una aceleración de DES; aparecen porque el programa paralelo ejecuta una fracción diminuta de los 500002 intentos secuenciales.

Para la llave difícil ocurre lo contrario. Con cuatro procesos, el naive realizó 883334 intentos antes de que el primer bloque alcanzara la solución. El tiempo fue mayor que el secuencial y el speedup cayó a 0.734. Por tanto, el naive varió entre 0.734 y 120.907 según la llave y no ofrece rendimiento predecible.

La distribución cíclica fue la alternativa más rápida de esta campaña. Sus speedups se mantuvieron entre 1.843 y 1.873 con dos procesos, y entre 2.686 y 2.922 con cuatro. La variación entre llaves es pequeña comparada con el naive y no utiliza un coordinador dedicado.

Master-worker también fue consistente. Con dos procesos dispone de un solo worker, por lo que su speedup quedó entre 0.958 y 0.982. Con cuatro procesos dispone de tres workers y alcanzó speedups entre 2.388 y 2.487. Su eficiencia por worker estuvo entre 0.796 y 0.829 para cuatro procesos, mientras su eficiencia total fue menor porque incluye al master.

## Conclusiones de la campaña

1. El naive puede parecer extraordinario o incluso ser más lento que el secuencial dependiendo únicamente de la posición de la llave.
2. Los intentos deben reportarse junto con el tiempo para distinguir paralelismo real de una reducción accidental del trabajo.
3. La distribución cíclica ofrece el mejor rendimiento y la implementación más simple para este costo uniforme por candidato.
4. Master-worker ofrece estabilidad y control del trabajo desperdiciado, pero sacrifica un proceso para coordinación y agrega mensajes por bloque.
5. Para esta carga uniforme se recomienda la estrategia cíclica. Master-worker resulta más atractivo si el costo de los candidatos es irregular o si se ejecuta con más procesos y bloques ajustados experimentalmente.
