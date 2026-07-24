# 🔄 PhaseManager.md

> **Gestión de fases — La máquina de estados de la sesión.**
> Controla la progresión del usuario a través de las 15 fases de mezcla.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Archivo:** Source/MixCoach/engine/PhaseManager.h/.cpp

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Fases](#2-fases)
3. [Transiciones](#3-transiciones)
4. [Eventos](#4-eventos)

---

## 1. Propósito

PhaseManager mantiene la máquina de estados de la sesión. Controla en qué fase está el usuario, qué transiciones son válidas, y emite eventos cuando la fase cambia.

---

## 2. Fases

| # | Fase | Descripción |
|:-:|:-----|:------------|
| 0 | Welcome | Bienvenida |
| 1 | Intention | Elegir Mix/Master |
| 2 | Genre | Elegir género |
| 3 | Reference | Cargar referencia |
| 4 | Messengers | Insertar Messengers |
| 5 | Mapping | Mapa de sesión |
| 6 | Gain Staging | Ajustar niveles |
| 7 | Balance | Balance de faders |
| 8 | EQ | Corrección tonal |
| 9 | Compression | Dinámica |
| 10 | Space | Profundidad |
| 11 | Automation | Movimiento |
| 12 | Master Check | Revisión final |
| 13 | Decision | Decisión |
| 14 | Report | Reporte final |

---

## 3. Transiciones

Las transiciones son unidireccionales: siempre hacia adelante.
Excepciones:
- Evidence → Coaching (volver al chat)
- Report → Welcome (nueva sesión)

---

## 4. Eventos

| Evento | Cuándo | Quién escucha |
|:-------|:-------|:--------------|
| `onPhaseEntered(Phase)` | Fase activada | ExperienceManager, UI |
| `onPhaseCompleted(Phase)` | Fase completada | ExperienceManager, Coach |
| `onPhaseSkipped(Phase, Phase)` | Fase saltada | ExperienceManager |

---

*Documento de gestión de fases — MixCoach — 3 julio 2026*
