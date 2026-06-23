# 🏗️ MASTER VISION — MixCoach + Messenger
> ⚠️ **LEE ESTE DOCUMENTO PRIMERO** — Es la fuente de verdad única para toda la UI.
> Los otros archivos en `workspace_memory/` son referencias complementarias.
>
> **Última actualización:** 5 Junio 2026 — Optimizaciones de performance (background cache, child-level repaint, waterfall 80 rows, phosphor 1500, VectorscopeSystem grid cache + phosphor Path), fix de visibilidad de labels (freq Hz + dB, grid M/L/R/S, crest gauge, correlation markers, metrics table — todos accentCyan/textDim bold ≥7.5px), tests unitarios ~5190 todos pasan

---

## 📦 QUÉ ES ESTO

Dos plugins VST3 para FL Studio:

| Plugin | Propósito | Archivo VST3 |
|:-------|:-----------|:--------------|
| **MixCoach** | Ingeniero de mezcla IA + Analyzers suite | `MixCoach.vst3` |
| **Messenger** | Track Inspector individual por pista | `Messenger.vst3` |

Ambos comparten el mismo tema visual (`MixCoachTheme`) y sistema de datos (`SlotRegistry`/`SharedData`).

---

## 🎯 LECTURA OBLIGATORIA PARA CUALQUIER IA

### Orden de lectura recomendado:
1. **ESTE DOCUMENTO** (MASTER_VISION.md) — visión global, colores, layout, mapeo
2. `referencia_visual_messenger.md` — detalles del Messenger
3. `referencia_visual_tab1_ai_coach.md` — detalles Tab 1 AI Coach
4. `referencia_visual_tab2_analyzers.md` — detalles Tab 2 Analyzers

### Referencias visuales (IMÁGENES):

Las imágenes de referencia están copiadas dentro del proyecto en:
```
MixCoach/workspace_memory/referencias_visuales/
├── Messenger.png                  ← Messenger track inspector (1.0 MB)
├── Mix Coach Pestaña  1.png       ← Tab 1 AI Coach completo (1.6 MB)
├── Mix Coach Pestaña 2.png        ← Tab 2 Analyzers completo (1.4 MB)
├── SESION 1 METER.png             ← MeterPanel close-up (321 KB)
├── SESION 2 SPECTRUM ANALYZER.png ← Spectrograph close-up (515 KB)
├── SESION 3 PHASE SCOPE.png       ← PhaseScope close-up (353 KB)
└── SESION 4 VU METERS.png         ← Vintage VU close-up (540 KB)
```

> ⚠️ **IMPORTANTE**: Cuando trabajes en un módulo, ABRE su imagen de referencia.
> Por ejemplo, si estás editando el MeterPanel, abre `SESION 1 METER.png`.
> Una imagen vale más que 1000 palabras de documentación.

También disponibles en su ubicación original:
```
C:\Proyectos\referencias\
├── Messenger.png
├── Mix Coach Pestaña  1.png
└── Mix Coach Pestaña 2.png
```

---

## 🎨 PALETA DEFINITIVA — REFERENCIA VISUAL (FUENTE DE VERDAD)

> ⚠️ Esta paleta proviene de las capturas de referencia PNG.
> Los colores acá definidos son el **target visual** al que debe migrarse.
> La implementación actual en `MixCoachTheme.h` puede diferir.

### Fondos
| Token | Hex (Referencia) | Hex (Implementación actual) | Uso |
|:------|:-----------------|:---------------------------|:----|
| `bgCanvas` | `#05080D` | `#03060A` | Fondo más profundo (canvas principal) |
| `bgMessenger` | `#05070D` | — | Fondo del Messenger UI |
| `bgPanelTab1` | `#091018` | `#050A12` | Paneles de Tab 1 AI Coach |
| `bgPanelTab2` | `#0A1018` | `#050A12` | Paneles de Tab 2 Analyzers |
| `bgCardMessenger` | `#090D15` | `#08131D` | Tarjeta flotante del Messenger |
| `bgCard` | `#0A1018` | `#08131D` | Card bg / glass panel fill |

