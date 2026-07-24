# 05 - SYSTEM STATES

> **Los estados tecnicos del sistema MixCoach y sus transiciones.**
> Define los estados internos de la aplicacion, maquina de fases, estados de UI y conectividad.
>
> **Version:** 1.0 | **Ultima actualizacion:** 29 junio 2026

---

## 1. Vision General

MixCoach opera como una **maquina de estados finitos** con varias capas de estado que se ejecutan en paralelo:

| Capa | Proposito | Quien la controla |
|:-----|:----------|:------------------|
| **Fases de Mentoria** | Progresion del flujo de mezcla | SessionProgression |
| **Conectividad IPC** | Comunicacion Messenger MixCoach | SharedData + SlotRegistry |
| **UI States** | Estados visuales de componentes | Cada componente UI |
| **LLM States** | Disponibilidad y estado del LLM | AiCoachAdapter |
| **Audio Analysis** | Ciclo de analisis DSP | CoachEngine + AudioAnalyzer |

---

## 2. Maquina de Fases (SessionProgression)

### 2.1 Fases de Mentoria

Las fases se definen en `SessionProgression.h`:

```cpp
enum class SessionPhase {
    kSetup,         // Configuracion inicial: genero, referencia
    kIdentity,      // Identificacion de pistas (Messengers)
    kMapping,       // Mapa de sesion (MixMap)
    kCoaching,      // Coaching activo (gain, EQ, compresion)
    kReference,     // Modo referencia activo
    kRefinement,    // Refinamiento artistico
    kReport         // Reporte final
};
```

### 2.2 Estados de Progresion

Cada fase tiene 4 estados internos:

| Estado | Significado | Indicador UI |
|:-------|:------------|:-------------|
| **Locked** | No disponible, fase anterior no completada | Candado Bloqueado |
| **Available** | Listo para iniciar | Pulse / Highlight |
| **InProgress** | Activo actualmente | Spinner / Barra |
| **Completed** | Terminado y aprobado | Checkmark |

### 2.3 Transiciones entre Fases

```
kSetup      -> (genero + referencia cargada)   -> kIdentity
kIdentity   -> (Messengers detectados + roles)  -> kMapping
kMapping    -> (MixMap confirmado)              -> kCoaching
kCoaching   -> (MixScore > 70 o usuario pide)   -> kReference
kReference  -> (Diferencia con ref < 20%)      -> kRefinement
kRefinement -> (Usuario solicita finalizar)     -> kReport
kReport     -> (Exportar o Nueva Sesion)        -> kSetup
```

### 2.4 Reglas de Transicion

| Regla | Descripcion |
|:------|:------------|
| **T1** | No se puede saltar una fase. kSetup -> kCoaching sin pasar por kIdentity esta prohibido |
| **T2** | El usuario puede volver a una fase anterior, pero pierde el estado Completed |
| **T3** | Cada fase requiere confirmacion explicita del usuario o de un trigger automatico |
| **T4** | Si el usuario no completa una fase en 5 minutos, el Coach pregunta si necesita ayuda |

---

## 3. Estados de Conectividad IPC

### 3.1 Estados de Slot (por Messenger)

Cada slot en el `SlotRegistry` puede estar en uno de estos estados:

| Estado | Significado | Indicador |
|:-------|:------------|:----------|
| **Empty** | Sin Messenger instalado | No aparece en UI |
| **Active** | Messenger funcionando, heartbeat presente | Verde Dot |
| **Stale** | Heartbeat perdido por < 30s | Amarillo Dot |
| **Disconnected** | Heartbeat perdido por > 30s | Rojo Dot |
| **Error** | Datos corruptos o inconsistentes | Warning |

### 3.2 Estados de Conectividad Global

| Estado | Condicion | Accion |
|:-------|:----------|:-------|
| **Connected** | >= 1 slot activo | Operacion normal |
| **Degraded** | Slots activos pero con algunos stale | Coach advierte |
| **Disconnected** | 0 slots activos | Coach guia para insertar Messengers |
| **Error** | SharedMemory no disponible | Mostrar mensaje de error + reintentar |

### 3.3 Heartbeat Timing

```
Active heartbeat:    cada 100ms (desde Messenger)
Stale timeout:       30s sin heartbeat
Force sync:          cada 1s (desde Background Worker)
Full re-scan:        cada 30s (desde CoachEngine)
```

---

## 4. Estados de UI por Componente

### 4.1 Estados Obligatorios por Componente

Todo componente UI debe manejar estos 4 estados:

```cpp
enum class UIState {
    Loading,    // Datos no disponibles, mostrando esqueleto/spinner
    Empty,      // No hay datos para mostrar (primera vez, sin sesion)
    Active,     // Datos disponibles y actualizados
    Error       // Error al obtener datos
};
```

### 4.2 Mapa de Estados por Componente

| Componente | Loading | Empty | Active | Error |
|:-----------|:--------|:------|:-------|:------|
| **DashboardScreen** | Spinner + "Analizando..." | "Bienvenido" + hint | Header + NextStep + Prioridades | Mensaje + Retry |
| **CoachChat** | Typing indicator | Mensaje de bienvenida | Burbujas de chat | "Error al conectar" |
| **MixMap** | "Construyendo mapa..." | "Sin Messengers" | Arbol de buses | "Error de conexion" |
| **ProgressScreen** | "Cargando historial..." | "Tu primera sesion" | Timeline + milestones | "Error al cargar" |
| **EndOfSession** | "Generando reporte..." | - | Score + correcciones | "Error al generar" |
| **AnalyzersPanel** | "Esperando audio..." | "Sin senal" | FFT + LUFS + fase | "Error de analisis" |
| **ReferencePanel** | "Analizando..." | "Arrastra tu referencia" | Comparacion visual | "Archivo no valido" |

