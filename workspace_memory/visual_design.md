# 🎨 Visual Design References — MixCoach & Messenger

> **Propósito:** Guía visual canónica para cualquier agente IA o desarrollador.
> Las imágenes en `UI_REFERENCES/` son la **fuente de verdad** del diseño esperado.
> **Última actualización:** 31 mayo 2026

---

## 📁 Imágenes de Referencia

| Imagen | Archivo | Descripción |
|:-------|:--------|:------------|
| Messenger Plugin | `UI_REFERENCES/Messenger.png` | UI completa del plugin por pista |
| MixCoach Pestaña 1 | `UI_REFERENCES/MixCoach_Tab1_AICoach.png` | Tab "AI Coach" — chat + lista de pistas |
| MixCoach Pestaña 2 | `UI_REFERENCES/MixCoach_Tab2_Analyzers.png` | Tab "Analyzers" — métricas y analizadores |

---

## 🎨 Sistema de Colores Global

### Paleta de Fondos (Dark Theme)
| Token | Color Hex | Uso |
|:------|:----------|:----|
| `bgCanvas` | `#0A0A0F` | Fondo más profundo, base de toda la UI |
| `bgDark` | `#0F0F18` | Fondo de paneles principales |
| `bgPanel` | `#141420` | Paneles elevados, tarjetas |
| `bgSurface` | `#1A1A2E` | Superficie de elementos interactivos |
| `bgGlass` | `#1E1E32` | Efecto glassmorphism |

### Paleta de Acentos
| Token | Color Hex | Uso |
|:------|:----------|:----|
| `accentPurple` | `#8B5CF6` / `#7C3AED` | Color principal de marca, títulos de sección |
| `accentCyan` | `#00B4D8` / `#00E5FF` | Acento AI, glow effects |
| `accentBlue` | `#3B82F6` | Elementos interactivos, botones secundarios |

### Colores de Bus (exactos)
| Bus | Color Hex | Dot Color |
|:----|:----------|:----------|
| Drums Bus | `#8B5CF6` (violeta) | `●` violeta |
| Bass Bus | `#3B82F6` (azul) | `●` azul |
| Guitars Bus | `#F97316` (naranja) | `●` naranja |
| Keys/Synths Bus | `#10B981` (verde-teal) | `●` verde |
| Vocals Bus | `#EC4899` (rosa) | `●` rosa |
| Master | `#EF4444` (rojo-coral) | `●` rojo |

### Tipografía
- **Fuente principal:** Sans-serif moderna (Inter / SF Pro / similar)
- **Labels de sección:** ALL CAPS, tamaño pequeño, color `accentPurple`, letra espaciada
- **Valores numéricos:** Fuente monoespaciada, tamaño grande, color blanco
- **Labels secundarios:** Gris claro (`#A1A1AA`), tamaño pequeño

---

## 🔌 Plugin: Messenger

> **Referencia:** `UI_REFERENCES/Messenger.png`

### Layout General
- Orientación: **Vertical**, 2 paneles apilados
- Fondo: negro profundo (`#08080F`)
- Esquinas redondeadas en cada panel (`border-radius: ~12px`)
- Sin borde visible entre paneles — separación por contraste de fondo
- Tamaño estimado: ~400×900px

### Panel Superior — "INFORMACIÓN DE PISTA"

**Estructura:**
- Header: label `INFORMACIÓN DE PISTA` en violeta, ALL CAPS
- Lista de filas con icono + label + valor (layout tipo formulario)
- Separadores horizontales sutiles entre filas (`#1A1A2E`)
- Padding interno generoso (~16px)

**Filas (en orden):**
| # | Icono | Label | Valor (ejemplo) | Notas |
|---|:------|:------|:----------------|:------|
| 1 | 🥁 (drum pad) | NOMBRE | `Kick` | Texto blanco, editable |
| 2 | ✦ (asterisk/star) | GRUPO | `Drums Bus` | Texto violeta (`accentPurple`), indica bus asignado |
| 3 | 🎯 (circle target) | COLOR | `●` círculo sólido | Dot coloreado según color del bus/track |
| 4 | 👾 (robot/face) | TIPO | `Batería / Transiente` | Texto gris claro |
| 5 | ☆ (star outline) | PRIORIDAD | `Alta` | Texto gris claro |
| 6 | 📋 (notepad) | NOTAS | `—` | Guión cuando está vacío |