### Brand / Accents
| Token | Hex (Referencia) | Hex (Implementación actual) | Uso |
|:------|:-----------------|:---------------------------|:----|
| **`accent`** | **`#A855F7`** | **`#D100FF`** | Títulos, tabs activas, branding — **MÁS MORADO, MENOS NEÓN** |
| `accentGlow` | `rgba(168,85,247,0.4)` | `#E000FF` | Glow morado más suave |
| **`accentCyan`** | **`#00B7FF`** | **`#00D9FF`** | Spectrum, LUFS meters, info técnica — **MÁS AZULADO** |
| `accentCyanBright` | — | `#66E6FF` | Glow cyan brillante |
| `accentCyanDim` | — | `#33CFFF` | Cyan secundario |

### Colores Funcionales (Meters / Status)
| Estado | Hex (Referencia) | Hex (Implementación actual) | Uso |
|:-------|:-----------------|:---------------------------|:----|
| **Señal saludable** | **`#4CAF50`** | **`#7CFF00`** | VU meter green — **MÁS VERDE ESTÁNDAR** |
| **Advertencia** | **`#FFC107`** | **`#FFD000`** | Warning — **MÁS AMARILLO CÁLIDO** |
| **Error / Clipping** | **`#FF5252`** | **`#FF3B3B`** | Error — **MÁS ROSA/ROJO** |

### Texto
| Token | Hex (Referencia) | Hex (Implementación) |
|:------|:-----------------|:--------------------|
| `textPrimary` | `#FFFFFF` | `#E8EDF2` |
| `textSecondary` | `#D1D5DB` | — |
| `textDim` | — | `#AAB4C0` |
| `textMuted` | — | `#6B7B8D` |

### Borders
| Token | Hex (Referencia) | Hex (Implementación) |
|:------|:-----------------|:--------------------|
| `border` | `rgba(255,255,255,0.08)` | `#1E3050` |

---

## ⚠️ DISCREPANCIAS CONOCIDAS vs REFERENCIA

### Paleta
| Componente | Referencia pide | Implementación actual | Prioridad |
|:-----------|:----------------|:---------------------|:----------|
| accent purple | `#A855F7` | `#D100FF` (más neón/violeta) | 🔴 Alta |
| accent cyan | `#00B7FF` (más azulado) | `#00D9FF` (más verde/cian) | 🟡 Media |
| border color | `rgba(255,255,255,0.08)` | `#1E3050` (azul oscuro) | 🟡 Media |
| backgrounds | `#05080D` / `#091018` / `#0A1018` | `#03060A` / `#050A12` / `#07111B` | 🟢 Baja |
| success green | `#4CAF50` | `#7CFF00` (neón) | 🟢 Baja |
| warning yellow | `#FFC107` | `#FFD000` | 🟢 Baja |
| error red | `#FF5252` | `#FF3B3B` | 🟢 Baja |

### Layout y Componentes
| Componente | Referencia pide | Implementación actual | Prioridad |
|:-----------|:----------------|:---------------------|:----------|
| **Tab 1 layout** | Columna izquierda = MessengerList (~33%) / Columna derecha = MasterMeterPanel (~67%) | Columna izquierda ~38% = MasterMeter + Chat + Refs / Columna derecha ~62% = MessengerList | 🔴 Alta |
| **Messenger corner radius** | ~**30px** (esquinas muy redondeadas) | 6px (`fillGlassPanel`) | 🟡 Media |
| **Messenger bg pattern** | Ligero patrón vertical de líneas sutiles recorriendo toda la imagen | No implementado | 🟢 Baja |
| **Tab 2 — bottom-right** | **LUFSMeter** (Momentary, Short-Term, Integrated) | Vintage VU Meters (L/R/M/S analógicos) | 🟡 Media |
| **Vectorscope color** | **Dinámico**: oscila entre cyan-azul `#00B7FF` y violeta `#A855F7` según correlación de fase | Fijo: morado eléctrico `#D100FF` | 🟡 Media |
| **Spectrum Analyzer bins** | 512 bins FFT, paleta **verde→cyan** | 40 bandas logarítmicas, paleta cyan `#00D9FF` | 🟢 Baja |
| **Master Meter VU gradient** | Verde `#4CAF50` → Amarillo `#FFC107` → Rojo `#FF5252` | Verde `#7CFF00` → Amarillo `#FFD000` → Rojo `#FF3B3B` | 🟢 Baja |
| **Messenger título color** | `#A855F7` con gradiente violeta-magenta | `#D100FF` neón sin gradiente | 🟡 Media |

---

## 📐 ESTRUCTURA GLOBAL

