# 03 - UI & UX PRINCIPLES

> **Los principios de interfaz y experiencia de usuario de MixCoach.**
> Define como se ve, como se siente, como se comporta y como se revela la interfaz.
>
> **Version:** 3.0 | **Ultima actualizacion:** 29 junio 2026
**Lectura complementaria obligatoria:** `workspace_memory/08_CHAT_COMMANDED_UI.md` (Chat-Commanded UI)
> **Cambios:** Renombrado desde 03_UI_GUIDELINES.md. Anadidas 6 reglas de oro de UI, Progressive Disclosure, alineacion con 02_USER_EXPERIENCE.md.

---

## 1. Las 6 Reglas de Oro de UI

Estas reglas gobiernan TODA decision de UI. Si un diseno viola cualquiera de estas reglas, el diseno esta equivocado.

### Regla #1 - La conversacion siempre manda

> **Nunca la interfaz.**

El chat es el centro de la experiencia. El usuario se comunica con el Coach. El Coach decide que herramientas abrir. La interfaz nunca debe interrumpir, distraer o competir con la conversacion.

**Implementacion:**
- El chat ocupa el 60% del espacio en la pantalla principal
- El input de texto siempre esta visible y enfocable
- Las notificaciones del sistema aparecen en el chat, no en popups
- Ninguna herramienta se abre sin que el Coach la solicite

### Regla #2 - El Coach abre las herramientas

> **Las herramientas nunca interrumpen al Coach.**

El usuario no busca paneles. El usuario habla con el Coach. Si hace falta un analizador, el Coach lo abre. Si hace falta el Mix Map, el Coach lo muestra. Las herramientas son extensiones del mentor, no un tablero de control.

**Implementacion:**
- No hay botones de "abrir analyzers" en la pantalla principal
- Cuando el Coach menciona un problema, ofrece un boton "Ver evidencia" que abre el analyzer
- Los toolsets aparecen contextualmente, no permanentemente

### Regla #3 - Toda herramienta aparece cuando el Coach la necesita

> **Ningun elemento UI existe antes de que la conversacion lo requiera.**

La referencia no existe hasta que el Coach pregunta por ella. El Mix Map no existe hasta que los Messengers estan listos. Los analizadores no se abren hasta que hay un problema que mostrar.

**Implementacion:**
- STATE 0: Solo chat. Nada mas.
- STATE 3: Aparece el DropZone de referencia.
- STATE 6: Aparece Session Mode (MixMap).
- STATE 7: Se abren analyzers contextualmente.

### Regla #4 - Cuando una tarea termina, se colapsa

> **Nunca desaparece completamente.**

Los bloques completados se convierten en un resumen colapsado: "Referencia: Afrobeat.wav". El usuario puede expandirlos para ver el historial, pero no ocupan espacio innecesario.

**Implementacion:**
- Bloques colapsables tipo Notion/Linear
- Check + titulo de la tarea completada
- Click para expandir detalles
- El bloque nunca se elimina - siempre accesible

### Regla #5 - El usuario nunca debe sentirse perdido

> **Siempre hay un "Que hago ahora?" visible.**

Cada pantalla, cada estado, cada momento debe responder tres preguntas: Que esta pasando? Por que? Que hago ahora?

**Implementacion:**
- El chat siempre muestra el ultimo mensaje del Coach
- El Coach siempre termina con un "Que te parece?" o "Prueba esto"
- Los estados loading/empty/error muestran texto informativo
- Nunca hay una pantalla en blanco sin contexto

### Regla #6 - Siempre debe existir un siguiente paso

> **El Coach nunca se queda en silencio.**

Despues de cada recomendacion, el Coach propone el siguiente paso. Despues de cada accion del usuario, el Coach reacciona. La conversacion nunca termina hasta que el usuario decide terminar la sesion.

**Implementacion:**
- Toda respuesta del Coach termina con un siguiente paso
- Hay una barra de progreso que muestra la etapa actual
- El Dashboard muestra "Tu Proximo Paso"
- Al final de la sesion, el reporte muestra el camino recorrido

---

## 2. Progressive Disclosure en UI

> **La interfaz nunca aparece completa. La interfaz se construye conforme avanza la conversacion.**

### Mapeo de Estados vs Elementos UI

