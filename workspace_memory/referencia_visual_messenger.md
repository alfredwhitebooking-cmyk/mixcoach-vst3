# 🎨 Guía de Referencia Visual — MESSENGER UI

> Basada en la captura de referencia `Messenger.png`
> Fecha de actualización: Junio 2026

---

## 🖼️ Estructura General

Interfaz cuadrada 1:1. Panel principal contenido dentro de una **tarjeta flotante centrada** con glassmorphism ligero.

```
┌──────────────────────────────────┐
│  INFORMACIÓN DE PISTA            │  ← Título morado brillante
├──────────────────────────────────┤
│  [✏️]  NOMBRE    [Kick        ] │  ← Icono lápiz + input
├──────────────────────────────────┤
│  [🎨]  COLOR     [● ▾        ] │  ← Icono paleta + color picker
├──────────────────────────────────┤
│  [📦]  TIPO      [Kick ▾     ] │  ← Icono caja + dropdown
├──────────────────────────────────┤
│  [➡️]  RUTEO     [Drums Bus ▾] │  ← Icono flecha + dropdown
└──────────────────────────────────┘
```

---

## 🎨 Paleta de Colores

| Elemento | Color | Uso |
|----------|-------|-----|
| Fondo general | `#05070D` | Canvas principal |
| Tarjeta | `#090D15` | Panel flotante central |
| Bordes | `rgba(255,255,255,0.08)` | Bordes sutiles |
| Texto principal | `#FFFFFF` | Valores editables |
| Texto secundario | `#D1D5DB` | Labels y metadata |
| Morado principal | `#A855F7` | Título, color picker, ruteo activo |
| Glow morado | `rgba(168,85,247,0.4)` | Hover glow |

---

## 🔲 Contenedor Principal

| Propiedad | Valor |
|-----------|-------|
| Ancho | ~85% del canvas |
| Esquinas | Muy redondeadas (~30px radius) |
| Fondo | Negro profundo semitransparente |
| Borde | Fino gris azulado tenue |
| Efecto | Glassmorphism ligero + sombra externa difuminada |

---

## 📝 Título

| Propiedad | Valor |
|-----------|-------|
| Texto | `INFORMACIÓN DE PISTA` |
| Tipografía | Sans-serif moderna (SF Pro Display / Inter / Segoe UI) |
| Transformación | **Uppercase** (todo mayúsculas) |
| Peso | Medium (500–600) |
| Color | Morado brillante `#A855F7` |
| Gradiente | Sutil violeta-magenta |
| Tamaño | ~3× el tamaño de las etiquetas de formulario |
| Alineación | Izquierda |
| Separación superior | ~70px del borde |
| Glow | `rgba(168,85,247,0.4)` |

---

## 📋 Estructura del Formulario

### Filas (4 total)

Cada fila contiene:
```
[Icono circular]  [LABEL uppercase]  [Campo de entrada]
```

Separación: líneas divisorias horizontales muy tenues entre filas.
Espaciado: amplio, respirable, mucho espacio negativo.

---

## 🖊️ Fila 1 — Nombre

**Icono**: Lápiz blanco minimalista (trazo fino, estilo outline) dentro de círculo oscuro.

**Label**: `NOMBRE` — color blanco grisáceo `#D1D5DB`

**Campo de texto**:
| Propiedad | Valor |
|-----------|-------|
| Forma | Rectángulo oscuro con bordes redondeados |
| Borde | Gris tenue |
| Fondo | Negro azulado |
| Texto | `Kick` — color blanco `#FFFFFF`, alineado izquierda |

---

## 🎨 Fila 2 — Color

**Icono**: Paleta de pintura outline blanca dentro de círculo oscuro.

**Label**: `COLOR`

**Selector de color**:
| Propiedad | Valor |
|-----------|-------|
| Forma | Rectángulo oscuro pequeño |
| Contenido | Círculo de color púrpura brillante `#A855F7` + flecha hacia abajo a la derecha |
| Acabado | Brillo + ligero glow |

---

## 📦 Fila 3 — Tipo

**Icono**: Caja o cubo outline blanco, estilo minimalista.

**Label**: `TIPO`

**Dropdown**:
| Propiedad | Valor |
|-----------|-------|
| Forma | Rectángulo oscuro |
| Texto | `Kick` — color blanco |
| Indicador | Flecha desplegable a la derecha |

---

## ➡️ Fila 4 — Ruteo

**Icono**: Flecha apuntando hacia la derecha, outline blanco.

**Label**: `RUTEO`

**Dropdown**:
| Propiedad | Valor |
|-----------|-------|
| Forma | Rectángulo oscuro |
| Texto | `Drums Bus` — **color morado brillante** `#A855F7` |
| Indicador | Flecha desplegable blanca a la derecha |

---

## 🔤 Tipografía

| Elemento | Fuente | Peso | Tamaño relativo |
|----------|--------|------|-----------------|
| Título | SF Pro Display / Inter / Segoe UI | 500–600 (Medium/SemiBold) | ~3× labels |
| Labels | SF Pro Display / Inter / Segoe UI | 400–500 (Regular/Medium) | Base |
| Valores | SF Pro Display / Inter / Segoe UI | 400 (Regular) | Base |

---

## ✨ Efectos Visuales

- **Glassmorphism ligero** en la tarjeta principal
- **Sombra externa difuminada** suave
- **Bordes finos** gris azulado muy tenue `rgba(255,255,255,0.08)`
- **Glow morado** en título y elementos activos `rgba(168,85,247,0.4)`
- **Esquinas muy redondeadas** (~30px) en contenedor principal
- **Espaciado amplio** — diseño premium, limpio, respirable

---

## 📐 Layout de Implementación

```cpp
// El panel debe ser un componente centrado con:
// - Fondo: #090D15
// - Borde: rgba(255,255,255,0.08)
// - Radio de esquina: ~30px
// - Padding interno amplio (70px superior para el título)

// Las 4 filas deben:
// - Tener icono circular ~24px a la izquierda
// - Label uppercase a la derecha del icono
// - Campo de entrada/selector a la derecha
// - Separador horizontal tenue entre filas
```