**Estilo de iconos:** Outline, tamaño ~18px, color gris claro (`#6B7280`), alineados verticalmente al centro de cada fila.

### Panel Inferior — "NIVELES"

**Estructura:**
- Header: label `NIVELES` en violeta, ALL CAPS
- Dividido en 3 columnas: INPUT | REDUCCIÓN DE GANANCIA | OUTPUT
- Fondo del panel: ligero contraste con bgCanvas

**Columna INPUT (izquierda):**
- Label: `INPUT` gris claro, centrado
- Valor numérico: `-8.4 dB` blanco grande
- VU Meter estéreo (L+R) vertical
  - Escala: `6, 0, -6, -12, -18, -24, -30, -36, -42, -48, -60`
  - Marcador de pico (triángulo blanco/gris) a la izquierda del meter
  - Barras con gradiente de color: `verde → amarillo → naranja` (de abajo hacia arriba)
  - Background de barra: negro con sutil borde

**Columna REDUCCIÓN DE GANANCIA (centro):**
- Label: `REDUCCIÓN DE GANANCIA` gris, centrado
- Valor: `0.0 dB` blanco grande
- **Medidor circular** tipo VU analógico:
  - Arco semicircular con escala: `-12, -6, 0, +6, +12`
  - Aguja violeta luminosa apuntando al centro (0)
  - Fondo oscuro del arco con marcas blancas
  - Sin caja exterior visible — el arco flota sobre el fondo

**Columna OUTPUT (derecha):**
- Idéntica a INPUT con valor `-8.6 dB`
- Mismo estilo de VU meter estéreo vertical

**Paleta del gradiente de VU bars (bottom → top):**
- `0% → 50%`: Verde (`#22C55E`)
- `50% → 70%`: Amarillo-verde (`#84CC16`)
- `70% → 85%`: Amarillo (`#EAB308`)
- `85% → 95%`: Naranja (`#F97316`)
- `95% → 100%`: Rojo (`#EF4444`)

---

## 🧠 Plugin: MixCoach — Pestaña 1 "AI COACH"

> **Referencia:** `UI_REFERENCES/MixCoach_Tab1_AICoach.png`

### Layout General
- Ventana full: barra superior + contenido dividido en 2 columnas + barra inferior
- **Columna izquierda** (~38% del ancho): Chat + Referencias
- **Columna derecha** (~62% del ancho): Lista de pistas detectadas

### Barra Superior (Header)
- Fondo: casi negro
- Logo `⚡ MIXCOACH` a la izquierda (ícono de onda + texto)
- Subtitle `AI COACH` al lado
- Tabs centrados: `AI COACH` (activo, subrayado violeta) | `ANALYZERS`
- Botón `VERIFICAR PROGRESO` a la derecha — fondo violeta sólido, texto blanco
- Iconos: `≡` (menú) + `?` (help) + `⚙` (settings) — esquina derecha

### Columna Izquierda

#### Sección 1 — "SESIÓN 1 – CHAT CON IA"
- Header: `SESIÓN 1 – CHAT CON IA` en violeta, pequeño, ALL CAPS
- Avatar del coach: ícono robot circular (~50px), fondo oscuro
- Nombre: `¡Hola, Ingeniero!` en blanco grande
- Subtitle: texto gris descriptivo
- Badge: `FASE ACTUAL: 2 – ORGANIZACIÓN` en amarillo/naranja, inline con ícono
- **Burbujas de chat:**
  - Mensajes del Coach: fondo `#1A1A2E`, borde sutil, esquinas redondeadas, texto gris claro, timestamp gris pequeño en esquina inferior
  - Mensajes del Usuario: fondo ligeramente distinto, alineados a la derecha, con `✓✓` azul
- **Input field:** fondo oscuro, placeholder `Escribe tu mensaje...`, gris, borde sutil
- **Botón enviar:** círculo violeta con ícono de avión de papel (→)

#### Sección 2 — "SESIÓN 2 – REFERENCIAS, ARCHIVOS Y ENLACES"
- Header: `SESIÓN 2 – REFERENCIAS, ARCHIVOS Y ENLACES` en violeta ALL CAPS
- Sub-tabs: `REFERENCIAS DE AUDIO` (activo, underline violeta) | `ENLACES ÚTILES`
- **Lista de archivos de referencia:**
  - Cada item: botón play ▶ + waveform coloreada + nombre + info + ícono borrar + `⋯`
  - Waveforms con colores distintos (violeta, cyan, verde) sobre fondo oscuro
  - Info: nombre archivo, calidad (48 kHz · 24 bit), duración
