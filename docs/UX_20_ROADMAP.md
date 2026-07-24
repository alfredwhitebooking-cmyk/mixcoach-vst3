# 🚀 UX 2.0 ROADMAP — Del plugin al mentor

> **Documento fuente del UX 2.0 Roadmap.**
> Este documento es el plan maestro original que guía toda la reestructuración de experiencia de usuario.
> Todos los documentos en `/docs/` derivan de este plan.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Estado:** 📌 Fundacional — Todo el equipo debe leerlo primero

---

## 🎯 Objetivo

Convertir MixCoach de una colección de paneles en un **mentor conversacional que guía toda la sesión**.

---

## 📋 Índice del Plan

| Fase | Nombre | Prioridad |
|:----:|:-------|:---------:|
| 0 | Congelar el desarrollo | 🔴 INMEDIATA |
| 1 | Definir la filosofía | 🔴 INMEDIATA |
| 2 | El Chat se convierte en el centro | 🔴 INMEDIATA |
| 3 | Progressive Disclosure | 🔴 INMEDIATA |
| 4 | Rediseñar las Tabs | 🔴 INMEDIATA |
| 5 | Automatizar la interfaz | 🟡 IMPORTANTE |
| 6 | Diseñar las conversaciones | 🟡 IMPORTANTE |
| 7 | Diseñar el flujo completo | 🟡 IMPORTANTE |
| 8 | Diseñar los paneles | 🟡 IMPORTANTE |
| 9 | Orquestador (ExperienceManager) | 🟡 IMPORTANTE |
| 10 | El LLM deja de responder y comienza a dirigir | 🔵 FUTURO |
| 11 | Gamificación | 🔵 FUTURO |
| 12 | Beta | 🔵 FUTURO |

---

## FASE 0 — Congelar el desarrollo

No agregar más features.
No agregar más analizadores.
No agregar más métricas.
No agregar más ventanas.

**Primero la experiencia.**

> **Resultado:** Todo el equipo desarrolla pensando únicamente en UX.

---

## FASE 1 — Definir la filosofía

Aquí nace el nuevo ADN.

Crear un documento llamado **MASTER_EXPERIENCE.md** que responda solamente estas preguntas:

1. ¿Por qué existe MixCoach?
2. ¿Qué siente el usuario?
3. ¿Cómo habla el coach?
4. ¿Cuándo aparece cada elemento?
5. ¿Cuándo desaparece?
6. ¿Qué controla el chat?
7. ¿Qué nunca debe hacer el sistema?

> Este documento será la Biblia de UX.

---

## FASE 2 — El Chat se convierte en el centro

**Antes:**
```
Dashboard → Referencia → MixMap → Coach → Progress
```

**Ahora todo nace aquí:**
```
Coach → Coach controla todo
```

El Chat será literalmente el controlador de la aplicación.

El Coach tendrá autoridad para:
- Mostrar paneles
- Ocultar paneles
- Cambiar pestañas
- Resaltar botones
- Desbloquear herramientas
- Abrir analizadores
- Cerrar analizadores
- Mostrar progreso
- Celebrar logros

---

## FASE 3 — Progressive Disclosure

Este probablemente sea el cambio más importante del proyecto.

Cuando el usuario abre MixCoach **solamente existe esto:**
- Chat. Nada más.

**Después aparece:**
1. Referencia
2. Preparar sesión
3. Mix Map
4. Tabs

**Nunca antes.**

Cada fase desbloquea la siguiente:

```
Welcome
  ↓
Mode (Mix / Master)
  ↓
Genre
  ↓
Reference
  ↓
Messengers
  ↓
MixMap
  ↓
Gain
  ↓
Balance
  ↓
EQ
  ↓
Compression
  ↓
Space
  ↓
Master Check
  ↓
Report
```

> **Nunca mostrar información adelantada.**

---

## FASE 4 — Rediseñar las Tabs

Eliminar todas las tabs actuales. Dejar solamente:

| Tab | Propósito |
|:----|:----------|
| **COACH** | Chat, Referencia, Checklist, Messenger, MixMap, Cards, Botones, Sugerencias. **Todo.** |
| **TOOLS** | Solo evidencia: Spectrum, LUFS, Vectorscope, Stereo, Phase. **No explica nada. Solo demuestra.** |
| **SESSION** | Progreso, Timeline, Logros, Reporte, Exportar, Historial. **Nada más.** |

---

## FASE 5 — Automatizar la interfaz

El Coach abre las herramientas. El usuario nunca busca.

**Ejemplo:**
```
Coach: "Necesito mostrarte el masking."
  ↓
Abre TOOLS
  ↓
Resalta Spectrum
  ↓
Zoom en 60Hz
  ↓
Regresa al chat
```

