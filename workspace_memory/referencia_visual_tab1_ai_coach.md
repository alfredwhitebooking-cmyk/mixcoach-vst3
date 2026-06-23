# 🎨 Guía de Referencia Visual — MIXCOACH TAB 1: AI COACH

> Basada en la captura de referencia `Mix Coach Pestaña 1.png`
> Fecha de actualización: Junio 2026

---

## 🏛️ Objetivo General

Vista principal del módulo AI Coach. Centro de trabajo guiado donde la IA acompaña al usuario durante la mezcla, organiza la información del proyecto y presenta análisis con recomendaciones accionables.

---

## 🎨 Paleta de Colores

| Elemento | Color | Uso |
|----------|-------|-----|
| Fondo principal | `#05080D` | Canvas principal |
| Paneles | `#091018` | Tarjetas y secciones |
| Bordes | `rgba(255,255,255,0.08)` | Bordes sutiles |
| **Acento principal** | **`#A855F7`** | Morado — títulos, branding, highlights |
| Acento secundario | `#00B7FF` | Cyan — info técnica, meters |
| Advertencia | `#FFC107` | Amarillo — warnings |
| Error | `#FF5252` | Rojo — clipping, errores |
| Correcto | `#4CAF50` | Verde — señal saludable |

---

## 📐 Estructura Global del Tab

```
┌──────────────────────────────────────────────────────────────┐
│  SESIÓN 1 — AI COACH          │  SESIÓN 3 — ORGANIZACIÓN    │
│                                │  ANÁLISIS Y SUGERENCIAS    │
│  ┌──────────────────────────┐  │                              │
│  │ 🤖 Avatar IA             │  │  [TIPO] [COLOR] [BUS]      │
│  │                          │  │  [▼ Colapsar] [▲ Expandir] │
│  │ Hola Ingeniero           │  │                              │
│  │ Estoy aquí para ayudarte │  │  ┌────────────────────────┐ │
│  │                          │  │  │ DRUMS BUS         [▼] │ │
│  │ FASE ACTUAL: 2           │  │  │ ○ Kick      ██ -6dB  │ │
│  │ Organización             │  │  │ ○ Snare     ██ -12dB │ │
│  ├──────────────────────────┤  │  ├────────────────────────┤ │
│  │ Mensajes IA / Usuario    │  │  │ BASS BUS          [▼] │ │
│  │ con timestamps           │  │  │ ○ Bass DI    ██ -8dB  │ │
│  │ scroll largo             │  │  └────────────────────────┘ │
│  ├──────────────────────────┤  │                              │
│  │ [Escribe...        ] [➤]│  │  Sugerencias IA             │
│  ├──────────────────────────┤  │  • Sube 1dB en 60Hz        │
│  │ SESIÓN 2 — REFERENCIAS   │  │  • Reduce 2dB en 300Hz     │
│  │ [Audio] [Enlaces] [Notas]│  │                              │
│  │ ┌──────────────────────┐ │  │                              │
│  │ │ Kick Ref.wav 3:45   │ │  │                              │
│  │ │ 48kHz 24bit         │ │  │                              │
│  │ │ [▶] [■] [🔊]  [✕]   │ │  │                              │
│  │ ├──────────────────────┤ │  │                              │
│  │ │ 📁 Arrastra archivos │ │  │                              │
│  │ └──────────────────────┘ │  │                              │
├──────────────────────────────┴──────────────────────────────┤
│  FASE ACTUAL: Organización | POP | TARGET: -14 LUFS | 48kHz │
└──────────────────────────────────────────────────────────────┘
```

---

## 📐 Proporciones del Layout

| Columna | Proporción | Contenido |
|---------|-----------|-----------|
| **Izquierda** | ~33% (1/3) | MessengerListComponent (lista de pistas) |
| **Derecha** | ~67% (2/3) | MasterMeterPanel (medidores: Peak, RMS, LUFS, Correlación) |

> **⚠️ NOTA DE LAYOUT**: La referencia muestra la lista de tracks a la izquierda y los meters a la derecha. Verificar si es el layout deseado vs la implementación actual.

---

## SESIÓN 1 — AI COACH (Columna izquierda)

### Componentes

#### 🤖 Avatar de IA
- Ubicación: Esquina superior izquierda del panel
- Función: Identidad visual del asistente, estado de conexión, estado del análisis
- Debe ser sustituible por futuras versiones del asistente

