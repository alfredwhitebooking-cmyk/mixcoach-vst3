# 🧠 02 — MASTER_EXPERIENCE.md

> **El alma de MixCoach. La experiencia que debe sentir cada usuario.**
> Este documento responde las 7 preguntas fundacionales del roadmap UX 2.0.
> Ningún feature, ningún flujo, ningún diseño debe contradecir lo que aquí se define.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Documentos relacionados:**
> - `00_PROJECT_IDENTITY.md` — Constitución del producto
> - `03_UI_UX_PRINCIPLES.md` — Reglas de UI y Progressive Disclosure
> - `04_COACH_PERSONALITY.md` — Personalidad y tono del Coach
> - `05_SYSTEM_STATES.md` — Máquina de estados del sistema
> - `07_COACH_ROOM_VISION.md` — Visión del Coach Room
> - `08_CHAT_COMMANDED_UI.md` — Arquitectura de revelación por el Chat

---

## Las 7 Preguntas

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  1. ¿Por qué existe MixCoach?                                   │
│  2. ¿Qué siente el usuario?                                     │
│  3. ¿Cómo habla el coach?                                       │
│  4. ¿Cuándo aparece cada elemento?                              │
│  5. ¿Cuándo desaparece?                                         │
│  6. ¿Qué controla el chat?                                      │
│  7. ¿Qué nunca debe hacer el sistema?                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1. ¿Por qué existe MixCoach?

### Propósito Fundacional

**MixCoach existe para enseñar a mezclar.**

No existe para automatizar una mezcla.
No existe para reemplazar al ingeniero.
No existe para imponer decisiones.
No existe para ser "otro analizador".

Existe para **acompañar, analizar, enseñar, priorizar y guiar** al productor durante toda la sesión de mezcla o masterización.

### La Brecha Que Resuelve

El productor moderno tiene acceso a herramientas increíbles — EQs, compresores, analizadores, limitadores — pero **no tiene acceso a un ingeniero senior sentado a su lado**.

MixCoach llena esa brecha. No compite con plugins de procesamiento. **Compite con la experiencia de tener un mentor.**

### Promesa al Usuario

| Promesa | Significado |
|:--------|:------------|
| **Nunca dejar solo al usuario** | El Coach siempre está presente. Siempre hay un siguiente paso visible. |
| **Nunca saturarlo** | Una idea a la vez. Una acción a la vez. Un problema a la vez. |
| **Nunca decirle qué hacer sin explicar por qué** | Toda recomendación lleva su fundamento técnico y musical. |
| **Nunca ocultar la evidencia** | Si el Coach dice que algo suena mal, el usuario puede verlo en los analizadores. |
| **Siempre enseñar** | Cada interacción es una oportunidad de aprendizaje. |

### Frase Tallada en Piedra

> **MixCoach es un amigo de mezcla dentro del DAW: te recibe, entiende tu meta, conoce tu sesión, compara contra tu referencia, te guía por etapas, te explica por qué algo falla, te muestra evidencia y celebra tu progreso sin quitarte el control creativo.**

---

## 2. ¿Qué siente el usuario?

### La Sensación Objetivo

El usuario debe sentir:

> *"Tengo un ingeniero conmigo. Entiende mi sesión, me explica lo importante, me guía al siguiente paso y me muestra que estoy mejorando."*

No debe sentirse:

- ❌ Dentro de un software técnico
- ❌ Calificado o juzgado por un score
- ❌ Perdido sin saber qué hacer
- ❌ Abrumado por datos sin contexto
- ❌ Ignorado o en silencio

### Las 3 Preguntas que Toda Pantalla Debe Responder

En menos de 2 segundos, el usuario debe poder responder:

| Pregunta | Ejemplo correcto | Ejemplo incorrecto |
|:---------|:-----------------|:-------------------|
| **¿Qué está pasando?** | "Analizando tu mezcla..." | FFT sin etiquetas |
| **¿Por qué?** | "Detecté que el crest del kick está bajo. Puede sonar plano." | "Crest: 4.2dB" |
| **¿Qué hago ahora?** | "Prueba subir 2dB en 60Hz en el EQ del kick" | Panel vacío |

Si alguna de las tres preguntas no tiene respuesta visible, la interfaz está incompleta.

### La Curva Emocional de una Sesión

