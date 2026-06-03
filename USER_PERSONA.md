# 👤 USER_PERSONA.md — ¿Para Quién es MixCoach?

> **Perfil del ingeniero de mezcla que usa MixCoach.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## 🎯 Perfil Principal: "El Ingeniero que Quiere Mejorar"

### Datos demográficos (estimados)

| Aspecto | Rango |
|---------|-------|
| **Edad** | 22-45 años |
| **DAW Principal** | FL Studio (20.0+) |
| **Experiencia** | 1-10 años produciendo/mezclando |
| **Géneros** | Electrónica, pop, urbano, rock (lo que sea que tenga pistas) |
| **Estilo de trabajo** | Solista o small studio |
| **Inglés** | Básico a intermedio (el usuario es hispanohablante) |

### Lo que sabe hacer

```markdown
✅ Cargar plugins VST3 en pistas
✅ Ajustar faders de volumen y panoramas
✅ Usar un ecualizador paramétrico básico
✅ Reconocer clipping y distortion
✅ Entender qué es un compresor (threshold, ratio, attack, release)
✅ Diferenciar entre mono y estéreo
✅ Saber qué es "headroom"
```

### Lo que está aprendiendo

```markdown
🔶 ¿Qué es LUFS exactamente y por qué -14 LUFS?
🔶 ¿Cómo identificar enmascaramiento espectral?
🔶 ¿Qué es correlación de fase y por qué importa?
🔶 ¿Cómo balancear el espectro sin ecualizador?
🔶 ¿Qué es crest factor y cómo usarlo?
🔶 ¿Cómo organizar una mezcla con 60+ pistas?
```

### Lo que NO le interesa

```markdown
❌ Teoría de audio avanzada (transformadas de Fourier, filtros IIR)
❌ Ingeniería de sonido de grado académico
❌ Calibración de monitores o acústica de sala
❌ Dolby Atmos o formatos inmersivos
❌ Mastering (por ahora — eso viene después)
```

---

## 😤 Sus Frustraciones (El Dolor que MixCoach Resuelve)

| Frustración | ¿Cómo ayuda MixCoach? |
|-------------|----------------------|
| "No sé si mi mezcla suena bien o mal" | Análisis objetivo: clipping, balance, fase |
| "Tengo 60 pistas y no sé por dónde empezar" | Fases progresivas: primero gain staging, luego organización... |
| "Mezclo durante horas y al final suena peor" | Detección de fatiga auditiva? No, pero los análisis objetivos no se cansan |
| "No entiendo por qué mi mezcla suena opaca" | Detección de falta de presencia en espectro |
| "Pongo muchos Messengers y FL Studio crashea" | Estabilidad con 100+ Messengers (optimizada) |
| "Gasto horas organizando nombres y colores" | MixCoach lo recuerda todo entre sesiones |
| "No sé si estoy listo para pasar a la siguiente fase" | PhaseManager muestra progreso y criterios de completitud |
| "Comparo mi mezcla con referencias y no sé qué ajustar" | Análisis espectral por bandas + sugerencias accionables |
| "El bouncing suena diferente a la mezcla" | Detección de fase negativa (compatibilidad mono) |

---

## 🎬 Su Flujo de Trabajo Típico en FL Studio

### Paso 1: Setup de la sesión
```
1. Abre FL Studio
2. Carga el proyecto de mezcla (o empieza uno nuevo)
3. Carga MixCoach en el canal Master
4. Carga Messengers en las pistas que quiere analizar
   → NOTA: FL Studio permite copiar un Messenger a N pistas
   → Esto puede crear 60+ instancias en segundos
   → El sistema DEBE soportar esta creación masiva sin crashear
```

### Paso 2: Mezcla (el trabajo real)
```
Mientras mezcla, MixCoach está en segundo plano:
├── Midiendo niveles en tiempo real
├── Detectando problemas (clipping, fase, enmascaramiento)
├── Mostrando analizadores en pestaña 2
└── Generando sugerencias en el chat (pestaña 1)

El usuario:
├── A veces abre MixCoach para ver analizadores
├── A veces lee el chat y aplica sugerencias
├── A veces ignora MixCoach completamente (y está bien)
└── Usa /next para avanzar de fase cuando siente que está listo
```

