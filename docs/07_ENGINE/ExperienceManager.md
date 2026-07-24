# 🎬 ExperienceManager.md

> **Orquestador de la experiencia — El director de cine del plugin.**
> Nuevo componente del UX 2.0. Controla qué aparece, cuándo aparece y qué animación se lanza.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Estado:** 🔮 Planeado — No implementado aún (UX 2.0 Fase 9)

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Responsabilidades](#2-responsabilidades)
3. [Lo que NO hace](#3-lo-que-no-hace)
4. [Arquitectura](#4-arquitectura)
5. [Estados de Orquestación](#5-estados-de-orquestacion)
6. [Integración con CoachEngine](#6-integracion-con-coachengine)

---

## 1. Propósito

ExperienceManager es el **director de toda la experiencia.** No analiza audio, no usa IA. Solo controla.

**Es el componente que hace que MixCoach se sienta vivo.**

---

## 2. Responsabilidades

| Responsabilidad | Descripción |
|:----------------|:------------|
| **Qué aparece** | Decide qué paneles son visibles en cada momento |
| **Qué desaparece** | Decide qué paneles se colapsan o esconden |
| **Qué pestaña abrir** | Cambia entre Coach/Session/Tools según la fase |
| **Qué animación lanzar** | Gatilla fade-in, slide-up, crossfade, celebraciones |
| **Qué fase mostrar** | Controla la progresión de la sesión |
| **Qué bloquear** | Mantiene tabs bloqueados hasta que corresponde |
| **Qué desbloquear** | ▶ Desbloquea tabs con animación cuando corresponde |

---

## 3. Lo que NO hace

- ❌ No analiza audio (eso es CoachEngine)
- ❌ No usa IA (eso es AiCoachAdapter)
- ❌ No genera prompts LLM
- ❌ No calcula métricas

---

## 4. Arquitectura

```
CoachEngine ───────→ ExperienceManager ───────→ UI Components
  (eventos)              │
                         │
                    ┌────┴────┐
                    ▼         ▼
              PanelReveal   NavigationShell
              Manager       (sidebar + content)
```

**Flujo:**
```
CoachEngine detecta que la fase cambió
  → ExperienceManager.advancePhase(newPhase)
    → PanelRevealManager.markPanelRevealed(panel)
      → NavigationShell.revealAnimation(component)
        → UI responde con fade-in + slide-up
```

---

## 5. Estados de Orquestación

| Estado UI | ExperienceManager acción | Componentes afectados |
|:----------|:------------------------|:----------------------|
| Welcome | Mostrar solo Chat + Avatar | ChatMessagesComponent |
| Reference | Revelar DropZone | DropZoneComponent |
| Messengers | Revelar track list | MessengerListComponent |
| Mapping | ▶ Desbloquear Session Tab | NavigationShell, MixMapComponent |
| Coaching | Focus en Chat + resaltar | CoachChatComponent |
| Evidence | ▶ Abrir Tools + Focus | NavigationShell, AnalyzersPanel |
| Report | ▶ Overlay Reporte | EndOfSessionComponent |

---

## 6. Integración con CoachEngine

ExperienceManager escucha eventos de CoachEngine:

```cpp
// CoachEngine emite:
onPhaseChanged(Phase newPhase);
onPanelNeeded(PanelId panel);
onEvidenceNeeded(AnalyzerType type);
onCelebration(CelebrationType type);

// ExperienceManager responde:
showPanel(PanelId panel, Animation anim);
hidePanel(PanelId panel);
unlockTab(TabId tab, Animation anim);
openAnalyzer(AnalyzerType type);
```

---

*Documento del orquestador — MixCoach — 3 julio 2026*
*⚠️ NO IMPLEMENTADO — UX 2.0 Fase 9. Este documento es la especificación para implementación futura.*
