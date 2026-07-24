# 🖼️ SCENE VISUAL SPEC — Las 5 Escenas Objetivo

> **FUENTE DE VERDAD VISUAL para los Sprints UX.**
> Este documento describe pixel a pixel cómo debe verse cada escena.
> Cualquier IA que toque UI **DEBE** leer este documento antes de escribir código.
>
> **Última actualización:** Julio 2026
> **Imágenes de referencia:** `workspace_memory/referencias_visuales/media__*.png`
> **Plan de implementación:** `workspace_memory/UX_VISION_PLAN.md`

---

## 📋 Sistema de Diseño Global

Antes de describir cada escena, estas reglas aplican a TODAS:

### Paleta (solo estos valores — nunca hardcodear)

| Token | Hex | Uso |
|:------|:----|:----|
| `MixCoachTheme::bgCanvas()` | `#05080D` | Fondo de toda ventana |
| `MixCoachTheme::accent()` | `#A855F7` | Morado principal — títulos, bordes activos, botones |
| `MixCoachTheme::accentGlow()` | `rgba(168,85,247,0.4)` | Glow suave detrás de elementos morados |
| `MixCoachTheme::accentCyan()` | `#00B7FF` | Info técnica, datos de audio, acentos secundarios |
| `MixCoachTheme::success()` | `#4CAF50` | Verde — confirmación, track OK |
| `MixCoachTheme::warning()` | `#FFC107` | Amarillo — advertencia |
| `MixCoachTheme::error()` | `#FF5252` | Rojo — problema crítico |
| `MixCoachTheme::border()` | `rgba(255,255,255,0.08)` | Bordes de cards y paneles |
| `MixCoachTheme::textDim()` | `rgba(255,255,255,0.45)` | Texto secundario, subtítulos, labels |

### Tipografía

| Uso | Tamaño | Peso | Color |
|:----|:------:|:----:|:------|
| Título principal de pantalla | 56px | Bold | Blanco |
| Palabra de marca ("MixCoach") | 56px | Bold | `#A855F7` |
| Subtítulo | 28px | Regular | `rgba(255,255,255,0.70)` |
| Label uppercase | 13px | SemiBold | `rgba(255,255,255,0.45)` letter-spacing 0.30em |
| Texto de tarjeta — título | 36px | Bold | `#A855F7` |
| Texto de tarjeta — descripción | 16px | Regular | `rgba(255,255,255,0.65)` |
| Texto de chat — coach | 17px | Regular | Blanco |
| Texto de chat — usuario | 17px | Regular | Blanco |
| Badge / pill | 12px | SemiBold | Según contexto |

### Animaciones (valores fijos para consistencia)

| Tipo | Duración | Easing | Uso |
|:-----|:--------:|:------:|:----|
| Entrada de pantalla (stagger) | 800ms total | easeOutCubic | Welcome, modo selection |
| Slide-up de panel | 200ms | easeOutQuad | Cualquier panel nuevo |
| Fade de elemento | 150ms | easeOutQuad | Hover, aparición |
| Robot floating | 3s loop | sin() | Solo en Welcome |
| Robot breathing | 4s loop | sin() | Solo en Welcome |
| Halo pulsing | 2s loop | sin() | Solo en Welcome |
| Hover de tarjeta | 100ms | easeOut | ModeSelectionCard |
| Progreso numérico | SmoothValue 800ms | exponencial | Barras de carga |

---

## 🎬 ESCENA 1 — Welcome

> **Referencia:** `media__1783374723803.png`
> **Estado:** `CoachRoomState::Welcome`
> **Componente:** `WelcomeComponent` + `RobotAvatarComponent`

### Descripción general

Pantalla de bienvenida full-screen. Fondo muy oscuro casi negro con tono índigo.
Sin sidebar, sin tabs, sin ningún panel. Solo el robot, el título y el campo de nombre.

### Layout (coordenadas relativas al centro)

