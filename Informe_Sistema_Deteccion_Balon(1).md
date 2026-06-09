# Sistema de Detección de Balón — SabanaHerons2026

## 1. Visión General del Pipeline

El sistema de detección de balón es un pipeline multi-etapa que transforma una imagen cruda de la cámara del NAO en un modelo estimado de posición y velocidad del balón en el campo. El flujo general es:

```
Imagen Cámara → Preprocesamiento → Candidatos (BallSpots) → Clasificación NN (BallPercept)
→ Filtrado (FilteredBallPercepts) → Estimación de Estado (BallModel) → Modelo de Equipo
```

Cada etapa está implementada como un **módulo** independiente dentro del framework, siguiendo el patrón `REQUIRES/PROVIDES` que define dependencias entre representaciones.



## 2. Fase 1: Preprocesamiento de Imagen

Antes de buscar el balón, la imagen de la cámara pasa por varios módulos de preprocesamiento que generan representaciones intermedias:

### ECImage (Enhanced Camera Image)
- **Archivo**: `Src/Representations/Perception/ImagePreprocessing/ECImage.h`
- **Descripción**: Contiene tres versiones procesadas de la imagen de la cámara:
  - `grayscaled`: Imagen en escala de grises (luminancia).
  - `saturated`: Imagen de saturación del color.
  - `hued`: Imagen de tono (hue).
- **Uso en detección de balón**: El `BallSpotsProvider` utiliza `grayscaled` y `saturated` para analizar píxeles candidatos. El `BallPerceptor` extrae parches de `grayscaled` para alimentar la red neuronal.

### CNSImage (Contrast Normalized Sobel Image)
- **Archivo**: `Src/Representations/Perception/ImagePreprocessing/CNSImage.h`
- **Descripción**: Imagen de respuestas Sobel normalizadas por contraste. Detecta bordes independientemente de las condiciones de iluminación.
- **Uso en detección de balón**: El `CNSBallSpotsProvider` busca contornos de esfera en esta imagen.

### ColorScanLineRegions
- **Archivo**: `Src/Representations/Perception/ImagePreprocessing/ColorScanLineRegions.h`
- **Descripción**: Segmenta líneas de escaneo verticales de la imagen en regiones con clasificación de color (`field`, `white`, `black`, `none`, `unset`).
- **Uso en detección de balón**: El `BallSpotsProvider` recorre estas regiones buscando transiciones de color que indiquen la presencia del balón.

### Otras representaciones de preprocesamiento

| Representación          | Uso en detección de balón                                                              |
| ----------------------- | -------------------------------------------------------------------------------------- |
| `CameraMatrix`          | Transforma coordenadas de imagen a coordenadas de campo y viceversa.                   |
| `CameraInfo`            | Proporciona parámetros intrínsecos de la cámara (resolución, distancia focal).         |
| `BodyContour`           | Enmascara la silueta del propio robot para evitar falsos positivos.                    |
| `FieldBoundary`         | Define el límite del campo en la imagen; descarta candidatos fuera del campo.          |
| `ImageCoordinateSystem` | Compensa la distorsión de la lente y el movimiento del robot.                          |
| `RelativeFieldColors`   | Proporciona funciones para clasificar píxeles como campo/no-campo de forma adaptativa. |



## 3. Fase 2: Generación de Candidatos (Ball Spots)

La representación central de esta fase es **`BallSpots`** (`Src/Representations/Perception/BallPercepts/BallSpots.h`), que contiene:
- `ballSpots`: Vector de posiciones 2D (`Vector2i`) en la imagen donde podría haber un balón.
- `firstSpotIsPredicted`: Indica si el primer candidato proviene de la predicción del modelo anterior (no de la imagen).

Existen **tres módulos alternativos** que pueden generar `BallSpots`:

### 3.1 BallSpotsProvider

- **Archivos**: `Src/Modules/Perception/BallPerceptors/BallSpotsProvider.h/.cpp`
- **Autor**: Jesse Richter-Klug
- **Enfoque**: Escaneo basado en regiones de color (método clásico, sin red neuronal).