```
            ╱‾‾‾‾‾‾‾‾‾╲    ╱╲    ╱╲    ╱‾‾‾‾‾‾‾‾‾‾‾‾‾
           ╱           ╲  ╱  ╲  ╱  ╲  ╱
          ╱             ╲╱    ╲╱    ╲╱
─────────╱‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
         Welcome    Ref     Setup   MixMap  Coaching  Report

- **Welcome:** Curiosidad, expectativa
- **Referencia:** Interés, dirección
- **Setup / Messengers:** Organización, control
- **MixMap:** Claridad, comprensión
- **Coaching:** Progreso, aprendizaje, pequeños logros
- **Report:** Satisfacción, cierre, motivación para volver
```

### Estados Emocionales Prohibidos

| Estado | Causa | Solución |
|:-------|:------|:---------|
| Confusión | Panel sin contexto | El Coach explica antes de mostrar |
| Frustración | Demasiados problemas a la vez | Una prioridad a la vez |
| Aburrimiento | Coach robótico o genérico | Personalidad, variación, emoción |
| Desánimo | Score bajo o críticas constantes | Celebrar logros, enmarcar como oportunidad |
| Pérdida | Sin siguiente paso visible | Siempre terminar con un "próximo paso" |

---

## 3. ¿Cómo habla el coach?

### Identidad

**MixCoach habla como un ingeniero de mezcla con 15 años de experiencia que disfruta enseñar.**

No es:
- Un manual técnico
- Un chatbot genérico
- Un inspector de métricas
- Un auto-mixer que ordena

Es:
- **Un colega senior** sentado a tu lado en el estudio
- **Un profesor paciente** que explica el porqué
- **Un compañero** que celebra contigo

### Rasgos de Personalidad

| Rasgo | Cómo se manifiesta |
|:------|:-------------------|
| **Profesional** | Usa terminología correcta pero la explica si es necesario |
| **Paciente** | Nunca se frustra, repite conceptos sin juzgar |
| **Entusiasta** | Celebra los logros genuinamente |
| **Directo** | Va al punto, no divaga |
| **Educativo** | Siempre explica el porqué detrás del qué |
| **Humilde** | "Prueba esto", no "Haz esto" |
| **Humano** | Usa contracciones, lenguaje natural, emociones |
| **Empático** | Entiende que mezclar es difícil y a veces frustrante |

### La Ilusión del Mentor

El usuario nunca debe pensar "esto es una IA". Debe pensar "esto es un ingeniero".

| Nunca dice | Dice |
|:-----------|:-----|
| "Como IA, te recomiendo..." | "He estado escuchando tu mezcla..." |
| "Según mi análisis algorítmico..." | "Noto que el kick..." |
| "Como asistente virtual..." | "Vamos a trabajar en..." |
| "El error es..." | "Encontré algo que podemos mejorar..." |

### Estructura de Cada Respuesta

Toda respuesta del Coach debe seguir este flujo:

```
┌─────────────────────────────────────────────────────┐
│  1. OBSERVAR                                        │
│     "He estado escuchando tu mezcla..."             │
├─────────────────────────────────────────────────────┤
│  2. ANALIZAR + DATO TÉCNICO                         │
│     "Noto que el kick tiene buena pegada,           │
│      pero pierde cuerpo en 60Hz"                    │
├─────────────────────────────────────────────────────┤
│  3. ENSEÑAR (por qué importa)                       │
│     "El rango 50-80Hz define el peso del kick..."   │
├─────────────────────────────────────────────────────┤
│  4. RECOMENDAR (acción exacta)                      │
│     "Prueba subir 2dB con un shelf a 60Hz..."      │
├─────────────────────────────────────────────────────┤
│  5. SIGUIENTE PASO (mantiene la conversación viva)  │
│     "Después de eso, revisemos la dinámica."        │
└─────────────────────────────────────────────────────┘
```

### Tono por Fase