```
╔══════════════════════════════════════════════════════════════╗
║  MIXCOACH  v1.0.0                           [−] [□] [×]     ║  ← Header 28px bold, top-left
║                                                              ║
║                     ┌──────────┐                            ║
║                     │  ROBOT   │  ← Círculo 300×300px       ║
║                     │   🤖     │    Glow morado #A855F7 40% ║
║                     │  (halo)  │    Floating ±2px Y         ║
║                     └──────────┘                            ║
║                                                              ║
║           Bienvenido a [MixCoach]                           ║  ← 56px, "MixCoach" en #A855F7
║                                                              ║
║       Tu [mentor] de mezcla impulsado por IA.               ║  ← 28px, "mentor" en #A855F7
║                                                              ║
║                   ¿CÓMO TE LLAMAS?                          ║  ← 13px uppercase, 0.30em tracking
║                                                              ║
║   ┌──────────────────────────────────────────────────────┐  ║
║   │  Escribe tu nombre...                                │  ║  ← Input 740×74px
║   └──────────────────────────────────────────────────────┘  ║    fondo #1A1826, borde 2px #7C3AED
║                                                              ║    radius 14px
║         ┌────────────────────────────────────────────┐      ║
║         │           COMENZAR  →                      │      ║  ← Botón 430×74px
║         └────────────────────────────────────────────┘      ║    gradiente #A855F7 → #7C3AED
║                                                              ║    radius 12px, glow morado
║       🛡️ Tus datos y tu música permanecen siempre en tu equipo. ║  ← Footer 16px, dimmed
╚══════════════════════════════════════════════════════════════╝
```

### Fondo

- Gradiente lineal vertical: `#06060B` → `#090812` → `#130E21`
- Glow radial centrado en el robot: `rgba(168,85,247,0.15)` radio ~400px
- Noise overlay 2% de opacidad (textura grain sutil)

### Robot Avatar

- Círculo con fondo `rgba(139,92,246,0.15)` y borde `rgba(139,92,246,0.3)` 1px
- Robot centered, escala 0.95→1.0 en reveal
- Halo: anillo exterior con alpha 0→0.55 pulsante (2s loop)
- Floating: translateY -2px↔+2px (3s loop, sin())
- Breathing: scale 1.0↔1.01 (4s loop)

### Animación de entrada (staggered 800ms total)

```
0ms    → Header aparece (fade)
100ms  → Robot aparece (fade + scale 0.95→1.0)
250ms  → Título aparece (fade + slide-up 8px)
350ms  → Subtítulo aparece (fade)
450ms  → Label "¿CÓMO TE LLAMAS?" aparece
530ms  → Input aparece
630ms  → Botón COMENZAR aparece
730ms  → Footer aparece
800ms  → Animaciones continuas del robot se activan
```

### Interacción

| Acción | Respuesta visual |
|:-------|:----------------|
| Hover sobre botón COMENZAR | Glow morado más intenso, brightness +5% |
| Click botón COMENZAR sin nombre | Borde del input se pone rojo, vibración sutil (3px) |
| Click con nombre válido | Fade-out de toda la pantalla (300ms), transición a Escena 2 |
| Enter en el input | Igual que click en COMENZAR |

---

## 🎬 ESCENA 2 — Selección de Modo

> **Referencia:** `media__1783374774089.png`
> **Estado:** `CoachRoomState::Intention`
> **Componente:** `ModeSelectionCard` (a crear) dentro de `CoachChatComponent`

### Descripción general

Pantalla full-screen (sin sidebar aún). Fondo mismo que Welcome.
Saludo personalizado arriba, pregunta central, dos tarjetas grandes lado a lado.

### Layout

