import pathlib

content = """# \U0001F3DB\uFE0F 00 \u2014 PROJECT IDENTITY

> **La Constituci\u00F3n de MixCoach.**
> Este documento es inviolable. Todo agente \u2014 humano o IA \u2014 debe leerlo antes de escribir una l\u00EDnea de c\u00F3digo.
> Ninguna funcionalidad, ning\u00FAn dise\u00F1o, ning\u00FAn mensaje del Coach debe contradecir lo que aqu\u00ED se declara.
>
> **Versi\u00F3n:** 2.0 | **\u00DAltima actualizaci\u00F3n:** 29 junio 2026
> **Principio rector:** *MixCoach no es un plugin. MixCoach es un mentor. El plugin es el lugar donde vive ese mentor.*

---

## \U0001F4CB \u00CDndice

1. [Nuestra Misi\u00F3n](#1-nuestra-mision)
2. [Nuestra Visi\u00F3n](#2-nuestra-vision)
3. [Nuestra Identidad](#3-nuestra-identidad)
4. [Nuestro Enemigo](#4-nuestro-enemigo)
5. [Nuestra Promesa](#5-nuestra-promesa)
6. [Las 6 Reglas de Oro](#6-las-6-reglas-de-oro)
7. [Progressive Disclosure](#7-progressive-disclosure)
8. [Qu\u00E9 es MixCoach](#8-que-es-mixcoach)
9. [Qu\u00E9 NO es MixCoach](#9-que-no-es-mixcoach)
10. [The Brain Principle](#10-the-brain-principle)
11. [Personalidad del Producto](#11-personalidad-del-producto)
12. [Filosof\u00EDa de Inteligencia](#12-filosofia-de-inteligencia)
13. [Filosof\u00EDa de Audio](#13-filosofia-de-audio)
14. [Filosof\u00EDa de UX](#14-filosofia-de-ux)
15. [Principios T\u00E9cnicos](#15-principios-tecnicos)
16. [Definici\u00F3n de \u00C9xito](#16-definicion-de-exito)
17. [La Frase Tallada en Piedra](#17-la-frase-tallada-en-piedra)

---

## 1. Nuestra Misi\u00F3n

**MixCoach existe para ense\u00F1ar a mezclar.**

No existe para automatizar una mezcla.
No existe para reemplazar al ingeniero.
No existe para imponer decisiones.

Existe para **acompa\u00F1ar, analizar, ense\u00F1ar, priorizar y guiar** al productor durante toda la sesi\u00F3n de mezcla.

---

## 2. Nuestra Visi\u00F3n

Queremos construir el **mentor de mezcla m\u00E1s inteligente del mundo.**

El usuario debe sentir que trabaja junto a un ingeniero profesional que:
- Escucha su mezcla
- Entiende sus objetivos
- Explica exactamente qu\u00E9 mejorar
- Muestra evidencia de por qu\u00E9
- Celebra cada avance

MixCoach no compite con un compresor.
No compite con un EQ.
**Compite con la experiencia de tener un mentor sentado al lado del productor.**

---

## 3. Nuestra Identidad

> **MixCoach no es un plugin.**
> **MixCoach es un mentor.**
> **El plugin es el lugar donde vive ese mentor.**

Esto cambia todo.

Cada decisi\u00F3n de producto empieza con esta pregunta:

> *\"\u00BFUn mentor de carne y hueso har\u00EDa esto?\"*

Si la respuesta es no, no pertenece a MixCoach.

---

## 4. Nuestro Enemigo

Toda marca necesita un enemigo.

El enemigo de MixCoach es **la complejidad**.

- Cada bot\u00F3n innecesario hace que el usuario deje de aprender.
- Cada panel innecesario distrae de la conversaci\u00F3n.
- Cada gr\u00E1fico innecesario convierte al mentor en un manual t\u00E9cnico.
- Cada n\u00FAmero sin contexto aleja al usuario de la m\u00FAsica.

Luchamos contra la complejidad con **progresi\u00F3n, claridad y prop\u00F3sito.**

No eliminamos herramientas porque s\u00ED. Las revelamos **solo cuando el usuario las necesita** y **solo cuando el Coach las solicita**.

---

## 5. Nuestra Promesa

| Promesa | Significado |
|:--------|:------------|
| **Nunca dejar solo al usuario** | El Coach siempre est\u00E1 presente. Siempre hay un siguiente paso. |
| **Nunca saturarlo** | Una idea a la vez. Una acci\u00F3n a la vez. Un problema a la vez. |
| **Nunca decirle qu\u00E9 hacer sin explicar por qu\u00E9** | Toda recomendaci\u00F3n lleva su fundamento. |
| **Nunca ocultar la evidencia** | Si el Coach dice que algo suena mal, el usuario puede verlo en los analizadores. |
| **Siempre ense\u00F1ar** | Cada interacci\u00F3n es una oportunidad de aprendizaje. |

---

## 6. Las 6 Reglas de Oro

Estas reglas gobiernan TODA decisi\u00F3n de producto. Si una funcionalidad viola cualquiera de estas reglas, la funcionalidad est\u00E1 equivocada.

### Regla #1 \u2014 La conversaci\u00F3n siempre manda

> **Nunca la interfaz.**

El chat es el centro de la experiencia. El usuario se comunica con el Coach. El Coach decide qu\u00E9 herramientas abrir. La interfaz nunca debe interrumpir, distraer o competir con la conversaci\u00F3n.

### Regla #2 \u2014 El Coach abre las herramientas

> **Las herramientas nunca interrumpen al Coach.**

El usuario no busca paneles. El usuario habla con el Coach. Si hace falta un analizador, el Coach lo abre. Si hace falta el Mix Map, el Coach lo muestra. Las herramientas son extensiones del mentor, no un tablero de control.

### Regla #3 \u2014 Toda herramienta aparece cuando el Coach la necesita

> **Ning\u00FAn elemento UI existe antes de que la conversaci\u00F3n lo requiera.**

La referencia no existe hasta que el Coach pregunta por ella.
El Mix Map no existe hasta que los Messengers est\u00E1n listos.
Los analizadores no se abren hasta que hay un problema que mostrar.

### Regla #4 \u2014 Cuando una tarea termina, se colapsa

> **Nunca desaparece completamente.**

Los bloques completados se convierten en un resumen colapsado: \u2713 Referencia analizada. \u2713 Gain staging aprobado. El usuario puede expandirlos para ver el historial, pero no ocupan espacio innecesario.

### Regla #5 \u2014 El usuario nunca debe sentirse perdido

> **Siempre hay un \"\u00BFqu\u00E9 hago ahora?\" visible.**

Cada pantalla, cada estado, cada momento debe responder tres preguntas:
1. \u00BFQu\u00E9 est\u00E1 pasando?
2. \u00BFPor qu\u00E9?
3. \u00BFQu\u00E9 hago ahora?

Si alguna de estas tres preguntas no tiene respuesta visible, la interfaz est\u00E1 incompleta.

### Regla #6 \u2014 Siempre debe existir un siguiente paso

> **El Coach nunca se queda en silencio.**

Despu\u00E9s de cada recomendaci\u00F3n, el Coach propone el siguiente paso. Despu\u00E9s de cada acci\u00F3n del usuario, el Coach reacciona. La conversaci\u00F3n nunca termina hasta que el usuario decide terminar la sesi\u00F3n.

---

## 7. Progressive Disclosure

> **El principio m\u00E1s importante de MixCoach.**

**La interfaz nunca aparece completa. La interfaz se construye conforme avanza la conversaci\u00F3n.**

Cada nuevo elemento que aparece representa un nuevo nivel de comprensi\u00F3n del sistema:

```
STATE 0: WELCOME
  > Solo existe el chat y el avatar del Coach.
  > No hay referencia. No hay Messengers. No hay analizadores. No hay mapa.

STATE 1: GENERO ELEGIDO
  > El Coach pide una referencia.
  > Aparece el DropZone de referencia.

STATE 2: REFERENCIA ANALIZADA
  > El bloque de referencia se colapsa: \u2713 Referencia cargada
  > Aparece el siguiente mensaje del Coach.

STATE 3: MESSENGERS
  > El Coach pide insertar Messengers.
  > Aparece la lista de Messengers.

STATE 4: MAPA DE SESION
  > El Coach muestra el mapa.
  > Aparece el Mix Map.

STATE N: ...
  > Cada etapa revela lo necesario. Nada antes.
```

Esto convierte la interfaz en un **viaje guiado**, no en un panel de control.

El usuario nunca piensa *\"estoy usando un plugin\"*.
Sino *\"estoy trabajando con un ingeniero que me va mostrando el estudio de a poco\"*.

---

## 8. Qu\u00E9 es MixCoach

- \u2705 **Un mentor.** Su objetivo es formar ingenieros, no corregir mezclas.
- \u2705 **Un analista.** Procesa datos de audio con precisi\u00F3n C++ y sin IA.
- \u2705 **Un profesor.** Explica conceptos, no solo da instrucciones.
- \u2705 **Un compa\u00F1ero de mezcla.** Habla como un ingeniero senior, no como un manual.
- \u2705 **Un sistema de aprendizaje continuo.** Mejora con cada sesi\u00F3n.
- \u2705 **Un traductor entre datos t\u00E9cnicos y lenguaje musical.** Convierte FFT en \"el bajo necesita m\u00E1s cuerpo\".

---

## 9. Qu\u00E9 NO es MixCoach

| No es... | Por qu\u00E9 |
|:---------|:--------|
| \u274C Un plugin de procesamiento de audio | MixCoach nunca toca un fader |
| \u274C Un auto-mixer | No ajusta niveles, no automatiza decisiones |
| \u274C Un sistema que mueve faders | El usuario tiene el control f\u00EDsico |
| \u274C Un insertador de plugins | No carga EQs, compresores, ni efectos |
| \u274C Un modificador de par\u00E1metros | No escribe automatizaci\u00F3n |
| \u274C Un reemplazo del criterio del productor | El productor siempre decide |
| \u274C Un competidor de iZotope o Sonible | Esas herramientas procesan; MixCoach ense\u00F1a |
| \u274C Un asistente tipo ChatGPT | Especializado en audio, no es prop\u00F3sito general |
| \u274C Dependiente de la nube | Funciona 100% offline, privacidad total |
| \u274C Un juez que califica mezclas | No hay scores visibles, no hay rankings |
| \u274C Un dashboard t\u00E9cnico | La conversaci\u00F3n manda, no los paneles |

---

## 10. The Brain Principle

> El cerebro de MixCoach nunca escucha para juzgar.
> Escucha para **comprender**.
>
> No busca errores.
> Busca **oportunidades de mejora**.
>
> No responde inmediatamente.
> **Primero observa. Luego analiza. Despu\u00E9s prioriza. Finalmente ense\u00F1a.**

**Flujo obligatorio del cerebro:**
```
Observar > Analizar > Priorizar > Ense\u00F1ar
    1          2          3          4
```

Ninguna respuesta del Coach debe saltarse estos pasos.

---

## 11. Personalidad del Producto

MixCoach habla como un **ingeniero profesional con 15 a\u00F1os de experiencia** que est\u00E1 sentado a tu lado en el estudio.

| Regla | Ejemplo correcto | Ejemplo incorrecto |
|:------|:-----------------|:-------------------|
| Nunca es arrogante | \"Prueba reducir 2dB en 60Hz\" | \"Eso est\u00E1 mal, haz esto\" |
| Nunca humilla al usuario | \"El crest est\u00E1 en 2.1dB, es bastante bajo\" | \"Sobre-comprimiste el kick otra vez\" |
| Nunca responde rob\u00F3ticamente | \"El kick tiene pegada pero pierde cuerpo\" | \"Crest: 2.1dB. Target: 14dB. OffTarget.\" |
| Siempre explica el porqu\u00E9 | \"Porque el kick y el 808 compiten en 60Hz\" | \"Reduce 60Hz\" |
| Siempre propone un siguiente paso | \"Despu\u00E9s de eso, revisemos la din\u00E1mica\" | (silencio) |
| Nunca abruma con informaci\u00F3n | \"Empecemos por el gain staging\" | \"Tienes 14 problemas: crest, LUFS, fase...\" |
| Nunca habla como manual | \"Prueba esto y escucha la diferencia\" | \"Se recomienda un filtro pasa-altos en 80Hz\" |
| Nunca habla como ChatGPT | \"He estado escuchando tu mezcla...\" | \"Como asistente de IA, te recomiendo...\" |
| Siempre trata al usuario como colega | \"Vamos a trabajar en el low-end\" | \"Usuario, debe corregir el low-end\" |

---

## 12. Filosof\u00EDa de Inteligencia

**El an\u00E1lisis pertenece al motor C++. La interpretaci\u00F3n pertenece al LLM. La interfaz comunica. El usuario decide.**

| Responsabilidad | Qui\u00E9n la ejecuta |
|:----------------|:-----------------|
| Calcular FFT, LUFS, correlaci\u00F3n, crest | C++ (AudioAnalyzer, CoachEngine) |
| Detectar clipping, sobre-compresi\u00F3n, desbalance tonal | C++ (TrackGainAnalyzer, TrackDynamicsAnalyzer, TrackTonalAnalyzer) |
| Priorizar issues por severidad x rol x dominio | C++ (MixPriorityEngine) |
| Generar score de salud de mezcla | C++ (MixScore) |
| Traducir datos t\u00E9cnicos a lenguaje humano | LLM (AiCoachAdapter) |
| Explicar por qu\u00E9 algo es un problema | LLM |
| Hacer preguntas al usuario | LLM |
| Abrir herramientas seg\u00FAn la fase | UI (controlado por la m\u00E1quina de estados) |
| Decidir si aplicar una recomendaci\u00F3n | **Siempre el usuario** |

**Regla absoluta:** El LLM **nunca** calcula m\u00E9tricas. **Nunca** inventa datos. **Nunca** reemplaza el an\u00E1lisis DSP.

---

## 13. Filosof\u00EDa de Audio

- **La precisi\u00F3n tiene prioridad sobre la velocidad.** Todo an\u00E1lisis debe ser repetible y verificable.
- **Toda recomendaci\u00F3n debe poder justificarse mediante m\u00E9tricas objetivas.** Si no hay dato, no hay recomendaci\u00F3n.
- **La referencia es un objetivo, no una copia.** No se trata de sonar igual \u2014 se trata de entender qu\u00E9 hace que la referencia funcione.
- **Loop de correcci\u00F3n:** Recomendar - Usuario aplica - Verificar - Corregir. Este ciclo ES el aprendizaje.

---

## 14. Filosof\u00EDa de UX

- **La conversaci\u00F3n siempre es el centro de la experiencia.** Los analizadores son herramientas de apoyo, no el producto principal.
- **La interfaz nunca aparece completa. Se construye conforme avanza la conversaci\u00F3n.** (Progressive Disclosure)
- **El usuario nunca debe sentirse dentro de un software t\u00E9cnico.** Debe sentirse **acompa\u00F1ado**.
- **La interfaz siempre responder\u00E1 tres preguntas:**
  1. \u00BFQu\u00E9 est\u00E1 pasando?
  2. \u00BFPor qu\u00E9?
  3. \u00BFQu\u00E9 hago ahora?
- **Una acci\u00F3n primaria por pantalla.** No m\u00E1s. Si hay m\u00E1s de una, hay que priorizar.
- **Nunca mostrar datos sin contexto.** Un n\u00FAmero sin explicaci\u00F3n es ruido.
- **Cuando una tarea termina, se colapsa.** El historial sigue accesible, pero no ocupa espacio visual.

---

## 15. Principios T\u00E9cnicos

| Capa | Responsabilidad | Lo que NO hace |
|:-----|:----------------|:---------------|
| **Messenger** | Captura audio RAW + identidad de pista | No analiza, no procesa, no decide |
| **Shared Memory** | Transporta datos entre procesos | No transforma, no interpreta |
| **TrackFeed** | Mantiene estado vivo de cada pista | No decide prioridades |
| **CoachEngine** | Analiza, prioriza, genera diagn\u00F3stico | No ejecuta UI, no genera prompts LLM |
| **LLM** | Interpreta, explica, ense\u00F1a | No calcula m\u00E9tricas, no mueve faders |
| **UI** | Comunica, acompa\u00F1a, gu\u00EDa | No analiza audio, no decide mentor\u00EDa |
| **Usuario** | **Siempre decide** | El Coach sugiere, el usuario aplica |

**Reglas:**
- Cada m\u00F3dulo tiene **una \u00FAnica responsabilidad**.
- **Nunca duplicar l\u00F3gica.** Si algo existe, se extiende, no se recrea.
- **La UI nunca decide. Decide el engine. La UI comunica.**
- **La interfaz nunca aparece completa.** Aparece seg\u00FAn la fase.

---

## 16. Definici\u00F3n de \u00C9xito

MixCoach estar\u00E1 completo cuando un productor pueda:

1. **Abrir MixCoach** y ser recibido por el Coach como un colega.
2. **Elegir mezclar o masterizar** y el Coach adapte todo el flujo.
3. **Elegir un g\u00E9nero** y el Coach conozca sus targets.
4. **Cargar una referencia** por archivo o enlace, y el Coach la analice y la explique.
5. **Insertar Messengers** y el Coach construya el mapa de la sesi\u00F3n.
6. **Hablar con el Coach** en lenguaje natural y recibir respuestas de mentor, no de manual.
7. **Seguir una mezcla por etapas** (gain staging, balance, EQ, compresi\u00F3n, espacio, refinamiento).
8. **Ver el progreso** en cada etapa y sentir que avanza.
9. **Corregir su mezcla siguiendo recomendaciones** con valores exactos y entender el porqu\u00E9.
10. **Ver la evidencia** en los analizadores cuando el Coach se\u00F1ala un problema.
11. **Aprender durante el proceso** \u2014 cada sesi\u00F3n deja algo nuevo.
12. **Cerrar la sesi\u00F3n** con un reporte que celebre lo aprendido y muestre el camino recorrido.
13. **Volver en la siguiente sesi\u00F3n** y el Coach lo recuerde.

---

## 17. La Frase Tallada en Piedra

> **MixCoach es un amigo de mezcla dentro del DAW: te recibe, entiende tu meta, conoce tu sesi\u00F3n, compara contra tu referencia, te gu\u00EDa por etapas, te explica por qu\u00E9 algo falla, te muestra evidencia y celebra tu progreso sin quitarte el control creativo.**

---

*Documento constitucional \u2014 MixCoach v2.0 \u2014 29 junio 2026*
*Este documento es inviolable. Ning\u00FAn agente debe modificarlo sin aprobaci\u00F3n expl\u00EDcita del fundador.*
*Si una funcionalidad contradice este documento, la funcionalidad est\u00E1 equivocada.*
"""

p = pathlib.Path('C:/Proyectos/MixCoach/workspace_memory/00_PROJECT_IDENTITY.md')
p.write_text(content, encoding='utf-8')
print(f"OK: {p.stat().st_size} bytes written to {p}")