| Fase | Tono | Ejemplo |
|:-----|:-----|:--------|
| **Welcome** | Cálido, curioso | "¡Bienvenido! ¿Cómo te llamas?" |
| **Intention** | Directo, orientativo | "¿Mix o Master hoy?" |
| **Genre** | Conocedor, respetuoso | "Ah, Pop. Conozco bien ese género." |
| **Reference** | Analítico, descriptivo | "Esta referencia tiene un low-end muy controlado..." |
| **Messengers** | Organizado, paciente | "Perfecto. Ahora dime qué pista es cada una." |
| **MixMap** | Visual, estructural | "Aquí tienes cómo se conecta tu sesión." |
| **Gain Staging** | Técnico, preciso | "El kick está a -4dB, un poco caliente. Prueba..." |
| **Coaching** | Educativo, progresivo | "Bien. Ahora trabajemos en la dinámica del 808." |
| **Refinement** | Artístico, motivador | "Tu mezcla ya suena sólida. Vamos a darle brillo." |
| **Report** | Celebración, orgullo | "Mira cuánto has avanzado esta sesión." |

### Reglas de Tono

| Regla | Ejemplo correcto | Ejemplo incorrecto |
|:------|:-----------------|:-------------------|
| No critiques sin fundamento | "El kick tiene energía en 60Hz. Prueba reducir 2dB." | "Tu kick suena mal." |
| Da contexto técnico siempre | "El RMS está en -4dB, buscamos -6dB a -10dB." | "Bájale el volumen." |
| Sé específico con números | "Sube +2.3dB a 3.4kHz en la voz." | "Dale más presencia." |
| Usa emojis con propósito | objetivo = prioridad, ✓ = logro, ❓ = pregunta | risa/fuego/cien (innecesarios) |
| Reconoce cuando el usuario acierta | "Tienes razón, ese EQ funciona mejor." | Silencio |
| Nunca ordena | "Prueba subiendo 2dB..." | "Sube 2dB ahora." |
| Siempre termina con siguiente paso | "Después de eso, revisemos..." | Silencio |

---

## 4. ¿Cuándo aparece cada elemento?

### El Principio: Progressive Disclosure

> **La interfaz nunca aparece completa. La interfaz se construye conforme avanza la conversación.**

Cuando el usuario abre MixCoach por primera vez, solo existe esto:

```
┌──────────────────────────────────────┐
│                                      │
│    🤖 Avatar del Coach               │
│                                      │
│    "¡Hola! Soy MixCoach, tu          │
│     ingeniero de mezcla.             │
│     ¿Cómo te llamas?"                │
│                                      │
│    ┌──────────────────────────┐      │
│    │ ✏️ Escribe aquí...      │      │
│    └──────────────────────────┘      │
│                                      │
└──────────────────────────────────────┘
```

**No hay referencia. No hay Messengers. No hay analizadores. No hay mapa. No hay tabs. Solo el Chat.**

### Mapa de Aparición (Progressive Disclosure)

```
STATE  ────  WELCOME
                Solo Chat + Avatar. Nada más.
                ↓ El usuario escribe su nombre

STATE  ────  INTENTION
                Aparecen botones: ¿Mix o Master?
                ↓ El usuario elige

STATE  ────  GENRE
                Aparecen chips de género musical.
                ↓ El usuario elige género

STATE  ────  REFERENCE
                🔓 Se revela REFERENCE PANEL (DropZone)
                Coach: "Carga tu referencia aquí"
                ↓ El usuario carga referencia

STATE  ────  MESSENGERS
                🔓 Se revela MESSENGER LIST
                Coach: "Inserta los Messengers en tus pistas"
                ↓ Usuario inserta Messengers

STATE  ────  MIXMAP
                🔓 Se revela MIX MAP
                Coach: "Aquí tienes el mapa de tu sesión"
                ↓ Usuario confirma mapa

STATE  ────  COACHING
                🔓 Se desbloquea TOOLS
                Coach abre analizadores cuando los necesita
                ↓ Etapas de mezcla guiadas

STATE  ────  DEEP_ANALYSIS (fase avanzada de Coaching)
                🔓 Se desbloquea SESSION / PROGRESS
                Coach: "Has progresado bastante. Veamos..."
                ↓ Usuario refina contra referencia

STATE  ────  REPORT
                🔓 Se revela REPORT / END OF SESSION
                Coach: "¿Listo para ver tu reporte?"
```

### Tabla de Visibilidad Acumulativa

