# 🎨 Guía de Referencia Visual — MIXCOACH TAB 2: METERING

> Basada en la implementación actual del código (`AnalyzersPanelComponent`)
> Fecha de actualización: Junio 2026

---

## 🏛️ Objetivo General

Sección de metering profesional del plugin MixCoach. Proporciona herramientas visuales de medición para evaluar el estado técnico de la mezcla master.

**Nota:** En `MainTabbedComponent` esta es la **Tab 1** (`📊 Metering`). La **Tab 2** (`✦ System`) contiene el `ProfessionalAnalyzersComponent` (VectorscopeSystem + PhaseCorrelationSystem como herramientas separadas).

---

## 🎨 Paleta de Colores

| Elemento | Color | Uso |
|----------|-------|-----|
| Fondo principal | `#05080D` | Canvas principal (dot grid sutil) |
| Paneles | `#0A1018` | Glass panels internos |
| Bordes | `rgba(255,255,255,0.08)` | Bordes sutiles |
| **Acento principal** | **`#A855F7`** | Morado — headers, branding, glow |
| Acento secundario | `#00B7FF` | Cyan — spectrum, meters técnicos |
| Advertencia | `#FFC107` | Amarillo |
| Error | `#FF5252` | Rojo — clipping, out of phase |
| Correcto / Saludable | `#4CAF50` | Verde — in phase, señal sana |
| Medidor PEAK | `#FFD000` | Amarillo cálido |
| Medidor RMS | `#00B7FF` | Cyan |
| LUFS Momentary | `#A855F7` | Morado |
| LUFS Short Term | `#00D9FF` | Cyan brillante |
| LUFS Integrated | `#7CFF00` | Verde lima |
| VU vintage fondo | `#F0DFB0` | Crema clásico |
| VU aguja | `#CC2222` | Rojo Neve |
| Crest Factor bajo | `#4CAF50` | Verde |
| Crest Factor medio | `#FFC107` | Amarillo |
| Crest Factor alto | `#FF5252` | Rojo |

---

## 📐 Layout Global (3 columnas arriba, 3 abajo)