### Paso 3: Cierre de sesión
```
1. Guarda el proyecto de FL Studio
2. MixCoach persiste los slots en backup files (%LOCALAPPDATA%)
3. La próxima vez que abra el proyecto:
   → Los slots se restauran con nombres, colores y buses
   → La fase de mentoría se reanuda desde donde quedó
```

---

## 💬 El Tono que MixCoach Debe Usar

### ✅ Correcto

```
"🔊 Kick enmascara a Bass en graves (150-300 Hz).
   💡 Prueba: reduce 3 dB en 200 Hz de Bass, o aplica sidechain."

"📊 Headroom: 4.5 dB. La pista con más nivel alcanza -4.5 dB.
   El rango ideal es -6 dB a -3 dB de pico en el master."

"🎛️ Exceso de agudos: la presencia domina la mezcla.
   Prueba un low-pass suave en 12-14 kHz."
```

### ❌ Incorrecto

```
"Tu mezcla está mal."                          ← Juicio, no mentoría
"La FFT muestra 3.2 dB de distorsión armónica." ← Jerga innecesaria
"Sube 1dB."                                    ← Sin contexto ni razón
"Error: slot index out of range."              ← Mensaje de sistema, no de mentor
```

**Regla de oro:** El tono debe ser el de un **ingeniero senior ayudando a un colega**. Profesional, específico, con fundamento técnico pero sin jerga innecesaria, y siempre con una acción recomendada.

---

## 📊 Qué Valora el Usuario (Prioridades)

| Prioridad | Lo que el usuario dice | Lo que significa para el producto |
|:---------:|------------------------|----------------------------------|
| 1️⃣ | "Que no crashee FL Studio" | Estabilidad absoluta. Ninguna feature vale un crash. |
| 2️⃣ | "Que sea rápido" | CPU < 0.05% por Messenger. UI a 30fps. Sin lags. |
| 3️⃣ | "Que me dé consejos útiles" | Las sugerencias deben ser accionables y específicas. |
| 4️⃣ | "Que se vea profesional" | UI oscura, densa, como iZotope/FabFilter. |
| 5️⃣ | "Que funcione con mis proyectos grandes" | Soporte para 60-100+ pistas sin degradación. |
| 6️⃣ | "Que me ayude a aprender" | Explicaciones claras, no solo datos. |

---

## 📈 Escenarios de Uso (Casos Reales)

### Escenario A: Principiante con 10 pistas
```
Contexto: Usuario nuevo, mezcla su primera canción seria
Necesita: Guía paso a paso, no abrumar con datos
MixCoach: Fases progresivas, tips básicos, logros motivacionales
Métrica de éxito: El usuario completa las 6 fases y siente que aprendió
```

### Escenario B: Intermedio con 40 pistas
```
Contexto: Usuario con experiencia, mezcla densa
Necesita: Detectar problemas rápido, no distraerse
MixCoach: Analizadores en pestaña 2, alerts de clipping/fase, enmascaramiento
Métrica de éxito: El usuario identifica y corrige 3 problemas que no había notado
```

### Escenario C: Avanzado con 80+ pistas (el usuario actual)
```
Contexto: Proyecto grande, muchas instancias de Messenger
Necesita: Estabilidad ante todo, rendimiento, datos precisos
MixCoach: Optimizado para 128 slots, batch reads, throttling inteligente
Métrica de éxito: FL Studio no crashea, CPU manejable, datos frescos
```

---

## 🔗 Referencias

| Documento | Relación |
|-----------|----------|
| `PRODUCT_VISION.md` | La visión del producto para la que este usuario existe |
| `DEFINITION_OF_DONE.md` | Validaciones diseñadas para proteger la experiencia de este usuario |
| `CoachEngine.cpp` | El tono y tipo de mensajes que recibe este usuario |
| `PhaseManager.cpp` | Las fases que guían a este usuario por la mezcla |

---

*Documento de persona de usuario — MixCoach Project*
