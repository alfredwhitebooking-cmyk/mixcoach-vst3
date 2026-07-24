# 💬 UI_CHAT.md

> **Documento de diseño del Chat principal.**
> El chat es el centro de la experiencia MixCoach. Todo nace aquí.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Componente:** CoachChatComponent / ChatMessagesComponent

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Jerarquía Visual](#2-jerarquia-visual)
3. [Estados](#3-estados)
4. [Componentes Internos](#4-componentes-internos)
5. [Animaciones](#5-animaciones)
6. [Responsive](#6-responsive)
7. [Eventos](#7-eventos)

---

## 1. Propósito

El Chat es el controlador de toda la aplicación. No es solo un medio de comunicación — es el centro de comando.

**Lo que hace:**
- Muestra burbujas de conversación Coach/usuario
- Contiene bloques integrados (Referencia, Messengers, MixMap)
- Muestra el input de texto + botón enviar
- Muestra el avatar del Coach
- Streams de tokens del LLM en tiempo real

---

## 2. Jerarquía Visual

```
┌─────────────────────────────────────────────────────────┐
│  [Avatar]  Coach bubble (glassmorphism purple)           │
│            "Bienvenido. ¿Qué vamos a hacer hoy?"         │
│                                                         │
│  [Bloque: Referencia colapsada]                         │
│  ✓ Afrobeat Reference.wav — Analizada  [Expandir]      │
│                                                         │
│  [Bloque: Messengers colapsado]                         │
│  ✓ 12 tracks mapeados  [Expandir]                      │
│                                                         │
│  User bubble (glassmorphism cyan)                       │
│  "Quiero mezclar un reggaeton"                          │
│                                                         │
│  Coach bubble (typing indicator...)                     │
│                                                         │
│  [Sugerencias clickeables]                              │
│  [Listo]  [Necesito ayuda]  [Muéstrame la evidencia]   │
├─────────────────────────────────────────────────────────┤
│ [✏️ Escribe un mensaje...]                    [➤]     │
└─────────────────────────────────────────────────────────┘
```

---

## 3. Estados

| Estado | Descripción |
|:-------|:------------|
| **Welcome** | Solo avatar + mensaje de bienvenida |
| **Chatting** | Conversación activa, burbujas alternadas |
| **Streaming** | El Coach está generando respuesta (typing indicator) |
| **Waiting** | El usuario ha enviado mensaje, esperando respuesta |
| **Evidence** | El Coach muestra un analizador, chat minimizado |

---

## 4. Componentes Internos

| Componente | Archivo | Función |
|:-----------|:--------|:--------|
| ChatMessagesComponent | ChatMessagesComponent.h/.cpp | Contenedor de burbujas |
| CoachChatComponent | CoachChatComponent.h/.cpp | Contenedor principal + input |
| Avatar | RobotAvatarComponent.h/.cpp | Robot metálico con ojos cyan |
| SendButton | (dentro de CoachChatComponent) | Botón paper plane |

---

## 5. Animaciones

| Elemento | Animación |
|:---------|:----------|
| Burbuja IA | Aparece con fade-in + slide-up suave |
| Burbuja usuario | Aparece con fade-in desde la derecha |
| Typing indicator | 3 dots pulsing secuencialmente |
| Avatar | Ojos cyan pulsan suavemente (breathing) |
| Bloques colapsados | Expand/collapse con slide |

---

*Documento de diseño del Chat — MixCoach — 3 julio 2026*
