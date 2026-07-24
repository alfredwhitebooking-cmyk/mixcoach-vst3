# ⚡ MixPriorityEngine.md

> **Priorización de issues — ¿Qué es lo más importante ahora?**
> Sistema de scoring multidimensional que ordena los problemas por impacto.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Archivo:** Source/MixCoach/engine/MixPriorityEngine.h/.cpp

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Fórmula de Prioridad](#2-formula-de-prioridad)
3. [Dimensiones](#3-dimensiones)
4. [Integración con UI](#4-integracion-con-ui)

---

## 1. Propósito

MixPriorityEngine determina **qué problema debe atacar el usuario primero.** No todos los issues tienen la misma importancia — un problema de gain en la voz principal es más crítico que un problema de fase en un hihat lejano.

---

## 2. Fórmula de Prioridad

```
priorityScore = severity × roleWeight × domainWeight × genreModifier
```

Cada dimensión contribuye al score final, que se usa para ordenar issues de mayor a menor prioridad.

---

## 3. Dimensiones

| Dimensión | Rango | Descripción |
|:----------|:-----:|:------------|
| Severidad | 0.0 - 1.0 | Qué tan lejos está del target |
| Role Weight | 0.0 - 1.0 | Importancia del rol (Voz > Kick > HiHat) |
| Domain Weight | 0.0 - 1.0 | Importancia del dominio (Gain > Tonal > Dynamics) |
| Genre Modifier | 0.8 - 1.2 | Ajuste por género (más peso a ciertos dominios) |

---

## 4. Integración con UI

El NextStepComponent en el Dashboard muestra el issue #1 de MixPriorityEngine como "Tu Próximo Paso".

---

*Documento de priorización — MixCoach — 3 julio 2026*
