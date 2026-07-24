# 01 - PRODUCT PHILOSOPHY

> **La filosofia de producto de MixCoach - que es, que no es, y por que existe.**
> Este documento profundiza en el "por que" del producto, complementando la constitucion (00).
>
> **Version:** 1.0 | **Ultima actualizacion:** 29 junio 2026

---

## 1. Nuestra Razon de Ser

**MixCoach existe para ensenar a mezclar.**

No existe para automatizar una mezcla.
No existe para reemplazar al ingeniero.
No existe para imponer decisiones.

Existe para **acompanar, analizar, ensenar, priorizar y guiar** al productor durante toda la sesion de mezcla.

### La Promesa Fundamental

> **Transformar a un productor que "sube faders hasta que suena bien" en un ingeniero que entiende por que cada decision funciona.**

Cada interaccion con MixCoach debe dejar al usuario con mas criterio del que tenia antes. No se trata de terminar la mezcla mas rapido. Se trata de terminar sabiendo mas.

### Nuestra Vision

Queremos construir el **mentor de mezcla mas inteligente del mundo.**

El usuario debe sentir que trabaja junto a un ingeniero profesional que:
- Escucha su mezcla
- Entiende sus objetivos
- Explica exactamente que mejorar
- Muestra evidencia de por que
- Celebra cada avance

MixCoach no compite con un compresor. No compite con un EQ. **Compite con la experiencia de tener un mentor sentado al lado del productor.**

---

## 2. The Brain Principle

> **El cerebro de MixCoach nunca escucha para juzgar. Escucha para comprender.**
>
> No busca errores. Busca **oportunidades de mejora**.
>
> No responde inmediatamente. **Primero observa. Luego analiza. Despues prioriza. Finalmente ensena.**

### Flujo Obligatorio del Cerebro

```
Observar > Analizar > Priorizar > Ensenar
    1          2          3          4
```

Ninguna respuesta del Coach debe saltarse estos pasos.

### Lo que el Cerebro NO Hace

| No hace... | Por que |
|:-----------|:--------|
| No escucha para calificar | El usuario no necesita una nota |
| No busca errores primero | Busca primero lo que esta bien |
| No responde instantaneamente | Espera a tener datos completos |
| No improvisa recomendaciones | Cada sugerencia viene del engine |

---

## 3. Que es MixCoach

- **Un mentor.** Su objetivo es formar ingenieros, no corregir mezclas.
- **Un analista.** Procesa datos de audio con precision C++ y sin IA.
- **Un profesor.** Explica conceptos, no solo da instrucciones.
- **Un companero de mezcla.** Habla como un ingeniero senior, no como un manual.
- **Un sistema de aprendizaje continuo.** Mejora con cada sesion.
- **Un traductor entre datos tecnicos y lenguaje musical.** Convierte FFT en "el bajo necesita mas cuerpo".

### En la Practica

Cuando el usuario abre MixCoach, esta abriendo una sesion con un ingeniero. El ingeniero:
1. Lo saluda por su nombre
2. Pregunta que va a mezclar
3. Pide una referencia para entender el target
4. Escucha la sesion completa
5. Identifica cada pista
6. Prioriza los problemas
7. Explica cada recomendacion
8. Muestra evidencia cuando hace falta

---

## 4. Que NO es MixCoach

| No es... | Por que |
|:---------|:--------|
| Un plugin de procesamiento de audio | MixCoach nunca toca un fader |
| Un auto-mixer | No ajusta niveles, no automatiza decisiones |
| Un sistema que mueve faders | El usuario tiene el control fisico |
| Un insertador de plugins | No carga EQs, compresores, ni efectos |
| Un modificador de parametros | No escribe automatizacion |
| Un reemplazo del criterio del productor | El productor siempre decide |
| Un competidor de iZotope o Sonible | Esas herramientas procesan; MixCoach ensena |
| Un asistente tipo ChatGPT | Especializado en audio, no es proposito general |
| Dependiente de la nube | Funciona 100% offline, privacidad total |
| Un juez que califica mezclas | No hay scores visibles, no hay rankings |
| Un dashboard tecnico | La conversacion manda, no los paneles |

---

## 5. Filosofia de Inteligencia

**El analisis pertenece al motor C++. La interpretacion pertenece al LLM. La interfaz comunica. El usuario decide.**

### Tabla de Responsabilidades

| Responsabilidad | Quien la ejecuta |
|:----------------|:-----------------|
| Calcular FFT, LUFS, correlacion, crest | C++ (AudioAnalyzer, CoachEngine) |
| Detectar clipping, sobre-compresion, desbalance tonal | C++ (TrackAnalyzer) |
| Priorizar issues por severidad x rol x dominio | C++ (MixPriorityEngine) |
| Generar score de salud de mezcla | C++ (MixScore) |
| Traducir datos tecnicos a lenguaje humano | LLM (AiCoachAdapter) |
| Explicar por que algo es un problema | LLM |
| Hacer preguntas al usuario | LLM |
| Abrir herramientas segun la fase | UI (maquina de estados) |
| Decidir si aplicar una recomendacion | **Siempre el usuario** |

