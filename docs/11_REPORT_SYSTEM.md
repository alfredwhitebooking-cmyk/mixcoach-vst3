# 📋 11 — REPORT SYSTEM

> **El sistema de reporte final de MixCoach.**
> Define el diseño del reporte de fin de sesión, la exportación HTML, y los datos que incluye.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Documento base:** `docs/_legacy/UI_REPORT.md`
> **Componente:** EndOfSessionComponent

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Estados](#2-estados)
3. [Secciones del Reporte](#3-secciones-del-reporte)
4. [Exportación HTML](#4-exportacion-html)
5. [Integración con el Coach](#5-integracion-con-el-coach)

---

## 1. Propósito

El reporte representa el cierre de la mentoría. Debe responder:
- ¿Qué aprendió el usuario?
- ¿Qué mejoró?
- ¿Qué decisiones tomó?
- ¿Qué métricas alcanzó?

**El reporte aparece únicamente al final, como overlay que reemplaza temporalmente el chat.**

### Layout

```
┌──────────────────────────────────────────────────────────────┐
│  📋 FIN DE SESIÓN — 4 julio 2026                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                   🌟 72 / 100                                │
│                  ▲ +12 desde inicio                          │
│                                                              │
│  ✅ Gain Staging     — Completado                            │
│  ✅ Balance          — Completado                            │
│  ✅ EQ               — Completado                            │
│  ✅ Compression      — Completado                            │
│  ⬜ Refinamiento     — Pendiente                             │
│  ⬜ Master Check     — Pendiente                             │
│                                                              │
│  💪 Mejoraste un 15% vs tu sesión anterior                  │
│                                                              │
│  [📄 Exportar Reporte HTML]                                  │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Estados

| Estado | Descripción |
|:-------|:------------|
| **Empty** | Sesión sin datos |
| **Loading** | Generando reporte |
| **Ready** | Reporte completo visible |
| **Error** | Error al generar |

---

## 3. Secciones del Reporte

### Vista Usuario (default)

| Sección | Contenido |
|:--------|:----------|
| Score Hero | Score numérico grande + mejora |
| Checkmarks | 6 dominios con ✓/◆/◇/✗ |
| Improvement | Delta cualitativo vs sesión anterior |

### Vista Técnica (toggle "Mostrar detalles")

| Sección | Contenido |
|:--------|:----------|
| Score Cards | 5 cards: Gain, Tonal, Dynamics, Spatial, Reference |
| Correcciones | Lista de cambios aplicados |
| Referencia | Comparación mix vs referencia (Sub, Punch, Density) |
| Progreso | Gráfico histórico multi-sesión |

---

## 4. Exportación HTML

- Botón [Exportar Reporte HTML]
- Genera HTML autocontenido con todo el reporte
- Se copia al portapapeles del sistema

---

## 5. Integración con el Coach

```
Coach: "¿Listo para tu reporte?"
Usuario: "Sí"
  → SessionProgression::advanceTo(Report)
  → EndOfSessionComponent se renderiza como overlay
  → Coach dice: "Mira cuánto has avanzado esta sesión."
  → Usuario exporta o cierra
  → SessionProgression::advanceTo(Memory)
```

---

*Documento del sistema de reporte — MixCoach — 4 julio 2026*
*Ver `docs/_legacy/UI_REPORT.md` para especificaciones detalladas del componente.*
