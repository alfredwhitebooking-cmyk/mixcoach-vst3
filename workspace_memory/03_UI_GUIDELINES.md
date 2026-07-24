# 🎨 03 — UI GUIDELINES

> **La constitución visual de MixCoach. Define cómo se ve, cómo se siente y cómo se comporta la interfaz.**
>
> Ningún agente debe modificar la UI sin leer primero este documento.
>
> **Versión:** 1.0 | **Última actualización:** 26 junio 2026

---

## 📋 Índice

1. [Filosofía UX](#1-filosofía-ux)
2. [El Chat es el Protagonista](#2-el-chat-es-el-protagonista)
3. [Jerarquía Visual](#3-jerarquía-visual)
4. [Las Tres Preguntas](#4-las-tres-preguntas)
5. [Layout y Navegación](#5-layout-y-navegación)
6. [Analizadores como Herramientas de Apoyo](#6-analizadores-como-herramientas-de-apoyo)
7. [Micro-interacciones y Animaciones](#7-micro-interacciones-y-animaciones)
8. [Paleta y Estilo Visual](#8-paleta-y-estilo-visual)
9. [Responsive y Escalado](#9-responsive-y-escalado)
10. [Estados de UI](#10-estados-de-ui)
11. [Copy y Microcopy](#11-copy-y-microcopy)
12. [Prohibiciones de UI](#12-prohibiciones-de-ui)
13. [Checklist de UI](#13-checklist-de-ui)

---

## 1. Filosofía UX

> **El usuario nunca debe sentirse dentro de un software técnico. Debe sentirse acompañado por un mentor.**

MixCoach NO es un plugin de medición. Es un mentor de mezcla que usa un plugin como medio de comunicación.

Cada píxel en pantalla debe responder a una de estas preguntas:

| Pregunta | Implicación |
|:---------|:------------|
| ¿Esto ayuda al usuario a mezclar mejor? | Si no, sácalo |
| ¿Esto educa al usuario? | Si no, replantéalo |
| ¿Esto reduce la fricción? | Si no, simplifícalo |
| ¿Esto es información o ruido? | Si es ruido, elimínalo |

**Principio fundamental:** Menos es más. Cada elemento adicional tiene que justificar su existencia.

---

## 2. El Chat es el Protagonista

### 2.1 Regla Absoluta

> **La conversación es el centro de la experiencia. Los analizadores son herramientas de apoyo, no el producto principal.**

### 2.2 Jerarquía de Pantallas

```
1. CHAT (MixCoachPanel)       → 60% del espacio   ← EL PROTAGONISTA
2. Dashboard                   → 20% del espacio   ← EL PRÓXIMO PASO
3. MixMap (MixMapComponent)    → 10% del espacio   ← EL CONTEXTO
4. Analyzers (AnalyzersPanel)  → 5% del espacio    ← SOPORTE
5. Reference (ReferencePanel)  → 5% del espacio    ← SOPORTE
```

### 2.3 El chat siempre visible

- El chat **nunca** se oculta completamente
- En cualquier pantalla, el chat debe ser accesible con 1 clic
- El input de texto siempre está visible y enfocable
- Las respuestas del coach siempre aparecen en el chat, no en popups

### 2.4 Comportamiento del Coach en el Chat

- Las respuestas del coach son **conversacionales**, no diagnósticos técnicos
- Una respuesta = una idea, no un párrafo de 20 líneas
- Después de cada respuesta, el coach sugiere el **siguiente paso**
- Si el usuario no responde en 30 segundos, el coach puede preguntar si necesita ayuda
- El coach **nunca** dice "Error:", "Warning:" o muestra números sin contexto
- El coach usa emojis con moderación: 🎯 para prioridades, ✅ para logros, 🤔 para preguntas

---

## 3. Jerarquía Visual

### 3.1 Regla del 60-30-10

```
60% → Contenido principal (chat)
30% → Soporte contextual (dashboard, mixmap)
10% → Herramientas de apoyo (analyzers, reference)
```

### 3.2 Regla de Una Acción Primaria

> **Cada pantalla tiene EXACTAMENTE una acción primaria.**

| Pantalla | Acción Primaria |
|:---------|:----------------|
| Chat | Escribir mensaje |
| Dashboard | Continuar con el próximo paso |
| MixMap | Seleccionar pista |
| Analyzers | (Solo observar) |
| Reference | Cargar/Comparar referencia |
| Report | Exportar resumen |

### 3.3 Regla de los Tres Clicks

> Cualquier funcionalidad debe ser accesible en máximo 3 clics desde cualquier pantalla.

---

## 4. Las Tres Preguntas

> **Cada pantalla debe responder estas tres preguntas en menos de 2 segundos:**

### 4.1 ¿Qué está pasando?

El título o indicador principal responde esto inmediatamente.

```
✅ "Analizando tu mezcla..."       → Fase 1 detectada
✅ "Revisando dinámica del kick"   → Acción en curso
❌ "MixScore: 0.72"                → Número sin contexto
❌ "FFT display"                   → No dice nada
```

### 4.2 ¿Por qué?

Un breve texto contextual explica la razón detrás del estado actual.

```
✅ "Detecté que el crest del kick está bajo. Puede sonar plano."
✅ "Tu mezcla tiene buena separación estéreo. Ahora trabajemos en profundidad."
❌ (silencio)
❌ "Crest: 4.2dB. OffTarget: -8dB"
```

### 4.3 ¿Qué hago ahora?

Cada pantalla ofrece el siguiente paso accionable.

```
✅ "Prueba subir 2dB el send de reverb en la voz"
✅ "Toca cualquier pista en el MixMap para ver sus métricas"
❌ (sin siguiente paso visible)
❌ Menú con 12 opciones
```

---

## 5. Layout y Navegación

### 5.1 Estructura General

```
┌──────────────────────────────────────────────────────┐
│ Sidebar (60px)  │   Content Area (resto)              │
│ ┌────────┐      │   ┌────────────────────────────┐    │
│ │ 🏠     │      │   │  Tab Content                │    │
│ │ 💬     │      │   │                            │    │
│ │ 📊     │      │   │                            │    │
│ │ 📈     │      │   │                            │    │
│ │ 🎯     │      │   │                            │    │
│ │ 📋     │      │   └────────────────────────────┘    │
│ └────────┘      │                                       │
└──────────────────────────────────────────────────────┘
```

### 5.2 Sidebar

- Ancho fijo: ~60px
- Iconos + tooltip, sin texto
- Sección activa: highlight + acento de color
- Tooltip al hover: "Chat", "Dashboard", "MixMap", etc.
- El sidebar está **siempre visible**

### 5.3 Navegación

- **Sin pestañas.** La navegación es por sidebar, no por tabs
- **Sin menús desplegables** en la navegación principal
- **Sin modales** que bloqueen la interacción (si es posible)
- **Sin back button.** El sidebar lleva a cualquier pantalla directamente

### 5.4 Transiciones

- **150ms crossfade** entre pantallas (ya implementado en NavigationShell)
- **No sliding** ni animaciones de 300ms+ que ralenticen
- **No scroll horizontal** bajo ninguna circunstancia

---

## 6. Analizadores como Herramientas de Apoyo

### 6.1 Propósito

Los analizadores (FFT, LUFS, fase, estéreo) existen para que el usuario **vea lo que el coach está viendo**, no para que sea un ingeniero de monitoreo.

### 6.2 Reglas de Analizadores

- **Los analizadores nunca muestran datos sin contexto.** Un FFT sin anotaciones de frecuencia es ruido.
- **Los analizadores nunca son la pantalla principal.** Siempre hay una pantalla más importante (chat, dashboard).
- **Los analizadores no tienen controles complejos.** No hay selectores de FFT size, window type, etc.
- **Los analizadores deben poder entenderse en 3 segundos.**

### 6.3 Elementos Prohibidos en Analizadores

| Prohibido | Alternativa |
|:----------|:------------|
| ❌ Números sin etiqueta | "LUFS: -14.2" → "Volumen general: -14.2 LUFS (ideal para streaming)" |
| ❌ FFT sin anotaciones | Marcar rango de sub, kick, voz, air |
| ❌ Selectores de resolución | FFT size fijo (2048 o 4096) |
| ❌ Ejes sin unidades | Siempre mostrar dB, Hz, ms |
| ❌ Colores sin leyenda | Tooltip al hover o leyenda minimalista |

### 6.4 Timing de Analizadores

- Meter bars: update 60fps (SmoothValue)
- FFT: update ~10fps (no más rápido, es información, no video)
- Vectorscope: update 30fps
- Phase correlation: update 30fps

---

## 7. Micro-interacciones y Animaciones

### 7.1 Reglas

- **Toda interacción debe tener feedback visual** en < 50ms
- **Las animaciones duran entre 100ms y 200ms.** Nada más lento
- **No hay animaciones decorativas** (spinners que giran sin propósito, partículas, etc.)
- **Los números cambian con transición**, nunca saltan (SmoothValue)

### 7.2 Micro-interacciones obligatorias

| Elemento | Feedback | Tiempo |
|:---------|:---------|:-------|
| Botón click | Scale bounce 0.95→1.0 | 100ms |
| Chip/tag | Fill color shift | 100ms |
| Hover en clickable | Cursor pointer + glow sutil | 50ms |
| Mensaje nuevo en chat | Fade in + slide up | 150ms |
| Cambio de fase | Crossfade entre paneles | 150ms |
| Score o métrica | SmoothValue tween | 200ms |
| Error temporal | Glow rojo 500ms, luego fade | 500ms |

### 7.3 Prohibido en Animaciones

| Prohibido | Razón |
|:----------|:-------|
| ❌ Partículas | Consumo CPU, distrae, no aporta información |
| ❌ Animaciones looping | Consume GPU, el usuario ya lo vio |
| ❌ Spinners sin timeout | Bloquea la interacción si falla |
| ❌ Slide de 500ms+ | Se siente lento |
| ❌ Animaciones que bloquean input | El usuario debe poder seguir trabajando |

---

## 8. Paleta y Estilo Visual

### 8.1 Tema

MixCoach usa un tema **oscuro premium** con acentos neón sutiles. No es un tema gamer, es un tema de estudio profesional.

### 8.2 Paleta

```css
--bg-dark:        #0D0D14    /* Fondo principal */
--bg-panel:       #141420    /* Paneles y tarjetas */
--bg-surface:     #1A1A2A    /* Superficies elevadas */
--bg-hover:       #222236    /* Hover states */
--text-primary:   #E8E8F0    /* Texto principal */
--text-secondary: #9898B0    /* Texto secundario */
--text-muted:     #686880    /* Texto deshabilitado */
--accent-neon:    #6C5CE7    /* Acento principal (púrpura) */
--accent-green:   #00D68F    /* Éxito/óptimo */
--accent-amber:   #FFB547    /* Warning */
--accent-red:     #FF4757    /* Error/crítico */
--glass:          rgba(255,255,255,0.03)  /* Glassmorphism */
```

### 8.3 Reglas de Color

- **Nunca usar colores sin significado semántico.** Cada color comunica algo.
- **Acento principal (púrpura):** elementos activos, botones primarios, bordes de panel activo
- **Verde:** pistas óptimas, scores altos, confirmación
- **Ámbar:** warnings, atención necesaria
- **Rojo:** errores, clipping, problemas críticos (usar con moderación)
- **No más de 3 colores por pantalla** (sin contar blanco/grises)
- **No usar gradientes fuertes.** Los gradientes son sutiles (< 10% de cambio)

### 8.4 Tipografía

- **Inter** para UI (variable font, disponible en Google Fonts)
- 3 tamaños: 9px (etiquetas), 11px (cuerpo), 13px (títulos)
- **No bold en cuerpos de texto.** Bold solo en títulos y etiquetas
- **No cursiva.** No monoespaciada (excepto números de meter)

### 8.5 Glassmorphism

Usar `MixCoachTheme::fillGlassPanel()` para paneles. Es el único patrón de panel permitido.

---

## 9. Responsive y Escalado

### 9.1 Escalado del Plugin

MixCoach debe verse bien desde 800x600 (mínimo absoluto) hasta 3840x2160.

### 9.2 Estrategia

- **Layout fluido.** Los paneles se expanden/contraen, no se reposicionan
- **Font size fijo.** No escala con resolución
- **Min width:** 800px. Por debajo, ocultar analizadores, mostrar solo chat
- **Max width:** sin límite. El contenido se centra con márgenes
- **Sidebar:** siempre 60px, no escala

### 9.3 Breakpoints internos

| Width | Comportamiento |
|:------|:---------------|
| < 800px | Solo chat + sidebar. Analizadores ocultos |
| 800-1024px | Chat + 1 panel lateral |
| 1024-1440px | Chat + dashboard + mixmap |
| > 1440px | Layout completo: chat, dashboard, mixmap, analyzers |

---

## 10. Estados de UI

Cada componente debe manejar estos estados:

### 10.1 Estados obligatorios

| Estado | Qué se muestra | Tiempo máximo |
|:-------|:---------------|:--------------|
| **Loading** | Esqueleto o shimmer, nunca blank | < 500ms, si > 500ms: spinner + texto |
| **Empty** | Mensaje informativo + acción sugerida | Inmediato |
| **Active** | Datos actualizados | 60fps |
| **Stale** | Datos viejos + opacidad reducida | El engine decide |
| **Error** | Mensaje amigable + opción de reintentar | Inmediato |
| **Offline** | Indicador + el resto funciona con datos cacheados | Inmediato |

### 10.2 Estados Prohibidos

```cpp
// ❌ INCORRECTO
if (data == nullptr) {
    return; // Blank screen, el usuario no sabe qué pasó
}

// ✅ CORRECTO
if (data == nullptr) {
    showLoadingState("Esperando datos de las pistas...");
    return;
}
```

---

## 11. Copy y Microcopy

### 11.1 Reglas de Texto en UI

- **Nunca usar jerga técnica sin explicación.** "Crest factor" → "Rango dinámico (crest)"
- **Nunca usar números sin contexto.** "MixScore: 72" → no existe. O no se muestra, o se traduce.
- **Botones: verbo + objeto.** "Exportar reporte", "Cargar referencia", no "OK", "Aceptar"
- **Errores: qué pasó + qué hacer.** "No se pudo cargar la referencia. Prueba con otro archivo WAV."
- **Títulos de pantalla: sustantivo.** "Dashboard", "Progreso", no "Pantalla de inicio"
- **Tooltips: explican, no repiten.** Botón de exportar → tooltip: "Exportar reporte como HTML", no "Exportar"

### 11.2 Tono

```
Usuario principiante:   "El kick tiene buen golpe pero pierde cuerpo en 60Hz"
Usuario avanzado:       "El kick necesita más presencia en el rango fundamental (60Hz)"
Ambos:                  Explicación clara + valor técnico opcional
```

---

## 12. Prohibiciones de UI

| Prohibición | Razón | Alternativa |
|:------------|:------|:------------|
| ❌ Score numérico visible | El usuario no debe sentirse calificado | Indicador cualitativo: "Bien", "Necesita trabajo" |
| ❌ Tablas de datos | Parecen debugger | Visualizaciones + contexto |
| ❌ Menús contextuales complejos | Fricción, el usuario busca, no navega | Acciones visibles |
| ❌ Scroll horizontal | Rompe toda la experiencia | Layout responsivo vertical |
| ❌ Popups de confirmación | Interrumpen el flujo | Acciones reversibles (undo) |
| ❌ Ventanas flotantes | Se pierden, el usuario no las encuentra | Paneles integrados |
| ❌ Rangos de frecuencia sin etiqueta | "200-400 Hz" no significa nada al productor | "Rango de nasalidad (200-400 Hz)" |
| ❌ Debug info en release | Números de slot, índices, IDs | Solo texto descriptivo |
| ❌ Términos como "isOptimal", "severity" | Son términos de engine, no de usuario | Traducir a lenguaje musical |
| ❌ Frecuencia de update inconsistente | Meters a 60fps y FFT a 10fps está bien, pero no cambiar sin razón | Documentar frecuencias |
| ❌ Layout que cambia sin transición | Confunde al usuario | Crossfade 150ms |

---

## 13. Checklist de UI

Antes de mergear cualquier cambio en UI:

- [ ] **¿La pantalla responde las 3 preguntas?** (Qué, Por qué, Qué hago)
- [ ] **¿Hay una sola acción primaria?**
- [ ] **¿El chat sigue siendo accesible con 1 clic?**
- [ ] **¿Los analizadores tienen contexto?**
- [ ] **¿No hay números sin etiqueta?**
- [ ] **¿Las animaciones duran < 200ms?**
- [ ] **¿El layout funciona en 800px y 1440px?**
- [ ] **¿Maneja loading, empty, error, active?**
- [ ] **¿No hay términos técnicos sin traducción?**
- [ ] **¿Se usó SmoothValue para métricas animadas?**
- [ ] **¿No hay tablas, scores visibles, o debug info?**

---

*Documento de guías de UI — MixCoach — 26 junio 2026*
*Todo cambio en UI debe pasar esta checklist antes de considerar la tarea completa.*