### PluginEditor (navegación principal)
```
┌──────────────────────────────────────────────────────────────┐
│ [⚡ MIXCOACH]    [AI COACH | ANALYZERS]    [≡ ? ⚙]         │  ← Top Navigation Bar
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   TAB 1 CONTENT           o         TAB 2 CONTENT           │  ← Tab content area
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ FASE ACTUAL: 2 – ORGANIZACIÓN  |  POP  |  -14 LUFS  | 48kHz │  ← Bottom Status Bar
└──────────────────────────────────────────────────────────────┘
```

---

## 📐 TAB 1 — AI COACH (MixCoachPanel)

```
┌───────────────────────────────┬──────────────────────────────────────┐
│  SESIÓN 1 - CHAT CON IA      │  SESIÓN 3 - PISTAS DETECTADAS...    │
│                               │                                      │
│  [Avatar IA 🤖 metallic]     │  [TIPO] [COLOR] [BUS]  [▼] [▲]     │
│                               ├──────────────────────────────────────┤
│  ┌─────────────────────────┐  │  ┌──────────────────────────────────┐│
│  │ Burbuja IA (glassmorph) │  │  │ DRUMS BUS                    [▼]││
│  │ púrpura translúcido     │  │  │ ○ 🥁 Kick    ████████░ -6.2dB   ││
│  ├─────────────────────────┤  │  │ ○ 🥁 Snare   ██████░░ -9.8dB   ││
│  │ Burbuja usuario (cyan)  │  │  │ ...                            ││
│  │ cyan translúcido         │  │  ├──────────────────────────────────┤
│  └─────────────────────────┘  │  │ BASS BUS                     [▼]│
│                               │  │ ○ 🎸 Bass     ███████░ -8.1dB   ││
│  ┌─────────────────────────┐  │  └──────────────────────────────────┘│
│  │ [✏️ Escribe...]    [⇒] │  │                                      │
│  └─────────────────────────┘  │                                      │
├───────────────────────────────┤                                      │
│  SESIÓN 2 - REFERENCIAS      │                                      │
│  [REF. AUDIO | ENLACES]      │                                      │
│  ┌─────────────────────────┐  │                                      │
│  │ ------- ~~~~~ ---       │  │                                      │
│  │ Kick Ref               │  │                                      │
│  │ 48kHz - 24bit - 3:45   │  │                                      │
│  ├─────────────────────────┤  │                                      │
│  │ [📁 Arrastra archivos]  │  │                                      │
│  └─────────────────────────┘  │                                      │
└───────────────────────────────┴──────────────────────────────────────┘
```

### Componentes y archivos

| Componente | Archivo | Estado | Descripción |
|:-----------|:--------|:-------|:------------|
| **MixCoachPanel** | `CoachChatComponent.h/.cpp` | ✅ | Contenedor principal de Tab 1 (38% / 62% split) |
| **ChatMessagesComponent** | `CoachChatComponent.h/.cpp` | ✅ | Burbujas con glassmorphism: purple/cyan translúcido al 80%, borders, glow exterior, glass highlight 45% |
| **SendButton** | `CoachChatComponent.h/.cpp` | ✅ | Botón cuadrado con icono paper plane |
| **MasterMeterPanel** | `MasterMeterPanel.h/.cpp` | ✅ | VU L/R, correlación, LUFS compacto, text row |
| **ReferencePanelComponent** | `ReferencePanelComponent.h/.cpp` | ✅ | Sub-tabs, drop zone, file list |
| **MessengerListComponent** | `MessengerListComponent.h/.cpp` | ✅ | Track list con meters, grouping, AI suggestions |
| **TrackDashboardComponent** | `TrackDashboardComponent.h/.cpp` | 🔧 | Dashboard de métricas agregadas por bus con updateDashboard() — mantenimiento mínimo |

### Chat Burbujas — Especificación

| Propiedad | Burbuja IA | Burbuja Usuario |
|:----------|:-----------|:----------------|
| **Fondo** | Púrpura oscuro translúcido `#1A0A2E` al 80% | Cyan oscuro translúcido `#0A2A3A` al 80% |
| **Borde** | Morado `#8B5CF6` al 20% alpha | Cyan `#00D9FF` al 18% alpha |
| **Glow exterior** | Purple glow backdrop al 6% | Cyan glow backdrop al 5% |
| **Glass highlight** | Gradiente blanco→transparente en 45% superior | Igual |
| **Alineación** | Izquierda | Derecha |

### SendButton — Especificación
- **Forma:** Cuadrado con esquinas redondeadas (4px corner radius)
- **Tamaño:** 28×28px
- **Icono:** Avión de papel (paper plane)
- **Hover glow:** Morado expansivo
- **Color:** Fondo morado, icono blanco