**Algoritmo**:
1. **Predicción**: Si el modelo anterior vio el balón hace menos de 100 ms, proyecta esa posición a la imagen actual y la añade como primer candidato.
2. **Escaneo de líneas**: Recorre las `ColorScanLineRegionsVerticalClipped` buscando regiones no clasificadas como `field` (césped).
3. **Validación de tamaño**: Calcula el tamaño esperado del balón a esa distancia usando `Geometry::Circle` y compara con la longitud de la región encontrada.
4. **Eliminación de duplicados**: Descarta candidatos demasiado cercanos entre sí (`minAllowedDistanceRadiusRelation`).
5. **Scan horizontal**: Ejecuta `correctWithScanLeftAndRight()` para ajustar la posición del candidato midiendo el ancho del objeto.
6. **Verificación de ruido**: Rechaza candidatos con demasiados píxeles "malos" (`noiseThreshold`).
7. **Verificación de verde**: Para candidatos pequeños, verifica que haya césped alrededor (`checkGreenAround()`).
8. **Filtro de obstáculos**: Elimina candidatos que caen dentro de un robot detectado (`isSpotClearlyInsideARobot()`).

**Dependencias (REQUIRES)**:
- `BallSpecification`, `BodyContour`, `CameraInfo`, `CameraMatrix`
- `ColorScanLineRegionsVerticalClipped`, `ECImage`, `FieldDimensions`
- `FrameInfo`, `ImageCoordinateSystem`, `ObstaclesImagePercept`
- `RelativeFieldColors`, `WorldModelPrediction`

**Parámetros configurables**:
| Parámetro                   | Descripción                                                 |
| --------------------------- | ----------------------------------------------------------- |
| `minRadiusOfWantedRegion`   | Radio mínimo para omitir la verificación de verde           |
| `greenEdge`                 | Ángulo para el cálculo del punto visible más bajo del balón |
| `ballSpotDistUsage`         | Porcentaje visible mínimo del balón requerido               |
| `scanLengthRadiusFactor`    | Factor para determinar la longitud máxima de escaneo        |
| `maxNumberOfSkippablePixel` | Píxeles máximos saltables consecutivamente                  |
| `noiseThreshold`            | Ratio máximo de píxeles incorrectos                         |
| `greenPercent`              | Porcentaje mínimo de verde requerido en verificación        |


### 3.2 CNSBallSpotsProvider

- **Archivos**: `Src/Modules/Perception/BallPerceptors/CNSBallSpotsProvider.h/.cpp`
- **Autores**: Thomas Röfer, Udo Frese, Jesse Richter-Klug
- **Enfoque**: Detección basada en contornos usando imágenes CNS (Contrast Normalized Sobel).

**Algoritmo**:
1. **Modelo 3D de la esfera**: Genera una malla triangular (`TriangleMesh::sphere`) del balón y precomputa una tabla de contornos para diferentes distancias y alturas de cámara.
2. **Espacio de búsqueda**: Define un `CylinderRing` cilíndrico centrado en la cámara, limitado por `maxTableRadius` y el rango de alturas de la cámara.
3. **Predicción**: Opcionalmente incluye la posición predicha del balón como punto de partida.
4. **Búsqueda por bloques**: Para cada región de interés (`BallRegions`), ejecuta `searchBlockAllPoses()` en la imagen CNS, buscando coincidencias de contorno.
5. **Refinamiento**: Opcionalmente refina las detecciones con iteraciones adicionales (`refineIterations`).
6. **Filtrado por respuesta**: Solo acepta detecciones con respuesta ≥ `minResponse`.
7. **Proyección**: Convierte las posiciones 3D detectadas a coordenadas de imagen 2D.

**Dependencias (REQUIRES)**:
- `BallRegions`, `BallSpecification`, `BallSpots` (como entrada existente)
- `CameraInfo`, `CameraMatrix`, `CNSImage`
- `FrameInfo`, `ImageCoordinateSystem`, `WorldModelPrediction`

**Parámetros configurables**:
| Parámetro             | Descripción                                            |
| --------------------- | ------------------------------------------------------ |
| `contourSubDivisions` | Niveles de subdivisión de la malla esférica            |
| `maxTableRadius`      | Distancia máxima cubierta por la tabla de contornos    |
| `minTableHeight`      | Altura mínima de cámara en la tabla                    |
| `maxTableHeight`      | Altura máxima de cámara en la tabla                    |
| `spacing`             | Discretización espacial de la tabla                    |
| `refineIterations`    | Iteraciones de refinamiento después de búsqueda global |
| `refineStepSize`      | Tamaño de paso durante refinamiento (en píxeles)       |
| `minResponse`         | Respuesta mínima requerida del detector de contornos   |