```
╔══════════════════════════════════════════════════════════════╗
║  🎵 MIXCOACH  v1.0.0                              [?]       ║
║                                                              ║
║                   ¡Hola, [Nombre]!                          ║  ← 20px, color #A855F7
║                                                              ║
║              ¿Qué vamos a hacer hoy?                        ║  ← 56px bold, blanco
║                                                              ║
║      Elige el flujo de trabajo que mejor se adapte.         ║  ← 18px, dimmed
║                                                              ║
║  ┌─────────────────────────┐  ┌─────────────────────────┐  ║
║  │                         │  │                         │  ║
║  │   [ÍCONO MESA MEZCLA]   │  │   [ÍCONO MASTERING]     │  ║  ← Área ícono 230px height
║  │    (faders + waveform)  │  │   (limiter + knob)      │  ║    Ícono ~120×120px centrado
║  │                         │  │                         │  ║
║  │       Mezclar           │  │      Masterizar          │  ║  ← 36px bold, #A855F7
║  │                         │  │                         │  ║
║  │  Trabaja en el balance  │  │  Optimiza el sonido      │  ║  ← 15px, dimmed, 3 líneas
║  │  claridad y espacio de  │  │  final para que suene    │  ║
║  │  cada elemento.         │  │  fuerte y profesional.   │  ║
║  │                         │  │                         │  ║
║  │  🎵 Análisis por pistas │  │  📊 Análisis global      │  ║  ← 13px, dimmed, bullets
║  │  🎚 Balance tonal       │  │  🔊 Loudness · LUFS      │  ║
║  │  🌊 Espacio             │  │  🔄 Compatibilidad        │  ║
║  │                         │  │                         │  ║
║  │          [→]            │  │          [→]            │  ║  ← Botón circular morado 48px
║  └─────────────────────────┘  └─────────────────────────┘  ║
║                                                              ║
║  🎓 No te preocupes, puedes cambiar esto más tarde.         ║  ← 14px, dimmed, footer
╚══════════════════════════════════════════════════════════════╝
```

### Tarjetas (ModeSelectionCard)

- Tamaño: ~480×420px cada una, separadas por 24px gap
- Fondo: `#0D0A1A` con borde `rgba(255,255,255,0.07)` 1px, radius 20px
- **Estado normal:** borde tenue, sin glow
- **Estado hover:** borde `#A855F7` 1px, glow `rgba(168,85,247,0.2)` exterior, scale 1.02, transición 100ms
- **Estado seleccionado / click:** fondo `rgba(168,85,247,0.12)`, borde `#A855F7` 2px, flecha → animada hacia derecha

### Ícono de la tarjeta Mezclar

```
Tres faders verticales con glow morado suave:
- Fader izquierdo: altura 60%, cap circular blanco
- Fader central: altura 80%, cap circular blanco (más alto)
- Fader derecho: altura 45%, cap circular blanco
- Track del fader: línea delgada gris oscuro
- Waveform de fondo difuso: morado muy tenue
- Colores: caps blancos, tracks rgba(255,255,255,0.15)
```

### Ícono de la tarjeta Masterizar

```
Espectro de barras verticales + knob central:
- 7-9 barras de equalización con alturas tipo montaña
- Barras en gradiente azul-cyan (#00B7FF) con brillo
- Knob circular grande centrado abajo: anillo con marca
- Fondo: muy oscuro, casi negro
```

### Botón flecha (círculo morado 48px)

- Fondo: `#A855F7`
- Ícono: `→` blanco 20px
- Hover: glow más intenso, scale 1.1
- Posición: centrado en la parte inferior de cada tarjeta

### Animación de entrada

- Las tarjetas entran con slide-up 200ms + fade, con 80ms de stagger entre ellas
- El saludo personalizado aparece 100ms antes que las tarjetas

---

## 🎬 ESCENA 3 — Referencia + Análisis

> **Referencia:** `media__1783374816779.png`
> **Estado:** `CoachRoomState::ReferenceStage`
> **Componente:** `ReferenceOnboardingCard` + `ReferenceAnalysisProgressCard` (a crear)

### Descripción general