```
┌─────────────────────────────────────────────────────────────────────┐
│ ╔══════════════════╗ ╔═════════════════════════════════════════════╗ │
│ ║  SESIÓN 2        ║ ║  SESIÓN 3                                  ║ │
│ ║  METER (28%W)    ║ ║  SPECTROGRAPH (72%W, 50%H)                 ║ │
│ ║                  ║ ║                                              ║ │
│ ║ ┌──┐ ┌──┐       ║ ║  FFT RTA — 512 bins                         ║ │
│ ║ │L │ │R │       ║ ║  20Hz ─────── 20kHz (log)                   ║ │
│ ║ └──┘ └──┘       ║ ║  Green → Cyan gradient                      ║ │
│ ║ PEAK  RMS       ║ ║  Smoothing 30fps                             ║ │
│ ║ LUFS   DR       ║ ║                                              ║ │
│ ║ ┌─┐┌─┐┌─┐     ║ ║                                              ║ │
│ ║ │M││S││I│ LUFS ║ ║                                              ║ │
│ ╚══════════════════╝ ╚═════════════════════════════════════════════╝ │
│ ───────────────────────────────────────────────────────────────────── │
│ ╔══════════════════╗ ╔════════════════╗ ╔══════════════════════════╗ │
│ ║  SESIÓN 5        ║ ║  SESIÓN 6      ║ ║  SESIÓN 4               ║ │
│ ║  PHASE SCOPE     ║ ║  CREST FACTOR  ║ ║  VU METERS              ║ │
│ ║  (28%W, 42%H)    ║ ║  (25%W, 42%H)  ║ ║  (47%W, 42%H)          ║ │
│ ║                  ║ ║                ║ ║                          ║ │
│ ║ Corr: -1──0──+1  ║ ║   ╭──────╮    ║ ║  ┌────────┐ ┌────────┐  ║ │
│ ║  ♦ +0.85         ║ ║  ╱ 12.5  ╲   ║ ║  │ ╭──╮  │ │ ╭──╮  │  ║ │
│ ║                  ║ ║  ╲      ╱   ║ ║  │ ╰──╯  │ │ ╰──╯  │  ║ │
│ ║  [Vectorscope]   ║ ║   ╰──────╯    ║ ║  │  L   │ │  R   │  ║ │
│ ║  Lissajous       ║ ║  PEAK RMS CRST║ ║  ├────────┼────────┤  ║ │
│ ║                  ║ ║                ║ ║  │ ╭──╮  │ │ ╭──╮  │  ║ │
│ ║  PEAK | RMS      ║ ║                ║ ║  │ ╰──╯  │ │ ╰──╯  │  ║ │
│ ╚══════════════════╝ ╚════════════════╝ ║  │  M   │ │  S   │  ║ │
│                                          ║  └────────┴────────┘  ║ │
│                                          ╚══════════════════════════╝ │
│ ──────────────────────────────────────────────────────────────────── │
│ ║ STATUS BAR: ⚡ MIXCOACH | ANALYZERS | POP | -14 LUFS | 48.0 kHz  ║ │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🧩 SESIÓN 2 — METER (MeterPanel)

**Ubicación**: Top-left, 28% width × 50% height.

### Componentes internos

```
┌─────────────────────────────┐
│ SESIÓN 2 - METER           │ ← Header morado con underline glow
├─────────────────────────────┤
│ ┌──┐    ┌──┐               │
│ │L │    │R │               │ ← VU L/R vertical bars
│ │  │    │  │               │    Gradient green→yellow→red
│ │  │    │  │               │    Peak white line marker
│ └──┘    └──┘               │    Digital value + channel label
│                             │
│ ┌──────┐ ┌──────┐          │
│ │PEAK  │ │RMS   │          │ ← Metric cards (dark bg, border glow)
│ │ -8.2 │ │-14.5 │          │    Big number + unit
│ │  dB  │ │  dB  │          │
│ └──────┘ └──────┘          │
│ ┌──────┐ ┌──────┐          │
│ │LUFS  │ │DR    │          │
│ │-12.0 │ │ 8.2  │          │
│ │  LU  │ │  dB  │          │
│ └──────┘ └──────┘          │
│                             │
│ ┌─┐┌─┐┌─┐                  │
│ │M││S││I│  LUFS            │ ← 3 vertical bars
│ └─┘└─┘└─┘                  │    MOM / ST / INT
│                             │    Target -14 LUFS line
│                             │    Dynamic color (red if > -5 LUFS)
└─────────────────────────────┘
```

### Especificación de barras VU L/R

| Propiedad | Valor |
|-----------|-------|
| Dirección | **Vertical** |
| Gradiente | Verde (#4CAF50) → Amarillo (#FFC107) → Rojo (#FF5252) |
| Peak marker | Línea blanca brillante con glow |
| Escala dB externa | -60 a 0 en pasos de 6, a la izquierda |
| Etiqueta canal | L/R abajo-izquierda |
| Valor digital | Naranja (#F97316) abajo-derecha |
| Ancho | 34% del MeterPanel |

### Especificación tarjetas métricas

| Card | Label | Color acento | Format |
|------|-------|-------------|--------|
| PEAK | Valor pico | Amarillo `#FFD000` | X.X dB |
| RMS | Valor RMS | Cyan `#00B7FF` | X.X dB |
| LUFS (I) | LUFS integrado | Morado `#A855F7` | X.X LU |
| DR | Dynamic Range (Peak-RMS) | Verde `#4CAF50` | X.X dB |

### Especificación barras LUFS (M, S, I)

| Barra | Label | Color | Descripción |
|-------|-------|-------|-------------|
| M | MOM | Morado `#A855F7` | LUFS Momentary |
| S | ST | Cyan `#00D9FF` | LUFS Short Term |
| I | INT | Verde lima `#7CFF00` | LUFS Integrated |

- Target -14 LUFS con línea de referencia blanca tenue
- Color dinámico: rojo si > -5 LUFS, amarillo si > -10 LUFS

---

## 📈 SESIÓN 3 — SPECTROGRAPH (SpectrographComponent)

**Ubicación**: Top-right, 72% width × 50% height.

### Especificación

| Propiedad | Valor |
|-----------|-------|
| Tipo | FFT en tiempo real (desde `AudioAnalyzer`) |
| Bins | `kNumSpectrumBins` (definido en Constants.h) |
| Eje X | Frecuencia (20 Hz → 20 kHz) |
| Eje Y | Nivel (dB) |
| Escala | **Logarítmica** |
| Paleta de colores | Verde → Cyan (bajo nivel = verde, pico = cyan) |
| Smoothing | 30fps con decaimiento visual |
| Borde del panel | Cyan `#00B7FF` sutil |

