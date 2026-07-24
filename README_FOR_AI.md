# README FOR AI AGENTS

> **Punto de entrada obligatorio para todo agente — humano o IA — que trabaje en MixCoach.**
>
> Este archivo define el orden en que DEBES leer la documentacion del proyecto.
> Saltarte este orden produce decisiones inconsistentes con la vision del producto.
>
> **Version:** 1.0 | **Ultima actualizacion:** 29 junio 2026

---

## Regla Fundamental

> **Si el codigo que vas a escribir rompe cualquiera de los principios definidos en estos documentos...**
> **No lo implementes. PropON otra solucion.**

No importa que tan efficiente sea tu codigo. No importa que tan elegante sea tu solucion tecnica.
Si contradice la identidad del producto, la experiencia del usuario o la arquitectura definida,
no pertenece a MixCoach.

---

## Orden de Lectura Obligatorio

Lee los documentos en este orden. Cada uno construye sobre el anterior.

### Nivel 1: Identidad del Producto (LEER PRIMERO)

| # | Documento | Que contiene | Tiempo estimado |
|:-:|:----------|:-------------|:----------------|
| 1 | `workspace_memory/00_PROJECT_IDENTITY.md` | La constitucion de MixCoach. Mision, vision, enemigo (la complejidad), 6 reglas de oro, principio de Progressive Disclosure. **El documento mas importante.** | 10 min |

### Nivel 2: Experiencia de Usuario

| # | Documento | Que contiene | Tiempo estimado |
|:-:|:----------|:-------------|:----------------|
| 2 | `workspace_memory/02_USER_EXPERIENCE.md` | La maquina de estados. STATE 0 (WELCOME) a STATE 8 (REPORT). Que elementos UI existen en cada momento. Cuando aparecen, cuando desaparecen, cuando se colapsan. | 10 min |
| 3 | `workspace_memory/03_UI_GUIDELINES.md` | Reglas de interfaz: las 3 preguntas, jerarquia visual, paleta de colores, animaciones, micro-interacciones, glassmorphism. | 8 min |
| 4 | `workspace_memory/07_COACH_ROOM_VISION.md` | La vision del Coach Room: bienvenida, referencia, Messengers, etapas de mezcla/master, progreso emocional. | 5 min |

### Nivel 3: Comportamiento del Coach

| # | Documento | Que contiene | Tiempo estimado |
|:-:|:----------|:-------------|:----------------|
| 5 | `workspace_memory/04_AI_RULES.md` | Personalidad del Coach (ingeniero senior con 15 anos de experiencia), tono, frases prohibidas, niveles de experiencia, estructura de respuestas. | 10 min |

### Nivel 4: Vision y Roadmap

| # | Documento | Que contiene | Tiempo estimado |
|:-:|:----------|:-------------|:----------------|
| 6 | `workspace_memory/MASTER_VISION.md` | Vision tecnica global: layout, componentes, paleta definitiva, checklist de implementacion. | 15 min |
| 7 | `workspace_memory/ROADMAP.md` | Roadmap de producto: sprints completados, sprints activos, prioridades. | 5 min |

### Nivel 5: Arquitectura y Codigo

| # | Documento | Que contiene | Tiempo estimado |
|:-:|:----------|:-------------|:----------------|
| 8 | `workspace_memory/01_ARCHITECTURE.md` | Arquitectura del sistema: flujo de datos, threads, IPC, capas, dependencias, archivos CORE. | 10 min |
| 9 | `workspace_memory/02_CODING_RULES.md` | Reglas de codigo: thread safety, IPC versionado, naming, patrones obligatorios, checklist pre-merge. | 10 min |
| 10 | `AI_CONTEXT.md` | Contexto consolidado para IA: QuickStart, mapa del sistema, ejemplos, leyes del sistema, checklist pre-cambio. | 15 min |

---

## Resumen para Agentes

```
NIVEL 1: PRODUCTO   >> 00_PROJECT_IDENTITY.md     (QUE somos)
NIVEL 2: UX         >> 02_USER_EXPERIENCE.md       (COMO se siente)
                      >> 03_UI_GUIDELINES.md        (COMO se ve)
                      >> 07_COACH_ROOM_VISION.md    (QUE experiencia dar)
NIVEL 3: COACH       >> 04_AI_RULES.md             (COMO habla)
NIVEL 4: VISION      >> MASTER_VISION.md            (QUE construimos)
                      >> ROADMAP.md                 (DONDE estamos)
NIVEL 5: TECNICO      >> 01_ARCHITECTURE.md         (COMO funciona)
                      >> 02_CODING_RULES.md          (COMO escribimos)
                      >> AI_CONTEXT.md               (CONTEXTO completo)
```

Lee SIEMPRE los Niveles 1 y 2 antes de tocar cualquier archivo.
Lee los Niveles 3, 4 y 5 antes de modificar el motor, la IA o la UI.

---

## Documentos que Aun No Existen (Proximamente)

Estos documentos estan planificados pero aun no han sido creados:

| Documento | Contenido esperado |
|:----------|:-------------------|
| `workspace_memory/01_PRODUCT_PHILOSOPHY.md` | Filosofia de producto (extraida de 00_PROJECT_IDENTITY) |
| `workspace_memory/04_COACH_PERSONALITY.md` | Personalidad del Coach (extraida de 04_AI_RULES) |
| `workspace_memory/05_SYSTEM_STATES.md` | Estados tecnicos de la aplicacion |
| `workspace_memory/06_FEATURE_SPECIFICATIONS.md` | Especificacion funcional por modulo |

---

## Checklist para Agentes

Antes de escribir cualquier codigo, verifica:

- [ ] Lei 00_PROJECT_IDENTITY.md (se que es MixCoach y que NO es)
- [ ] Lei 02_USER_EXPERIENCE.md (se en que estado aparece cada elemento)
- [ ] Lei 03_UI_GUIDELINES.md (se como se ve y se siente la interfaz)
- [ ] Lei 04_AI_RULES.md (se como habla el Coach)
- [ ] Lei 01_ARCHITECTURE.md (se como fluyen los datos)
- [ ] Lei 02_CODING_RULES.md (se las reglas de codigo)
- [ ] El cambio que voy a hacer RESPETA Progressive Disclosure (Regla #3)
- [ ] El cambio que voy a hacer NO rompe la maquina de estados
- [ ] El cambio que voy a hacer MANTIENE el chat como centro de la experiencia (Regla #1)

Si alguna respuesta es NO, detente. PropON otra solucion.

---

*Documento de entrada para IA - MixCoach v1.0 - 29 junio 2026*
*Obligatorio. No omitir.*