- **Drop zone:** borde punteado, ícono nube-upload, texto "Arrastra y suelta archivos aquí o", botón `EXPLORAR ARCHIVOS` (outline violeta)
- Formatos: `WAV, AIFF, FLAC`

### Columna Derecha

#### Sección 3 — "SESIÓN 3 – PISTAS DETECTADAS, MEDICIÓN Y SUGERENCIAS"
- Header: violeta ALL CAPS
- **Toolbar de filtros:** `AGRUPAR POR:` + chips `TIPO` | `COLOR` | `BUS` (el activo tiene fondo sutil) + `COLAPSAR TODO` | `EXPANDIR TODO` (texto gris, derecha)
- **Lista agrupada por Bus:**

**Estructura de grupo expandido:**
```
▼  ██ DRUMS BUS                          -6.1 dB    -18.2 LUFS
   [ícono] Kick    [━━━━━━━━━━━━━━━━]   -6.2 dB   Sube 1.0 dB en 60 Hz    ···
   [ícono] Snare   [━━━━━━━━━━━━━━━━]   -8.1 dB   Sube 0.5 dB en 200 Hz   ···
```

**Detalles de cada fila de pista:**
- Dot coloreado `●` (color del bus, tamaño ~8px)
- Ícono del tipo de instrumento (outline, gris)
- Nombre de pista (blanco)
- **Mini meter horizontal** con gradiente verde→amarillo→naranja→rojo, ancho variable según nivel
- Valor dB (blanco, monospace)
- **Sugerencia IA** (texto cyan `#00B4D8`): ej. "Sube 1.0 dB en 60 Hz" — esta es la parte de IA
- Botón `···` (más opciones, gris)

**Header de grupo (colapsable):**
- `▼` chevron + dot coloreado bus + nombre del bus en mayúsculas + valor dB + LUFS a la derecha

**Barra Inferior (Footer):**
- `FASE ACTUAL: 2 – ORGANIZACIÓN` badge en naranja/amarillo
- `GÉNERO ACTUAL:` + valor en violeta
- `TARGET:` + `-14 LUFS` en blanco
- `SAMPLE RATE:` + `48.0 kHz` en blanco
- Ícono settings a la derecha

---

## 📊 Plugin: MixCoach — Pestaña 2 "ANALYZERS"

> **Referencia:** `UI_REFERENCES/MixCoach_Tab2_Analyzers.png`

### Layout General
- Grid de **5 paneles** en layout asimétrico:
  - Izquierda: 1 columna angosta (lista de pistas)
  - Centro-derecha: 4 paneles en grid 2×2 (con panel superior derecho ocupando más espacio)
- Barra inferior: misma que Tab 1

### Panel Izquierdo — "SESIÓN 1 – PLAYLIST / MESSENGERS"
- Header: `SESIÓN 1 – PLAYLIST` violeta ALL CAPS
- Sub-header: `MESSENGERS & GRUPOS` + botón `+ GRUPO` + ícono grid
- **Lista jerárquica:**
  - Item Master: `⚡ Master` → `Master Bus` — dot rojo, `···`
  - Grupos colapsables con dot de color + nombre en mayúsculas + flecha colapso
  - Pistas dentro de grupo: ícono + nombre + sub-tipo + dot verde (conectado) + `···`
- **Footer del panel:** `Mensajeros activos: 16` + `● Conectado` (dot verde)

### Panel Centro-Superior — "SESIÓN 2 – METER"
- Dividido en 2 sub-secciones:

**Sub-sección izquierda (Stereo Meter):**
- Labels `L` y `R`
- **Barras VU stereo verticales** con gradiente (mismo estilo Messenger)
- Escala: `0, -6, -12, -18, -24, -30, -36, -42, -48, -54, -60`
- Marcadores pico numéricos abajo: `-1.2` / `-1.0` en naranja/amarillo
- **Métricas en texto grande** al centro:
  - `PEAK: -1.2 dB`
  - `RMS: -18.1 dB`
  - `LUFS (I): -14.0 LUFS`
  - `DR: 9.8 dB`
  (Labels en gris pequeño, valores en blanco grande monospace)

**Sub-sección derecha (LUFS MDR):**
- Labels `LUFS MDR`, `L`, `R`
- Barras LUFS verticales — color cyan (`#00B4D8`), más angostas
- Marcadores triángulares laterales (hold peaks) en cyan
- Escala similar pero para LUFS (`6, 0, -6, -12, -18, -24...`)
- Valores abajo: `-13.8` / `-13.6` en cyan

