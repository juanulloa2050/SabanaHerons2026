# CORRECCIONES

Estado: todos los puntos fueron revisados. Cada uno tiene su **Respuesta** y dónde quedó reflejado.
Los tickets nuevos (MZ-038 a MZ-044) están como *propuestos/backlog*: **no suman al presupuesto de 50 h / 45 h** hasta que decidan qué sacar.

## Roadmap

* Corregidos: restricciones de tiempo (sección 2), heading por giroscopio (3.5 y 4.5), máquina de estados y botones (3.4 y 4.7), ruta 2×4 (sección 6), sonar (8.1), preguntas Q2–Q11 (sección 11), checklist (sección 10) y riesgos (sección 12).


## Tickets

* Nuevos: MZ-038 a MZ-042 (propuestos, ~8.5 h de autoría) y MZ-043, MZ-044 (backlog). Actualizados: MZ-006 (preguntas) y MZ-007 (mapa de botones, logger con `InertialData`).


# Notas


# Dudas implementacion

* Se usara el giroscopio del robot para saber el norte? me gustaria probar que el robot siempre sepa en que direccion esta mirando y en base a eso por ejemplo solo poner los AprilTag en la cara norte de las paredes

  **Respuesta:** Sí al giroscopio, con un matiz: el NAO **no tiene magnetómetro**, así que no existe "norte" real. Lo que sí hay es el yaw integrado del giroscopio (`InertialData.orientation3D`, incluye la rotación en z). Definimos un **norte del laberinto**: el yaw al momento de "ubicado en posición inicial" es el offset, y `heading = snap90(yaw − offset)`. El yaw deriva despacio, así que cada tag visto de frente re-alinea el offset. Los AprilTag en la cara norte son un buen **experimento**: con heading conocido, la cara del tag se deduce sin geometría de imagen. Pero ojo: ese tag solo se ve cuando el robot mira al norte y esa pared existe; en otras direcciones depende de odometría + giroscopio. Y el layout real lo decide la organización (Q1). → Roadmap 3.5 y 4.5, **MZ-038** (incluye medir la deriva con los logs del Lab 1: hay que agregar `InertialData` al logger antes del 13 oct, ya quedó en MZ-007).

* PLantear dentro del desarrollo un sistema de logs para verificar el performance y toma de decisiones del robot

  **Respuesta:** Ya existe el logger de B-Human (graba representaciones fuera del estado *Initial*), pero graba **qué pasó**, no **por qué decidió**. Se agrega una representación de eventos de decisión (estado, celda, acción, motivo: replan/watchdog/tagFix/modo seguro, aristas que cambiaron, corrección por cada tag, tiempo real vs. estimado por celda) y un script que resume cada intento. El "tiempo real vs. estimado" además alimenta la calibración de costos del planificador. → **MZ-039**.

* Sonar	Not implemented. LoLA "Sonar" is ignored: Revisa la posibilidad y utilidad de activar el sonar para evitar colisiones con las paredes

  **Respuesta:** Para **evitar colisiones, no sirve**: el sonar mira al frente, tiene un rango mínimo de ~25 cm (según la documentación del NAO), y el margen lateral es de ~10 cm, o sea, el choque ocurre por debajo de lo que el sensor puede medir; además tiene baja tasa de actualización y el torso se mueve al caminar. Como **chequeo de pared frontal** es marginal pero posible (a una celda de distancia, 75 cm, sí mide). Decisión: **no usarlo para controlar**; hacer un spike de 1.5 h que solo **loguea** los valores en el Lab 2/3 y decide con datos. Esos números del sonar son de la documentación, no medidos por nosotros: el spike los confirma. → Roadmap 8.1, **MZ-044**.

* Debido a que fisicamente no podremos testear un 8*8, seria bueno poder probar un 2*4 donde por camine recto, derecha, camine uno, derecha, camine recto 0 -> 3 -> 11 -> 8. para probar giro y caminata

  **Respuesta:** Buena idea, y la numeración cuadra: son los ids reales (`row·8 + col`), o sea un sub-bloque del 8×8: celdas 0–3 de la fila 0 y 8–11 de la fila 1. Ruta: 3 celdas al este, giro a la derecha, 1 celda al sur, giro a la derecha, 3 celdas al oeste. Limitación física: los 6 paneles del banco no alcanzan para un 2×4 con paredes (hacen falta 15), así que en el laboratorio va con **cinta en el piso** (prueba giro, caminata y deriva) y la versión con paredes corre en simulación. → Roadmap sección 6, **MZ-040** (usa tiempo ya reservado del Lab 2/3).