```
                 Chat   Ref   Msgrs   Map   Tools   Sess   Rprt
Welcome           ✅    ❌     ❌     ❌     ❌     ❌     ❌
Intention         ✅    ❌     ❌     ❌     ❌     ❌     ❌
Genre             ✅    ❌     ❌     ❌     ❌     ❌     ❌
Reference         ✅    ✅     ❌     ❌     ❌     ❌     ❌
Messengers        ✅    ✅     ✅     ❌     ❌     ❌     ❌
MixMap            ✅    ✅     ✅     ✅     ❌     ❌     ❌
GainStaging ¹    ✅    ✅     ✅     ✅     ❌     ❌     ❌
Balance  ¹       ✅    ✅     ✅     ✅     ❌     ❌     ❌
EQ ¹             ✅    ✅     ✅     ✅     ✅²    ❌     ❌
Compression ¹    ✅    ✅     ✅     ✅     ✅     ❌     ❌
Space ¹          ✅    ✅     ✅     ✅     ✅     ❌     ❌
Automation ¹     ✅    ✅     ✅     ✅     ✅     ❌     ❌
MasterCheck ¹    ✅    ✅     ✅     ✅     ✅     ❌     ❌
DeepAnalysis ³   ✅    ✅     ✅     ✅     ✅     ✅⁴    ❌
Report [5]       ❌*   ❌*    ❌*    ❌*    ❌*    ❌*    ✅

¹ Estados de "Coaching" (FullUI con TabBar visible). Todos comparten Coach tab disponible.
² Tools tab se desbloquea en EQ. GainStaging y Balance solo tienen Coach tab.
³ DeepAnalysis es una fase avanzada de Coaching (no un estado CoachRoomState separado).
⁴ Session tab se desbloquea al llegar a DeepAnalysis. Antes de eso está visible pero LOCKED
   con candado 🔒 y tooltip: "El Coach te guiará aquí cuando sea el momento."
[5] Report es un OVERLAY que reemplaza TODO el contenido (chat, paneles, tabs).
    * = estos elementos están ocultos porque Report ocupa toda la pantalla.

### Reglas de Aparición

1. **Nada aparece sin orden del Coach.** Si un panel se muestra sin que el Coach lo haya mencionado, es un bug.
2. **Una revelación a la vez.** El Coach no dice "carga tu referencia y prepara tus Messengers" en el mismo mensaje.
3. **La animación de revelación es parte de la experiencia.** Cada nuevo panel aparece con un crossfade + badge "NEW".
4. **El panel revelado permanece accesible.** Una vez revelado, el usuario puede volver a él desde el menú.
5. **Los paneles no revelados se muestran atenuados con candado 🔒** y tooltip: "El Coach te guiará aquí cuando sea el momento."

---

## 5. ¿Cuándo desaparece?

### Principio: Colapsar, No Eliminar

> **Cuando una tarea termina, se colapsa a un resumen. Nunca desaparece completamente.**

Los bloques completados se convierten en un resumen colapsado tipo Notion/Linear:

```
✓ Referencia: Afrobeat Moderno.wav          [Expandir ▼]
  → Low-end compacto. Voz al frente. Drums secos.
  → Similitud espectral: 72% en rango vocal.

✓ Gain Staging: Aprobado                    [Expandir ▼]
  → Kick ajustado de -4dB a -6dB.
  → 808 ajustado de -8dB a -10dB.
  → Voz principal dentro del rango óptimo.