### 3.3 BOPPerceptor (Multi-Perceptor)

- **Archivos**: `Src/Modules/Perception/MultiPerceptors/BOPPerceptor.h/.cpp`
- **Autor**: Arne Hasselbring
- **Enfoque**: Red neuronal de imagen completa que detecta **simultáneamente** balón, obstáculos y marca de penalti.

**Arquitectura**:
- Ejecuta una **única red neuronal** sobre la imagen completa de la cámara.
- La red recibe la imagen en formato **YUYV (UInt8)** y produce un mapa de salida multicanal con 4 canales:
  - Canal 0 (`ballIndex`): Mapa de calor del balón.
  - Canal 1 (`penaltyMarkIndex`): Mapa de calor de la marca de penalti.
  - Canal 2 (`obstaclesIndex`): Mapa de calor de obstáculos.
  - Canal 3: Canal adicional.

**Algoritmo para BallSpots**:
1. Ejecuta la red neuronal sobre `CameraImage` completa (solo una vez por frame).
2. Recorre el mapa de calor del canal de balón.
3. Encuentra el **máximo global** que supere `ballThreshold` (0.1 por defecto).
4. Escala las coordenadas de la resolución de salida a la resolución de imagen original usando `scale`.
5. Genera un único `BallSpot` en esa ubicación.

**Red neuronal utilizada**: `Config/NeuralNets/BOP/net.h5` (y `net.onnx`)

**Dependencias (REQUIRES)**:
- `CameraInfo`, `CameraImage`, `CameraMatrix`
- `FieldDimensions`, `ImageCoordinateSystem`

**PROVIDES**: `BallSpots`, `PenaltyMarkRegions`, `ObstacleScan`, `SegmentedObstacleImage`



## 4. Fase 3: Clasificación y Validación (BallPerceptor)

- **Archivos**: `Src/Modules/Perception/BallPerceptors/BallPerceptor.h/.cpp`
- **Autores**: Bernd Poppinga, Felix Thielke, Gerrit Felsch
- **Enfoque**: Clasificación de candidatos con **tres redes neuronales** especializadas.

### Arquitectura de Redes Neuronales (Encoder-Classifier-Corrector)

El `BallPerceptor` utiliza un pipeline de **tres redes neuronales** secuenciales compiladas con **CompiledNN** (JIT compilation con asmjit):

```
Parche de imagen → [Encoder] → Embedding → [Classifier] → Probabilidad
                                    ↓
                              [Corrector] → (x, y, radio)
```

#### 1. Encoder (`encoder`)
- **Modelo**: `Config/NeuralNets/BallPerceptor/encoder.h5` (y `.onnx`)
- **Entrada**: Parche cuadrado de la imagen en escala de grises, tamaño `patchSize × patchSize × 1`.
- **Salida**: Vector de características (embedding) que codifica la información visual del parche.
- **Función**: Extrae una representación compacta del parche de imagen.

#### 2. Classifier (`classifier`)
- **Modelo**: `Config/NeuralNets/BallPerceptor/classify.h5` (y `.onnx`)
- **Entrada**: El embedding producido por el Encoder.
- **Salida**: Un valor escalar (probabilidad entre 0 y 1) indicando la confianza de que el parche contiene un balón.
- **Función**: Decide si el parche contiene un balón o no.

#### 3. Corrector (`corrector`)
- **Modelo**: `Config/NeuralNets/BallPerceptor/corrector.h5` (y `.onnx`)
- **Entrada**: El embedding producido por el Encoder.
- **Salida**: Vector de 3 valores: `(x, y, radio)` — posición corregida y radio del balón dentro del parche.
- **Función**: Refina la posición exacta y el tamaño del balón. Solo se ejecuta si la probabilidad supera `guessedThreshold`.

### Algoritmo completo del BallPerceptor:

1. **Verificación**: Si las redes no están compiladas correctamente, retorna sin detección.
2. **Para cada BallSpot** (candidato del paso anterior):
   a. **Proyección inversa**: Transforma la posición de imagen a coordenadas de campo, calcula el tamaño esperado del balón.
   b. **Extracción de parche**: Recorta un parche cuadrado (`ballArea × ballArea` píxeles) de la imagen `ECImage.grayscaled`, centrado en el candidato, y lo reescala a `patchSize × patchSize`.
   c. **Normalización**: Aplica normalización al parche según `normalizationMode`:
      - `normalizeContrast`: Normaliza el contraste ignorando los píxeles más extremos.
      - `normalizeBrightness`: Normaliza el brillo.
   d. **Encoder**: Ejecuta la red encoder sobre el parche normalizado.
   e. **Classifier**: Alimenta el embedding al clasificador. Obtiene la probabilidad `pred`.
   f. **Corrector** (si `pred > guessedThreshold`): Alimenta el embedding al corrector para obtener posición y radio refinados.
3. **Selección del mejor candidato**: Elige el candidato con la mayor probabilidad.
4. **Generación de BallPercept**:
   - Si la mejor probabilidad > `acceptThreshold` → estado `seen`.
   - Si la mejor probabilidad > `guessedThreshold` (pero < `acceptThreshold`) → estado `guessed`.
   - Se computa la covarianza de la medición usando `MeasurementCovariance`.

### Umbrales de decisión:
| Umbral             | Descripción                                                     |
| ------------------ | --------------------------------------------------------------- |
| `guessedThreshold` | Mínimo para considerar un candidato como posible balón          |
| `acceptThreshold`  | Mínimo para aceptar completamente un balón como "visto"         |
| `ensureThreshold`  | Si se supera, se acepta inmediatamente y se detiene la búsqueda |


### Caso especial — Portero en Penalty Shootout:
Cuando el estado del juego es `opponentPenaltyShot` y el robot está en posición de portero (`sitDownKeeper`), el módulo aplica una lógica especial: ordena los candidatos por posición vertical y acepta cualquiera dentro de la zona de penalti sin requerir la clasificación completa por red neuronal.

**Dependencias (REQUIRES)**:
- `BallSpots`, `BallSpecification`, `CameraInfo`, `CameraMatrix`
- `ECImage`, `FieldDimensions`, `GameState`
- `MeasurementCovariance`, `MotionInfo`, `RobotPose`

**PROVIDES**: `BallPercept`



## 5. Fase 4: Filtrado de Percepciones (BallPerceptFilter)

- **Archivos**: `Src/Modules/Modeling/BallStateEstimator/BallPerceptFilter.h/.cpp`
- **Autor**: Tim Laue
- **Propósito**: Eliminar falsos positivos antes de pasar las percepciones al estimador de estado.

### Mecanismos de filtrado:

1. **Exclusión por límite del campo**: Percepciones más allá de `fieldBorderExclusionDistance` fuera del campo son descartadas.
2. **Exclusión por proximidad a compañeros**: Si el balón está en el borde de la imagen y cerca de un compañero, se descarta (`robotBanRadius`).
3. **Verificación temporal**: Se requiere que al menos `requiredPerceptionCountNear` o `requiredPerceptionCountFar` percepciones recientes sean consistentes (buffer de verificación).
4. **Coherencia cámara superior/inferior**: Si se vio un balón en la cámara inferior recientemente, ignora balones lejanos detectados en la cámara superior (`farBallIgnoreTimeout`, `farBallIgnoreDistance`).
5. **Aceptación de balones adivinados (`guessed`)**:
   - Solo se aceptan si hay suficientes balones "vistos" (`seen`) cercanos.
   - O si se detecta movimiento consistente del balón (velocidad > `minimumVelocityForMotionDetection`).
   - O si se acaba de realizar un pateo (dentro de `timeSpanForAcceptingKickedGuessedBalls`).
6. **Exclusión de balones en la otra mitad** (para pruebas en media cancha): Configurable con `disableBallInOtherHalfForTesting`.

**Dependencias (REQUIRES)**:
- `BallPercept`, `BallSpecification`, `CameraInfo`, `FieldDimensions`
- `FieldFeatureOverview`, `FrameInfo`, `GameState`, `MotionInfo`
- `Odometer`, `TeamData`, `TeammatesBallModel`, `WorldModelPrediction`
- `RobotPose` (USES)

**PROVIDES**: `FilteredBallPercepts`



## 6. Fase 5: Estimación de Estado (BallStateEstimator)

- **Archivos**: `Src/Modules/Modeling/BallStateEstimator/BallStateEstimator.h/.cpp`
- **Autor**: Tim Laue
- **Propósito**: Estimar posición y velocidad del balón usando **filtros de Kalman múltiples**.

