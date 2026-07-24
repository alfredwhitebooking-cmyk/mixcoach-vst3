# 🗺️ UI_MIXMAP.md

> **Documento de diseño del Mix Map.**
> Árbol jerárquico de la sesión con routing visual.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Componente:** MixMapComponent

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Jerarquía Visual](#2-jerarquia-visual)
3. [Estados](#3-estados)
4. [Interacción](#4-interaccion)

---

## 1. Propósito

Mostrar al usuario el mapa completo de su sesión: qué pistas existen, cómo están organizadas en buses, y cómo fluye la señal.

---

## 2. Jerarquía Visual

```
┌─────────────────────────────────────────────────────────┐
│  📋 SESIÓN                                              │
│                                                         │
│  DRUMS BUS ──────────────────────────────────────────┐  │
│  ├── 🥁 Kick     ████████░░  -6.2dB  │▌│  ░░░░░░  │  │
│  ├── 🥁 Snare    ██████░░░░  -9.8dB  │▌│  ░░░░    │  │
│  └── 🥁 HiHat    ██████░░░░  -12.1dB │▌│  ░░░░░░░ │  │
│                    ╰──────╯                               │
│                        ▼                                  │
│  BASS BUS ───────────────────────────────────────────┐  │
│  └── 🎸 808 Bass  ████████░░  -8.1dB  │▌│  ░░░░    │  │
│                    ╰──────╯                               │
│                        ▼                                  │
│  VOCALS BUS ─────────────────────────────────────────┐   │
│  └── 🎤 Voz      █████████░  -4.2dB  │▌│  ░░░░░░░░│  │
│                                                         │
│  ─────────────── MASTER BUS ───────────────────────     │
│  Drums │ Bass │ Vocals │ Master                         │
│    ●    │  ●   │   ●    │   ●                           │
└─────────────────────────────────────────────────────────┘
```

---

## 3. Estados

| Estado | Descripción |
|:-------|:------------|
| **Empty** | Sin datos de sesión |
| **Loading** | Construyendo mapa... |
| **Ready** | Mapa completo y navegable |
| **Confirming** | Usuario revisando y confirmando |
| **Confirmed** | Mapa aceptado por el usuario |

---

## 4. Interacción

- **Hover en pista** → Tooltip con info detallada + sugerencia de bus
- **Clic en pista** → Resalta en el chat (highlight de track)
- **Clic en bus** → Expande/colapsa las pistas de ese bus
- **Botón [Confirmar Mapa]** → Avanza a la siguiente fase

---

*Documento de diseño del Mix Map — MixCoach — 3 julio 2026*