---

## 📐 TAB 2 — ANALYZERS (AnalyzersPanelComponent)

```
┌──────────────────────┬──────────────────────────────────────────────────┐
│  SESIÓN 2 - METER   │  SESIÓN 3 - RTA SPECTRUM                        │
│  (28% W, 50% H)      │  (72% W, 50% H)                                 │
│                      │                                                  │
│  ┌────┬──────┬────┐ │  ┌──────────────────────────────────────────────┐│
│  │ VU │CARDS │LUFS│ │  │  0dB ┤                                     ││
│  │ L  │PEAK  │ L  │ │  │      ┤  ██  ████  ████████  ██████         ││
│  │ ██ │RMS   │ L  │ │  │ -45dB ┤────────────────────────             ││
│  │ ██ │LUFS  │ R  │ │  │      20Hz    200Hz   2kHz     20kHz         ││
│  │ R  │DR    │ R  │ │  └──────────────────────────────────────────────┘│
│  └────┴──────┴────┘ │                                                  │
├──────────────────────┴──────────────────────────────────────────────────┤
│  SESIÓN 5 - PHASE SCOPE     │  SESIÓN 4 - VU METERS                    │
│  (48% W, 50% H)              │  (52% W, 50% H)                          │
│                              │                                          │
│  ┌────────────────────────┐  │  ┌──────────────┬──────────────┐        │
│  │ [-1───0───+1]   +0.92 │  │  │  ┌────────┐  │  ┌────────┐  │        │
│  │ ┌────────────────────┐ │  │  │  │ ╭────╮ │  │  │ ╭────╮ │  │        │
│  │ │      ╭───╮        │ │  │  │  │ ╰────╯ │  │  │ ╰────╯ │  │        │
│  │ │     ╱     ╲       │ │  │  │  │   L    │  │  │   R    │  │        │
│  │ │    (  ⬤   ) M    │ │  │  ├────────────┼──┼────────────┤  │        │
│  │ │     ╲     ╱ L▶R  │ │  │  │  ┌────────┐│  │┌────────┐  │        │
│  │ │      ╰───╯        │ │  │  │  │ ╭────╮ ││  │╭────╮ │  │        │
│  │ └────────────────────┘ │  │  │  │ ╰────╯ ││  │╰────╯ │  │        │
│  │            CREST  8.2dB│  │  │  │   M    ││  │   S    │  │        │
│  │ ┌────────┬───────────┐ │  │  └──────────────┴──────────────┘        │
│  │ │PEAK    │RMS  |CREST│ │  │                                          │
│  │ │-1.2dB  │-9.4dB|8.2 │ │  │                                          │
│  │ └────────┴───────────┘ │  │                                          │
│  └────────────────────────┘  │                                          │
├──────────────────────────────┴──────────────────────────────────────────┤
│  STATUS BAR: ⚡ MIXCOACH | ANALYZERS | GENRE: POP | TARGET: -14 LUFS | 48.0 kHz │
└──────────────────────────────────────────────────────────────────────────┘
```

### Componentes y archivos

| Componente | Archivo | Estado | Descripción |
|:-----------|:--------|:-------|:------------|
| **AnalyzersPanelComponent** | `AnalyzersPanelComponent.h/.cpp` | ✅ | Contenedor 2×2 grid + status bar |
| **MeterPanel (SESIÓN 2)** | `AnalyzersPanelComponent.h/.cpp` | ✅ | VU L/R + 4 metric cards + LUFS dual |
| **SpectrographComponent (SESIÓN 3)** | `SpectrographComponent.h/.cpp` | ✅ | RTA 40 bandas + Waterfall 3D (80 slices, fade quadrático, ~15fps) — frecuencias Hz en accentCyan #00D9FF 9px bold con strip oscuro + tick marks; dB axis en accentCyan #00D9FF 8px bold, ambos fuera del static cache |
| **VintageVUMeters (SESIÓN 4)** | `AnalyzersPanelComponent.h/.cpp` | ✅ | 2×2 vintage analog cream VU meters |
| **PhaseScopePanel (SESIÓN 5)** | `AnalyzersPanelComponent.h/.cpp` | ✅ | Correlation meter (diamante + zones) + VectorscopeComponent + Crest gauge semicircular con needle + metrics table PEAK/RMS/CREST |
| **VectorscopeComponent** | `VectorscopeComponent.h/.cpp` | ✅ | Circular scope con phosphor trail purple `#D100FF`, grid labels accentCyan 8px bold |
| **MeterCard** | `AnalyzersPanelComponent.h/.cpp` | ✅ | PEAK/RMS/LUFS/DR mini cards |
| **Status Bar** | `AnalyzersPanelComponent.cpp` | ✅ | Footer con info de sesión |