### Panel Derecho-Superior — "SESIÓN 3 – SPECTRUM ANALYZER"
- Header: `SESIÓN 3 – SPECTRUM ANALYZER` + ícono settings
- Sub-label: `RTA` (Real Time Analyzer)
- **Gráfica de barras FFT:**
  - Fondo: cuadrícula oscura con líneas `#1A1A2E`
  - Barras: gradiente **cyan a azul** (`#00E5FF` → `#0EA5E9`), bordes suaves
  - Escala X: `20, 30, 50, 70, 100, 200, 300, 500, 700, 1k, 2k, 3k, 5k, 7k, 10k, 20k` Hz
  - Escala Y: `0, -5, -10, -15, -20, -25, -30, -35, -40, -45` dB
  - Etiquetas de frecuencia: gris pequeño, en la parte inferior
  - Etiquetas de dB: gris pequeño, a la izquierda
  - Barras altas en medios (`200Hz–3kHz`), caída en extremos — curva típica de mezcla

### Panel Centro-Inferior — "SESIÓN 5 – PHASE SCOPE"
- Dividido en 2 sub-secciones: Correlación + Crest Factor / Vectorscope

**Correlación:**
- Label: `CORRELACIÓN`
- **Barra horizontal de correlación** (`-1` a `+1`):
  - Gradiente: rojo (izq, `-1`) → verde (centro, `0`) → verde brillante (der, `+1`)
  - Marcador triangular apuntando al valor actual (`0.72`)
- Valor numérico grande: `0.72` en verde (`#22C55E`)

**Crest Factor (gauge circular):**
- Label: `CREST FACTOR`
- **Gauge semicircular** con arco de colores:
  - Verde (`0–10`) → amarillo (`10–20`) → rojo (`20–30`)
  - Aguja negra apuntando al valor
  - Escalas: `0, 5, 10, 15, 20, 25, 30`
- Valor: `9.2 dB` en blanco grande
- Sub-valores: `PEAK: -1.2 dBFS`, `RMS: -18.1 dBFS`, `CREST: 9.2 dB` en gris pequeño

**Vectorscope:**
- Label: `VECTORSCOPE`
- Fondo: negro con cuadrícula circular sutil
- Labels: `M` (arriba), `L` (izq), `R` (der), `S` (abajo)
- **Nube de puntos violeta** (`#8B5CF6` con transparencia/glow) — patrón de lissajous vertical (mono coherente)
- Fondo oscuro, sin bordes visibles del osciloscope

### Panel Derecho-Inferior — "SESIÓN 4 – VU METERS"
- Header: `SESIÓN 4 – VU METERS`
- Grid 2×2 de **4 VU meters analógicos**:
  - L (LEFT), R (RIGHT), M (MID), S (SIDE)
- **Estilo analógico vintage:**
  - Fondo tipo madera/cuero envejecido (`#C4A35A` / beige-marrón)
  - Escala impresa: `-20, -10, -7, -5, -3, -2, -1, 0, +1, +2, +3` VU
  - Zona roja: `+1` a `+3`
  - Aguja oscura (negra/gris oscuro)
  - Label `VU` central
  - Marco oscuro (`#1A1A1A`) tipo bisel
- Valor numérico debajo de cada VU: ej. `-1.2 dB`, gris claro

### Barra Inferior (Footer — idéntica a Tab 1)
- Logo `⚡ MIXCOACH` + `ANALYZERS` (tab activo)
- `GÉNERO ACTUAL:` + valor violeta
- `TARGET:` + ícono + `-14 LUFS`
- `SAMPLE RATE:` + `48.0 kHz`
- Iconos: sliders + settings + config (derecha)

---

## 🔧 Componentes Compartidos y Patrones

### VU Meter Vertical (patrón estándar)
```
Gradiente de barra (bottom → top):
  0%   – 50%:  Verde      #22C55E
  50%  – 70%:  Lima       #84CC16
  70%  – 85%:  Amarillo   #EAB308
  85%  – 95%:  Naranja    #F97316
  95%  – 100%: Rojo       #EF4444

Escala (top → bottom): 6, 0, -6, -12, -18, -24, -30, -36, -42, -48, -60
Marcador de pico: triángulo ◀ gris-blanco lateral
Fondo de barra: #0A0A0F con borde sutil
```

