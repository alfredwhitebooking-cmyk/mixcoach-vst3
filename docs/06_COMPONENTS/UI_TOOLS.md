# 📊 UI_TOOLS.md

> **Documento de diseño del espacio TOOLS (Analizadores).**
> Tools solo contiene evidencia técnica. No explica nada. Solo demuestra.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Componentes:** AnalyzersPanelComponent, SpectrographComponent, VectorscopeComponent, PhaseScopePanel, MeterPanel, VintageVUMeters

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Jerarquía Visual](#2-jerarquia-visual)
3. [Estados](#3-estados)
4. [Componentes Internos](#4-componentes-internos)
5. [Animaciones](#5-animaciones)
6. [Integración con el Coach](#6-integracion-con-el-coach)

---

## 1. Propósito

Tools es el espacio de evidencia técnica. Cuando el Coach dice "Noto que el kick compite con el 808 en 60Hz", el usuario puede pulsar [Ver evidencia] y Tools se abre automáticamente con el Spectrum mostrando exactamente esa región.

**Reglas:**
- ❌ Nunca explica. Eso lo hace el Coach.
- ❌ Nunca recomienda. Eso lo hace el Coach.
- ✅ Solo muestra datos técnicos.
- ✅ Se abre cuando el Coach lo solicita.
- ✅ El usuario también puede navegar manualmente.

---

## 2. Jerarquía Visual

```
┌──────────────────────────┬─────────────────────────────────┐
│  METER PANEL             │  SPECTRUM ANALYZER              │
│  (28% W, 50% H)          │  (72% W, 50% H)                │
│                          │                                  │
│  VU L/R gradiente        │  0dB ┤                            │
│  Metric cards: PEAK      │      ┤ ██  ████  ████████        │
│  LUFS dual L/R           │ -45dB┤▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄    │
│                          │     20Hz  200Hz  2kHz  20kHz    │
├──────────────────────────┼─────────────────────────────────┤
│  PHASE SCOPE              │  VINTAGE VU METERS              │
│  (48% W, 50% H)           │  (52% W, 50% H)                │
│                           │                                  │
│  [-1────0────+1]  +0.92   │  [L]  [R]  [M]  [S]            │
│  Vectorscope circular     │  Agujas analógicas cream        │
│  Crest gauge              │  Glass overlay                  │
│  Metrics table            │                                  │
└───────────────────────────┴──────────────────────────────────┘
```

---

## 3. Estados

| Estado | Descripción |
|:-------|:------------|
| **No Signal** | Sin datos de audio (messengers no conectados) |
| **Analyzing** | Datos llegando, meters activos |
| **Ready** | Datos disponibles y actualizados |
| **Highlight** | El Coach ha resaltado un analizador específico |

---

## 4. Componentes Internos

| Componente | Archivo | Función |
|:-----------|:--------|:--------|
| AnalyzersPanelComponent | AnalyzersPanelComponent.h/.cpp | Contenedor 2×2 grid |
| MeterPanel | MeterPanel.h/.cpp | VU L/R + 4 metric cards + LUFS |
| SpectrographComponent | SpectrographComponent.h/.cpp | RTA 40 bandas + Waterfall 3D |
| PhaseScopePanel | PhaseScopePanel.h/.cpp | Correlation + Vectorscope + Crest |
| VectorscopeComponent | VectorscopeComponent.h/.cpp | Scope circular |
| VintageVUMeters | VintageVUMeters.h/.cpp | 2×2 vintage analog VU |
| StereoWidthMeter | StereoWidthMeter.h/.cpp | Barra de ancho estéreo |
| PhaseCorrelationMeter | PhaseCorrelationMeter.h/.cpp | Barra de correlación |

---

## 5. Animaciones

| Elemento | Animación |
|:---------|:----------|
| Meters | SmoothValue attack rápido (50ms), release medio (200ms) |
| Spectrum | FFT bins se actualizan en tiempo real |
| Vectorscope | Phosphor trail (traza que se desvanece lentamente) |
| Waterfall | 80 slices, ~15fps, scroll vertical suave |

---

## 6. Integración con el Coach

Cuando el Coach detecta un problema:

```
Coach: "Noto que el kick y el 808 compiten en 60Hz."
       [Ver evidencia]  ← usuario hace clic
         ↓
Tools se abre automáticamente con Spectrum
         ↓
Spectrum resalta la región 50-80Hz
         ↓
[Volver al Coach]  ← usuario vuelve al chat
```

---

*Documento de diseño de Tools — MixCoach — 3 julio 2026*