### VU Meter (SESIÓN 2) — Especificación
- **Gradiente:** 🟢 Verde `#7CFF00` (abajo) → 🟡 Amarillo `#FFD000` (30%) → 🔴 Rojo `#FF3B3B` (top)
- **Escala externa:** Izquierda, 0dB a -60dB en pasos de 6
- **Digital readout:** Naranja `#F97316` abajo
- **Peak marker:** Línea blanca brillante con glow
- **LUFS bars:** Cyan `#00D9FF` / `#33CFFF` con target triangles

### Vectorscope (SESIÓN 5) — Especificación
- **Color trace:** Morado eléctrico `#D100FF` (NO cyan)
- **Phosphor decay:** 0.965/frame, 512 puntos
- **Grid:** 4 círculos concéntricos + crosshairs + diagonales 45° + labels M/L/R/S en **accentCyan #00D9FF 8px bold**
- **Correlation meter:** Barra horizontal con gradiente rojo→verde, diamante (+/- value), zone markers **textDim 7.5px bold**
- **Crest gauge:** Semicircular con needle, segmentos de color, scale marks **textDim 7.5px bold**, título **8px bold success**
- **Metrics table:** PEAK/RMS/CREST labels **7.5px bold** alpha 75%

### Vintage VU Meters (SESIÓN 4) — Especificación
- **Fondo:** Crema cálido `#DCCB9E`
- **Aguja:** Negra `#1A1A0A` con highlight sutil
- **Zona roja:** Extremo derecho del arco `#CC3333` al 60%
- **Marco:** Café oscuro `#2A1A0A` con inner bevel
- **Glass overlay:** Gradiente blanco→transparente al 10% alpha, 40% superior
- **Labels:** L, R, M, S en café oscuro
- **Scale:** -20, -10, -5, 0, +3, +6 (vintage VU style)

### Spectrograph (SESIÓN 3) — Especificación
- **Color barras:** Cyan `#00D9FF` con glow
- **Grid:** Líneas cyan oscuro, opacidad baja
- **Frecuencias:** 20Hz → 20kHz, escala logarítmica
- **Rango dB:** 0dB a -45dB
- **Etiqueta:** "RTA" + icono settings arriba derecha
- **NO arcade — científico, preciso**
- **Waterfall 3D**: 80 slices de historial, fade quadrático alpha 0.55→0.02, ~15fps, buffer circular. Optimizado con precálculo de bandRects (99% menos log2 por frame)
- **Freq labels**: accentCyan #00D9FF al 90%, 9px bold, strip oscuro de contraste #050A12 al 70%, tick marks verticales, dibujados en paint() después de barras + waterfall
- **dB axis labels**: accentCyan #00D9FF al 80%, 8px bold, label height 11px, dibujados en paint() (fuera del static cache) para visibilidad máxima

---

## 📐 MESSENGER — Track Inspector

```
┌──────────────────────────────────────┐
│  INFORMACIÓN DE PISTA                │  ← Título #D100FF, uppercase, glow
├──────────────────────────────────────┤
│  ✏️  NOMBRE    [Kick              ] │  ← Icono lápiz en círculo translúcido
├──────────────────────────────────────┤
│  🎨  COLOR     [● ▾              ] │  ← Icono paleta, dropdown con preview
├──────────────────────────────────────┤
│  📦  TIPO      [Kick ▾           ] │  ← Icono caja, dropdown minimalista
├──────────────────────────────────────┤
│  ➡️  RUTEO     [Drums Bus ▾      ] │  ← Icono flecha, accent morado en valor
└──────────────────────────────────────┘
```

### Componentes y archivos