| STATE | Elementos Visibles | Elementos Ocultos |
|:------|:-------------------|:-------------------|
| **STATE 0: WELCOME** | Chat + Avatar | Referencia, Session, Analyzers, Report |
| **STATE 1: INTENTION** | Chat + Botones Mix/Master | Session, Analyzers, Report |
| **STATE 2: GENRE** | Chat + Chips genero | Analyzers, Report |
| **STATE 3: REFERENCE** | Chat + DropZone + Reference | Analyzers, Report |
| **STATE 4: MESSENGERS** | Chat + MessengerList | Report |
| **STATE 5: MAPPING** | Chat + Session Mode (MixMap) | Report |
| **STATE 6: COACHING** | Chat + Session Mode + Tools | Report |
| **STATE 7: EVIDENCE** | Chat + Session + Analyzers abiertos | Report |
| **STATE 8: REPORT** | Overlay Report + Chat oculto | - |

### Principio de Bloques

Cada etapa completada genera un bloque colapsable:

```
STATE 3 completado -> [Referencia: Afrobeat.wav] (colapsado)
STATE 4 completado -> [Messengers: 8/8 detectados] (colapsado)
STATE 5 completado -> [Mix Map: Confirmado] (colapsado)
```

El chat mantiene el historial completo en formato conversacional. Los bloques colapsables son el resumen visual.

---

## 3. Filosofia UX

> **El usuario nunca debe sentirse dentro de un software tecnico. Debe sentirse acompanado por un mentor.**

MixCoach NO es un plugin de medicion. Es un mentor de mezcla que usa un plugin como medio de comunicacion.

Cada pixel en pantalla debe responder a una de estas preguntas:

| Pregunta | Implicacion |
|:---------|:------------|
| Esto ayuda al usuario a mezclar mejor? | Si no, sacalo |
| Esto educa al usuario? | Si no, replantealo |
| Esto reduce la friccion? | Si no, simplificalo |
| Esto es informacion o ruido? | Si es ruido, eliminarlo |

**Principio fundamental:** Menos es mas. Cada elemento adicional tiene que justificar su existencia.

---

## 4. El Chat es el Protagonista

### 4.1 Regla Absoluta

> **La conversacion es el centro de la experiencia. Los analizadores son herramientas de apoyo, no el producto principal.**

### 4.2 Jerarquia de Pantallas

```
1. CHAT (CoachChat)             -> 60% del espacio   <- EL PROTAGONISTA
2. Dashboard/Session/MixMap     -> 20% del espacio   <- CONTEXTO
3. Analyzers (Spectrum, VU...)  -> 10% del espacio   <- EVIDENCIA
4. Reference (Comparison)       -> 10% del espacio   <- NORTE
```

### 4.3 El chat siempre visible

- El chat **nunca** se oculta completamente
- En cualquier pantalla, el chat debe ser accesible con 1 clic
- El input de texto siempre esta visible y enfocable
- Las respuestas del coach siempre aparecen en el chat, no en popups

### 4.4 Comportamiento del Coach en el Chat

- Las respuestas del coach son **conversacionales**, no diagnosticos tecnicos
- Una respuesta = una idea, no un parrafo de 20 lineas
- Despues de cada respuesta, el coach sugiere el **siguiente paso**
- Si el usuario no responde en 30 segundos, el coach puede preguntar si necesita ayuda
- El coach **nunca** dice "Error:", "Warning:" o muestra numeros sin contexto
- El coach usa emojis con moderacion: objetivo para prioridades, check para logros, pregunta para preguntas

---

## 5. Jerarquia Visual

### 5.1 Regla del 60-30-10

```
60% -> Contenido principal (chat)
30% -> Soporte contextual (dashboard, mixmap)
10% -> Herramientas de apoyo (analyzers, reference)
```

### 5.2 Regla de Una Accion Primaria

> **Cada pantalla tiene EXACTAMENTE una accion primaria.**

| Pantalla | Accion Primaria |
|:---------|:----------------|
| Chat | Escribir mensaje |
| Dashboard | Continuar con el proximo paso |
| Session / MixMap | Explorar / Confirmar |
| Analyzers | (Solo observar / Ver evidencia) |
| Reference | Cargar / Comparar |
| Report | Exportar resumen |

### 5.3 Regla de los Tres Clicks

> Cualquier funcionalidad debe ser accesible en maximo 3 clics desde cualquier pantalla.

---

## 6. Las Tres Preguntas

> **Cada pantalla debe responder estas tres preguntas en menos de 2 segundos:**

### 6.1 Que esta pasando?