El chat del coach aparece por primera vez. El coach habla (burbuja izquierda con avatar robot mini),
luego aparece el widget de carga de referencia inline en el chat.
La interfaz es un chat con el widget embebido dentro del scroll.

### Layout

```
╔══════════════════════════════════════════════════════════════╗
║  [sin sidebar todavía]                                       ║
║                                                              ║
║  ┌──────────────────────────────────────────────────────┐   ║
║  │ 🤖 [avatar mini]  MixCoach                           │   ║  ← Burbuja coach izquierda
║  │                   Genial! Ahora quiero saber         │   ║    Fondo #1A0A2E 80%
║  │                   cómo quieres sonar.                │   ║    Borde #A855F7 1px
║  │                   10:45                              │   ║    radius 16px
║  └──────────────────────────────────────────────────────┘   ║
║                                                              ║
║  ╔════════════════════════╦═════════════════════════╗       ║
║  ║ ☁️ CARGAR REFERENCIA   ║ 🔗 ENLACE DE REFERENCIA  ║       ║  ← Tarjeta embebida 2 columnas
║  ║                        ║                         ║       ║    Borde #A855F7 1px
║  ║  ┌──────────────────┐  ║  ┌───────────────────┐  ║       ║    Fondo #0D0B1A
║  ║  │  ☁️ (upload icon) │  ║  │ Pega el enlace... │  ║       ║
║  ║  │                  │  ║  └───────────────────┘  ║       ║
║  ║  │  Arrastra y      │  ║                         ║       ║
║  ║  │  suelta aquí     │  ║  Servicios compatibles: ║       ║
║  ║  │  o haz clic      │  ║  [YT] [Spotify] [AM]   ║       ║
║  ║  └──────────────────┘  ║                         ║       ║
║  ║                        ║  [Analizar enlace →]    ║       ║
║  ║  🎵 MiRef.wav          ║                         ║       ║
║  ║  4:32 · 24bit/44.1kHz  ║                         ║       ║
║  ║  [×]                   ║                         ║       ║
║  ║                        ║                         ║       ║
║  ║  [🎵 Analizar ref. →]  ║                         ║       ║
║  ╚════════════════════════╩═════════════════════════╝       ║
║                                                              ║
║  ┌──────────────────────────────────────────────────────┐   ║  ← Burbuja usuario derecha
║  │  Perfecto, déjame buscar una referencia...   10:46 ✓ │   ║    Fondo #0A2A3A 80%
║  └──────────────────────────────────────────────────────┘   ║    Borde #00B7FF 1px
║                                                              ║
║  ┌──────────────────────────────────────────────────────┐   ║  ← Coach pensando
║  │ 🤖 [avatar mini]  • • •                               │   ║    Puntos animados (typing)
║  └──────────────────────────────────────────────────────┘   ║
║                                                              ║
║  ┌──────────────────────────────────────────────────┐[→]   ║  ← Input siempre visible
║  │  Escribe tu respuesta...                          │      ║
║  └──────────────────────────────────────────────────┘      ║
╚══════════════════════════════════════════════════════════════╝
```

### Burbuja del Coach (izquierda)

- Avatar mini: 36×36px, mismo robot reducido
- Nombre "MixCoach" en `#A855F7` 13px SemiBold encima de la burbuja
- Fondo burbuja: `rgba(26,10,46,0.80)` con backdrop-blur
- Borde: `#A855F7` 1px, radius `16px 16px 16px 4px` (esquina inferior izquierda pequeña)
- Timestamp: 12px, dimmed, abajo a la derecha

### Burbuja del Usuario (derecha)

- Sin avatar
- Fondo: `rgba(10,42,58,0.80)` con backdrop-blur
- Borde: `#00B7FF` 1px, radius `16px 16px 4px 16px` (esquina inferior derecha pequeña)
- Double-check `✓✓` en cyan si el mensaje fue procesado

### Widget de referencia (ReferenceOnboardingCard)