---

## 🎚️ SESIÓN 4 — VU METERS (VintageVUMeters)

**Ubicación**: Bottom-right, 47% width × 42% height.

### Especificación

| Propiedad | Valor |
|-----------|-------|
| Canales | **Left**, **Right**, **Mid**, **Side** (4 medidores en grid 2×2) |
| Estilo | **Analógico vintage** (Neve-style) |
| Fondo | Crema clásico `#F0DFB0` con gradiente |
| Marco | Metálico oscuro con bordes 3D |
| Aguja | **Roja** `#CC2222` con sombra + highlight |
| Pivote | Dorado `#AA8A4A` con centro rojo |
| Zona roja | Arco rojo `#DD2222` en extremo derecho |
| Tornillos | Esquineros decorativos |
| Cristal | Reflection overlay (30% height) |
| Peak hold | Marcador diamante rojo con decaimiento (1.5s hold + 30dB/sec) |
| Escala VU | -20 → +5 (no lineal: 0 VU ≈ 55% del arco) |
| Marcas | -20, -10, -5, 0, +3, +5 |

### Layout 2×2

```
┌────────────────┐ ┌────────────────┐
│  ╭──────────╮  │ │  ╭──────────╮  │
│  │  VU Arc  │  │ │  │  VU Arc  │  │
│  ╰──────────╯  │ │  ╰──────────╯  │
│       L        │ │       R        │
├────────────────┤ ├────────────────┤
│  ╭──────────╮  │ │  ╭──────────╮  │
│  │  VU Arc  │  │ │  │  VU Arc  │  │
│  ╰──────────╯  │ │  ╰──────────╯  │
│       M        │ │       S        │
└────────────────┘ └────────────────┘
```

### Derivación Mid/Side
- M = (L + R) * 0.5
- S = (L - R) * 0.5

---

## 🔄 SESIÓN 5 — PHASE SCOPE (PhaseScopePanel)

**Ubicación**: Bottom-left, 28% width × 42% height.

### Componentes

#### Correlation Meter

| Propiedad | Valor |
|-----------|-------|
| Escala | -1 → 0 → +1 |
| Representación | Barra horizontal con gradiente por zonas |
| Marcadores | -1, 0, +1 con línea central |
| Indicador | **Diamante brillante** con glow |
| Color | Rojo (< -0.3) → Amarillo (< 0.3) → Verde (> 0.3) |
| Valor digital | Texto grande a la derecha (ej: "+0.85") |

#### Vectorscope (VectorscopeComponent)

| Propiedad | Valor |
|-----------|-------|
| Tipo | Visualizador polar (Lissajous) — L en X, R en Y |
| Fondo | Radial gradient (radar style) |
| Grid | Círculos concéntricos + crosshairs + diagonales 45° |
| Traza | Phosphor trail con decaimiento |
| Color traza | Dinámico según correlación |
| Ejes | L (izquierda), R (derecha), R (arriba), L (abajo) |
| Marcas tick | Cada 30° en círculo externo |

#### Metrics Row

| Métrica | Color |
|---------|-------|
| PEAK | Amarillo `#FFD000` |
| RMS | Cyan `#00B7FF` |

---

## 📐 SESIÓN 6 — CREST FACTOR (CrestPanel)

**Ubicación**: Bottom-center, 25% width × 42% height.

### Especificación

| Propiedad | Valor |
|-----------|-------|
| Tipo | Medidor **semicircular** con aguja |
| Rango | 0 → 24 dB (kMaxCrest) |
| Arco fondo | Gris tenue |
| Arco activo | Gradiente dinámico según nivel |
| Aguja | Gris oscuro con shadow + highlight |
| Pivote | Blanco con centro brillante |
| Valor central | Texto grande "X.X dB" |

### Colores del arco por nivel

| Rango | Color |
|-------|-------|
| 0 - 30% (0-7.2 dB) | Verde `#4CAF50` |
| 30% - 50% (7.2-12 dB) | Lima |
| 50% - 70% (12-16.8 dB) | Amarillo `#FFC107` |
| 70% - 90% (16.8-21.6 dB) | Naranja |
| 90% - 100% (21.6-24 dB) | Rojo `#FF5252` |

### Metrics Row (3 columnas)