El titulo o indicador principal responde esto inmediatamente.

```
"Analizando tu mezcla..."       -> Fase 1 detectada
"Revisando dinamica del kick"   -> Accion en curso
"MixScore: 0.72"                -> Numero sin contexto (INCORRECTO)
```

### 6.2 Por que?

Un breve texto contextual explica la razon detras del estado actual.

```
"Detecte que el crest del kick esta bajo. Puede sonar plano."
"Tu mezcla tiene buena separacion estereo. Ahora trabajemos en profundidad."
(silencio) -> INCORRECTO
"Crest: 4.2dB. OffTarget: -8dB" -> INCORRECTO
```

### 6.3 Que hago ahora?

Cada pantalla ofrece el siguiente paso accionable.

```
"Prueba subir 2dB el send de reverb en la voz"
"Toca cualquier pista en el MixMap para ver sus metricas"
(sin siguiente paso visible) -> INCORRECTO
Menu con 12 opciones -> INCORRECTO
```

---

## 7. Layout y Navegacion

### 7.1 Estructura General (3 Modos)

```
+------------------------------------------------------------------+
|  [MIXCOACH]    [FASE: Setup]    [GENERO: Pop]                     |  <- Top Bar
|  [Referencia: Afrobeat.wav 72% ███████░░░]                        |  <- Ref Progress
+------------------------------------------------------------------+
|                                                                   |
|  +--------------------------------------------------------------+ |
|  |                     CONTENT AREA                             | |
|  |                                                              | |
|  |  (1) COACH MODE - Chat + bloques colapsables                 | |
|  |  (2) SESSION MODE - MixMap + TrackFeed + Roles               | |
|  |  (3) TOOLS MODE - Analyzers: Spectrum, VU, Phase...          | |
|  |                                                              | |
|  +--------------------------------------------------------------+ |
|                                                                   |
+------------------------------------------------------------------+
|  [Gain Staging ████████░░ 42%]   [Proximo: Balance]              |  <- Progress Bar
+------------------------------------------------------------------+
```

### 7.2 Transiciones entre Modos

- El Coach controla cuando se cambia de modo
- Boton "Volver al Coach" en Session y Tools
- Crossfade 150ms entre modos
- Sin sidebar fijo - el Coach es la navegacion

---

## 8. Analizadores como Herramientas de Apoyo

### 8.1 Proposito

Los analizadores (FFT, LUFS, fase, estereo) existen para que el usuario **vea lo que el coach esta viendo**, no para que sea un ingeniero de monitoreo.

### 8.2 Reglas de Analizadores

- **Los analizadores nunca muestran datos sin contexto.** Un FFT sin anotaciones de frecuencia es ruido.
- **Los analizadores nunca son la pantalla principal.** Siempre hay una pantalla mas importante (chat).
- **Los analizadores no tienen controles complejos.** No hay selectores de FFT size, window type, etc.
- **Los analizadores deben poder entenderse en 3 segundos.**

### 8.3 Elementos Prohibidos en Analizadores

| Prohibido | Alternativa |
|:----------|:------------|
| Numeros sin etiqueta | "LUFS: -14.2" -> "Volumen general: -14.2 LUFS (ideal para streaming)" |
| FFT sin anotaciones | Marcar rango de sub, kick, voz, air |
| Selectores de resolucion | FFT size fijo (2048 o 4096) |
| Ejes sin unidades | Siempre mostrar dB, Hz, ms |
| Colores sin leyenda | Tooltip al hover o leyenda minimalista |

### 8.4 Timing de Analizadores

- Meter bars: update 60fps (SmoothValue)
- FFT: update ~10fps
- Vectorscope: update 30fps
- Phase correlation: update 30fps

---

## 9. Micro-interacciones y Animaciones

### 9.1 Reglas

- **Toda interaccion debe tener feedback visual** en < 50ms
- **Las animaciones duran entre 100ms y 200ms.** Nada mas lento
- **No hay animaciones decorativas** (spinners que giran sin proposito, particulas, etc.)
- **Los numeros cambian con transicion**, nunca saltan (SmoothValue)

### 9.2 Micro-interacciones obligatorias

| Elemento | Feedback | Tiempo |
|:---------|:---------|:-------|
| Boton click | Scale bounce 0.95->1.0 | 100ms |
| Chip/tag | Fill color shift | 100ms |
| Hover en clickable | Cursor pointer + glow sutil | 50ms |
| Mensaje nuevo en chat | Fade in + slide up | 150ms |
| Cambio de modo | Crossfade entre modos | 150ms |
| Score o metrica | SmoothValue tween | 200ms |
| Error temporal | Glow rojo 500ms, luego fade | 500ms |