| Componente | Archivo | Estado | Descripción |
|:-----------|:--------|:-------|:------------|
| **MessengerAudioProcessorEditor** | `Source/Messenger/ui/PluginEditor.cpp` | ✅ | Glass panel 6px radius + border glow cyan + título INFORMACIÓN DE PISTA |
| **Name input** | `Source/Messenger/ui/PluginEditor.cpp` | ✅ | TextEditor con bg translúcido + focus glow animado SmoothValue |
| **Color picker** | `Source/Messenger/ui/PluginEditor.cpp` | ✅ | Dropdown PopupMenu con preview circle + hover glow animado |
| **Tipo ComboBox** | `Source/Messenger/ui/PluginEditor.cpp` | ✅ | Dropdown tipo con icono caja |
| **Ruteo ComboBox** | `Source/Messenger/ui/PluginEditor.cpp` | ✅ | Dropdown ruteo con accent morado + icono flecha |

### Messenger — Especificación
- **Header:** `"INFORMACIÓN DE PISTA"` — uppercase, tracking amplio, `#D100FF`, glow leve + LED dot pulsing suave
- **Iconos:** ✏️ 🎨 📦 ➡️ en círculos translúcidos con borde tenue (✅ IMPLEMENTADOS — hover animation purple via SmoothValue)
- **Divisores:** 4 líneas sutiles entre cada fila (✅ IMPLEMENTADOS)
- **Inputs:** Fondo negro translúcido, borde gris tenue, glow en focus (✅ animado con SmoothValue 60fps)
- **Color picker:** Dropdown compacto PopupMenu con preview circle + hover glow animado (✅ IMPLEMENTADO)
- **Espaciado:** Amplio, premium, respirable (✅ APLICADO)
- **Panel:** Vidrio oscuro con borde redondeado 6px, outer glow cyan, glass highlight top 25% (✅ IMPLEMENTADO)
- **Animaciones hover/focus:** SmoothValue con timer 60fps, attack 80-200ms, release 250-400ms (✅ IMPLEMENTADAS)

---

## 📋 ESTADO DE IMPLEMENTACIÓN — CHECKLIST GLOBAL

### MESSENGER
| Ítem | Estado | Notas |
|:-----|:-------|:------|
| Panel flotante centrado + glass panel | ✅ | Rounded 6px + border glow cyan + glass highlight |
| Título "INFORMACIÓN DE PISTA" | ✅ | Purple neon `#D100FF` + uppercase + tracking |
| Iconos ✏️ 🎨 📦 ➡️ en círculos | ✅ | 4 line icons con hover animation (fade to purple) |
| Input nombre con focus glow | ✅ | SmoothValue animación 200ms attack / 300ms release |
| Color picker dropdown | ✅ | PopupMenu 8 colores + preview circle + hover glow |
| Divisores entre filas | ✅ | 4 líneas sutiles `#1A2A3E` alpha 0.5 |
| Espaciado premium | ✅ | 4px gaps, 28px rows, 8px padding |
| Animaciones hover/focus + LED pulse | ✅ | SmoothValue + timer 60fps (attack 80-200ms, release 250-400ms) |

### TAB 1 — AI COACH
| Ítem | Estado | Notas |
|:-----|:-------|:------|
| Burbuja IA glassmorphism | ✅ | Púrpura translúcido con glass highlight |
| Burbuja usuario cyan translúcido | ✅ | Glassmorphism aplicado |
| SendButton cuadrado | ✅ | 4px radius, paper plane |
| Avatar IA metálico | ✅ | Robot Path-drawing: cabeza metallic gradient + ojos cyan `#00D9FF` + antena glow |
| ReferencePanel sub-tabs | ✅ | Audio refs + enlaces |
| Upload area dashed | ✅ | Con botón EXPLORAR |
| Track list con meters | ✅ | MessengerListComponent completo |
| Grouping TIPO/COLOR/BUS | ✅ | Chips con cyan glow |
| AI suggestions por track | ✅ | Texto contextual |
| MasterMeterPanel (VU + LUFS) | ✅ | Peak glow con SmoothValue + clip monitor + text row |

