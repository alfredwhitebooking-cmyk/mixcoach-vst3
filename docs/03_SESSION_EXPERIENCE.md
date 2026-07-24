# 🎭 03 — SESSION EXPERIENCE

> **La simulación completa de una sesión de MixCoach.**
> Esto no es documentación técnica. Es una obra de teatro en 3 actos.
> Cada escena describe QUÉ ve el usuario, QUÉ dice el Coach, QUÉ hace el usuario,
> QUÉ paneles aparecen/desaparecen, y CÓMO se siente.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Documentos relacionados:**
> - `02_EXPERIENCE_MANIFESTO.md` — Filosofía UX
> - `04_PHASES.md` — Todas las fases de la sesión
> - `05_UI_ARCHITECTURE.md` — Qué paneles existen y cuándo aparecen

---

## 📋 Índice

1. [Elenco](#1-elenco)
2. [ACTO I: Descubrimiento](#2-acto-i-descubrimiento)
   - [Escena 1: Welcome — Solo el Coach](#21-escena-1-welcome)
   - [Escena 2: Intención — ¿Mix o Master?](#22-escena-2-intencion)
   - [Escena 3: Género — El Contexto](#23-escena-3-genero)
   - [Escena 4: Referencia — El Norte](#24-escena-4-referencia)
   - [Escena 5: Setup — Los Messengers](#25-escena-5-setup)
3. [ACTO II: La Mezcla](#3-acto-ii-la-mezcla)
   - [Escena 6: MixMap — El Mapa](#31-escena-6-mixmap)
   - [Escena 7: Gain Staging — La Base](#32-escena-7-gain-staging)
   - [Escena 8: Balance — El Cimiento](#33-escena-8-balance)
   - [Escena 9: EQ — El Tono](#34-escena-9-eq)
   - [Escena 10: Compresión — La Dinámica](#35-escena-10-compresion)
   - [Escena 11: Espacio — La Profundidad](#36-escena-11-espacio)
   - [Escena 12: Master Check — La Verificación](#37-escena-12-master-check)
4. [ACTO III: El Cierre](#4-acto-iii-el-cierre)
   - [Escena 13: Refinamiento — El Arte](#41-escena-13-refinamiento)
   - [Escena 14: Reporte — El Recuerdo](#42-escena-14-reporte)
   - [Escena 15: Despedida — Hasta la Próxima](#43-escena-15-despedida)

---

## 1. Elenco

| Personaje | Descripción |
|:----------|:------------|
| **El Coach** | Ingeniero de mezcla con 15 años de experiencia. Paciente, profesional, entusiasta. Nunca dice "soy una IA". |
| **El Usuario** | Productor musical. Puede ser principiante o experimentado. Siempre tiene el control creativo. |
| **Los Paneles** | Reference, Messengers, MixMap, Analyzers, Progress, Report. Entran y salen cuando el Coach los necesita. |

---

## 2. ACTO I: Descubrimiento

La sesión comienza. El usuario acaba de abrir MixCoach en el canal master de FL Studio.

### 2.1 Escena 1: Welcome

#### Lo que ve el usuario

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│                    🤖 Avatar del Coach                      │
│                (Robot metálico, ojos cyan)                   │
│                                                              │
│                                                              │
│    ┌────────────────────────────────────────────────────┐    │
│    │                                                    │    │
│    │   ¡Hola! Soy MixCoach, tu ingeniero de mezcla.     │    │
│    │   ¿Cómo te llamas?                                 │    │
│    │                                                    │    │
│    └────────────────────────────────────────────────────┘    │
│                                                              │
│    ┌──────────────────────────────────────────────┐         │
│    │  ✏️ Escribe tu nombre aquí...                │         │
│    └──────────────────────────────────────────────┘         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

**No hay nada más.** No hay sidebar. No hay referencia. No hay analizadores. Solo el Coach y el chat.

#### Lo que NO existe (aún)

| Elemento | Estado |
|:---------|:-------|
| Reference Panel | ❌ No existe |
| Messenger List | ❌ No existe |
| Mix Map | ❌ No existe |
| Tools / Analyzers | ❌ No existe |
| Progress | ❌ No existe |
| Report | ❌ No existe |
| Sidebar tabs | ❌ No existe |

#### Coach dice

> *"¡Hola! Soy MixCoach, tu ingeniero de mezcla. ¿Cómo te llamas?"*

#### Usuario hace

Escribe su nombre: **"Alex"**

#### Cómo se siente

Curiosidad, expectativa. No hay complejidad abrumadora. Solo un saludo y una pregunta simple.

#### Transición

El nombre se guarda. El Coach lo usará durante toda la sesión. Transición suave a la siguiente escena.

---

### 2.2 Escena 2: Intención

#### Lo que ve el usuario

El mismo layout, pero ahora aparecen dos botones debajo del mensaje del Coach:

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│   🎛️ ¡Genial, Alex! ¿Qué vamos a hacer hoy?                │
│                                                              │
│        ┌──────────────┐     ┌────────────────┐              │
│        │   🎛 MIX     │     │    🎱 MASTER   │              │
│        └──────────────┘     └────────────────┘              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

#### Coach dice

> *"¡Genial, Alex! ¿Qué vamos a hacer hoy? ¿Vas a mezclar una canción o masterizar un tema?"*

#### Usuario hace

Clickea **🎛 MIX**

#### Cómo se siente

Empoderamiento. El usuario elige su camino. MixCoach se adapta.

#### Lo que cambia

- Se activa `CoachMode::Mix` en el engine
- Todos los targets, thresholds y lenguaje se configuran para modo mezcla
- Aparece la siguiente pregunta

---

### 2.3 Escena 3: Género

#### Lo que ve el usuario

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│   Perfecto, una mezcla. ¿Qué género vamos a trabajar?       │
│                                                              │
│   ┌────────┐ ┌──────────┐ ┌───────────┐ ┌──────────┐       │
│   │  POP   │ │   ROCK   │ │ REGGAETON │ │  HOUSE   │       │
│   └────────┘ └──────────┘ └───────────┘ └──────────┘       │
│   ┌────────┐ ┌──────────┐ ┌───────────┐ ┌──────────┐       │
│   │  HIP   │ │   LOFI   │ │   TRAP    │ │   OTRO   │       │
│   │  HOP   │ │          │ │           │ │          │       │
│   └────────┘ └──────────┘ └───────────┘ └──────────┘       │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

Los chips de género aparecen como sugerencias clickeables. También puede escribirlo si no está en la lista.

#### Coach dice

> *"Perfecto, una mezcla. ¿Qué género vamos a trabajar hoy?"*

#### Usuario hace

Selecciona **"REGGAETON"**

#### Cómo se siente

El usuario siente que el Coach conoce su mundo. Los targets de género se cargan.

#### Lo que cambia (invisible para el usuario)

- `GenreProfiles` carga targets específicos para reggaetón
- TrackRole thresholds se ajustan (crest target del kick en reggaetón: 10-14dB)
- ReferenceDrivenEngine se prepara

---

### 2.4 Escena 4: Referencia

#### Lo que ve el usuario

Aparece un nuevo elemento en la pantalla: el **Reference Panel** se desliza desde abajo con un crossfade suave y un badge "NEW" que parpadea.

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│   Reggaetón, excelente. Conozco bien ese género.            │
│                                                              │
│   Antes de empezar, necesito saber a qué quieres que        │
│   suene tu mezcla. ¿Tienes una canción de referencia?       │
│                                                              │
│   ┌──────────────────────────────────────────────────────┐  │
│   │  🔵 NUEVO — REFERENCIA                              │  │
│   │                                                      │  │
│   │  ┌──────────────────────────────────────────────┐   │  │
│   │  │                                              │   │  │
│   │  │   📁 Arrastra un archivo WAV/MP3 aquí        │   │  │
│   │  │                                              │   │  │
│   │  │        [ o pega un enlace de YouTube ]       │   │  │
│   │  │                                              │   │  │
│   │  └──────────────────────────────────────────────┘   │  │
│   └──────────────────────────────────────────────────────┘  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

#### Coach dice

> *"Antes de empezar, necesito saber a qué quieres que suene tu mezcla. ¿Tienes una canción de referencia? Puedes arrastrar un archivo WAV o pegar un enlace de YouTube."*

#### Usuario hace

Arrastra "Bad Bunny - Titi Me Pregunto.wav" a la DropZone.

#### Animación

1. Archivo entra en la dropzone → borde dashed se vuelve sólido con glow verde
2. Barra de progreso: "Analizando referencia..." con spinner
3. Tras ~2 segundos: el panel se colapsa a un resumen

```
   ✓ Referencia: Titi Me Pregunto — Analizada  [Expandir ▼]
     → Low-end compacto. Voz al frente. Drums secos.
     → Energía: Alta (82%)
     → Similitud espectral contigo: — (aún no hay mezcla)
```

#### Coach dice

> *"He analizado tu referencia. Esto es lo que veo: energía alta, low-end compacto, voz al frente. No vamos a copiarla. Vamos a entender por qué funciona y a buscar esa energía y pegada en tu mezcla."*

#### Cómo se siente

El usuario siente dirección. Ya hay un norte sonoro. El Coach no solo carga un archivo — lo entiende.

---

### 2.5 Escena 5: Setup (Messengers)

#### Lo que ve el usuario

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│   ✓ Referencia cargada. Ahora necesito conocer tu sesión.   │
│                                                              │
│   🔵 NUEVO — MESSENGER LIST                                 │
│   ┌──────────────────────────────────────────────────────┐  │
│   │  🎚 MESSENGERS DETECTADOS                            │  │
│   │                                                      │  │
│   │  🥁 Kick       ████████░░  -6.2dB    🟢 Saludable   │  │
│   │  🥁 Snare      ██████░░░░  -9.8dB    🟡 Atención    │  │
│   │  🥁 HiHat      ██████░░░░  -12.1dB   🟢 Saludable   │  │
│   │  🎸 808 Bass   ████████░░  -8.1dB    🟢 Saludable   │  │
│   │  🎤 Voz        █████████░  -4.2dB    🟠 Revisar     │  │
│   │  ...                                                  │  │
│   │                                                      │  │
│   │      [✅ Listo, continuemos]                         │  │
│   └──────────────────────────────────────────────────────┘  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

Aparece la Messenger List con todas las pistas detectadas. Cada pista muestra su nombre, nivel, y health dot.

#### Coach dice

> *"He detectado 6 pistas en tu sesión. Voy a asignar roles automáticamente basándome en su espectro. ¿El Kick es este? ¿El 808 es este? Confirma o ajusta los roles."*

#### Usuario hace

- Confirma los roles (todos correctos)
- Asigna buses: Kick → Drums Bus, 808 → Bass Bus, Voz → Vocal Bus
- Hace clic en "Listo, continuemos"

#### Cómo se siente

Organización, control. El usuario siente que el Coach está poniendo orden en su sesión.

#### Transición

Setup completado. Transición al ACTO II.

---

## 3. ACTO II: La Mezcla

### 3.1 Escena 6: MixMap

#### Lo que ve el usuario

```
   🔵 NUEVO — MIX MAP
   ┌──────────────────────────────────────────────────────┐
   │  📋 SESIÓN                                           │
   │                                                      │
   │  DRUMS BUS ──────────────────────────────────        │
   │  ├── 🥁 Kick     ████████░░  -6.2dB                 │
   │  ├── 🥁 Snare    ██████░░░░  -9.8dB                 │
   │  └── 🥁 HiHat    ██████░░░░  -12.1dB                │
   │                                                      │
   │  BASS BUS ──────────────────────────────────         │
   │  └── 🎸 808 Bass ████████░░  -8.1dB                 │
   │                                                      │
   │  VOCALS BUS ────────────────────────────────         │
   │  └── 🎤 Voz      █████████░  -4.2dB                 │
   │                                                      │
   │  ───────────────  MASTER BUS  ──────────────         │
   │                                                      │
   │            [✅ Confirmar Mapa]                       │
   └──────────────────────────────────────────────────────┘
```

#### Coach dice

> *"Aquí tienes el mapa de tu sesión. Así es como se conectan tus pistas. ¿Se ve correcto?"*

#### Usuario hace

Confirma el mapa.

#### Coach dice

> *"Mapa confirmado. Ahora sí, empecemos a trabajar."*

#### Transición a Gain Staging

El sidebar aparece por primera vez con 3 tabs: **Coach, Session, Tools**. Todos los tabs existen ahora, pero Session y Tools están bloqueados con candado 🔒.

El tab Coach está activo por defecto con el chat.

---

### 3.2 Escena 7: Gain Staging

#### Coach dice

> *"🔊 Empezamos con Gain Staging. El objetivo es simple: ajustar niveles para que nada clipee y todo tenga headroom saludable.*
>
> *• Picos entre -18dB y -12dB por pista*
> *• Master sin clipping*
> *• Sin procesar todavía — solo niveles.*
>
> *Veo que la Voz está a -4.2dB. Prueba bajarla a -10dB."*

#### Lo que ve el usuario

En la Messenger List, la pista **Voz** se resalta con un glow **🔴 rojo** (dominio: gain). Tooltip: "Voz: -4.2dB — sobre el target. Prueba bajar a -10dB."

#### Usuario hace

Baja el fader de la Voz en FL Studio.

#### Coach detecta

Tras el próximo ciclo de análisis (~8s), el engine detecta que la voz ahora está a -10dB.

#### Coach dice

> *"Bien. Ese ajuste ayudó. Ahora revisemos el Kick, que está a -6.2dB. Prueba bajarlo a -10dB también."*

#### Cuando todos los niveles están correctos

> *"✅ Gain Staging completado. Todos los niveles tienen headroom saludable. Pasamos a Balance."*

#### Cómo se siente

El usuario siente que está aprendiendo. Cada ajuste tiene un propósito y una explicación.

---

### 3.3 Escena 8: Balance

#### Coach dice

> *"🎚 Balance — Ahora vamos a equilibrar los niveles relativos entre instrumentos. El Kick y el 808 son la base. La voz debe estar al frente, no enterrada."*

#### Proceso

El Coach guía al usuario ajustando faders relativos, verificando en mono, paneando instrumentos.

#### Cuando el balance es correcto

> *"✅ Balance completado. Menos del 30% de pares desbalanceados. Pasamos a EQ."*

---

### 3.4 Escena 9: EQ

#### Lo que ve el usuario

```
   🔓 NUEVO — TOOLS DESBLOQUEADOS
   
   El tab Tools ya no tiene candado 🔒.
   Aparece un badge "NUEVO" sobre el icono de Tools.
```

El Coach ha decidido que es hora de mostrar evidencia técnica.

#### Coach dice

> *"🔊 Balance Tonal — Es hora de esculpir. He notado que el Kick y el 808 compiten en 60Hz. Hay enmascaramiento."*

```
   [Ver evidencia] ← botón clickeable en el chat
```

#### Usuario hace

Clickea "Ver evidencia".

#### Lo que pasa

1. El tab **Tools** se abre automáticamente con el **Spectrum Analyzer**
2. La región **50-80Hz** está resaltada con un overlay glow
3. Se superpone el espectro del Kick (rojo) y del 808 (azul) para mostrar el solapamiento

```
┌──────────────────────────────────────────────────────────────┐
│  📊 SPECTRUM ANALYZER — Enmascaramiento en 60Hz            │
│                                                              │
│  0dB ┤                                                        │
│      ┤   ┌────┐                                               │
│      ┤   │    │ ████████████  Kick                           │
│      ┤   │    │ ░░░░████████  808                            │
│ -45dB┤▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄                          │
│     20Hz  60Hz  200Hz  2kHz  20kHz                           │
│                                                              │
│               ╔══ SOLAPAMIENTO ══╗                          │
│                                                              │
│  [⏎ Volver al Coach]                                         │
└──────────────────────────────────────────────────────────────┘
```

#### Coach dice (en el chat, mientras el analyzer está visible)

> *"¿Ves cómo se solapan en 60Hz? El Kick pierde definición porque el 808 está ocupando su espacio. Prueba un HPF en el 808 a 80Hz y un shelf boost de 2dB a 60Hz en el Kick."*

#### Usuario hace

Aplica el ajuste en FL Studio. Luego clickea "Volver al Coach".

#### El sistema

Cuando el usuario vuelve al chat, el sistema lanza `startAutoReturn()` con countdown de 5s... pero el usuario ya volvió manualmente, así que se cancela.

#### Coach verifica

> *"Bien. Ese HPF le dio aire al Kick. ¿Cómo suena ahora? Yo noto más definición en el low-end."*

#### Cuando EQ está completo

> *"✅ EQ completado. Balance espectral saludable. Pasamos a Compresión."*

---

### 3.5 Escena 10: Compresión

#### Coach dice

> *"🔊 Compresión — Vamos a controlar la dinámica. El crest del Kick está en 4dB, lo que significa que está sobre-comprimido. Buscamos un crest entre 8-14dB."*

#### El sistema

El analyzer se abre automáticamente mostrando el Crest Gauge, con la aguja señalando 4dB y una zona verde marcando el rango 8-14dB.

#### Usuario hace

Ajusta el compresor del Kick (reduce ratio, sube threshold).

#### Coach verifica

> *"El crest subió a 8dB. Mucho mejor. ¿Notas cómo el Kick respira más?"*

---

### 3.6 Escena 11: Espacio

#### Coach dice

> *"🌊 Espacio y Profundidad — Vamos a crear la escena sonora. La mezcla es sólida pero suena plana. Necesitamos profundidad."*

#### El sistema

El Phase Scope se abre mostrando la correlación estéreo.

#### Coach guía

> *"La correlación está en 0.95, casi mono. Prueba abrir el paneo de los HiHats y agregar un reverb suave a la voz."*

---

### 3.7 Escena 12: Master Check

#### Coach dice

> *"🏆 Master Check — Verificación final. ¿Cómo suena tu mezcla contra la referencia?"*

#### El sistema

La Reference Match Panel se activa mostrando el % de match por dimensión:

```
Sub:     ██████████░░  82%  ✅
Punch:   ████████░░░░  68%  🟡
Density: ███████████░  85%  ✅
Air:     ██████░░░░░░  58%  🔴
```

#### Coach dice

> *"La mezcla está al 72% de match con tu referencia. El Air (presencia) es el punto más débil. ¿Quieres refinarlo o estás satisfecho?"*

#### Usuario decide

*"Estoy satisfecho."*

#### Coach

> *"Buen trabajo, Alex. Has completado las 7 etapas de mezcla. Tu sesión está lista para refinar o cerrar."*

---

## 4. ACTO III: El Cierre

### 4.1 Escena 13: Refinamiento

*(Solo si MixScore >= 70)*

#### Coach dice

> *"🎨 Tu mezcla suena sólida. Hablemos de calidad artística. He analizado 5 dimensiones creativas:"*

#### El sistema

El **RefinementHeroComponent** aparece con 5 barras radiales:

```
DEPTH     ████████░░  72%
IMPACT    ██████░░░░  58%
MOVEMENT  █████████░  85%
GLUE      ███████░░░  65%
EMOTION   █████████░  82%
```

#### Coach dice

> *"Tu mezcla tiene buen movimiento y emoción. El impacto podría mejorar — es lo que separa una mezcla correcta de una profesional. ¿Quieres trabajar en eso?"*

#### Usuario decide

Quiere cerrar la sesión.

---

### 4.2 Escena 14: Reporte

#### Coach dice

> *"¿Listo para ver tu reporte?"*

#### Usuario

*"Sí."*

#### Lo que ve el usuario

El chat se desvanece con un crossfade. Un overlay ocupa toda la pantalla:

```
┌──────────────────────────────────────────────────────────────┐
│  📋 FIN DE SESIÓN — 4 julio 2026                           │
│                                                              │
│              ╔════════════╗                                 │
│              ║  72 / 100  ║  ▲ +12 desde inicio             │
│              ╚════════════╝                                 │
│                                                              │
│  ✅ Gain Staging     — Completado                            │
│  ✅ Balance          — Completado                            │
│  ✅ EQ               — Completado                            │
│  ✅ Compression      — Completado                            │
│  ✅ Space            — Completado                            │
│  ✅ Master Check     — Completado                            │
│  ⬜ Refinement       — Pendiente                             │
│                                                              │
│  💪 Mejoraste un 15% vs tu sesión anterior                  │
│                                                              │
│  Correcciones aplicadas: 12/18 (67%)                        │
│  • Kick gain → +3dB  • Snare comp → 4:1                     │
│  • 808 HPF → 80Hz    • Voz EQ → -2dB en 3kHz               │
│                                                              │
│  Progreso multi-sesión: 42% → 72% 📈                        │
│                                                              │
│  [📄 Exportar Reporte HTML]  [🔙 Nueva Sesión]             │
└──────────────────────────────────────────────────────────────┘
```

#### Coach dice

> *"Mira cuánto has avanzado esta sesión. 12 correcciones aplicadas, +12 puntos de score. Tu oído está mejorando, Alex."*

#### Usuario hace

Clickea "Exportar Reporte HTML". El reporte se copia al portapapeles.

---

### 4.3 Escena 15: Despedida

#### Coach dice

> *"💾 Sesión guardada. La próxima vez que me abras, recordaré tu nombre, tu género y dónde lo dejamos.*
>
> *Has aprendido sobre gain staging, balance, EQ, compresión y espacio. Cada sesión te acerca más a sonar como los profesionales.*
>
> *¡Hasta la próxima, Alex!"*

#### El sistema

- `SessionProgression::Phase::Memory` — datos persistidos
- Próxima apertura: el Coach saluda por nombre y pregunta si quiere retomar donde dejó

#### Cómo se siente el usuario

> *"Tengo un ingeniero conmigo. Mejoré mi mezcla y aprendí algo nuevo. Quiero volver."*

---

*Documento de experiencia de sesión — MixCoach — 4 julio 2026*
*Cada escena aquí descrita debe ser reproducible por el sistema.*
*Si una interacción no se siente como esta narrativa, es un bug.*
