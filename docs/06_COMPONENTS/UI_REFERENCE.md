# 🎯 UI_REFERENCE.md

> **Documento de diseño del Panel de Referencia.**
> La referencia es el objetivo de la mezcla. Este panel permite cargarla, analizarla y comparar.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Componente:** ReferencePanelComponent / ReferenceVisualPanel / DropZoneComponent / ReferenceMatchPanel

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Jerarquía Visual](#2-jerarquia-visual)
3. [Estados](#3-estados)
4. [Componentes Internos](#4-componentes-internos)
5. [Animaciones](#5-animaciones)
6. [Flujo de Datos](#6-flujo-de-datos)

---

## 1. Propósito

Permitir al usuario cargar una referencia (archivo WAV/MP3 o URL), analizarla automáticamente, y usarla como objetivo durante toda la mezcla.

---

## 2. Jerarquía Visual

### Estado expandido (DropZone)

```
┌─────────────────────────────────────────────────────────┐
│  REFERENCIA                                             │
│                                                         │
│  [ Archivo ] [ URL ]                                    │
│                                                         │
│  ┌─────────────────────────────────────────────────┐    │
│  │                                                 │    │
│  │   📁 Arrastra un archivo WAV/MP3 aquí           │    │
│  │                                                 │    │
│  │           [ o pega un enlace ]                  │    │
│  │                                                 │    │
│  └─────────────────────────────────────────────────┘    │
│                                                         │
│  [Cancelar]  [Analizar]                                 │
└─────────────────────────────────────────────────────────┘
```

### Estado colapsado (después de analizar)

```
✓ Referencia
  Afrobeat Reference.wav — Analizada
  [Expandir]
```

---

## 3. Estados

| Estado | Descripción |
|:-------|:------------|
| **Empty** | Sin referencia cargada |
| **Drop** | Archivo arrastrado, esperando confirmación |
| **Uploading** | Cargando archivo (barra de progreso) |
| **Analyzing** | Analizando referencia (spinner + "Analizando perfil espectral...") |
| **Ready** | Referencia analizada y lista |
| **Error** | Formato no soportado, archivo corrupto |

---

## 4. Componentes Internos

| Componente | Archivo | Función |
|:-----------|:--------|:--------|
| ReferencePanelComponent | ReferencePanelComponent.h/.cpp | Contenedor con sub-tabs (audio/enlace) |
| DropZoneComponent | DropZoneComponent.h/.cpp | Zona de arrastre con validación |
| ReferenceVisualPanel | ReferenceVisualPanel.h/.cpp | Comparación visual de espectro |
| ReferenceMatchPanel | ReferenceMatchPanel.h/.cpp | Match espectral, similitud global |

---

## 5. Animaciones

| Elemento | Animación |
|:---------|:----------|
| DropZone | Borde dashed → solid cuando se arrastra archivo |
| Bloque colapsado | Slide-up + fade cuando se analiza |
| Visual panel | Barras de comparación se llenan con SmoothValue |

---

*Documento de diseño del Panel de Referencia — MixCoach — 3 julio 2026*