---

## 5. Estados del LLM

### 5.1 Estados del Servicio LLM

| Estado | Significado | UI |
|:-------|:------------|:---|
| **Available** | LLM responde normalmente | Indicador verde (interno) |
| **Slow** | LLM tarda > 3s en responder | Indicador ambar (interno) |
| **Degraded** | LLM responde con errores parciales | Usar fallback + log |
| **Offline** | LLM no disponible | Usar respuesta template del engine |

### 5.2 Modos de Operacion

```
Modo normal:
  LLM disponible -> Coach responde con LLM

Modo degradado:
  LLM lento -> Coach usa cache de respuestas recientes

Modo offline:
  LLM caido -> Coach usa generateFallbackResponse()

Timeout: 5s
Reintentos: 1 antes de degradar
Check de salud: cada 60s
```

### 5.3 Fallback Template (sin LLM)

Cuando el LLM no esta disponible, el Coach genera respuestas template desde el engine:

```cpp
juce::String generateFallbackResponse(const TrackAdvice& topAdvice) {
    return "He notado algo en " + topAdvice.trackName + ": "
         + topAdvice.humanMessage + ". "
         + "Prueba: " + topAdvice.actionText + ". "
         + "Como suena despues de ese ajuste?";
}
```

---

## 6. Estados de Analisis de Audio

### 6.1 Ciclo de Analisis

El AudioAnalyzer opera en ciclos:

| Estado | Descripcion | Duracion |
|:-------|:------------|:---------|
| **Idle** | Sin audio, esperando senal | Indefinido |
| **Buffering** | Acumulando muestras para FFT | ~2.9ms (1 bloque) |
| **Analyzing** | Procesando FFT/LUFS/fase | ~1-5ms |
| **Ready** | Datos disponibles para lectura | Hasta proximo bloque |

### 6.2 Estados por Tipo de Analisis

| Analisis | Frecuencia | Latencia |
|:---------|:-----------|:---------|
| FFT (1024-point) | Cada bloque (~2.9ms) | Tiempo real |
| LUFS (EBU R128) | Cada 100ms | 400ms integrated |
| Phase correlation | Cada bloque | Tiempo real |
| Crest factor | Cada 100ms | 100ms |
| Band energies (30) | Cada bloque | Tiempo real |
| Track analysis | Cada ~8s (periodicAnalysis) | 8s |

---

## 7. Matriz de Transiciones

### 7.1 Transiciones de Fase

| From \ To | kSetup | kIdentity | kMapping | kCoaching | kReference | kRefinement | kReport |
|:----------|:-------|:----------|:---------|:-----------|:-----------|:------------|:--------|
| kSetup | - | genero+ref | - | - | - | - | - |
| kIdentity | reset | - | roles | - | - | - | - |
| kMapping | reset | - | - | confirm | - | - | - |
| kCoaching | reset | - | - | - | score>70 | score>85 | - |
| kReference | reset | - | - | - | - | diff<20% | - |
| kRefinement | reset | - | - | - | - | - | finish |
| kReport | new | - | - | - | - | - | - |

### 7.2 Eventos que Disparan Transiciones

| Evento | De | A | Condicion |
|:-------|:---|:--|:----------|
| `onGenreSelected()` | kSetup | kIdentity | Genero seleccionado |
| `onReferenceLoaded()` | kSetup/kIdentity | kIdentity/kMapping | Referencia cargada y analizada |
| `onRolesConfirmed()` | kIdentity | kMapping | Todos los Messengers con rol asignado |
| `onMapConfirmed()` | kMapping | kCoaching | Usuario confirma el mapa |
| `onScoreThreshold()` | kCoaching | kReference | MixScore >= 70 |
| `onRefinementReady()` | kReference | kRefinement | Diferencia con referencia < 20% |
| `onSessionEnd()` | kRefinement | kReport | Usuario solicita finalizar |
| `onNewSession()` | kReport | kSetup | Usuario inicia nueva sesion |

---

## 8. Manejo de Errores y Degradacion Graceful

### 8.1 Estrategia General

Ante cualquier error, el sistema debe:
1. **No mostrar pantallas en blanco** - siempre mostrar un estado informativo
2. **No crashear** - capturar excepciones, loguear, continuar
3. **Informar al usuario** - con lenguaje amigable, no tecnico
4. **Ofrecer una accion** - reintentar, cancelar, o seguir con datos parciales

### 8.2 Escenarios de Error

| Escenario | Deteccion | Accion |
|:----------|:-----------|:-------|
| **SharedMemory no disponible** | CreateFileMappingW falla | Mostrar "Conectando Messengers..." + reintentar cada 5s |
| **Slot corrupto** | Checksum invalido | Ignorar slot, marcar como Error, loguear |
| **LLM timeout** | No responde en 5s | Usar fallback template del engine |
| **LLM response invalida** | JSON malformado | Reintentar 1 vez, luego fallback |
| **Audio buffer underrun** | Silencio inesperado | Mostrar "Esperando senal de audio..." |
| **Demasiados slots** | > 128 Messengers | Ignorar exceso, mostrar warning |
| **Fase saltada** | Usuario intenta coaching sin Messengers | Coach redirige: "Primero necesito conocer tu sesion..." |

### 8.3 Indicador de Estado del Sistema

```cpp
enum class SystemHealth {
    kHealthy,       // Todo funciona correctamente
    kWarning,       // Problema menor (slot stale, LLM lento)
    kCritical,      // Problema mayor (sin Messengers, LLM offline)
    kError          // Error critico (SharedMemory caida)
};
```

Este indicador es **interno** - nunca se muestra al usuario como numero o codigo. Se traduce a lenguaje del Coach.

---

*Documento de estados del sistema - MixCoach v1.0 - 29 junio 2026*