### Arquitectura de filtros:

Se mantienen **dos tipos de hipótesis simultáneamente** (definidos en `BallStateEstimateFilters.h`):

#### StationaryBallKalmanFilter (Balón estático)
- **Estado**: Posición 2D $(x, y)$.
- **Modelo**: El balón no se mueve.
- **Proceso**: Kalman filter estándar con ruido de proceso para manejar incertidumbre.

#### RollingBallKalmanFilter (Balón en movimiento)
- **Estado**: Posición y velocidad 2D $(x, y, v_x, v_y)$.
- **Modelo**: Modelo lineal de desaceleración por fricción: $s = v \cdot t + \frac{1}{2} a \cdot t^2$.
- **Proceso**: Kalman filter extendido que incorpora el modelo de fricción del `BallPhysics`.

### Algoritmo:

1. **Predicción**: Cada hipótesis se propaga en el tiempo usando odometría y el modelo de fricción.
2. **Actualización**: Cuando llega una `FilteredBallPercept`, se actualiza cada hipótesis con la medición.
3. **Ponderación**: Se calcula la verosimilitud (likelihood) de cada hipótesis.
4. **Selección**: Se elige la hipótesis más probable como la estimación principal.
5. **Creación/eliminación**: Se crean nuevas hipótesis para nuevas percepciones y se eliminan las de baja probabilidad (máximo `maxNumberOfHypotheses`).
6. **Detección de desaparición**: Si el balón debería ser visible pero no se detecta, incrementa un contador. Tras `ballDisappearedThreshold` frames, se marca como desaparecido.

### Modelo de salida (`BallModel`):

```
BallModel
├── estimate: BallState
│   ├── position: Vector2f (posición relativa al robot, mm)
│   ├── velocity: Vector2f (velocidad relativa al robot, mm/s)
│   ├── radius: float (radio del balón, mm)
│   └── covariance: Matrix2f (incertidumbre de la posición)
├── lastPerception: Vector2f (última posición percibida)
├── timeWhenLastSeen: unsigned (timestamp de la última vez visto)
├── timeWhenDisappeared: unsigned (timestamp de desaparición)
├── seenPercentage: unsigned char (porcentaje de veces visto recientemente, 0-100)
├── riskyMovingEstimate: BallState (hipótesis alternativa de balón rodando)
└── riskyMovingEstimateIsValid: bool
```

**Dependencias (REQUIRES)**:
- `BallContactChecker`, `BallSpecification`, `BodyContour`, `CameraInfo`
- `CameraMatrix`, `ExtendedGameState`, `FieldDimensions`
- `FilteredBallPercepts`, `FrameInfo`, `GameState`
- `ImageCoordinateSystem`, `MotionInfo`, `Odometer`, `WorldModelPrediction`

**PROVIDES**: `BallModel`



## 7. Fase 6: Detección de Contacto (BallContactCheckerProvider)

- **Archivos**: `Src/Modules/Modeling/BallStateEstimator/BallContactCheckerProvider.h/.cpp`
- **Autor**: Tim Laue
- **Propósito**: Detectar colisiones entre los pies del robot y el balón.

### Algoritmo:

1. Calcula la posición de ambos pies del robot usando el modelo cinemático (`RobotModel`, `TorsoMatrix`, `JointAngles`).
2. Aproxima cada pie como un **círculo** con radio `footRadius` y offset `leftFootOffset`.
3. Compara la posición estimada del balón con la posición de los pies.
4. Si hay colisión, calcula la nueva velocidad del balón basándose en un **modelo elástico** con masas `footMass` y `ballMass`.
5. Añade incertidumbre al vector de velocidad post-contacto (`kickDeviation`).

**PROVIDES**: `BallContactChecker` — una función `collide()` que puede ser invocada por el `BallStateEstimator`.



## 8. Fase 7: Modelo de Equipo (TeammatesBallModelProvider)

- **Archivos**: `Src/Modules/Modeling/TeammatesBallModelProvider/TeammatesBallModelProvider.h/.cpp`
- **Autor**: Tim Laue
- **Propósito**: Fusionar las observaciones de balón de todos los compañeros de equipo.

### Algoritmo:

1. Recibe las observaciones de balón de cada compañero vía `TeamData` (comunicación inter-robot).
2. Almacena las observaciones en buffers individuales por jugador (`BufferedBall`).
3. Valida cada observación (timeout, calidad de la pose del observador).
4. Genera candidatos activos (`ActiveBall`) y computa un modelo de balón fusionado.
5. El resultado se usa como información adicional en el `BallPerceptFilter` (para verificar percepciones) y en el `WorldModelPrediction`.

**PROVIDES**: `TeammatesBallModel`



## 9. Redes Neuronales Involucradas

### Resumen de todas las redes neuronales del sistema:

| Red            | Ubicación                                            | Formato   | Entrada                      | Salida                       | Módulo que la usa |
| -------------- | ---------------------------------------------------- | --------- | ---------------------------- | ---------------------------- | ----------------- |
| **Encoder**    | `Config/NeuralNets/BallPerceptor/encoder.h5/.onnx`   | H5 / ONNX | Parche grayscale `N×N×1`     | Vector embedding             | `BallPerceptor`   |
| **Classifier** | `Config/NeuralNets/BallPerceptor/classify.h5/.onnx`  | H5 / ONNX | Vector embedding             | Escalar [0,1] (probabilidad) | `BallPerceptor`   |
| **Corrector**  | `Config/NeuralNets/BallPerceptor/corrector.h5/.onnx` | H5 / ONNX | Vector embedding             | Vector 3D `(x,y,radio)`      | `BallPerceptor`   |
| **BOP Net**    | `Config/NeuralNets/BOP/net.h5/.onnx`                 | H5 / ONNX | Imagen completa YUYV `W×H×2` | Mapa multicanal `W'×H'×4`    | `BOPPerceptor`    |


### Tecnología de inferencia: CompiledNN

Todas las redes se ejecutan usando **CompiledNN** (`Util/CompiledNN/`), un motor de inferencia desarrollado por B-Human que:
- Compila los modelos de red neuronal a **código máquina nativo** usando la biblioteca `asmjit`.
- Soporta modelos en formato **HDF5** (.h5) y **ONNX** (.onnx).
- Acepta entradas en `float32` o `uint8`.
- En macOS ARM64, puede utilizar **CoreML** como backend alternativo.



## 10. Representaciones Clave

### Representaciones de Percepción

| Representación | Archivo                                                     | Contenido                                                            |
| -------------- | ----------------------------------------------------------- | -------------------------------------------------------------------- |
| `BallSpots`    | `Src/Representations/Perception/BallPercepts/BallSpots.h`   | Lista de posiciones candidatas (píxeles) donde podría haber un balón |
| `BallPercept`  | `Src/Representations/Perception/BallPercepts/BallPercept.h` | Posición en imagen, radio, posición relativa, covarianza y estado    |


### Representaciones de Modelado

| Representación         | Archivo                                               | Contenido                                                   |
| ---------------------- | ----------------------------------------------------- | ----------------------------------------------------------- |
| `FilteredBallPercepts` | `Src/Representations/Modeling/FilteredBallPercepts.h` | Lista de percepciones verificadas con timestamp             |
| `BallModel`            | `Src/Representations/Modeling/BallModel.h`            | Estado estimado del balón (posición, velocidad, covarianza) |
| `BallContactChecker`   | `Src/Representations/Modeling/BallContactChecker.h`   | Función para detectar colisión pie-balón                    |
| `TeammatesBallModel`   | `Src/Representations/Modeling/TeammatesBallModel.h`   | Modelo fusionado de observaciones del equipo                |
| `WorldModelPrediction` | `Src/Representations/Modeling/WorldModelPrediction.h` | Predicción propagada del modelo anterior                    |
| `BallDropInModel`      | `Src/Representations/Modeling/BallDropInModel.h`      | Información sobre fuera de campo y drop-in                  |


### Representaciones de Configuración

| Representación      | Archivo                                                 | Contenido                                          |
| ------------------- | ------------------------------------------------------- | -------------------------------------------------- |
| `BallSpecification` | `Src/Representations/Configuration/BallSpecification.h` | Radio (50 mm), fricción, distancia máxima de pateo |



## 11. Diagrama de Flujo del Pipeline Completo

