# 📋 UI_REPORT.md

> **Documento de diseño del Reporte final.**
> El reporte aparece al final de la sesión como un overlay que reemplaza temporalmente el chat.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Componente:** EndOfSessionComponent

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Jerarquía Visual](#2-jerarquia-visual)
3. [Estados](#3-estados)
4. [Secciones del Reporte](#4-secciones-del-reporte)
5. [Animaciones](#5-animaciones)
6. [Exportación](#6-exportacion)

---

## 1. Propósito

El reporte representa el cierre de la mentoría. Debe responder:
- ¿Qué aprendió el usuario?
- ¿Qué mejoró?
- ¿Qué decisiones tomó?
- ¿Qué métricas alcanzó?

**El reporte aparece únicamente al final, como overlay que reemplaza temporalmente el chat.**

---

## 2. Jerarquía Visual

### Vista Usuario (default)

```
┌─────────────────────────────────────────────────────────┐
│  📋 FIN DE SESIÓN — 3 julio 2026                       │
├─────────────────────────────────────────────────────────┤
│                                                         │
│                   🌟 72 / 100                          │
│                  ▲ +12 desde inicio                     │
│                                                         │
│  ✅ Gain Staging     — Completado                       │
│  ✅ Balance          — Completado                       │
│  ✅ EQ               — Completado                       │
│  ✅ Compression      — Completado                       │
│  ⬜ Refinamiento     — Pendiente                        │
│  ⬜ Master Check     — Pendiente                        │
│                                                         │
│  💪 Mejoraste un 15% vs tu sesión anterior             │
│                                                         │
│  [📄 Exportar Reporte HTML]                             │
└─────────────────────────────────────────────────────────┘
```

### Vista Técnica (toggle "Mostrar detalles técnicos")

Añade:
- Desglose de 5 scores: Gain, Tonal, Dynamics, Spatial, Reference
- Correcciones aplicadas (12/18)
- Comparación vs referencia (Sub, Punch, Density)
- Progreso multi-sesión

---

## 3. Estados

| Estado | Descripción |
|:-------|:------------|
| **Empty** | Sesión sin datos |
| **Loading** | Generando reporte |
| **Ready** | Reporte completo visible |
| **Error** | Error al generar |

---

## 4. Secciones del Reporte

| Sección | Visible en | Contenido |
|:--------|:-----------|:----------|
| Score Hero | User + Technical | Score numérico grande + mejora |
| Checkmarks | User | 6 dominios con ✓◆◇✗ |
| Improvement | User | Delta cualitativo vs sesión anterior |
| Score Cards | Technical | 5 cards con Gain/Tonal/Dynamics/Spatial/Reference |
| Correcciones | Technical | Lista de cambios aplicados |
| Referencia | Technical | Comparación mix vs referencia |
| Progreso | Technical | Gráfico histórico multi-sesión |

---

## 5. Animaciones

| Elemento | Animación |
|:---------|:----------|
| Overlay | Fade-in + slide-up 200ms |
| Score hero | Cuenta animada 0→72 |
| Barras de score | Llenado progresivo con SmoothValue |
| Checkmarks | Aparecen secuencialmente con bounce |

---

## 6. Exportación

- Botón [Exportar Reporte HTML]
- Genera HTML autocontenido con todo el reporte
- Se copia al portapapeles del sistema

---

*Documento de diseño del Reporte — MixCoach — 3 julio 2026*