- Ancho: 100% del chat, máximo 760px
- Altura: ~260px
- Header de cada columna: uppercase 12px SemiBold, icon + texto
- Columna izquierda (CARGAR REFERENCIA):
  - Drop zone con borde dashed `rgba(168,85,247,0.4)` cuando vacío
  - Drop zone con ícono ☁️ + texto "Arrastra y suelta aquí / o haz clic"
  - Al cargar: fila con ícono 🎵 + nombre + metadata + botón [×]
  - Botón "Analizar referencia": ancho completo, gradiente morado, ícono waveform
- Columna derecha (ENLACE DE REFERENCIA):
  - TextEditor con placeholder "Pega el enlace aquí..."
  - Fila de logos: YouTube (rojo), Spotify (verde), Apple Music (rojo-rosa) — 28px cada uno
  - Botón "Analizar enlace": igual que el de la izquierda
- Divisor vertical: línea 1px `rgba(255,255,255,0.08)`

### Barra de progreso de análisis (ReferenceAnalysisProgressCard)

Aparece en el chat después del click en "Analizar":

```
┌──────────────────────────────────────────────────────────┐
│  🤖 [avatar]  Analizando tu referencia...                │
│                                                          │
│  ██████████████████████░░░░░░░░░░░░░░░░  63%            │
│  Calculando LUFS y perfil espectral...                   │
│                                                          │
│                                              10:46       │
└──────────────────────────────────────────────────────────┘
```

- Número porcentual: 48px bold, `#A855F7`, animado con SmoothValue
- Barra: fondo `rgba(255,255,255,0.08)`, fill gradiente `#7C3AED` → `#A855F7`
- Texto de estado cambia según el progreso (ver UX_VISION_PLAN.md §Sprint UX-2)
- Al llegar a 100%: check ✅ + colapsa a resumen de 1 línea

---

## 🎬 ESCENA 4 — MixMap

> **Referencia:** `media__1783375035535.png`
> **Estado:** `CoachRoomState::MixMapStage`
> **Componente:** `MixMapComponent` (existente) + `MixMapDetailPanel` (a crear)

### Descripción general

Primera pantalla con sidebar izquierda visible. El árbol jerárquico ocupa el centro.
Panel lateral derecho con detalle del track seleccionado. Header con switch Mapa/Lista.

### Layout

```
╔═══╦═══════════════════════════════════════════╦═══════════╗
║   ║  MixMap                     [Mapa][Lista] ║  KICK  [×]║  ← Panel detalle 280px
║   ║  Vista visual de tu mezcla.  [⚙ Ajustes] ║           ║
║ S ║                                           ║  Problema ║
║ I ║              [MASTER]                     ║  detectado║
║ D ║           -13.8 LUFS                      ║           ║
║ E ║        ╱        │        ╲                ║  NIVEL    ║
║ B ║  [DRUM BUS] [MUSIC BUS] [VOC BUS]         ║  -6.2 dB  ║
║ A ║   -9.1dB    -12.4dB    -11.2dB            ║  ██████░  ║  ← Medidor RMS
║ R ║    ╱╲          ╱╲         ╱╲              ║           ║
║   ║ [KK][SN]   [BS][PI]  [LV][BV]            ║  PANORAMA ║
║   ║  ...        ...         ...              ║  L──●──R  ║  ← Dot en centro
║   ║                                           ║           ║
║   ║                                           ║  ECUALI.  ║
║   ║                                           ║  [curva]  ║  ← Mini EQ 6 bandas
║   ║                                           ║  60  250  ║
║   ║                                           ║  5k       ║
║   ║ SESIÓN ACTIVA                             ║           ║
║   ║ Midnight Drive  88.2BPM · Fm              ║  RUTA     ║
║   ║                                           ║  Kick     ║
║   ║ 🤖 MixCoach                               ║  ↓Kick Bus║
║   ║ ● AI Mentor                               ║  ↓Drum Bus║
║   ║ Hablando sobre: Kick                      ║  ↓Master  ║
║   ║                                           ║           ║
║   ║ [waveform animada]                        ║  [🎧 Solo]║  ← Botón escuchar
║   ╠═══════════════════════════════════════════╣           ║
║   ║ El Kick necesita más cuerpo. [Mostrar→]   ║           ║
╚═══╩═══════════════════════════════════════════╩═══════════╝
```