#### Mensaje de Bienvenida
- Saludo inicial: `Hola Ingeniero`
- Estado del proyecto
- Resumen del análisis
- Próximos pasos
- **Contenido 100% dinámico**

#### Indicador de Fase Actual
- Texto: `FASE ACTUAL:`
- Ejemplos: Organización, Balance, EQ, Compresión, Espacialidad, Mastering
- **No son fases obligatorias**

#### Historial de Conversación
- Contenedor de mensajes con scroll
- Soporta: mensajes IA, mensajes usuario, timestamps, conversaciones largas

#### Campo de Entrada
- Placeholder: `¿Qué debo corregir primero?`
- Permite escribir instrucciones en lenguaje natural
- Botón de envío integrado visualmente con el estilo del plugin

---

## SESIÓN 2 — REFERENCIAS, ARCHIVOS Y RECURSOS (Columna izquierda, abajo)

### Sistema de Pestañas
- Permite múltiples categorías: Referencias de Audio, Enlaces Útiles, Notas, Recursos
- **Estructura extensible**

### Lista de Archivos
- Muestra: nombre, duración, formato, frecuencia de muestreo
- Controles: Play, Pause, Stop, Nivel
- Acciones: Importar, Eliminar, Reemplazar, Organizar

### Zona Drag & Drop
- Área para arrastrar archivos
- Formatos aceptados: WAV, AIFF, FLAC, MP3

---

## SESIÓN 3 — ORGANIZACIÓN, ANÁLISIS Y SUGERENCIAS (Columna derecha)

### Agrupación de Pistas
- Botones de filtro: `TIPO` | `COLOR` | `BUS`
- Permite cambiar dinámicamente la vista
- **No son categorías obligatorias**

### Estructura Jerárquica
```
Proyecto
 ├── Bus (Drums Bus)
 │   ├── Pista (Kick)
 │   ├── Pista (Snare)
 │   └── Pista (Hi-Hat)
 ├── Bus (Bass Bus)
 │   └── Pista (Bass DI)
 └── Bus (Vocals Bus)
     └── Pista (Lead Vocal)
```

### Cards de Pista

Cada pista contiene:

| Elemento | Descripción |
|----------|-------------|
| **Estado** | Indicador visual: Correcto 🟢 / Advertencia 🟡 / Problema 🔴 |
| **Nombre** | Texto del track |
| **Medidor** | Barra de nivel (Peak, RMS, LUFS, ganancia relativa) |
| **Métrica** | Valor numérico en dB (ej: -6 dB, -12 dB) |
| **Sugerencia IA** | Acción recomendada (ej: "Sube 1 dB en 60 Hz") |

### Control de Expansión
- Expandir Todo / Colapsar Todo
- Expansión individual por grupo (bus)

---

## SESIÓN 4 — BARRA DE ESTADO INFERIOR

| Elemento | Ejemplos |
|----------|----------|
| Fase Actual | Organización, Balance, EQ, Compresión |
| Género Detectado | Rock, Pop, Trap, Reggaeton, EDM, Hip Hop |
| Target LUFS | -14 LUFS, -10 LUFS, -8 LUFS (configurable) |
| Sample Rate | 44.1 kHz, 48 kHz, 96 kHz (automático del proyecto) |
| Configuración | Acceso rápido a preferencias |

---

## 🔤 Tipografía

| Elemento | Estilo |
|----------|--------|
| Headers | Uppercase, morado `#A855F7`, tracking amplio |
| Labels | Gris tenue, secundario |
| Valores técnicos | Blancos, limpios, precisos |

---

## ✨ Efectos Visuales

- **Dark mode premium** — fondo `#05080D`
- **Glassmorphism sutil** en paneles `#091018`
- **Sombras suaves** y **bordes discretos** `rgba(255,255,255,0.08)`
- **Iluminación neón violeta** `#A855F7`
- **Jerarquía visual clara**
- **Espaciado amplio** — optimizado para sesiones largas de mezcla
- **Tipografía moderna** altamente legible

---

## ⚠️ PRINCIPIO FUNDAMENTAL

> La referencia es **solo visual**. NO reutilizar literalmente:
> - Valores numéricos (dB, LUFS)
> - Nombres de pistas (Kick, Snare, etc.)
> - Buses específicos
> - Géneros
> - Sugerencias de EQ
>
> **Todo el contenido debe generarse dinámicamente** desde el motor de análisis real del plugin.