### TAB 2 — ANALYZERS
| Ítem | Estado | Notas |
|:-----|:-------|:------|
| MeterPanel VU L/R gradiente | ✅ | Verde→amarillo→rojo |
| Metric cards (PEAK/RMS/LUFS/DR) | ✅ | Con colores accent |
| LUFS dual L/R | ✅ | Cyan `#00D9FF` con target triangles |
| Spectrograph RTA | ✅ | 40 bandas log, grid cyan-tinted, glow en barras, peak markers #88F7FF, gradiente #00D9FF→#33CFFF, **Waterfall 3D** 80 slices, frecuencias Hz visibles (accentCyan 9px bold + strip oscuro), dB axis visibles (accentCyan 8px bold, fuera del static cache) |
| PhaseScope correlation meter | ✅ | Barra con diamante, zone markers textDim 7.5px bold |
| Vectorscope trace color | ✅ | Morado eléctrico `#D100FF` (corregido) |
| Vectorscope/Crest proporción | ✅ | 75/25 (corregido) |
| Crest gauge needle | ✅ | Gris oscuro `#3A3A3A` con highlight metálico (corregido) |
| Vectorscope grid labels | ✅ | accentCyan #00D9FF 8px bold (antes 6.5px textMuted 35%) |
| Crest gauge scale labels | ✅ | textDim #AAB4C0 7.5px bold (antes 5.0px textMuted 50%) |
| Correlation zone markers | ✅ | textDim #AAB4C0 7.5px bold (antes 6.0px textMuted 35%) |
| Metrics table labels | ✅ | 7.5px bold alpha 75% (antes 6.5px alpha 65%) |
| Vintage VU cream bg | ✅ | `#DCCB9E`, glass overlay 35% sharpen |
| Vintage VU black needle | ✅ | `#1A1A0A`, scale -20/+6 corregido (bug: marks crampeadas en 28% del arco) |
| Vintage VU glass overlay | ✅ | White 0.14→0.02 gradient, 35% height, minor tick marks añadidos |
| Status bar footer | ✅ | Logo + info técnica |
| Dot grid background | ✅ | iZotope/NUGEN style |

### PERFORMANCE AUDIT & BUGFIXES
| Ítem | Estado | Notas |
|:-----|:-------|:------|
| PhaseCorrelationSystem timer | ✅ | Corregido: `startTimerHz(30)` faltante en constructor (no arrancaba si visible desde inicio) |
| MeterPanel SmoothValues | ✅ | Corregido: 4 SV nunca avanzados (VU bars congeladas). Nueva `advanceVisuals()` + llamado desde `smoothVisuals()` |
| MasterMeterPanel Unicode | ✅ | Corregido: ⚡ mostraba texto literal por doble backslash |
| Waterfall band precalc | ✅ | Optimizado: 9600→80 `freqToX()` calls por frame (99% menos log2) |
| Background cache (gradient + dot grid) | ✅ | Image cache en AnalyzersPanelComponent: gradient + dot grid dibujados una vez, rebuild en resize |
| Child-level repaint | ✅ | smoothVisuals() ahora repinta cada hijo individualmente, no el padre completo |
| Waterfall 80 rows (antes 120) | ✅ | 33% menos fillRect en drawWaterfall |
| Vectorscope phosphor 1500 (antes 3000) | ✅ | 50% menos elipses en phosphor trail |
| VectorscopeSystem grid cache | ✅ | ProfessionalAnalyzers: grid + background cacheados en Image (rebuild en resize), solo phosphor + correlation se redibujan por frame |
| VectorscopeSystem phosphor Path | ✅ | 1500 fillEllipse individuales → 3 Path batches (glow/trail/bright) — ~99% menos draw calls en phosphor |
| VectorscopeComponent grid labels | ✅ | M/L/R/S: 6.5px textMuted 35% → 8.0px bold accentCyan 70% |
| PhaseScopePanel crest gauge | ✅ | Scale labels 5.0px→7.5px bold textDim, title 6.5px→8.0px bold |
| PhaseScopePanel correlation markers | ✅ | Zone markers (-1, 0, +1) 6.0px→7.5px bold textDim, label widths +2px |
| PhaseScopePanel metrics table | ✅ | PEAK/RMS/CREST labels 6.5px→7.5px bold, alpha 65%→75% |
| Freq labels visibles | ✅ | Movidos de staticCache a paint() post-barras; accentCyan #00D9FF 9px bold + strip oscuro #050A12 + tick marks |
| dB axis labels visibles | ✅ | Movidos de staticCache a paint(); accentCyan #00D9FF 8px bold, label height 11px, matching estilo freq |
| Tests unitarios | ✅ | 11 suites, ~5190 tests, **0 fallos** — todos pasan |

### TEMA (MixCoachTheme)
| Ítem | Estado | Notas |
|:-----|:-------|:------|
| Colors actualizados con referencia | ✅ | `#D100FF` / `#00D9FF` / `#7CFF00` / `#FFD000` / `#FF3B3B` — todo alineado |
| truePeak() corregido | ✅ | Ahora usa `error()` (`#FF3B3B`) en vez de old `#EF4444` |
| MessengerListComponent colores locales | ✅ | Reemplazados por MixCoachTheme |
| ReferencePanelComponent hardcoded colors | ✅ | Reemplazados por MixCoachTheme::accent/accentCyan/meterGreen |