### Sidebar izquierda

- Ancho: 64px
- Fondo: `#060810`
- Íconos: Coach 🧠, Tools 🎚, MixMap 🗺️, Session 📋
- Tab activa (MixMap): ícono en `#A855F7`, fondo `rgba(168,85,247,0.15)` pill
- Tabs bloqueadas: ícono dimmed con 🔒 mini

### Árbol jerárquico (MixMapComponent)

- MASTER: rectángulo 160×48px, fondo `#1A1A2E`, borde `#A855F7` 1px
  - Label: "MASTER" uppercase + LUFS valor
  - Líneas de conexión: `#A855F7` 2px con gradiente que se difumina

- Buses (nivel 1): rectángulos 120×44px por bus, color según `MixCoachTheme::busColour()`
  - DRUM BUS: `#FF6B35` (naranja)
  - MUSIC BUS: `#4ECDC4` (teal)
  - VOCAL BUS: `#FFE66D` (amarillo)
  - Label: nombre + dB nivel

- Tracks (nivel 2): rectángulos 100×40px
  - Ícono de instrumento 14px a la izquierda (según TrackRole):
    - Kick/Snare/HiHat → 🥁
    - Bass/808 → 🎵 (onda baja)
    - Piano/Synth → 🎹
    - Guitar → 🎸
    - Vocal → 🎤
    - FX/Pad → 🌊
  - Nombre abreviado + nivel dB
  - Health dot 6px en esquina superior derecha (verde/amarillo/rojo)

- Track seleccionado: borde `#A855F7` 2px, glow morado exterior

### Panel lateral derecho (MixMapDetailPanel — a crear)

**Ancho fijo:** 280px
**Fondo:** `#060810` con borde izquierdo `rgba(255,255,255,0.08)` 1px

```
ESTRUCTURA DEL PANEL:

┌─ HEADER (48px) ─────────────────────────┐
│  🥁  KICK                            [×] │  ← Ícono 24px + nombre bold + close
│  ● Problema detectado                    │  ← Badge rojo 10px dot + texto 12px
└──────────────────────────────────────────┘

┌─ NIVEL (72px) ───────────────────────────┐
│  NIVEL                         RMS       │  ← Label uppercase 11px + tipo derecha
│  -6.2 dB                                 │  ← Valor 28px bold blanco
│  ████████████░░░░░░░░░░░░░░░░  ← Medidor │  ← Barra animada (SmoothValue)
│  -48    -24    -12    -6    0  dBFS      │  ← Escala
└──────────────────────────────────────────┘

┌─ PANORAMA (52px) ────────────────────────┐
│  PANORAMA                             C  │  ← Label + posición texto "C/L/R"
│  L ───────────────[●]─────────────── R  │  ← Track con dot morado en posición
└──────────────────────────────────────────┘

┌─ EQUALIZACIÓN (100px) ───────────────────┐
│  EQUALIZACIÓN                            │
│  ┌────────────────────────────────────┐  │
│  │        ·      ·                   │  │  ← Curva EQ dibujada con Path
│  │   ···    ────    ──·─────         │  │    Fill: rgba(168,85,247,0.15)
│  │                         ···       │  │    Stroke: #A855F7 1.5px
│  └────────────────────────────────────┘  │
│  60Hz  250Hz  1kHz  5kHz  16kHz          │  ← Labels frecuencia 9px dimmed
│  +2.4  -1.6   +0.9                       │  ← Valores de ganancia activos
└──────────────────────────────────────────┘

┌─ RUTA DE SEÑAL (80px) ───────────────────┐
│  RUTA DE SEÑAL                           │
│  ⬤ Kick                                  │  ← Dot blanco + nombre
│    ↓                                     │  ← Flecha dimmed
│  ⬤ Kick Bus                              │
│    ↓                                     │
│  ⬤ Drum Bus                              │
│    ↓                                     │
│  ⬤ Master                                │
└──────────────────────────────────────────┘

┌─ ACCIÓN (48px) ──────────────────────────┐
│  [🎧 Escuchar en solo              →]    │  ← Botón outline morado full-width
└──────────────────────────────────────────┘
```