* Posibilidad de tambien movimientos diagonales en segundo recorrido

  **Respuesta:** Con una advertencia geométrica: en un pasillo una diagonal pura **no es posible**. Una diagonal del centro de la celda (r,c) al de (r+1,c+1) pasa justo por el poste de la esquina interior de una curva en L, y el margen lateral es de ~10 cm. Solo sirve en **bloques abiertos de 2×2** (sin paredes internas), donde ahorra ~0.4 celda por paso. Lo que sí aplica a las curvas es el giro rodante (ver siguiente punto). El ticket empieza contando cuántos bloques 2×2 abiertos y curvas en L hay en los laberintos generados; si el ahorro medio es menor a ~3 s por corrida, se descarta. → **MZ-043** (backlog).

* Plantear como hacer los giros en esquinas sin estar completamente quieto, depronto en primer viaje hacerlo estando quieto y en segundo usar giro optimizado 

  **Respuesta:** Es exactamente el plan: intento 1 gira en el sitio en el centro de la celda (preciso y seguro); intento 2 usa **giro rodante** (arco mientras camina) solo en esquinas ya conocidas como abiertas. El planificador ya modela `moving ∈ {0,1}`, así que el cambio es una acción nueva con su costo y un chequeo de margen. Queda detrás de un parámetro (`rollingTurns.enabled`, por defecto `false`) y solo se activa si en el banco hay 0 contactos y 0 caídas en 10 repeticiones. → **MZ-043**. Se relaciona con MZ-036 (RL para giros).

* seria bueno implementar un boton de "ubicado en posicion inicial" ahi empieza a analizar lo que ya puede ver y el de inicio que se toca para iniciar el movimiento indicado por el referee

  **Respuesta:** Implementado en el diseño: `headMiddle` ×1 = **ubicado en posición inicial** (empieza a recolectar), `headFront` ×1 = **inicio** (el referee). Para no chocar, la secuencia de prueba de movimiento que estaba en `headMiddle` pasó a doble toque; los triples siguen reservados para diagnósticos de B-Human. → Roadmap 3.4 y 4.7, **MZ-041**, MZ-007.

* Si esta ubicado en el centro del cuadro 0 para iniciar el laberinto mirando hacia el frente (sin pared al frente), lo mas posibles es que no vea el tag del punto que esta ubicado
    * implementar una funcion para que en el estado "ubicado en posicion inicial" solo mueva la cabeza de derecha a izquierda para analizar lo que ya ve por un momento por ejemplo por 5 segundos, luego de esto solo mira al frente y espera el estado de "inicio". Esto debe ser activable o desactivable en el deploy porque depende de la respuesta de si el tiempo empieza a correr desde que sale del primer bloque o no. tambien se debe poder activar u desactivar en la simulacion

  **Respuesta:** Correcto, y es peor de lo que parece: las paredes de la celda de salida quedan a los lados y atrás, y el tag está a ~30 cm de altura y ~25 cm de distancia, así que la cabeza tiene que **inclinarse hacia abajo** además de girar. Se implementa como estado `StartScan` con parámetros `startScan.enabled`, `durationMs` (5000 por defecto), `yawRangeDeg` y `pitchDeg` en `handleMaze.cfg`. En **deploy** se cambia el valor y se vuelve a desplegar; en **simulación** se carga el mismo `.cfg` (el ticket verifica si `set parameters:` funciona en caliente y lo deja anotado). Si llega `headFront` durante el escaneo, arranca de inmediato. La decisión de activarlo depende de la pregunta **Q11**. Bonus: en ese escaneo el robot identifica la esquina de salida (0/7/56/63) y fija el norte del laberinto. → **MZ-041**.