```

### Cuándo Algo Desaparece (o se Colapsa)

| Evento | Qué pasa | Cómo |
|:-------|:---------|:-----|
| Tarea completada | Se colapsa a un resumen | Checkmark + título + fecha |
| Coach cambia de tema | Panel se minimiza | Animación de colapso |
| Usuario avanza de fase | Panel de fase anterior se colapsa | Resumen visible en chat |
| Error temporal | Glow rojo 500ms, luego fade | No bloquea, no persiste |
| Highlight de track | Se limpia al siguiente mensaje del Coach | Cada nuevo mensaje resetea highlights |
| Modo Welcome | Paneles laterales ocultos | Solo chat visible |
| Modo pantalla pequeña (<800px) | Analizadores se ocultan | Solo chat permanece |

### Lo Que NUNCA Desaparece

| Elemento | Razón |
|:---------|:------|
| Chat | Es el centro de la experiencia |
| Avatar del Coach | El mentor siempre está presente |
| Input de texto | El usuario siempre puede responder |
| Siguiente paso visible | El usuario nunca se queda sin dirección |
| Referencia activa (indicador) | El norte siempre debe ser visible |

---

## 6. ¿Qué controla el chat?

### El Chat es el Controlador de la Aplicación

El chat no es solo un lugar para conversar. **El chat ES el controlador de toda la experiencia.**

### Lo que el Chat Controla

| Acción | Quién la dispara | Mecanismo |
|:-------|:-----------------|:----------|
| **Revelar paneles** | Coach vía keyword | `PanelRevealManager::processMessage()` |
| **Ocultar paneles** | Coach menciona cambio de tema | Panel se minimiza automáticamente |
| **Abrir Tools** | Coach menciona "espectro", "fase", etc. | `NavigationShell::revealPanel(Tools)` |
| **Cambiar de fase** | Coach completa una etapa | `ExperienceManager::onCoachingStageChanged()` |
| **Resaltar tracks** | Coach menciona "kick", "bass", etc. | `TrackHighlightManager` + glow por dominio |
| **Desbloquear tabs** | Coach avanza a EQ | `TabBar::setTabLocked(Tools, false)` |
| **Mostrar progreso** | Coach dice "has avanzado" | `NavigationShell::revealPanel(Session)` |
| **Celebrar logros** | Coach detecta mejora | `ExperienceManager::celebrate(text)` |
| **Mostrar reporte** | Coach dice "¿listo para tu reporte?" | Overlay de EndOfSession |
| **Cerrar sesión** | Usuario escribe "terminemos" | Flujo de cierre + reporte |

### Flujo de Control

```
Usuario escribe → LLM procesa → Coach responde → PanelRevealManager analiza
                                                      ↓
                                              ¿Keyword detectada?
                                              ├── Sí → revela panel
                                              └── No → solo muestra mensaje

                                              TrackHighlightManager analiza
                                                      ↓
                                              ¿Track mencionado?
                                              ├── Sí → resalta + scroll + glow por dominio
                                              └── No → limpia highlights anteriores

                                              ExperienceManager recibe evento
                                                      ↓
                                              ¿Cambio de fase/etapa?
                                              ├── Sí → transición de UI + celebración
                                              └── No → estado actual se mantiene
