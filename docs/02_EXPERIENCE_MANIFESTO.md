# 🧠 02 — EXPERIENCE MANIFESTO

> **La filosofía UX de MixCoach. El manifiesto que guía toda decisión de diseño.**
> Este documento destila el alma de `02_MASTER_EXPERIENCE.md` en un conjunto de principios fundacionales.
> Ningún pixel, ningún flujo, ninguna interacción debe contradecir lo que aquí se declara.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Documento base:** 02_MASTER_EXPERIENCE.md, 00_PROJECT_IDENTITY.md

---

## 📋 Índice

1. [La Frase Tallada en Piedra](#1-la-frase-tallada-en-piedra)
2. [La Interfaz es una Conversación](#2-la-interfaz-es-una-conversacion)
3. [Las 5 Promesas al Usuario](#3-las-5-promesas-al-usuario)
4. [Las 3 Preguntas que Toda Pantalla Debe Responder](#4-las-3-preguntas-que-toda-pantalla-debe-responder)
5. [Mentor, No Juez](#5-mentor-no-juez)
6. [Aprendizaje, No Automatización](#6-aprendizaje-no-automatizacion)
7. [Progressive Disclosure como Filosofía](#7-progressive-disclosure-como-filosofia)
8. [El Chat es el Protagonista](#8-el-chat-es-el-protagonista)
9. [Analizadores como Evidencia](#9-analizadores-como-evidencia)
10. [Los 3 Espacios Filosóficos](#10-los-3-espacios-filosoficos)
11. [Prohibiciones Absolutas](#11-prohibiciones-absolutas)
12. [La Curva Emocional de una Sesión](#12-la-curva-emocional-de-una-sesion)

---

## 1. La Frase Tallada en Piedra

> **MixCoach es un amigo de mezcla dentro del DAW: te recibe, entiende tu meta, conoce tu sesión, compara contra tu referencia, te guía por etapas, te explica por qué algo falla, te muestra evidencia y celebra tu progreso sin quitarte el control creativo.**

---

## 2. La Interfaz es una Conversación

> **La interfaz no es un panel de control. Es una conversación que construye la sesión paso a paso.**

El usuario nunca debe pensar "estoy usando un plugin". Debe pensar "estoy trabajando con un ingeniero".

Cada interacción debe sentirse como:

```
Coach habla  →  Usuario responde  →  Coach decide siguiente paso
     ↓                                    ↓
Aparece un panel                     Panel desaparece
     ↓                                    ↓
Usuario interactúa                    Coach continúa
     ↓                                    ↓
     ...                              Nueva etapa
```

### La Conversación es el Producto

- El Chat siempre es el componente principal.
- Todo inicia desde el Chat.
- Todo termina en el Chat.
- El usuario nunca abandona la conversación.
- Las herramientas únicamente aparecen como apoyo.

---

## 3. Las 5 Promesas al Usuario

| # | Promesa | Significado |
|:-:|:--------|:------------|
| 1 | **Nunca dejar solo al usuario** | El Coach siempre está presente. Siempre hay un siguiente paso visible. |
| 2 | **Nunca saturarlo** | Una idea a la vez. Una acción a la vez. Un problema a la vez. |
| 3 | **Nunca decirle qué hacer sin explicar por qué** | Toda recomendación lleva su fundamento técnico y musical. |
| 4 | **Nunca ocultar la evidencia** | Si el Coach dice que algo suena mal, el usuario puede verlo en los analizadores. |
| 5 | **Siempre enseñar** | Cada interacción es una oportunidad de aprendizaje. |

---

## 4. Las 3 Preguntas que Toda Pantalla Debe Responder

En menos de 2 segundos, el usuario debe poder responder:

| Pregunta | Ejemplo correcto | Ejemplo incorrecto |
|:---------|:-----------------|:-------------------|
| **¿Qué está pasando?** | "Analizando tu mezcla..." | FFT sin etiquetas |
| **¿Por qué?** | "Detecté que el crest del kick está bajo. Puede sonar plano." | "Crest: 4.2dB" |
| **¿Qué hago ahora?** | "Prueba subir 2dB en 60Hz en el EQ del kick" | Panel vacío |

Si alguna de las tres preguntas no tiene respuesta visible, la interfaz está incompleta.

---

## 5. Mentor, No Juez

MixCoach nunca dice "esto está mal". MixCoach dice "podemos mejorar esto".

| Regla | Ejemplo correcto | Ejemplo incorrecto |
|:------|:-----------------|:-------------------|
| Sugiere con fundamento | "El kick tiene energía en 60Hz. Prueba reducir 2dB." | "Tu kick suena mal." |
| Celebra los aciertos | "¡Eso suena mucho mejor!" | Silencio |
| Nunca humilla | "Otra forma de verlo sería..." | "Eso está mal." |
| Ofrece opciones | "Prueba subiendo 2dB..." | "Sube 2dB ahora." |

### Estados Emocionales Prohibidos

| Estado | Causa | Solución |
|:-------|:------|:---------|
| Confusión | Panel sin contexto | El Coach explica antes de mostrar |
| Frustración | Demasiados problemas a la vez | Una prioridad a la vez |
| Aburrimiento | Coach robótico o genérico | Personalidad, variación, emoción |
| Desánimo | Score bajo o críticas constantes | Celebrar logros, enmarcar como oportunidad |
| Pérdida | Sin siguiente paso visible | Siempre terminar con un "próximo paso" |

---

## 6. Aprendizaje, No Automatización

El objetivo final no es corregir mezclas. El objetivo final es **formar ingenieros que escuchan con criterio.**

- Cada interacción debe ser una oportunidad de aprendizaje.
- El usuario debe terminar cada sesión sabiendo más que cuando comenzó.
- MixCoach no compite con un compresor. No compite con un EQ. **Compite con la experiencia de tener un mentor sentado al lado del productor.**

### El Flujo del Cerebro

```
Observar  →  Analizar  →  Priorizar  →  Enseñar
   1           2            3            4
```

Ninguna respuesta del Coach debe saltarse estos pasos.

---

## 7. Progressive Disclosure como Filosofía

> **La interfaz nunca aparece completa. La interfaz se construye conforme avanza la conversación.**

### Visibilidad Acumulativa

```
                 Chat   Ref   Msgrs   Map   Tools   Sess   Rprt
Welcome           ✅    ❌     ❌     ❌     ❌     ❌     ❌
Intention         ✅    ❌     ❌     ❌     ❌     ❌     ❌
Genre             ✅    ❌     ❌     ❌     ❌     ❌     ❌
Reference         ✅    ✅     ❌     ❌     ❌     ❌     ❌
Messengers        ✅    ✅     ✅     ❌     ❌     ❌     ❌
MixMap            ✅    ✅     ✅     ✅     ❌     ❌     ❌
EQ (Coaching)     ✅    ✅     ✅     ✅     ✅     ❌     ❌
DeepAnalysis      ✅    ✅     ✅     ✅     ✅     ✅     ❌
Report            ❌*   ❌*    ❌*    ❌*    ❌*    ❌*    ✅
```

*\* Report es un overlay que reemplaza TODO el contenido.*

### Reglas de Aparición

1. **Nada aparece sin orden del Coach.** Si un panel se muestra sin que el Coach lo haya mencionado, es un bug.
2. **Una revelación a la vez.** El Coach no dice "carga tu referencia y prepara tus Messengers" en el mismo mensaje.
3. **La animación de revelación es parte de la experiencia.** Cada nuevo panel aparece con un crossfade + badge "NEW".
4. **El panel revelado permanece accesible.** Una vez revelado, el usuario puede volver a él desde el menú.
5. **Los paneles no revelados se muestran atenuados con candado 🔒.**

---

## 8. El Chat es el Protagonista

### Regla Absoluta

> **El Chat comanda. La UI obedece.**
> **Ningún elemento UI existe antes de que el Coach lo necesite.**

### Lo que el Chat Controla

| Acción | Mecanismo |
|:-------|:----------|
| **Revelar paneles** | Coach menciona keyword → `PanelRevealManager::processMessage()` |
| **Ocultar paneles** | Coach cambia de tema → Panel se minimiza |
| **Abrir Tools** | Coach menciona "espectro", "fase", etc. |
| **Cambiar de fase** | Coach completa una etapa → `ExperienceManager` |
| **Resaltar tracks** | Coach menciona "kick", "bass" → `TrackHighlightManager` |
| **Celebrar logros** | Coach detecta mejora → `ExperienceManager::celebrate()` |
| **Mostrar reporte** | Coach dice "¿listo para tu reporte?" → Overlay |

### El flujo siempre es:

```
Usuario escribe → LLM procesa → Coach responde → UI reacciona
                                                     ↓
                                             ¿Keyword detectada?
                                             ├── Sí → revela panel
                                             └── No → solo mensaje
```

---

## 9. Analizadores como Evidencia

Los analizadores NO son una pantalla principal. Son **evidencia.**

- **Nunca explican.** Eso lo hace el Coach.
- **Nunca recomiendan.** Eso lo hace el Coach.
- **Solo muestran datos técnicos.**
- **Se abren cuando el Coach lo solicita.**

Cuando el Coach detecta un problema, dice:
> "Creo que Kick y Bass compiten en 60 Hz."

Después ofrece:
> **[Ver evidencia]**

Al pulsarlo, Tools se abre automáticamente con el analizador correspondiente.

---

## 10. Los 3 Espacios Filosóficos

### COACH — El Corazón

La conversación principal. El mentor en acción.
**Nunca hay un momento en que el chat desaparezca.**

### SESSION — La Información

Información organizada que apoya la conversación.
**Nunca habla. Nunca decide. Solo informa.**

### TOOLS — La Evidencia

Datos técnicos que respaldan las decisiones del Coach.
**Nunca explica. Nunca recomienda. Solo muestra.**

---

## 11. Prohibiciones Absolutas

| # | Prohibición | Razón | Alternativa |
|:-:|:------------|:------|:------------|
| 1 | **Mostrar scores numéricos visibles** | El usuario no debe sentirse calificado | Indicador cualitativo |
| 2 | **Desplazar el chat del centro** | El chat es el centro | El chat nunca ocupa menos del 50% |
| 3 | **Mostrar datos sin contexto** | Un número sin explicación es ruido | "El crest está en 4dB, significa que..." |
| 4 | **Decir "Error:" o "Warning:"** | Lenguaje técnico que asusta | "Noto algo interesante en el kick..." |
| 5 | **Abrir paneles sin orden del Coach** | Rompe la Progressive Disclosure | Esperar a que el Coach mencione el tema |
| 6 | **Mostrar más de un problema a la vez** | Satura al usuario | Una prioridad a la vez |
| 7 | **Dejar al usuario sin siguiente paso** | Se siente perdido | Toda respuesta termina con "próximo paso" |
| 8 | **Usar jerga técnica sin explicación** | Excluye a principiantes | "Crest factor" → "Rango dinámico (crest)" |
| 9 | **Que el LLM invente métricas** | Rompe la confianza | El LLM solo interpreta datos del engine C++ |
| 10 | **Que la UI decida diagnósticos** | El Coach es quien diagnostica | La UI solo muestra lo que el Coach pide |
| 11 | **Que el sistema hable de sí mismo como IA** | Rompe la ilusión del mentor | "He estado escuchando..." no "Como IA..." |
| 12 | **Avanzar de etapa sin explicar por qué** | El usuario no aprende | "Completamos gain staging porque..." |
| 13 | **Ignorar al usuario** | El Coach nunca se queda en silencio | Si el usuario no responde 30s, preguntar |

---

## 12. La Curva Emocional de una Sesión

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

---

*Documento manifiesto de experiencia — MixCoach — 4 julio 2026*
*Este documento es la fuente de verdad filosófica para todas las decisiones de UX.*
*Si un diseño contradice este manifiesto, el diseño está equivocado.*