**Otro ejemplo:**
```
"Excelente. Veamos cuánto has avanzado."
  ↓
Abre SESSION
  ↓
Muestra 32%
  ↓
Regresa.
```

> Eso hace sentir vivo el producto.

---

## FASE 6 — Diseñar las conversaciones

Crear **COACH_PERSONALITY.md** que contenga:
- Tono
- Vocabulario
- Humor
- Empatía
- Celebraciones
- Errores
- Cómo explicar
- Cómo preguntar
- Cómo felicitar
- Cómo corregir
- Cómo enseñar

**Ejemplo:** Nunca decir "Error encontrado." Siempre "Buen trabajo. Encontré algo que podemos mejorar."

---

## FASE 7 — Diseñar el flujo completo

Crear **SESSION_FLOW.md** con absolutamente todas las fases:

```
Welcome → Reference → Analyze → Organization → Gain → Balance → EQ
→ Compression → Space → Automation → Master Check → Decision → Report → Save
```

Cada fase debe documentar:
- Objetivo
- Componentes visibles
- Componentes ocultos
- Eventos
- Animaciones
- Mensajes
- Botones
- Condiciones para avanzar

---

## FASE 8 — Diseñar los paneles

Cada panel tendrá su propio documento:

| Documento | Panel |
|:----------|:------|
| `UI_CHAT.md` | Chat principal |
| `UI_REFERENCE.md` | Panel de referencia |
| `UI_MIXMAP.md` | Mapa de sesión |
| `UI_TOOLS.md` | Analizadores |
| `UI_SESSION.md` | Progreso y timeline |
| `UI_REPORT.md` | Reporte final |

Cada documento incluirá:
- Wireframe
- Jerarquía visual
- Estados
- Animaciones
- Eventos
- Responsive
- Transiciones

---

## FASE 9 — Orquestador

Crear un componente nuevo: **ExperienceManager**

Será el director de toda la experiencia.
- No analiza audio.
- No usa IA.
- Solo controla.

**Responsabilidades:**
- Qué aparece
- Qué desaparece
- Qué pestaña abrir
- Qué animación lanzar
- Qué fase mostrar
- Qué bloquear
- Qué desbloquear

> Sería el director de cine del plugin.

---

## FASE 10 — El LLM deja de responder y comienza a dirigir

**Antes:** LLM responde preguntas.
**Nuevo:** LLM decide:
- Qué enseñar
- Qué mostrar
- Qué panel abrir
- Qué problema atacar
- Qué celebrar
- Qué desbloquear

> El LLM deja de ser un chatbot. Se convierte en el mentor.

---

## FASE 11 — Gamificación

Solo cuando toda la experiencia funcione. Agregar:
- Logros
- Rachas
- Porcentaje
- XP
- Badges
- Sesiones
- Ranking personal

> **No antes.**

---

## FASE 12 — Beta

Cuando todo esto funcione:
1. Invitar productores reales
2. Grabar absolutamente todo
3. Dónde dudan
4. Dónde preguntan
5. Qué no entienden
6. Qué les encanta
7. Ajustar

---

## 📁 Nueva estructura de documentación

```
/docs
  00_PROJECT_IDENTITY.md        — Qué es MixCoach
  01_PRODUCT_VISION.md           — La visión comercial
  02_MASTER_EXPERIENCE.md        — Cómo debe sentirse una sesión
  03_SESSION_FLOW.md             — Flujo completo de mezcla y mastering
  04_COACH_PERSONALITY.md        — Cómo habla el mentor
  05_UI_SYSTEM.md                — Reglas globales de UX/UI
  06_COMPONENTS/
    UI_CHAT.md                   — Chat principal
    UI_REFERENCE.md              — Panel de referencia
    UI_TOOLS.md                  — Analizadores
    UI_SESSION.md                — Progreso y timeline
    UI_REPORT.md                 — Reporte final
    UI_MIXMAP.md                 — Mapa de sesión
  07_ENGINE/
    CoachEngine.md               — Motor de mentoría
    MixPriorityEngine.md         — Priorización de issues
    ExperienceManager.md         — Orquestador de experiencia
    PhaseManager.md              — Gestión de fases
  08_AI/
    LLM_PROMPTS.md               — Prompts del LLM
    DECISION_TREE.md             — Árbol de decisiones
    CONTEXT_SYSTEM.md            — Sistema de contexto
  09_ROADMAP.md                  — Estado del proyecto y siguientes fases
  UX_20_ROADMAP.md               — ESTE DOCUMENTO (plan fuente)
```

---

*Documento fuente del UX 2.0 Roadmap — MixCoach — 3 julio 2026*
*Este documento es el plan maestro. Todos los documentos en /docs/ derivan de este plan.*