### 9.3 Prohibido en Animaciones

| Prohibido | Razon |
|:----------|:-------|
| Particulas | Consumo CPU, distrae, no aporta informacion |
| Animaciones looping | Consume GPU, el usuario ya lo vio |
| Spinners sin timeout | Bloquea la interaccion si falla |
| Slide de 500ms+ | Se siente lento |
| Animaciones que bloquean input | El usuario debe poder seguir trabajando |

---

## 10. Paleta y Estilo Visual

### 10.1 Tema

MixCoach usa un tema **oscuro premium** con acentos neon sutiles. No es un tema gamer, es un tema de estudio profesional.

### 10.2 Paleta

```
--bg-dark:        #0D0D14    Fondo principal
--bg-panel:       #141420    Paneles y tarjetas
--bg-surface:     #1A1A2A    Superficies elevadas
--bg-hover:       #222236    Hover states
--text-primary:   #E8E8F0    Texto principal
--text-secondary: #9898B0    Texto secundario
--text-muted:     #686880    Texto deshabilitado
--accent-neon:   #A855F7    Acento principal (purpura)
--accent-green:   #00D68F    Exito/optimo
--accent-amber:   #FFB547    Warning
--accent-red:     #FF4757    Error/critico
--glass:          rgba(255,255,255,0.03)  Glassmorphism
```

### 10.3 Reglas de Color

- **Nunca usar colores sin significado semantico.** Cada color comunica algo.
- **Acento principal (purpura):** elementos activos, botones primarios, bordes de panel activo
- **Verde:** pistas optimas, scores altos, confirmacion
- **Ambar:** warnings, atencion necesaria
- **Rojo:** errores, clipping, problemas criticos (usar con moderacion)
- **No mas de 3 colores por pantalla** (sin contar blanco/grises)
- **No usar gradientes fuertes.** Los gradientes son sutiles (< 10% de cambio)

### 10.4 Glassmorphism

Usar `MixCoachTheme::fillGlassPanel()` para paneles. Es el unico patron de panel permitido.

---

## 11. Responsive y Escalado

### 11.1 Estrategia

- **Layout fluido.** Los paneles se expanden/contraen, no se reposicionan
- **Font size fijo.** No escala con resolucion
- **Min width:** 800px. Por debajo, ocultar analizadores, mostrar solo chat
- **Max width:** sin limite. El contenido se centra con margenes

### 11.2 Breakpoints internos

| Width | Comportamiento |
|:------|:---------------|
| < 800px | Solo chat. Analizadores ocultos |
| 800-1024px | Chat + 1 panel lateral |
| 1024-1440px | Chat + dashboard + mixmap |
| > 1440px | Layout completo: chat, dashboard, mixmap, analyzers |

---

## 12. Estados de UI

Cada componente debe manejar estos estados:

### 12.1 Estados obligatorios

| Estado | Que se muestra | Tiempo maximo |
|:-------|:---------------|:--------------|
| **Loading** | Esqueleto o shimmer, nunca blank | < 500ms, si > 500ms: spinner + texto |
| **Empty** | Mensaje informativo + accion sugerida | Inmediato |
| **Active** | Datos actualizados | 60fps |
| **Stale** | Datos viejos + opacidad reducida | El engine decide |
| **Error** | Mensaje amigable + opcion de reintentar | Inmediato |
| **Offline** | Indicador + el resto funciona con datos cacheados | Inmediato |

### 12.2 Estados Prohibidos

```
// INCORRECTO
if (data == nullptr) {
    return; // Blank screen, el usuario no sabe que paso
}

// CORRECTO
if (data == nullptr) {
    showLoadingState("Esperando datos de las pistas...");
    return;
}
```

---

## 13. Copy y Microcopy

### 13.1 Reglas de Texto en UI

- **Nunca usar jerga tecnica sin explicacion.** "Crest factor" -> "Rango dinamico (crest)"
- **Nunca usar numeros sin contexto.** "MixScore: 72" -> no existe. O no se muestra, o se traduce.
- **Botones: verbo + objeto.** "Exportar reporte", "Cargar referencia"
- **Errores: que paso + que hacer.** "No se pudo cargar la referencia. Prueba con otro archivo WAV."
- **Titulos de pantalla: sustantivo.** "Dashboard", "Progreso"
- **Tooltips: explican, no repiten.** Boton de exportar -> tooltip: "Exportar reporte como HTML"