---

## ⚙️ IMPLEMENTACIÓN — REGLAS PARA LA IA

### 1. Siempre verificar antes de editar
- Leer el archivo fuente completo antes de hacer cambios
- Verificar que los imports existan
- No romper el layout de otros componentes

### 2. Usar MixCoachTheme para colores
- **NUNCA** hardcodear colores
- Si necesitas un color que no existe en `MixCoachTheme.h`, AÑÁDELO al theme
- Usar `withAlpha()` para variantes de transparencia

### 3. Patrón de renderizado
- Separar capas estáticas (grid, fondo) de dinámicas (meters, traces)
- Cachear lo estático en `juce::Image` (ver `VectorscopeComponent::gridCache_`)
- Usar `SmoothValue` para animaciones suaves (attack/release)
- Llamar `repaint()` solo cuando hay cambios visibles

### 4. Glass panel pattern (usar `MixCoachTheme::fillGlassPanel()`)
```cpp
MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);
// Esto dibuja: shadow externo + fondo oscuro + glass highlight + borde
```

### 5. Layout
- Preferir `removeFromLeft()` / `removeFromTop()` para dividir bounds
- Guardar rectángulos intermedios en variables (NO descartarlos)
- Usar proporciones relativas (porcentajes), no pixeles fijos
- Mantener padding interno amplio (3-6px)

### 6. Animaciones
- SmoothValue: attack rápido (1-5), release medio (100-250)
- Meters: Decay natural, no jitter
- Hover: Iluminación leve, elevación mínima
- High FPS (60fps), ultra fluido

---

## 🔗 ARCHIVOS CLAVE DEL PROYECTO

```
MixCoach/
├── Source/
│   ├── Common/
│   │   ├── memory/
│   │   │   ├── SlotRegistry.h/.cpp     ← Registro de slots de pistas
│   │   │   └── SharedData.h/.cpp       ← Datos compartidos IPC
│   │   ├── types/
│   │   │   ├── Types.h                 ← TrackAudioResult, Telemetry, etc.
│   │   │   └── Constants.h             ← Constantes globales
│   │   └── audio/
│   │       └── AudioAnalysis.h/.cpp    ← Estructuras de análisis
│   ├── Messenger/
│   │   ├── core/
│   │   │   ├── PluginProcessor.h/.cpp  ← DSP del Messenger
│   │   │   └── MessengerType.h         ← Tipos del Messenger
│   │   └── ui/
│   │       └── PluginEditor.h/.cpp     ← UI del Messenger (track inspector)
│   └── MixCoach/
│       ├── audio/
│       │   ├── AudioAnalyzer.h/.cpp    ← Analizador de audio maestro
│       │   └── ReferenceAnalyzer.h/.cpp ← Análisis de referencias
│       ├── core/
│       │   ├── PluginProcessor.h/.cpp  ← DSP del MixCoach
│       │   └── PluginEditor.h/.cpp     ← Editor principal (tabs + navegación)
│       ├── engine/
│       │   ├── CoachEngine.h/.cpp      ← Lógica de IA / sugerencias
│       │   └── PhaseManager.h/.cpp     ← Gestión de fases
│       └── UI/
│           ├── MixCoachTheme.h         ← 🎨 TEMA VISUAL (colores, efectos)
│           ├── SmoothValue.h/.cpp      ← Interpolación suave
│           ├── CoachChatComponent.h/.cpp    ← Panel AI Coach (Tab 1)
│           ├── MasterMeterPanel.h/.cpp      ← Master meters Tab 1
│           ├── MessengerListComponent.h/.cpp ← Track list con meters
│           ├── ReferencePanelComponent.h/.cpp ← Referencias
│           ├── AnalyzersPanelComponent.h/.cpp  ← Analyzers (Tab 2)
│           ├── SpectrographComponent.h/.cpp    ← RTA spectrum
│           ├── VectorscopeComponent.h/.cpp     ← Vectorscope circular
│           └── ... otros componentes
├── CMakeLists.txt                    ← Build system
└── workspace_memory/
    ├── MASTER_VISION.md              ← 🔥 ESTE DOCUMENTO
    ├── referencia_visual_messenger.md
    ├── referencia_visual_tab1_ai_coach.md
    └── referencia_visual_tab2_analyzers.md
```
