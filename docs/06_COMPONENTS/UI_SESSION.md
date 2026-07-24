# 📈 UI_SESSION.md

> **Documento de diseño del espacio SESSION (Progreso y Timeline).**
> Session contiene la información organizada de la sesión: Mix Map, progreso, logros.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Componentes:** ProgressScreen, MixMapComponent

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Jerarquía Visual](#2-jerarquia-visual)
3. [Estados](#3-estados)
4. [Componentes Internos](#4-componentes-internos)
5. [Animaciones](#5-animaciones)

---

## 1. Propósito

Session es el espacio de información de la sesión. Se desbloquea en STATE 5 (Mapping).

**Reglas:**
- ❌ Nunca habla. Nunca decide.
- ✅ Solo muestra información organizada.
- ✅ Mix Map: árbol jerárquico de buses y pistas.
- ✅ Timeline gamificado: hitos, rachas, progreso histórico.

---

## 2. Jerarquía Visual

```
┌─────────────────────────────────────────────────────────┐
│  📈 PROGRESS        🔥 3 sesiones seguidas              │
├─────────────────────────────────────────────────────────┤
│  🚀 Session Progression                                 │
│  ● Setup → ● Reference → ● Analysis → ● Coaching →    │
│  🎯 Refine → ○ Report                                    │
├─────────────────────────────────────────────────────────┤
│  📊 Progress History                                    │
│  Día 1  ████████░░░░  42%                              │
│  Día 3  ████████████░  58%                              │
│  Día 8  █████████████  89%  ← Hoy                     │
├─────────────────────────────────────────────────────────┤
│  🏆 Milestones                                         │
│  [✓] Primera mezcla  [✓] Score > 50                   │
│  [ ] Score > 80      [✓] Usó referencia                │
└─────────────────────────────────────────────────────────┘
```

---

## 3. Estados

| Estado | Descripción |
|:-------|:------------|
| **Locked** | Antes de STATE 5 (Mapping) — bloqueado con candado |
| **Ready** | Mix Map disponible, sesión mapeada |
| **In Progress** | Coaching activo, progreso actualizándose |
| **Complete** | Sesión completada, timeline completo |

---

## 4. Componentes Internos

| Componente | Archivo | Función |
|:-----------|:--------|:--------|
| ProgressScreen | ProgressScreen.h/.cpp | Timeline gamificado tipo Duolingo |
| MixMapComponent | MixMapComponent.h/.cpp | Árbol jerárquico de la sesión |

---

## 5. Animaciones

| Elemento | Animación |
|:---------|:----------|
| Desbloqueo Session Tab | ▶ Badge "NEW" con fade-in + glow |
| Progress bar | SmoothValue 300ms ease-out |
| Milestones | Checkmark aparece con bounce suave |
| Timeline | Eventos aparecen con fade-in secuencial |

---

*Documento de diseño de Session — MixCoach — 3 julio 2026*