### Estilo de Sección Header
```
- Texto: ALL CAPS
- Color: accentPurple (#8B5CF6) o violeta claro (#A78BFA)
- Tamaño: ~11px
- Letter-spacing: 0.1em
- Formato: "SESIÓN N – NOMBRE DE SECCIÓN"
- Sin borde inferior — solo el contraste de color distingue el header
```

### Separadores de Fila (lista de pistas)
```
- Altura: 1px
- Color: #1A1A2E (muy sutil)
- Sin espacio adicional — la fila siguiente empieza inmediatamente
```

### Mini Meter Horizontal (en lista de pistas)
```
- Alto: ~6px
- Bordes: redondeados (~3px)
- Gradiente: igual que VU vertical pero horizontal (izq verde → der rojo)
- Fondo: #0F0F18
- Ancho dinámico según nivel de la pista
```

### Botones de Acción Principal
```
Primario (VERIFICAR PROGRESO):
  - Fondo: #7C3AED (violeta sólido)
  - Texto: blanco, bold
  - Border-radius: ~6px
  - Sin borde

Secundario (EXPLORAR ARCHIVOS):
  - Fondo: transparente
  - Borde: 1px violeta (#7C3AED)
  - Texto: violeta
```

### Sugerencias de IA (texto en lista de pistas)
```
- Color: #00B4D8 (cyan)
- Tamaño: ~11px
- Formato: "Acción + valor + en + frecuencia/banda"
  Ejemplos: "Sube 1.0 dB en 60 Hz"
            "Baja 1.5 dB en 8 kHz"
            "Recorta 2.0 dB en 250 Hz"
- Siempre a la derecha del valor dB, antes del botón ···
```

---

## ⚠️ Qué NO debe hacer el diseño

- ❌ NO usar fondos claros — toda la UI es dark theme
- ❌ NO usar colores saturados sin el contexto dark (los colores se ven sobre fondos muy oscuros)
- ❌ NO usar bordes pesados entre paneles — separación por contraste de fondo
- ❌ NO hardcodear colores de bus — siempre desde `MixCoachTheme` usando las constantes de `Constants.h`
- ❌ NO mostrar controles de procesamiento de audio (esto es solo mentoría/análisis)
- ❌ NO usar tipografía serif — fuente sans-serif moderna siempre
- ❌ NO mezclar el estilo analógico (VU meters vintage) con elementos planos en el mismo panel — el estilo analógico es exclusivo para los VU meters de la Sesión 4

---

## 🗺️ Mapeo Visual → Archivo de Código

| Elemento Visual | Archivo de implementación |
|:----------------|:--------------------------|
| Messenger – Panel de información | `Source/Messenger/ui/PluginEditor.cpp` |
| Messenger – VU meters (INPUT/OUTPUT) | `Source/MixCoach/UI/StereoVUMeter.cpp` |
| Messenger – Gauge circular (GR) | **PENDIENTE** — no implementado aún |
| MixCoach Tab 1 – Chat | `Source/MixCoach/UI/CoachChatComponent.cpp` |
| MixCoach Tab 1 – Lista de pistas con sugerencias | `Source/MixCoach/UI/MessengerListComponent.cpp` |
| MixCoach Tab 1 – Referencias de audio | `Source/MixCoach/UI/ReferencePanelComponent.cpp` |
| MixCoach Tab 2 – Lista Playlist/Messengers | `Source/MixCoach/UI/PlaylistComponent.cpp` |
| MixCoach Tab 2 – Meter (Peak/RMS/LUFS) | `Source/MixCoach/UI/MeterComponent.cpp` |
| MixCoach Tab 2 – Spectrum Analyzer | `Source/MixCoach/UI/SpectrographComponent.cpp` |
| MixCoach Tab 2 – Phase Scope + Correlación | `Source/MixCoach/UI/PhaseCorrelationMeter.cpp` + `PhaseScopePanel.cpp` |
| MixCoach Tab 2 – Crest Factor gauge | `Source/MixCoach/UI/CrestHistogram.cpp` |
| MixCoach Tab 2 – Vectorscope | `Source/MixCoach/UI/VectorscopeComponent.cpp` |
| MixCoach Tab 2 – VU Meters analógicos | `Source/MixCoach/UI/AnalogVUMeter.cpp` |
| Tema global | `Source/MixCoach/UI/MixCoachTheme.h` |

---

*Documento generado a partir de las imágenes de referencia del usuario — 31 mayo 2026*