```
┌─────────────────────────────────────────────────────────┐
│                    CÁMARA NAO                           │
│              (Imagen YUYV 640×480)                      │
└──────────────────────┬──────────────────────────────────┘
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
   ┌──────────┐  ┌──────────┐  ┌──────────┐
   │ ECImage  │  │ CNSImage │  │CameraImage│
   │(gray/sat)│  │ (bordes) │  │  (YUYV)   │
   └────┬─────┘  └────┬─────┘  └─────┬─────┘
        │              │              │
        ▼              ▼              ▼
┌──────────────┐┌────────────────┐┌──────────────┐
│BallSpots     ││CNSBallSpots    ││BOPPerceptor  │
│Provider      ││Provider        ││(Red BOP)     │
│(scan lines)  ││(contornos 3D)  ││(imagen compl)│
└──────┬───────┘└───────┬────────┘└──────┬───────┘
       │                │               │
       └────────┬───────┘               │
                ▼                       │
         ┌────────────┐                 │
         │ BallSpots  │◄────────────────┘
         │ (candidatos│
         │  en imagen)│
         └─────┬──────┘
               │
               ▼
      ┌─────────────────┐
      │  BallPerceptor  │
      │  ┌───────────┐  │
      │  │  Encoder   │  │  ← encoder.h5
      │  │     ↓      │  │
      │  │ Classifier │  │  ← classify.h5
      │  │     ↓      │  │
      │  │ Corrector  │  │  ← corrector.h5
      │  └───────────┘  │
      └────────┬────────┘
               │
               ▼
        ┌────────────┐
        │ BallPercept│ (posición, radio, status, covarianza)
        └─────┬──────┘
              │
              ▼
     ┌──────────────────┐
     │ BallPerceptFilter│ (elimina falsos positivos)
     └────────┬─────────┘
              │
              ▼
   ┌────────────────────┐
   │FilteredBallPercepts│
   └─────────┬──────────┘
             │
     ┌───────┼──────────────────┐
     ▼       │                  ▼
┌──────────┐ │    ┌──────────────────────┐
│BallContact│ │   │  BallStateEstimator  │
│Checker   │ │    │ ┌──────────────────┐ │
│Provider  │─┘    │ │StationaryKalman  │ │
└──────────┘      │ │RollingKalman     │ │
                  │ └──────────────────┘ │
                  └──────────┬───────────┘
                             │
                             ▼
                      ┌────────────┐
                      │ BallModel  │ (posición, velocidad, covarianza)
                      └──────┬─────┘
                             │
                    ┌────────┼────────┐
                    ▼                 ▼
          ┌──────────────┐  ┌───────────────────┐
          │WorldModel    │  │TeammatesBallModel │
          │Prediction    │  │Provider           │
          │(propagación) │  │(fusión de equipo) │
          └──────────────┘  └───────────────────┘
```



## 12. Herramientas Auxiliares

### BallPhysics
- **Archivo**: `Src/Tools/Modeling/BallPhysics.h/.cpp`
- **Descripción**: Funciones estáticas para el modelo físico de desaceleración lineal del balón:
  - `getEndPosition()`: Calcula dónde se detendrá el balón.
  - `propagateBallPosition()`: Predice la posición del balón en $t$ segundos.
  - `propagateBallPositionAndVelocity()`: Predice posición y velocidad.
  - `velocityForDistance()`: Calcula la velocidad inicial necesaria para alcanzar una distancia.

### BallLocatorTools
- **Archivo**: `Src/Tools/Modeling/BallLocatorTools.h`
- **Descripción**: Funciones utilitarias para la localización del balón:
  - `getLikelihoodOfPosition()`: Calcula la verosimilitud de una posición dada la media y covarianza.
  - `getLikelihoodOfMean()`: Calcula la verosimilitud de la media.
  - `getSmallestLineWithCircleIntersectionFactor()`: Intersección línea-círculo.
  - `ballShouldBeVisibleInCurrentImage()`: Determina si el balón debería ser visible dada la pose actual y los parámetros de la cámara.

### PatchUtilities
- **Uso**: Extracción y normalización de parches de imagen para las redes neuronales.
- **Modos de extracción**: Diferentes métodos de interpolación al reescalar parches.
- **Modos de normalización**: `normalizeContrast` y `normalizeContrast` con ratio de outliers configurable.

### Projection / Transformation
- **Archivos**: `Src/Tools/Math/Projection.h`, `Src/Tools/Math/Transformation.h`
- **Uso**: Conversión entre coordenadas de imagen, coordenadas relativas al robot y coordenadas de campo.



