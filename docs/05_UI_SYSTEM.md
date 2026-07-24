# 🎨 05 — UI SYSTEM

> **Reglas globales de UX/UI para MixCoach.**
> Este documento define el sistema de diseño visual: layout, colores, tipografía, animaciones, glassmorphism, y patrones de interacción.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Documento base:** 02_MASTER_EXPERIENCE.md, MixCoachTheme.h

---

## 📋 Índice

1. [Filosofía Visual](#1-filosofia-visual)
2. [Paleta de Colores](#2-paleta-de-colores)
3. [Glassmorphism](#3-glassmorphism)
4. [Layout General](#4-layout-general)
5. [Tipografía](#5-tipografia)
6. [Animaciones](#6-animaciones)
7. [Estados de Componentes](#7-estados-de-componentes)
8. [Patrones de Interacción](#8-patrones-de-interaccion)
9. [Iconografía](#9-iconografia)
10. [Responsive / Escalado](#10-responsive--escalado)

---

## 1. Filosofía Visual

MixCoach debe sentirse **premium, profesional y acogedor.**

No es un plugin más. Es el estudio de un ingeniero.

| Principio | Descripción |
|:----------|:------------|
| **Oscuro pero no plano** | Fondos #05080D con glassmorphism y glow |
| **Jerarquía visual clara** | Lo importante se ve primero |
| **Mínimo ruido** | Cada píxel tiene un propósito |
| **Consistencia** | Un solo tema, un solo lenguaje visual |
| **Elegancia técnica** | Los datos se ven bien porque están bien diseñados |

---

## 2. Paleta de Colores

### Fondos

| Token | Hex | Uso |
|:------|:----|:----|
| `bgCanvas` | `#04060A` | Fondo más profundo (canvas principal) |
| `bgMessenger` | `#05070D` | Fondo del Messenger UI |
| `bgPanel` | `#0A1018` | Paneles y tarjetas |
| `bgDarker` | `#060A10` | Fondo de meter rails |
| `bgCard` | `#090D15` | Card bg / glass panel fill |
| `bgInput` | `#08101A` | Input fields background |

### Brand / Accents

| Token | Hex | Uso |
|:------|:----|:----|
| **accent** | **#A855F7** | Títulos, sidebar activo, branding |
| **accentCyan** | **#00B7FF** | Spectrum, LUFS meters, info técnica |

### Colores Funcionales

| Token | Hex | Uso |
|:------|:----|:----|
| **success** | **#4CAF50** | VU meter green, señal saludable |
| **warning** | **#F59E0B** | Advertencia, cerca del límite |
| **error** | **#EF4444** | Error, clipping, fuera de rango |

### Texto

| Token | Hex | Uso |
|:------|:----|:----|
| `textPrimary` | `#FFFFFF` | Texto principal |
| `textSecondary` | `#CBD5E1` | Texto secundario |
| `textDim` | `#94A3B8` | Texto tenue |
| `textMuted` | `#64748B` | Texto muy tenue (labels, footnotes) |

### Borders

| Token | Valor | Uso |
|:------|:------|:----|
| `border` | `rgba(255,255,255,0.08)` | Bordes de paneles y cards |

---

## 3. Glassmorphism

MixCoach usa glassmorphism como su firma visual. Todos los paneles y cards usan este patrón.

### Cómo dibujar un glass panel

```cpp
MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);
// Esto dibuja:
// 1. Shadow externo
// 2. Fondo oscuro translúcido
// 3. Glass highlight (gradiente blanco superior)
// 4. Borde sutil
```

### Esquinas redondeadas

| Token | Radio | Uso |
|:------|:------|:----|
| `small` | 4px | Botones, inputs pequeños |
| `medium` | 6px | Paneles, cards, contenedores |
| `large` | 12px | Modales, overlays |
| `pill` | 999px | Badges, pills, tags |

---

## 4. Layout General

### Estructura de la ventana

```
┌──────────────────────────────────────────────────────────────┐
│ [Progress Bar: Setup > Ref > Map > Coach > Done]    42%    │  ← Siempre visible
├────┬─────────────────────────────────────────────────────────┤
│    │                                                         │
│ S  │  CONTENT AREA                                           │
│ I  │  (cambia según sección activa)                          │
│ D  │                                                         │
│ E  │  ┌─────────────────────────────────────────────────┐    │
│ B  │  │ Panel activo según sección del sidebar           │    │
│ A  │  │ (Dashboard / MixMap / Analysis / Reference        │    │
│ R  │  │  / Coach / Progress / Report)                    │    │
│    │  └─────────────────────────────────────────────────┘    │
├────┴─────────────────────────────────────────────────────────┤
│ [Status Bar: FASE | GÉNERO | TARGET | kHz]                  │  ← Siempre visible
└──────────────────────────────────────────────────────────────┘
```

### Sidebar

- Ancho fijo: **64px**
- 7 secciones verticales: Dashboard, MixMap, Analysis, Reference, Coach, Progress, Report
- Icono + tooltip en hover
- Hover glow + active highlight

### Espaciado

| Token | Píxeles |
|:------|:--------|
| XXS | 2px |
| XS | 4px |
| SM | 8px |
| MD | 12px |
| LG | 16px |
| XL | 20px |
| XXL | 28px |
| XXXL | 36px |

---

## 5. Tipografía

### Fuentes

- **Principal:** Inter (o fallback system-ui, sans-serif)
- **Monospace:** JetBrains Mono (para valores técnicos)

### Tamaños

| Token | Size | Weight | Uso |
|:------|:-----|:-------|:----|
| Hero | 28px | Bold | Títulos de pantalla |
| Title | 18px | SemiBold | Títulos de panel |
| Subtitle | 14px | Medium | Subtítulos, secciones |
| Body | 13px | Regular | Texto general |
| Small | 11px | Regular | Labels, footnotes |
| Tiny | 9px | Medium | Métricas muy compactas |

### Tracking (letter-spacing)

- Títulos: +0.5px
- UPPERCASE labels: +1.2px
- Body: 0px

---

## 6. Animaciones

### Filosofía

Las animaciones en MixCoach son **funcionales, no decorativas.**
Cada animación tiene un propósito: guiar la atención, indicar transiciones, celebrar logros.

### Timing

| Animación | Duración | Easing |
|:----------|:---------|:-------|
| Panel reveal (fade-in + slide-up) | 200ms | ease-out quad |
| Tab switch (crossfade) | 150ms | ease-out |
| SmoothValue attack | 50ms (1-5 frames) | exponencial |
| SmoothValue release | 200ms (100-250 frames) | exponencial |
| Hover glow | 200ms | ease-out |
| Progress bar update | 300ms | ease-out |
| Celebration pulse | 500ms | ease-out |

### Reveal de paneles (UX 2.0)

Cuando el Coach revela un panel por primera vez:

```cpp
// 1. Antes: panel invisible
panel.setAlpha(0.0f);
panel.setTransform(AffineTransform::translation(0, 20));

// 2. Durante 200ms (12 frames a 60fps):
// Alpha: 0.0 → 1.0
// TranslateY: 20px → 0px
// Ease-out quad

// 3. Después: transform limpiado, alpha = 1.0
panel.setTransform({});
panel.setAlpha(1.0f);
```

### Transiciones entre paneles

Crossfade de 150ms entre secciones del sidebar (NavigationShell).

---

## 7. Estados de Componentes

Cada componente debe manejar estos estados visuales:

| Estado | Visual | Cuándo |
|:-------|:-------|:-------|
| **Empty** | Placeholder + hint | Sin datos, primera carga |
| **Loading** | Spinner animado (8 dots orbit) | Analizando, cargando |
| **Ready** | Contenido normal | Datos disponibles |
| **Error** | ⚠️ Icono + mensaje + [Retry] | Algo salió mal |

### Ejemplo: Dashboard

```
[Empty]   🎧 "Welcome to MixCoach" + hint pill
[Loading] ◌◌◌◌◌◌◌◌ "Analizando tu sesión..."
[Ready]   Header + Objetivo + NextStep + Prioridades
[Error]   ⚠️ "No pudimos analizar la sesión" [Reintentar]
```

---

## 8. Patrones de Interacción

### Botones

| Tipo | Estilo | Uso |
|:-----|:-------|:----|
| **Primary** | Filled accent purple | Acción principal |
| **Secondary** | Glass panel + border | Acción secundaria |
| **Ghost** | Sin fondo, solo texto | Acción terciaria |
| **Icon** | 28x28px, glass circle | Acciones de toolbar |
| **Suggestion** | Chip glass con hover | Sugerencias clickeables |

### Inputs

| Tipo | Estilo |
|:-----|:-------|
| **Text input** | Fondo #08101A, borde sutil, focus glow cyan |
| **Dropdown** | Glass panel, chevron, PopupMenu |
| **Toggle** | Switch estilizado con animación |

### Tooltips

- Aparecen en hover (200ms delay)
- Glass panel pequeño (6px radius)
- Texto small, max 3 líneas

---

## 9. Iconografía

### Estilo

- Línea fina (stroke 1.5px)
- Esquinas redondeadas
- Tamaño base: 16x16px (en sidebar), 14x14px (en contenido)
- Color: textSecondary por defecto, accent en hover/active

### Iconos usados

| Concepto | Icono |
|:---------|:------|
| Dashboard | 🏠 |
| MixMap | 🗺️ |
| Analysis | 📊 |
| Reference | 🎯 |
| Coach | 💬 |
| Progress | 📈 |
| Report | 📋 |
| Settings | ⚙️ |
| Play | ▶️ |
| Pause | ⏸️ |
| Close | ✕ |
| Expand | ▶ |
| Collapse | ▼ |
| Check | ✓ |
| Warning | ⚠️ |
| Error | ❌ |

---

## 10. Responsive / Escalado

MixCoach es un plugin VST3 con tamaño de ventana fijo (recomendado) o redimensionable.

### Breakpoints (si es redimensionable)

| Ancho mínimo | Comportamiento |
|:-------------|:---------------|
| < 800px | Sidebar colapsa a iconos sin labels |
| 800-1100px | Layout normal |
| > 1100px | Layout expandido con más espacio para paneles |

### Reglas de escalado

- Los paneles usan proporciones relativas (porcentajes), no píxeles fijos
- Los meters y gráficos escalan con el contenedor
- El texto mantiene tamaño fijo (no escala con la ventana)

---

*Documento del sistema UI — MixCoach v1.0 — 3 julio 2026*
*Toda implementación visual debe seguir estas reglas. Si un cambio contradice este documento, el cambio debe replantearse.*