* Disenar algun boton para reiniciar la memoria del maze, por ejemplo usar el funcionamiento actual de bhuman que al darle los 3 botones el robot se sienta, en ese momento reiniciar la memoria de lo aprendidio en el laberinto, mientras que el robot no este sentado no recoletar informacion, si esta en ready desde ese momento empieza a recolectar informacion y a guardarla hasta como dije antes que se le da a los 3 botones y se sienta, ahi reinicia la info.

  **Respuesta:** Verificado en el código: mantener los 3 botones de la cabeza 1 s pone `playerState = unstiff` (`GameStateProvider.cpp`), y un toque en el pecho lo reactiva. Se usa ese estado: al entrar en `unstiff` se borra `MazeMap`, la pose y el contador de intentos, y mientras está sentado no se recolecta nada; la recolección empieza tras "ubicado en posición inicial". **Riesgo a cuidar:** el mismo gesto entre el intento 1 y el 2 **destruiría el mapa**. Por eso el robot dice "map cleared", y quedó en el checklist: no mantener los 3 botones entre intentos. La pickup (Standby) **no** borra el mapa. → Roadmap 3.4, 4.7, 10 y 12, **MZ-042**.


## Dudas  ya planteadas en el roadmap y deben ser modificadas 

**Q2** — *§2.4 dice 1 minuto para presentarse; §2.7 dice 5 minutos.* Tu aclaración: el 1 minuto es el tiempo que tiene el equipo para llegar al laberinto después de que lo llaman; los 5 minutos son para completar el laberinto.

  **Respuesta:** Corregido. Q2 queda **resuelta por el reglamento** (se conserva el número para no romper referencias) y la tabla de restricciones separa ahora "Time to present: 1 min" de "Attempt timeout: 5 min". Solo se vuelve a preguntar si la organización dice otra cosa.

**Q3** — *Tiempo máximo por intento (3–5 min).* Reformular: ¿los 2 minutos de regreso del primer intento entran en los 5 minutos? Ej.: si se demora 4 min en llegar, ¿tiene los 2 min completos para regresar o solo 1?

  **Respuesta:** Reformulada tal cual: *"¿Los ≤2 min de regreso al inicio cuentan dentro de los 5 min del intento? Si el intento 1 tarda 4 min en llegar a la meta, ¿el robot tiene los 2 min completos para regresar o solo el minuto restante?"*. Impacto: presupuesto de exploración y el estado `ReturnToStart`.

**Q4** — *¿Habrá reconocimiento visual del laberinto antes de la primera corrida?* Tu aclaración: no es necesaria, porque aunque yo lo vea el robot no va a ver nada distinto.

  **Respuesta:** De acuerdo; además el mapa no se puede ingresar a mano. Q4 queda **retirada** (marcada así en la tabla, número conservado).

**Q5** — *Goal detection.* Sin comentarios tuyos; se mantiene tal cual (¿la meta es una sola celda? ¿qué tamaño tiene el área de color?).

**Q6** — *Initial heading.* Sin comentarios tuyos; se mantiene tal cual (¿el robot se coloca mirando el lado abierto de la celda de salida?).

**Q7** — *"¿Puede el robot escribir su propio mapa en su almacenamiento y recargarlo tras un crash?" — No entiendo bien esta pregunta.*

  **Respuesta:** Hoy el mapa vive **solo en la memoria RAM** del programa. Si el programa se cae (o alguien mantiene el botón del pecho 3 s, que lo apaga), el mapa del intento 1 se pierde y el intento 2 tendría que explorar de nuevo. La pregunta es: ¿podemos dejar que el **robot mismo** guarde en su disco el mapa que él aprendió, y lo lea él mismo si se reinicia? No es "ingresar un mapa a mano" (eso está prohibido y descalifica); es una copia de seguridad de lo que él descubrió. Reescrita en lenguaje simple en el roadmap. Prioridad baja: si la respuesta es "no", no cambia nada del plan.


# Dudas competencia

Todas pasaron a la lista de preguntas para la organización (roadmap sección 11, y MZ-006 ya las incluye: ahora son 11 filas en total).

* Estara el laberinto alineado con el norte, sur etc. Esto para poder ubicar el robor con el giroscopio

  **Respuesta:** El norte geográfico da igual (sin magnetómetro no lo podemos medir); lo que importa es que el laberinto sea una cuadrícula regular, y eso lo da el reglamento. Lo que sí hay que confirmar es si el **id 0 siempre está en la misma esquina y los ejes están alineados con la arena**, o si pueden rotar/espejar el laberinto entre equipos. → **Q8**.