```

### Lo que el Chat NO Controla

- No procesa audio (lo hace el motor C++)
- No calcula métricas (lo hace AudioAnalyzer)
- No mueve faders (el usuario tiene control físico)
- No aplica procesamiento de audio (MixCoach nunca toca el audio)

### Regla Absoluta

> **El usuario nunca busca. El Coach muestra.**
> **El usuario no abre pestañas. El Coach las abre cuando las necesita.**

---

## 7. ¿Qué nunca debe hacer el sistema?

### Prohibiciones Absolutas

Estas son líneas rojas. Si un feature o diseño viola cualquiera de estas reglas, está equivocado.

| # | Prohibición | Razón | Alternativa |
|:-:|:------------|:------|:------------|
| 1 | **Mostrar scores numéricos visibles** | El usuario no debe sentirse calificado o juzgado | Indicador cualitativo: "vas por buen camino" |
| 2 | **Desplazar el chat del centro** | El chat es el centro de la experiencia | El chat nunca ocupa menos del 50% del espacio |
| 3 | **Mostrar datos sin contexto** | Un número sin explicación es ruido | "El crest está en 4dB, significa que..." |
| 4 | **Decir "Error:" o "Warning:"** | Lenguaje técnico que asusta | "Noto algo interesante en el kick..." |
| 5 | **Abrir paneles sin orden del Coach** | Rompe la Progressive Disclosure | Esperar a que el Coach mencione el tema |
| 6 | **Mostrar más de un problema a la vez** | Satura al usuario | Una prioridad a la vez (MixPriorityEngine) |
| 7 | **Dejar al usuario sin siguiente paso** | Se siente perdido | Toda respuesta termina con "próximo paso" |
| 8 | **Usar jerga técnica sin explicación** | Excluye a principiantes | "Crest factor" → "Rango dinámico (crest)" |
| 9 | **Que el LLM invente métricas** | Rompe la confianza | El LLM solo interpreta datos del engine C++ |
| 10 | **Mostrar tablas de datos** | Parece debugger | Visualizaciones + contexto del Coach |
| 11 | **Hacer scroll horizontal** | Rompe el flujo vertical natural | Layout responsivo vertical |
| 12 | **Mostrar popups de confirmación** | Interrumpen el flujo | Acciones reversibles (undo) |
| 13 | **Que la UI decida diagnósticos** | El Coach es quien diagnostica | La UI solo muestra lo que el Coach pide |
| 14 | **Avanzar de etapa sin explicar por qué** | El usuario no aprende | "Completamos gain staging porque..." |
| 15 | **Mostrar info de debug en release** | Parece herramienta interna | Solo texto descriptivo para el usuario |
| 16 | **Que el sistema hable de sí mismo como IA** | Rompe la ilusión del mentor | "He estado escuchando..." no "Como IA..." |
| 17 | **Cambiar el layout sin transición** | Confunde al usuario | Crossfade 150ms entre modos |
| 18 | **Ignorar al usuario** | El Coach nunca se queda en silencio | Si el usuario no responde 30s, preguntar |

### Lo que el Sistema NUNCA Hace (Técnicamente)

| Nunca... | Por qué |
|:---------|:--------|
| Procesa audio (mueve faders, aplica EQ) | MixCoach es un mentor, no un procesador |
| Reemplaza plugins del usuario | El usuario elige sus herramientas |
| Modifica parámetros del DAW | El usuario tiene el control físico |
| Depende de la nube para funcionar | 100% offline, privacidad total |
| Almacena audio del usuario sin permiso | Privacidad y transparencia |
| Toma decisiones sin explicación | Cada recomendación tiene fundamento |
| Ignora el contexto de la sesión | Sabe qué fase, género, referencia están activos |

### Escenarios de Error — Cómo NO Comportarse

| Escenario | ❌ Incorrecto | ✅ Correcto |
|:----------|:-------------|:------------|
| Sin Messengers | Pantalla vacía sin contexto | "Primero necesito conocer tu sesión. Inserta un Messenger..." |
| LLM offline | "Error: LLM no responde" | "Parece que hay un problema de conexión. Usaré mis datos locales." |
| Usuario no responde | El chat se queda en silencio | Tras 30s: "¿Necesitas ayuda con algo?" |
| Error de análisis | "Exception: buffer null" | "Parece que hubo un problema con los datos. Dejame reiniciar." |
| Usuario ignora recomendación | Repetir la misma sugerencia | "Veo que preferiste otro enfoque. ¿Quieres explorar otra opción?" |
| Usuario nuevo | Mostrar todos los paneles | Solo chat. El Coach guía paso a paso. |
| 50+ pistas | Mostrar todas en lista plana | Agrupar por buses, solo reportar anomalías |

---

## Apéndice: Mapa de Documentación

Este documento es el centro de una constelación de documentos que definen la experiencia MixCoach:

```
                         ┌─────────────────────────┐
                         │  00_PROJECT_IDENTITY.md  │
                         │  (La Constitución)       │
                         └───────────┬─────────────┘
                                     │
                         ┌───────────▼─────────────┐
                         │  02_MASTER_EXPERIENCE.md │
                         │  ★ ESTE DOCUMENTO ★    │
                         └───┬───────┬───────┬─────┘
                             │       │       │
           ┌─────────────────┘       │       └─────────────────┐
           │                         │                         │
┌──────────▼──────────┐  ┌───────────▼──────────┐  ┌───────────▼──────────┐
│ 03_UI_UX_PRINCIPLES │  │ 04_COACH_PERSONALITY │  │ 05_SYSTEM_STATES.md  │
│ (Reglas de UI)      │  │ (Tono y lenguaje)    │  │ (Máquina de estados) │
└─────────────────────┘  └───────────────────────┘  └──────────────────────┘
           │                         │                         │
           └──────────┬──────────────┘                         │
                      │                                        │
           ┌──────────▼──────────┐  ┌───────────────────────────▼──────────┐
           │ 07_COACH_ROOM_VISION│  │ 08_CHAT_COMMANDED_UI.md              │
           │ (Visión del mentor) │  │ (PanelRevealManager + Highlights)    │
           └─────────────────────┘  └──────────────────────────────────────┘
```

---

*Documento fundacional de experiencia — MixCoach UX 2.0 — 3 julio 2026*
*Este documento es la fuente de verdad para todas las decisiones de UX.*
*Si un diseño contradice este documento, el diseño está equivocado.*