### Regla Absoluta

**El LLM nunca calcula metricas. Nunca inventa datos. Nunca reemplaza el analisis DSP.**

### Por Que esta Separacion

1. **Precision:** El C++ da valores exactos y repetibles
2. **Costo:** No gastar tokens para hacer matematica que corre en nanosegundos en C++
3. **Confianza:** El usuario sabe que los datos vienen del engine, no de una "opinion" de IA
4. **Privacidad:** El analisis DSP es local. Solo el prompt textual va al LLM

---

## 6. Filosofia de Audio

### Principios

- **La precision tiene prioridad sobre la velocidad.** Todo analisis debe ser repetible y verificable.
- **Toda recomendacion debe poder justificarse mediante metricas objetivas.** Si no hay dato, no hay recomendacion.
- **La referencia es un objetivo, no una copia.** No se trata de sonar igual - se trata de entender que hace que la referencia funcione.
- **Loop de correccion:** Recomendar -> Usuario aplica -> Verificar -> Corregir. Este ciclo ES el aprendizaje.

### El Ciclo de Aprendizaje

```
Coach: "El kick pierde pegada. Crest esta en 4dB, target 14dB."
  -> Usuario: Ajusta el compresor
  -> Coach: "Ahora crest esta en 10dB. Mas cerca. Prueba..."
  -> Usuario: Aprende que relacion hay entre threshold y crest
```

Cada iteracion de este ciclo deja al usuario con mas conocimiento. No solo con una mezcla mejor.

---

## 7. Filosofia de UX

- **La conversacion siempre es el centro de la experiencia.** Los analizadores son herramientas de apoyo, no el producto principal.
- **La interfaz nunca aparece completa. Se construye conforme avanza la conversacion.** (Progressive Disclosure)
- **El usuario nunca debe sentirse dentro de un software tecnico.** Debe sentirse acompanado.
- **La interfaz siempre respondera tres preguntas:**
  1. Que esta pasando?
  2. Por que?
  3. Que hago ahora?
- **Una accion primaria por pantalla.** No mas. Si hay mas de una, hay que priorizar.
- **Nunca mostrar datos sin contexto.** Un numero sin explicacion es ruido.
- **Cuando una tarea termina, se colapsa.** El historial sigue accesible, pero no ocupa espacio visual.

---

## 8. Principios Tecnicos

| Capa | Responsabilidad | Lo que NO hace |
|:-----|:----------------|:---------------|
| **Messenger** | Captura audio RAW + identidad de pista | No analiza, no procesa, no decide |
| **Shared Memory** | Transporta datos entre procesos | No transforma, no interpreta |
| **TrackFeed** | Mantiene estado vivo de cada pista | No decide prioridades |
| **CoachEngine** | Analiza, prioriza, genera diagnostico | No ejecuta UI, no genera prompts LLM |
| **LLM** | Interpreta, explica, ensena | No calcula metricas, no mueve faders |
| **UI** | Comunica, acompana, guia | No analiza audio, no decide mentoria |
| **Usuario** | **Siempre decide** | El Coach sugiere, el usuario aplica |

---

## 9. Definicion de Exito

MixCoach estara completo cuando un productor pueda:

1. **Abrir MixCoach** y ser recibido por el Coach como un colega.
2. **Elegir mezclar o masterizar** y el Coach adapte todo el flujo.
3. **Elegir un genero** y el Coach conozca sus targets.
4. **Cargar una referencia** por archivo o enlace, y el Coach la analice y la explique.
5. **Insertar Messengers** y el Coach construya el mapa de la sesion.
6. **Hablar con el Coach** en lenguaje natural y recibir respuestas de mentor, no de manual.
7. **Seguir una mezcla por etapas** (gain staging, balance, EQ, compresion, espacio, refinamiento).
8. **Ver el progreso** en cada etapa y sentir que avanza.
9. **Corregir su mezcla siguiendo recomendaciones** con valores exactos y entender el porque.
10. **Ver la evidencia** en los analizadores cuando el Coach senala un problema.
11. **Aprender durante el proceso** - cada sesion deja algo nuevo.
12. **Cerrar la sesion** con un reporte que celebre lo aprendido y muestre el camino recorrido.
13. **Volver en la siguiente sesion** y el Coach lo recuerde.

---

*Documento de filosofia de producto - MixCoach v1.0 - 29 junio 2026*
*Extraido de 00_PROJECT_IDENTITY.md como documento independiente.*
