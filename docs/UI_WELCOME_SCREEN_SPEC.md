# 🎯 UI Welcome Screen — Especificación Definitiva

> **Documento de referencia para la implementación de la pantalla de bienvenida de MixCoach.**
> Fecha: Julio 2026
> Esta especificación es la fuente de verdad para la Welcome Screen. Cualquier cambio debe actualizar este documento primero.

---

## Filosofía

Diseñar una pantalla de bienvenida minimalista, moderna y premium con estética futurista enfocada en inteligencia artificial para producción musical.

La interfaz debe transmitir:
- Elegancia
- Tecnología
- IA
- Audio profesional
- Producto premium tipo iZotope / UAD / FabFilter pero con identidad propia

Toda la composición gira alrededor del color **púrpura** como color de identidad.

---

## Resolución Base

**1600 x 900** — Escalable mediante FlexBox o LayoutManager.

---

## Fondo

### Color
Gradiente extremadamente oscuro:
| Posición | Color |
|:---------|:------|
| Top      | `#06060B` |
| Center   | `#090812` |
| Bottom   | `#130E21` |

No usar negro puro. Debe sentirse profundo.

### Efecto
- Gran luz radial difusa detrás del contenido principal
- Color: `#8B5CF6`
- Opacidad: 10-15%
- Blur enorme
- Debe iluminar ligeramente el centro de la pantalla

### Textura
- Ruido muy fino
- Opacity: 2%
- Evitar un fondo completamente plano

---

## Márgenes

| Lado | Padding |
|:----|:--------|
| Top | 24 px |
| Left | 24 px |
| Right | 24 px |
| Bottom | 24 px |

---

## Header Superior

### Ubicación: Top Left

#### Logo
- Icono púrpura, 28x28 px
- A la derecha: **MIXCOACH**
- Fuente: Inter Bold
- Color: White

#### Versión
- Debajo: **v1.0.0**
- Color: `#8F8F9D`
- Muy pequeño

### Ubicación: Top Right — Botones Ventana
- Dos botones: **—** y **X**
- Color: `#BEBEC7`
- Hover: Morado

---

## Contenido Principal

Todo centrado vertical y horizontalmente.

### Avatar IA (Elemento más importante)
- Tamaño: **300x300 px**
- Círculo transparente:
  - Fill: `rgba(255,255,255,0.04)`
  - Borde: 1px, `rgba(255,255,255,0.08)`
- Glow exterior: `#8B5CF6`
  - Blur: 80px
  - Opacidad: 55%

#### Robot
- Robot 3D estilo Pixar
- Colores: Blanco, Negro brillante, Morado
- Pantalla facial negra
- Dos ojos morados
- Audífonos grandes
- En el pecho: Logo de MixCoach
- El robot ocupa aproximadamente el 75% del círculo

### Título Principal (debajo del robot)
- Texto: **Bienvenido a MixCoach**
- Fuente: Inter ExtraBold, 56px
- Colores:
  - "Bienvenido a" → Blanco
  - "MixCoach" → `#A855F7`
- Debe ser el único texto con énfasis

### Subtítulo
- Texto: **Tu mentor de mezcla impulsado por IA.**
- Fuente: 28px
- Color: `#B8B8C5`
- Solo la palabra **"mentor"** en púrpura (`#A855F7`)
- Separación: 50px

### Etiqueta
- Texto: **¿CÓMO TE LLAMAS?**
- Fuente: Inter SemiBold, 13px
- Tracking: 0.30em
- Mayúsculas
- Color: `#A0A0AE`

### Caja de Texto
| Propiedad | Valor |
|:----------|:------|
| Ancho | 740 px |
| Altura | 74 px |
| Fondo | `#1A1826` |
| Border | 2px, `#7C3AED` con opacidad |
| Border Radius | 14 px |
| Placeholder | "Escribe tu nombre..." |
| Placeholder Color | `#8C8C98` |
| Fuente | Inter Regular, 22px |
| Padding izquierdo | 26 px |
| Focus | Glow exterior `#8B5CF6`, blur suave |

### Botón
| Propiedad | Valor |
|:----------|:------|
| Separación | 40 px |
| Ancho | 430 px |
| Altura | 74 px |
| Border Radius | 14 px |
| Gradiente | Top `#A855F7` → Bottom `#7C3AED` |
| Glow exterior | `#8B5CF6`, Blur 45px |
| Texto | **COMENZAR** |
| Fuente | Inter Bold, 24px |
| Color | White |
| Flecha | → a la derecha, White |
| Separación texto-flecha | 40 px |
| Hover | Subir ligeramente +2px, Glow aumenta, Escala 102% |

### Footer
| Propiedad | Valor |
|:----------|:------|
| Separación | 55 px |
| Ícono | Escudo pequeño, color `#777785` |
| Texto | "Tus datos y tu música permanecen siempre en tu equipo." |
| Color texto | `#7F7F8E` |
| Fuente | 16 px |

---

## Paleta

| Token | Hex | Uso |
|:------|:---|:----|
| Primary Purple | `#8B5CF6` | Glows, borders, focus |
| Secondary Purple | `#A855F7` | Botón, "MixCoach" |
| Dark Purple | `#5B21B6` | (reserva) |
| Background | `#090812` | Fondo centro |
| Surface | `#181624` | Input background? |
| Border | `#2B2540` | Bordes |
| White | `#FFFFFF` | Texto principal |
| Gray | `#B8B8C5` | Subtítulo |
| Muted | `#7D7D89` | Texto secundario |

---

## Tipografía

**Inter** — Pesos: Regular, Medium, SemiBold, Bold, ExtraBold

---

## Animaciones

### Robot
- Flotación vertical muy lenta (2–4 px)
- Respiración sutil mediante escala (100% ↔ 101%)
- Halo púrpura pulsante

### Botón
- Glow animado
- Escala al hacer hover
- Transición de 180–220 ms

### Caja de texto
- Glow púrpura al recibir foco
- Cursor blanco
- Fade suave en el placeholder

### Aparición inicial (Staggered)
Animación escalonada en este orden:
1. Logo (fade-in)
2. Robot (fade + escala 0.95 → 1.0)
3. Título
4. Subtítulo
5. Etiqueta
6. Campo de texto
7. Botón
8. Footer

Duración total: **700–900 ms**

---

## Arquitectura recomendada en JUCE

```
MainComponent
│
├── HeaderComponent
│   ├── Logo
│   ├── VersionLabel
│   └── WindowButtons
│
├── WelcomePanel
│   ├── RobotAvatarComponent
│   ├── TitleLabel
│   ├── SubtitleLabel
│   ├── NameLabel
│   ├── TextEditor
│   ├── StartButton
│   └── FooterComponent
│
└── BackgroundRenderer
    ├── RadialGradient
    ├── NoiseTexture
    └── PurpleGlow
```

---

## Estilo visual

La interfaz debe sentirse como una combinación entre:
- **Apple Vision Pro** (minimalismo y profundidad)
- **OpenAI ChatGPT Desktop** (limpieza y jerarquía)
- **iZotope Ozone** (acabado profesional para audio)
- **Linearity + Framer** (espaciado moderno)
- **Material Design 3** adaptado a estética oscura y premium

El resultado final debe ser una pantalla de bienvenida elegante, con abundante espacio negativo, iluminación púrpura sutil, transiciones suaves y una composición perfectamente centrada.