* el goal reach lo debe detectar el robot o se debe cambiar de estado de manera manual para que regrese al inicio?

  **Respuesta:** El diseño actual asume que el **robot lo detecta solo** (color del piso + posición en el mapa). Que un humano toque un botón en la meta solo se puede si el reglamento lo permite (la activación es solo por sensor del robot, ≤10 s tras el silbato). Se pregunta, y si se permite se deja un toque manual como respaldo. → **Q9**.

* Que pasa si el robot no regresa al inicio

  **Respuesta:** El diseño ya lo cubre: alguien lo levanta, entra en `Standby`, conserva el mapa y reinicia la pose en la celda de salida. Lo que no sabemos es si hay **penalización** y quién puede levantarlo. → **Q10**.

* El tiempo empieza a contar desde que sale del primer cuadro o desde que se empieza a mover?

  **Respuesta:** No lo sabemos; se pregunta. Esta respuesta decide si el escaneo inicial (`startScan.enabled`) se activa: solo si el reloj **no** corre durante ese escaneo. → **Q11**.


# Segunda ronda (tickets)

* Borrar los tickets 002 y 005

  **Respuesta:** Hecho. MZ-002 (protección de rama) y MZ-005 (confirmación de registro) se eliminaron del resumen, del cuerpo, del Gantt y del roadmap (la regla 7 del flujo Git y el DoD del Sprint 0 ya no los mencionan). Los números **no se reutilizan**. Consecuencia: sin protección de rama, las reglas 1–4 (nadie hace push directo, PR obligatorio, revisión cruzada) quedan por convención, no por el servidor.

* Quitar el tiempo de review de las tareas

  **Respuesta:** Hecho en todos los tickets, la tabla y el Gantt. La revisión cruzada sigue existiendo como proceso (regla 4), solo que ya no tiene horas. Nuevos totales: **Bryam 46.25 h, Wilson 39.75 h** (antes 50 / 45). Esto libera ~3.75 h y ~5.25 h: los tickets propuestos casi caben (Bryam +5.0 h lo pasaría a 51.25 h; Wilson +2.0 h por MZ-039).

* 014 como opcional

  **Respuesta:** Hecho: MZ-014 queda marcado **opcional** en título, tabla, Gantt y roadmap. Sin banco, MZ-015, MZ-017 y MZ-027 se hacen con cinta en el piso, y el margen lateral se verifica en simulación. Wilson baja a 36.25 h si no se hace.

* Todas las tareas físicas en una sección aparte

  **Respuesta:** Hecho: nueva sección **"Physical robot tickets (testing method not settled)"** en `TICKETS_LABERINTO.md` con MZ-014, 015, 017, 023, 027, 031, 033 y los propuestos/backlog que son de laboratorio (MZ-040, MZ-044). Cada sprint deja una nota que apunta allí. Las dependencias de tickets de software hacia tickets físicos pasaron a **suaves** (marcadas con `*`): usan datos del laboratorio si existen y simulación si no. Ojo: MZ-022 (reporte de precisión de tags) solo se cierra con logs reales. MZ-036, 037 y 043 siguen en backlog; solo su validación final necesita el robot. Falta decidir **cómo vamos a testear sin laboratorio**; dejé como plan B usar los 5 min de la zona de calibración de la competencia como primera prueba real.

* Que el robot diga una frase al tocar los botones indicando a qué estado cambia

  **Respuesta:** Hecho: tabla **"State announcements"** en el roadmap 4.7 (por ejemplo "positioned", "scanning", "ready", "exploring", "speed run", "goal", "returning", "standby", "map cleared", "safe mode"). Usa el skill `Say` que ya existe. Dos detalles del código: `Say` ignora el mismo texto dos veces seguidas, por eso las frases de estados consecutivos son distintas y hay un criterio para verificar que "exploring" se repite en una segunda corrida; y se puede apagar con `announce.enabled`. Pendiente de verificar en MZ-042: que el robot pueda hablar estando sentado (unstiff). Implementación en MZ-024, MZ-041 y MZ-042.

* (último punto vacío)

  **Respuesta:** La lista terminaba con un `*` sin texto. Si había algo más que querías agregar, mandalo y lo incluyo.
