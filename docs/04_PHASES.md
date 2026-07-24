# 🗺️ 04 — PHASES

> **Todas las fases de la sesión de MixCoach.**
> Define objetivos, componentes visibles/ocultos, eventos, Coach dice, y condiciones para avanzar.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Documento base:** `03_SESSION_FLOW.md` (versión técnica detallada)
> **Implementación:** `SessionProgression.h/.cpp`, `PhaseManager.h/.cpp`, `CoachEngineSetup.cpp`

---

## 📋 Índice

1. [Visión General](#1-vision-general)
2. [SessionProgression — Fases de Alto Nivel](#2-sessionprogression)
3. [MentorPhase — Fases de Mezcla](#3-mentorphase)
4. [SetupStep — Sub-fases de Configuración](#4-setupstep)
5. [CoachRoomState — Estados de la UI](#5-coachroomstate)
6. [Diagrama de Estados Completo](#6-diagrama-de-estados-completo)
7. [Master Mode — Variantes](#7-master-mode)

---

## 1. Visión General

MixCoach opera en **tres capas de fase** que corren en paralelo:

| Capa | Propósito | Controlador | 
|:-----|:----------|:------------|
| **SessionProgression** | Ciclo de vida global de la sesión | `SessionProgression.h` |
| **MentorPhase** | Progresión técnica de mezcla | `PhaseManager.h` |
| **CoachRoomState** | Estados visuales de la UI | `CoachRoomState.h` |

### Timeline Completo

```
Setup ──→ Ref ──→ Análisis ──→ Gain ──→ Balance ──→ EQ ──→ Comp ──→ Space ──→ MasterCheck ──→ Refine ──→ Report ──→ Memory
  │         │         │            │        │         │       │        │         │             │          │          │
  ▼         ▼         ▼            ▼        ▼         ▼       ▼        ▼         ▼             ▼          ▼          ▼
Welcome  Cargar   Conocer      Ajustar  Faders    EQ por  Compres.  Reverb/  Comparar      Refinar   Exportar   Recordar
         ref      mezcla       niveles  + paneo   pista   y sat.    Delay    vs ref        arte      reporte
```

---

## 2. SessionProgression (Fases de Alto Nivel)

### Las 7 Fases

```
Setup ──→ LoadReference ──→ DeepAnalysis ──→ GuidedCoaching ──→ Refinement ──→ Report ──→ Memory
```

| Fase | Objetivo | Detección Automática | Visible |
|:-----|:---------|:---------------------|:--------|
| **Setup** | Configuración inicial: nombre, modo, género, referencia, Messengers | Se inicia al abrir el plugin | Chat + Reference + MessengerList |
| **LoadReference** | Cargar y analizar referencia | `hasReference() == true` | Chat + Reference + MatchPanel |
| **DeepAnalysis** | Análisis espectral completo | `referenceSummary_.valid == true` | Chat + Reference + MatchPanel |
| **GuidedCoaching** | Mezcla por etapas (gain → balance → EQ → comp → espacio → master check) | Correcciones en historial | Chat + MessengerList + MixMap + Tools |
| **Refinement** | Refinamiento artístico (solo si MixScore ≥ 70) | `refinementProfile_.isRelevant == true` | Chat + RefinementHero + Progress |
| **Report** | Reporte final | Usuario finaliza sesión | EndOfSessionComponent (overlay) |
| **Memory** | Persistencia de datos | Reporte generado | — (proceso interno) |

### Transiciones

```
From → To                     Evento                         Condición
──────────────────────────────────────────────────────────────────────────
Setup → LoadReference         setReferenceAudio()            Referencia cargada
LoadReference → DeepAnalysis  onReferenceSummaryReady()      summary.valid == true
DeepAnalysis → GuidedCoaching periodicAnalysis()             correctionHistory no vacío
GuidedCoaching → Refinement   onMixScoreThreshold()          MixScore >= 70
Refinement → Report           onSessionEnd()                 Usuario finaliza
Report → Memory               onSessionSaved()               Datos persistidos
```

---

## 3. MentorPhase (Fases de Mezcla)

### Las 7 Fases de Mezcla (Mix Mode)

```
Organización ──→ Gain Staging ──→ Balance ──→ EQ ──→ Compresión ──→ Espacio ──→ Master Check
```

#### 3.1 Organización

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Identificar pistas: nombre, rol, color, bus |
| **Coach dice** | "Vamos a poner orden. Asigna roles a cada pista." |
| **Paneles visibles** | Chat + MessengerList |
| **Condición para avanzar** | `identifiedTracks > 0` + `bussedTracks >= totalTracks` + `routingValidated == true` |

#### 3.2 Gain Staging

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Ajustar niveles para headroom saludable (-18dB a -3dB), sin clipping |
| **Coach dice** | "Ajusta niveles para que ninguna pista clipee. Picos entre -18dB y -12dB." |
| **Paneles visibles** | Chat + MessengerList + Master Meter |
| **Condición para avanzar** | `clippingCount == 0` + `-18dB < maxGlobalPeak < -3dB` |

#### 3.3 Balance

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Balancear niveles relativos entre instrumentos usando faders y paneo |
| **Coach dice** | "Kick y bajo como base. Voz al frente. Escucha en mono." |
| **Paneles visibles** | Chat + MessengerList + Master Meter |
| **Condición para avanzar** | `unbalancedPairs / totalPairs < 30%` |

#### 3.4 EQ

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Corrección de balance tonal: EQ por pista, carving espectral |
| **Evento clave** | `🔓 Tools se desbloquean por primera vez` |
| **Coach dice** | "Es hora de esculpir. HPF en pistas sin graves, carving espectral." |
| **Paneles visibles** | Chat + MessengerList + Master Meter + **Tools 🔓** |
| **Condición para avanzar** | `spectralChecked == true` + `spectralTiltOk == true` |

#### 3.5 Compresión

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Controlar dinámica: compresores, crest factor saludable |
| **Coach dice** | "Compresores para nivelar picos. Crest objetivo: 8-14dB." |
| **Paneles visibles** | Chat + MessengerList + Master Meter + Tools |
| **Condición para avanzar** | `6dB <= avgCrestFactor <= 14dB` |

#### 3.6 Espacio

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Crear profundidad: reverb, delay, ancho estéreo |
| **Coach dice** | "Crea la escena sonora: panorama, reverb, profundidad." |
| **Paneles visibles** | Chat + MessengerList + Master Meter + Tools |
| **Condición para avanzar** | `hasSpatialFx == true` + `0.3 < avgCorrelation < 0.8` |

#### 3.7 Master Check

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Verificación final: referencia, LUFS target, compatibilidad mono |
| **Coach dice** | "Verificación final. ¿Suena bien en mono? ¿Headroom adecuado?" |
| **Paneles visibles** | Chat + MessengerList + Master Meter + Tools + Progress |
| **Transición** | Última fase. Puede pasar a Refinement o Report. |

---

## 4. SetupStep (Sub-fases de Configuración)

El flujo de setup es un diálogo interactivo de bienvenida con 9 sub-fases:

```
NotStarted → WaitingForName → WaitingForMode → [Mix] WaitingForReferenceFirst → WaitingForGenre → WaitingForConfirm → WaitingForSetupInstructions → Complete
                                                  [Master] WaitingForDestination → WaitingForConfirm → Complete
```

| Step | Coach dice | Input del usuario | Panel que se revela |
|:-----|:-----------|:------------------|:--------------------|
| NotStarted | — | — | — |
| WaitingForName | "¿Cómo te llamas?" | Nombre | — |
| WaitingForMode | "¿Mix o Master?" | "Mix" / "Master" | Botones Mix/Master |
| WaitingForReferenceFirst | "Carga tu referencia" | Archivo WAV/MP3 | 🔓 Reference Panel |
| WaitingForGenre | "¿Qué género?" | Pop, Rock, Reggaeton... | Chips de género |
| WaitingForConfirm | "¿Es correcto?" | Sí / Cambiar | — |
| WaitingForSetupInstructions | "Inserta Messengers" | Listo | 🔓 Messenger List |
| Complete | "Setup completado." | — | — |

---

## 5. CoachRoomState (Estados de la UI)

Los estados visuales que guían la progressive disclosure:

```
Welcome → Intention → Genre → ReferenceStage → MessengerStage → MixMapStage → GainStaging → Balance → EQ → Compression → Space → Automation → MasterCheck → Report
```

| Estado | Visible | Oculto | Notas |
|:-------|:--------|:-------|:------|
| Welcome | Solo Chat + Avatar | Todo | Modo más restrictivo |
| Intention | Chat + Botones Mix/Master | Reference, Messengers, Tools | — |
| Genre | Chat + Chips de género | Tools, Session, Report | — |
| ReferenceStage | Chat + Reference Panel | Messengers, Tools, Session | Reference se revela aquí |
| MessengerStage | Chat + Reference + MessengerList | Tools, Session | — |
| MixMapStage | Chat + Ref + Msgrs + MixMap | Tools, Session | MixMap se revela |
| GainStaging | Chat + Ref + Msgrs + MixMap | Tools, Session | Tools bloqueado 🔒 |
| Balance | Chat + Ref + Msgrs + MixMap | Tools, Session | — |
| EQ | Chat + Ref + Msgrs + MixMap + Tools | Session | Tools se desbloquea 🔓 |
| Compression | Chat + Ref + Msgrs + MixMap + Tools | Session | — |
| Space | Chat + Ref + Msgrs + MixMap + Tools | Session | — |
| MasterCheck | Chat + Ref + Msgrs + MixMap + Tools + Progress | Report | Progress se revela |
| Report | Overlay de Reporte | Chat oculto | Overlay reemplaza todo |

---

## 6. Diagrama de Estados Completo

```
                                ┌─────────────┐
                                │   WELCOME   │  Solo Chat + Avatar
                                └──────┬──────┘
                                       │ Usuario escribe nombre
                                       ▼
                                ┌─────────────┐
                                │  INTENTION  │  Botones Mix/Master
                                └──────┬──────┘
                                       │ Mix / Master
                                  ┌────┴────┐
                                  ▼         ▼
                          ┌──────────┐ ┌───────────┐
                          │ REF.     │ │  MASTER   │
                          │ FIRST(V6)│ │ DESTINO   │
                          └────┬─────┘ └─────┬─────┘
                               │ Ref cargada │ Confirmado
                               ▼             ▼
                          ┌──────────┐ ┌───────────┐
                          │  GENRE   │ │  SETUP    │
                          └────┬─────┘ └───────────┘
                               │ Confirmado
                               ▼
                          ┌──────────┐
                          │  SETUP   │
                          │INSTRUCT. │  Insertar Messengers
                          └────┬─────┘
                               │ "Listo"
                               ▼
                     ╔═══════════════════╗
                     ║    MIXMAPSTAGE    ║  🔓 MixMap revelado
                     ╚═══════════════════╝
                               │
                     ╔═══════════════════╗
                     ║   GAIN STAGING    ║  Ajustar niveles
                     ╚═══════════════════╝
                               │
                     ╔═══════════════════╗
                     ║     BALANCE       ║  Faders + paneo
                     ╚═══════════════════╝
                               │
                     ╔═══════════════════╗
                     ║       EQ          ║  🔓 Tools desbloqueados
                     ╚═══════════════════╝
                               │
                     ╔═══════════════════╗
                     ║   COMPRESSION     ║  Compresores + saturación
                     ╚═══════════════════╝
                               │
                     ╔═══════════════════╗
                     ║      SPACE        ║  Reverb, delay, panorama
                     ╚═══════════════════╝
                               │
                     ╔═══════════════════╗
                     ║   MASTER CHECK    ║  🔓 Progress revelado
                     ╚═══════════════════╝
                               │
                    ┌──────────┴──────────┐
                    ▼                     ▼
          ┌─────────────────┐   ┌─────────────────┐
          │   REFINEMENT    │   │     REPORT      │
          │ (MixScore ≥ 70) │   │ (End Session)   │
          └────────┬────────┘   └────────┬────────┘
                   │                     │
                   └──────────┬──────────┘
                              ▼
                     ┌─────────────────┐
                     │     MEMORY      │
                     │ (Sesión guardada)│
                     └─────────────────┘
```

---

## 7. Master Mode

En Master Mode, MentorPhase cambia sus descripciones para enfocarse en el master bus.

```
Setup ──→ Organización ──→ Gain Staging ──→ Balance ──→ EQ ──→ Compresión ──→ Espacio ──→ Master Check
 │           │                │               │          │         │              │             │
 ▼           ▼                ▼               ▼          ▼         ▼              ▼             ▼
Elegir     Configurar       Medir LUFS,    Balance    EQ sutiles  Compresión    Ancho         Comparar
destino    destino          True Peak,     espectral  al master   suave +       estéreo +     vs ref +
                            crest          global                 limitación    correlación   veredicto
```

### Targets por Destino

| Destino | LUFS Target | True Peak Máx |
|:--------|:------------|:--------------|
| Spotify | -14 LUFS | -1 dBTP |
| Apple Music | -16 LUFS | -1 dBTP |
| YouTube | -14 LUFS | -1 dBTP |
| Club | -8 LUFS | -0.5 dBTP |
| Streaming General | -14 LUFS | -1 dBTP |
| CD | -9 LUFS | -0.1 dBTP |

---

*Documento de fases — MixCoach — 4 julio 2026*
*Versión resumida. Ver `03_SESSION_FLOW.md` para la especificación técnica completa.*