### Footer del MixMap (barra inferior)

- Fondo `#060810`, altura 60px
- Izquierda: avatar robot mini + "El Kick necesita más cuerpo. Hay una caída de -6dB..."
- Derecha: botón "Mostrar en Tools" (morado) + "Marcar como resuelto" (outline)

---

## 🎬 ESCENA 5 — Coaching Activo

> **Referencia:** `media__1783375117791.png`
> **Estado:** `CoachRoomState::GainStaging` en adelante
> **Componente:** `TrackProblemCard` + `QuickReplyBar` (a crear)

### Descripción general

La UI completa está activa. El Coach muestra problemas agrupados por familia
en tarjetas ricas dentro del chat. Quick-reply buttons debajo del último mensaje.

### Layout del chat principal

```
╔═════════════════════════════════════════════════════════╗
║  ┌──────────────────────────────────────────────────┐  ║
║  │ 🤖 MixCoach                                      │  ║  ← Burbuja coach
║  │ He estado analizando tu mezcla y he detectado   │  ║
║  │ 3 pistas problemáticas en el grupo Batería.     │  ║
║  │ Están afectando el balance y la claridad.       │  ║
║  │                                       11:23     │  ║
║  └──────────────────────────────────────────────────┘  ║
║                                                         ║
║  ┌──────────────────────────────────────────────────┐  ║  ← TrackProblemCard
║  │ 🥁  Grupo: Batería                               │  ║    Fondo #0A0814
║  │ ● 3 pistas problemáticas detectadas             │  ║    Borde rgba(255,255,255,0.08)
║  │──────────────────────────────────────────────────│  ║    radius 12px
║  │ 🥁 Snare Top    [Problema]                       │  ║
║  │ Exceso en 2.5kHz    [~~/\~~/~]  Impact:●●●●○    │  ║    Mini waveform coloreado
║  │                              Enmas: Kick, OH L  │  ║    Dots de impacto 5 total
║  │                                    [Ver más →]  │  ║
║  │──────────────────────────────────────────────────│  ║
║  │ 🥁 Kick In      [Problema]                       │  ║
║  │ Demasiado subgrave  [████▄▃▂▁]  Impact:●●●●○    │  ║
║  │                              Enmas: Bass, Snare │  ║
║  │                                    [Ver más →]  │  ║
║  │──────────────────────────────────────────────────│  ║
║  │ ➕ Hi Hat        [Problema]                       │  ║
║  │ Agudos agresivos    [/\/\/\/\]  Impact:●●●○○    │  ║
║  │                              Enmas: OH, Vocales │  ║
║  │                                    [Ver más →]  │  ║
║  │──────────────────────────────────────────────────│  ║
║  │ 💡 Te recomiendo trabajar en estas pistas        │  ║  ← Tip footer del card
║  │    para liberar espacio en el grupo.             │  ║
║  │                              [Ir a Tools →]     │  ║
║  └──────────────────────────────────────────────────┘  ║
║                                                         ║
║  ┌──────────────────────────────────────────────────┐  ║  ← Burbuja coach pregunta
║  │ 🤖 ¿Quieres que te guíe con ajustes            │  ║
║  │ específicos para estas 3 pistas?         11:24  │  ║
║  └──────────────────────────────────────────────────┘  ║
║                                                         ║
║  [🎵 Sí, guíame paso a paso] [🎚 Sugerencias] [⏰ Después]  ║  ← QuickReplyBar
║                                                         ║
║  ┌─────────────────────────────────────────┐  [→]     ║  ← Input
║  │  Escribe tu respuesta...               │          ║
║  └─────────────────────────────────────────┘          ║
╚═════════════════════════════════════════════════════════╝
```

