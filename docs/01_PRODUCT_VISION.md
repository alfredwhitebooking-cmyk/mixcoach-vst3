# 🎯 01 — PRODUCT VISION

> **Qué es MixCoach y qué NO es. La visión comercial del producto.**
> Este documento define los límites filosóficos y comerciales del producto.
> Si una propuesta contradice estos límites, no pertenece a MixCoach.
>
> **Versión:** 2.0 | **Última actualización:** 3 julio 2026
> **Documento base:** 00_PROJECT_IDENTITY.md, UX_20_ROADMAP.md

---

## 📋 Índice

1. [La Gran Idea](#1-la-gran-idea)
2. [El Mercado](#2-el-mercado)
3. [La Conversación es el Producto](#3-la-conversacion-es-el-producto)
4. [El Plugin es el Lugar, No el Producto](#4-el-plugin-es-el-lugar-no-el-producto)
5. [Mentor, No Juez](#5-mentor-no-juez)
6. [Aprendizaje, No Automatización](#6-aprendizaje-no-automatizacion)
7. [Progressive Disclosure como Filosofía](#7-progressive-disclosure-como-filosofia)
8. [Los 3 Espacios Filosóficos](#8-los-3-espacios-filosoficos)
9. [Analizadores como Evidencia](#9-analizadores-como-evidencia)
10. [Referencias como Objetivo](#10-referencias-como-objetivo)
11. [Progreso como Motivación](#11-progreso-como-motivacion)
12. [Reporte como Cierre](#12-reporte-como-cierre)
13. [Principios de Diseño](#13-principios-de-diseno)
14. [El Orquestador (ExperienceManager)](#14-el-orquestador-experiencemanager)
15. [El LLM como Mentor](#15-el-llm-como-mentor)
16. [Reglas para Cualquier IA](#16-reglas-para-cualquier-ia)

---

## 1. La Gran Idea

MixCoach no vende "un plugin con IA".

MixCoach vende **la primera mezcla guiada por un mentor que construye la interfaz junto contigo.**

Esa diferencia cambia por completo cómo el usuario percibe el producto.

> El usuario nunca debe pensar "Estoy usando un plugin".
> Debe pensar "Estoy trabajando con un ingeniero."

---

## 2. El Mercado

### Problema

Los productores musicales pasan años aprendiendo a mezclar. Muchos nunca llegan a sentirse seguros. Hay miles de tutoriales, cientos de plugins analizadores, y docenas de "asistentes de IA", pero **ningún producto guía al usuario de principio a fin** como lo haría un mentor humano.

### Solución

MixCoach es el primer mentor de mezcla inteligente que:
- **Guía** toda la sesión desde la bienvenida hasta el reporte final
- **Enseña** en cada interacción — el usuario termina sabiendo más
- **Se adapta** al nivel, género y referencia del usuario
- **Respeta** el control creativo — el usuario siempre decide

### Competidores indirectos

| Competidor | Enfoque | Diferencia de MixCoach |
|:-----------|:--------|:-----------------------|
| iZotope Neutron / Ozone | Procesamiento automático | MixCoach no procesa — enseña |
| Sonible smart: | Plugins inteligentes | MixCoach no compite con plugins — es un mentor |
| LANDR | Mastering automático | MixCoach guía, no automatiza |
| ChatGPT / Claude | Asistente general | MixCoach es especializado en audio + guía la UI |

---

## 3. La Conversación es el Producto

La conversación es el producto. La interfaz únicamente apoya esa conversación.

- El Chat siempre es el componente principal.
- Todo inicia desde el Chat.
- Todo termina en el Chat.
- El usuario nunca abandona la conversación.
- Las herramientas únicamente aparecen como apoyo.

---

## 4. El Plugin es el Lugar, No el Producto

MixCoach es un mentor que vive dentro de un plugin VST3.

El plugin es el lugar donde el mentor recibe al usuario, analiza su sesión y lo guía.

Pero el producto no es el plugin.
El producto es **la experiencia de trabajar con ese mentor.**

---

## 5. Mentor, No Juez

MixCoach nunca dice "esto está mal".
MixCoach dice "podemos mejorar esto".

- Sugiere con fundamento. Nunca critica.
- Celebra los aciertos. Nunca humilla por los errores.
- Ofrece opciones. Nunca obliga.

---

## 6. Aprendizaje, No Automatización

El objetivo final no es corregir mezclas.
El objetivo final es **formar ingenieros que escuchan con criterio.**

Cada interacción debe ser una oportunidad de aprendizaje.
El usuario debe terminar cada sesión sabiendo más que cuando comenzó.

---

## 7. Progressive Disclosure como Filosofía

La interfaz nunca se muestra completa. Se construye paso a paso.

- Cada nuevo elemento representa un nuevo paso del aprendizaje.
- Las herramientas aparecen únicamente cuando el Coach las necesita.
- Cuando una tarea termina, esa herramienta se minimiza.

Este principio convierte la interfaz en un viaje guiado.

---

## 8. Los 3 Espacios Filosóficos

### TAB 1 — COACH: El Corazón

La conversación principal. El mentor en acción.
**Nunca hay un momento en que el chat desaparezca.**

### TAB 2 — SESSION: La Información

Información organizada que apoya la conversación.
**Nunca habla. Nunca decide. Solo informa.**

### TAB 3 — TOOLS: La Evidencia

Datos técnicos que respaldan las decisiones del Coach.
**Nunca explica. Nunca recomienda. Solo muestra.**

---

## 9. Analizadores como Evidencia

Los analizadores NO son una pantalla principal.
Son **evidencia.**

Cuando el Coach detecta un problema, dice algo como:

> "Creo que Kick y Bass compiten en 60 Hz."

Después ofrece:

> [Ver evidencia]

Al pulsarlo, TAB 3 se abre automáticamente con el analizador correspondiente.
Cuando termina, el usuario vuelve al Chat.

---

## 10. Referencias como Objetivo

Las referencias son el objetivo del usuario. No son un archivo cualquiera.

Cada referencia debe analizarse completamente **antes de comenzar la mezcla.**
El análisis ocurre en segundo plano.
El Coach utiliza esa información durante TODA la sesión.

---

## 11. Progreso como Motivación

El progreso nunca será un espacio independiente.
Siempre será un **indicador permanente** en la barra superior.

Debe mostrar:
- Fase actual
- Porcentaje
- Etapas completadas
- Próxima etapa

El progreso debe generar **motivación.** Nunca ansiedad.

---

## 12. Reporte como Cierre

El reporte representa el cierre de la mentoría.

Debe responder:
- Qué aprendió el usuario
- Qué mejoró
- Qué decisiones tomó
- Qué métricas alcanzó

Aparece únicamente al final, como un overlay que reemplaza temporalmente el chat.

---

## 13. Principios de Diseño

- Eliminar complejidad.
- Eliminar ruido.
- Eliminar pantallas innecesarias.
- Eliminar botones innecesarios.
- Cada píxel debe tener un propósito.

---

## 14. El Orquestador (ExperienceManager)

**Nuevo en UX 2.0.** ExperienceManager será el director de toda la experiencia.

- No analiza audio.
- No usa IA.
- Solo controla qué aparece, qué desaparece, qué pestaña abrir, qué animación lanzar.

Ver documento completo: [`07_ENGINE/ExperienceManager.md`](./07_ENGINE/ExperienceManager.md)

---

## 15. El LLM como Mentor

**Nuevo en UX 2.0.** El LLM deja de responder preguntas y comienza a dirigir.

**Antes:** LLM responde preguntas.
**Ahora:** LLM decide qué enseñar, qué mostrar, qué panel abrir, qué problema atacar.

> El LLM deja de ser un chatbot. Se convierte en el mentor.

---

## 16. Reglas para Cualquier IA

Antes de proponer una nueva función debes responder:

- ¿Hace que el usuario aprenda mejor?
- ¿Reduce complejidad?
- ¿Respeta la conversación como centro?
- ¿Respeta Progressive Disclosure?
- ¿Hace que el Coach siga siendo el protagonista?

Si alguna respuesta es NO, la propuesta debe replantearse.

---

*Documento de visión de producto — MixCoach v2.0 — 3 julio 2026*