### 13.2 Tono

```
Usuario principiante:   "El kick tiene buen golpe pero pierde cuerpo en 60Hz"
Usuario avanzado:       "El kick necesita mas presencia en el rango fundamental (60Hz)"
Ambos:                  Explicacion clara + valor tecnico opcional
```

---

## 14. Prohibiciones de UI

| Prohibicion | Razon | Alternativa |
|:------------|:------|:------------|
| Score numerico visible | El usuario no debe sentirse calificado | Indicador cualitativo |
| Tablas de datos | Parecen debugger | Visualizaciones + contexto |
| Menus contextuales complejos | Friccion | Acciones visibles |
| Scroll horizontal | Rompe la experiencia | Layout responsivo vertical |
| Popups de confirmacion | Interrumpen el flujo | Acciones reversibles (undo) |
| Ventanas flotantes | Se pierden | Paneles integrados |
| Rangos de frecuencia sin etiqueta | "200-400 Hz" no significa nada | "Rango de nasalidad (200-400 Hz)" |
| Debug info en release | Numeros de slot, indices, IDs | Solo texto descriptivo |
| Terminos como "isOptimal", "severity" | Son terminos de engine | Traducir a lenguaje musical |
| Layout que cambia sin transicion | Confunde al usuario | Crossfade 150ms |

---

## 14. Regla #7 — PanelReveal System (Chat-Commanded UI)

> **Ningun elemento UI aparece sin que el Coach lo solicite.**

Ver documento completo: `workspace_memory/08_CHAT_COMMANDED_UI.md`

### 14.1 Principio

El chat es el unico punto de entrada. Cuando el Coach dice "carga tu referencia", aparece el Reference Panel. Cuando dice "activo los analizadores", se desbloquean Tools. Nada existe antes de que el Coach lo necesite.

### 14.2 Reglas

1. **Keyword Reveal:** Palabras clave en el mensaje del Coach disparan la revelacion del panel correspondiente
2. **Track Highlight:** Cuando el Coach menciona un track, se resalta en la Messenger List con glow del dominio del problema
3. **SetupStep Reveal:** El estado del setup dialogue tambien controla revelaciones

### 14.3 Mapa de Keywords

| Keyword en mensaje del Coach | Panel que se revela |
|:-----------------------------|:--------------------|
| "referencia", "carga tu..." | Reference Panel |
| "messenger", "pista", "track" | Messenger List |
| "mapa", "routing", "bus" | Mix Map / Session |
| "analizador", "espectro", "frecuencia" | Tools (Spectrum) |
| "fase", "phase", "correlacion" | Tools (Phase Scope) |
| "LUFS", "loudness", "master meter" | Master Meter |
| "progreso", "historial", "sesion" | Session / Progress |
| "reporte", "finalizar" | Report |

### 14.4 Track Highlight Colors

| Dominio | Color | Uso |
|:--------|:------|:----|
| gain | Rojo #EF4444 | Nivel incorrecto |
| tonal | Naranja #F97316 | EQ/Balance tonal |
| dynamics | Amarillo #F59E0B | Compresion |
| spatial | Cyan #00B7FF | Fase/Stereo |
| masking | Purpura #A855F7 | Enmascaramiento |

---

## 15. Checklist de UI

Antes de mergear cualquier cambio en UI:

- [ ] **La pantalla responde las 3 preguntas?** (Que, Por que, Que hago)
- [ ] **Hay una sola accion primaria?**
- [ ] **El chat sigue siendo accesible con 1 clic?**
- [ ] **Los analizadores tienen contexto?**
- [ ] **No hay numeros sin etiqueta?**
- [ ] **Las animaciones duran < 200ms?**
- [ ] **El layout funciona en 800px y 1440px?**
- [ ] **Maneja loading, empty, error, active?**
- [ ] **No hay terminos tecnicos sin traduccion?**
- [ ] **Se uso SmoothValue para metricas animadas?**
- [ ] **No hay tablas, scores visibles, o debug info?**

---

*Documento de principios de UI/UX - MixCoach v2.0 - 29 junio 2026*
*Renombrado desde 03_UI_GUIDELINES.md. Anadidas 6 reglas de oro + Progressive Disclosure.*