| Métrica | Color |
|---------|-------|
| PEAK | Amarillo `#FFD000` |
| RMS | Cyan `#00B7FF` |
| CREST | Verde `#4CAF50` |

---

## 📋 STATUS BAR

**Ubicación**: Bottom, 100% width × 8% height (mínimo 38px).

### Elementos (de izquierda a derecha)

| Elemento | Ejemplo | Color |
|----------|---------|-------|
| Logo | `⚡ MIXCOACH` | Morado `#A855F7` |
| Separador | Línea vertical | Tenue |
| Modo | `ANALYZERS` | Cyan neón `#00D9FF` |
| Separador | Línea vertical | Tenue |
| Género label | `GENRE` | Texto muted |
| Género valor | `POP` | Texto primary |
| Separador | Línea vertical | Tenue |
| Target label | `TARGET` | Texto muted |
| Target valor | `-14 LUFS` | Cyan `#00B7FF` |
| Separador | Línea vertical | Tenue |
| SR label | `SR` | Texto muted |
| SR valor | `48.0 kHz` | Texto dim |
| Settings icons | `≡ ? ⚙` | Texto muted |

---

## ✨ Estilo Visual General

### Características
- **Dark mode premium** — fondo `#05080D`
- **Dot grid sutil** `rgba(255,255,255,0.01)` cada 32px (cached en imagen, no repinta cada frame)
- **Paneles `#0A1018`** con glassmorphism ligero
- **Bordes sutiles** `rgba(255,255,255,0.08)`
- **Esquinas redondeadas** (4-6px radius)
- **Neón púrpura** `#A855F7` como color principal
- **Acento cyan** `#00B7FF` para borders técnicos
- **Headers** con texto morado + underline gradient glow
- **SmoothValues** para todas las animaciones (attack/release configurables)

### Optimizaciones de rendering
- Background con dot grid se cachea en `juce::Image` y solo se repinta en resize
- Cada sub-componente se repinta a sí mismo sin repintar el padre
- SmoothValues avanzan a 60fps desde `smoothVisuals()` en `MainTabbedComponent`

---

## 🗺️ Mapeo de Componentes a Código

| Sesión | Clase C++ | Archivo | Componentes |
|--------|-----------|---------|-------------|
| Header global | `MainTabbedComponent` | `MainTabbedComponent.h/.cpp` | 3 tabs (Mix Coach, Metering, System) |
| **SESIÓN 2** | `MeterPanel` | `AnalyzersPanelComponent.h/.cpp` | VU L/R, 4 metric cards, LUFS bars |
| **SESIÓN 3** | `SpectrographComponent` | `SpectrographComponent.h/.cpp` | FFT RTA spectrum analyzer |
| **SESIÓN 4** | `VintageVUMeters` | `AnalyzersPanelComponent.h/.cpp` | 4 vintage VU L/R/M/S |
| **SESIÓN 5** | `PhaseScopePanel` | `AnalyzersPanelComponent.h/.cpp` | Correlation + Vectorscope + PEAK/RMS |
| **SESIÓN 6** | `CrestPanel` | `CrestPanel.h/.cpp` | Crest gauge semicircular + metrics |
| **STATUS BAR** | `AnalyzersPanelComponent::paint()` | `AnalyzersPanelComponent.cpp` | Footer con info de sesión |
| **Tab 3 (System)** | `ProfessionalAnalyzersComponent` | `ProfessionalAnalyzersComponent.h/.cpp` | VectorscopeSystem + PhaseCorrelationSystem |

---

## ⚠️ PRINCIPIO FUNDAMENTAL

> **Reutilizar de la implementación actual como definitiva:**
> - Layout y jerarquía visual (3 columnas superior, 3 columnas inferior)
> - Distribución de sesiones con sus anchos (%) actuales
> - Componentes UI y estilos gráficos
> - SmoothValues para animaciones

> **NO modificar sin aprobación:**
> - Distribución de porcentajes (28/72/28/25/47)
> - Orden de sesiones
> - Componentes existentes (MeterPanel, Spectrograph, PhaseScope, VintageVU, Crest)
>
> **Todo lo dinámico** proviene del motor de análisis real (`AudioAnalyzer`), incluyendo:
> - Valores de pico/RMS/LUFS
> - Espectro FFT
> - Correlación de fase
> - Samples estéreo para vectorscope
