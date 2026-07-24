# 📦 CONTEXT_SYSTEM.md

> **Sistema de contexto del LLM.**
> Define qué información se envía al LLM en cada interacción.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026

---

## 📋 Índice

1. [Filosofía](#1-filosofia)
2. [Capas de Contexto](#2-capas-de-contexto)
3. [Estructura del Contexto](#3-estructura-del-contexto)
4. [Historial de Conversación](#4-historial-de-conversacion)

---

## 1. Filosofía

El LLM recibe **solo la información que necesita** para la interacción actual. No más. El contexto se construye en capas.

---

## 2. Capas de Contexto

| Capa | Contenido | Tamaño estimado | Siempre incluida |
|:-----|:----------|:----------------:|:-----------------:|
| **Base** | Personalidad, reglas, modo actual | ~2K tokens | ✅ Sí |
| **Sesión** | Género, referencia, fase, nivel | ~500 tokens | ✅ Sí |
| **Estado** | TrackAdvice[], MixPriorityEngine | ~2K tokens | ❌ Solo si hay datos |
| **Historial** | Últimas N interacciones | ~1K tokens | ❌ Solo las relevantes |
| **Evidencia** | Datos de analizador específico | ~500 tokens | ❌ Solo si aplica |

---

## 3. Estructura del Contexto

```
[Capa Base - Siempre]
- Personalidad del Coach
- Reglas de comportamiento
- Modo actual (mix/master)

[Capa Sesión - Siempre]
- Género musical
- Referencia cargada (sí/no + nombre)
- Fase actual
- Nivel de experiencia del usuario

[Capa Estado - Cuando hay datos]
- Top 3 issues prioritarios
- Score general (si aplica)
- Progreso de fase actual

[Capa Historial - Últimas relevantes]
- Última recomendación hecha
- Última respuesta del usuario
- Última verificación (si aplica)

[Capa Evidencia - Cuando aplica]
- Datos del analizador que el Coach quiere mostrar
- Región de frecuencia a resaltar
```

---

## 4. Historial de Conversación

| Elemento | Se guarda | Se envía al LLM |
|:---------|:---------:|:----------------:|
| Último mensaje del usuario | ✅ Siempre | ✅ Siempre |
| Última respuesta del Coach | ✅ Siempre | ✅ Siempre |
| Recomendaciones activas | ✅ Sí | ✅ Si no se han resuelto |
| Acciones del usuario | ✅ Últimas 10 | ❌ Solo relevantes |
| Historial completo | ✅ En sesión | ❌ Nunca |

El historial se poda automáticamente para mantener el contexto manejable.

---

*Documento del sistema de contexto — MixCoach — 3 julio 2026*