### TrackProblemCard — especificación

**Header de grupo:**
- Fondo: `#0D0A1A`, padding 12px 16px
- Ícono familia 20px + nombre bold + contador de problemas con dot rojo pulsante

**Fila por track:**
- Altura: 56px, padding 12px 16px
- Separador: línea 1px `rgba(255,255,255,0.06)`
- Layout horizontal:
  1. Ícono instrumento 20px (según TrackRole)
  2. Nombre track 15px bold
  3. Badge "Problema" o "Advertencia" (12px, pill color)
  4. Mini waveform 80×20px (generado desde bandEnergies)
  5. "Impact:" + dots (●●●●○) — llenos según priority score
  6. "Enmas:" + nombres de tracks que enmascaran (12px dimmed)
  7. Botón "Ver más →" (outline pequeño, derecha)

**Mini waveform (80×20px):**
- Generado con Path desde los 30 `bandEnergies` del slot
- Color por severity:
  - 🔴 Crítico (OffTarget gain/tonal): `#FF5252`
  - 🟡 Advertencia (NearTarget): `#FFC107`
  - 🟢 OK: `#4CAF50`
  - Morado: `#A855F7` (neutro)

**Dots de impacto (5 posiciones):**
- Valor de `MixPriorityEngine::getPriorityScore()` mapeado a 1-5
- Dot lleno: `#A855F7` 8px circle
- Dot vacío: `rgba(255,255,255,0.2)` 8px circle
- Gap: 4px entre dots

**Footer del card:**
- Fondo: `rgba(168,85,247,0.05)`, padding 10px 16px
- Ícono 💡 + texto sugerencia 13px dimmed
- Botón "Ir a Tools" alineado a la derecha

### QuickReplyBar — especificación

- Posición: sticky debajo del último mensaje del coach, encima del input
- Fondo: `rgba(168,85,247,0.06)`, padding 8px 16px
- Scroll horizontal si hay más de 4 botones
- Cada botón (pill):
  - Fondo: `rgba(168,85,247,0.12)`, borde `rgba(168,85,247,0.3)` 1px, radius 20px
  - Texto: 14px, color `rgba(255,255,255,0.85)`
  - Padding: 8px 16px
  - Hover: fondo `rgba(168,85,247,0.25)`, borde `#A855F7`, scale 1.03
  - Click: desaparece la barra + el texto se envía como mensaje de usuario
- Botones en Escena 5:
  - `🎵 Sí, guíame paso a paso`
  - `🎚 Dame sugerencias generales`
  - `⏰ No, lo revisaré después`

---

## 🔗 Relación con el código

| Escena | Estado `CoachRoomState` | Componente principal |
|:-------|:------------------------|:---------------------|
| 1 — Welcome | `Welcome` | `WelcomeComponent` ✅ existe |
| 2 — Selección modo | `Intention` | `ModeSelectionCard` ❌ crear |
| 3 — Referencia | `ReferenceStage` | `ReferenceOnboardingCard` ❌ crear |
| Barra 0→100% | `ReferenceStage` | `ReferenceAnalysisProgressCard` ❌ crear |
| 4 — MixMap | `MixMapStage` | `MixMapComponent` ✅ + `MixMapDetailPanel` ❌ crear |
| 5 — Coaching | `GainStaging`+ | `TrackProblemCard` ❌ + `QuickReplyBar` ❌ crear |

**Ver especificaciones de implementación:** `workspace_memory/UX_VISION_PLAN.md`

---

*Scene Visual Spec — MixCoach — Julio 2026*
*Esta es la fuente de verdad visual. Si el código no coincide con este documento, el código está mal.*
