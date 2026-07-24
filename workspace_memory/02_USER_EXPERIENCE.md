# 02 - USER EXPERIENCE

> **La maquina de estados de MixCoach.**
> Este documento define EXACTAMENTE que elementos UI existen en cada momento de la sesion.
> Ningun agente debe implementar una pantalla, boton o panel sin verificar en que estado aparece.
>
> **Version:** 1.0 | **Ultima actualizacion:** 29 junio 2026
> **Documento base:** 00_PROJECT_IDENTITY.md (especialmente SS7 Progressive Disclosure)

---

## Indice

1. [Arquitectura de la Experiencia](#1-arquitectura-de-la-experiencia)
2. [Elementos UI Referenciados](#2-elementos-ui-referenciados)
3. [STATE 0: WELCOME](#3-state-0-welcome)
4. [STATE 1: INTENTION](#4-state-1-intention)
5. [STATE 2: GENRE](#5-state-2-genre)
6. [STATE 3: REFERENCE](#6-state-3-reference)
7. [STATE 4: MESSENGERS](#7-state-4-messengers)
8. [STATE 5: MAPPING](#8-state-5-mapping)
9. [STATE 6: COACHING](#9-state-6-coaching)
10. [STATE 7: EVIDENCE](#10-state-7-evidence)
11. [STATE 8: REPORT](#11-state-8-report)
12. [Diagrama de Estados](#12-diagrama-de-estados)
13. [Reglas de Transicion](#13-reglas-de-transicion)
14. [Estados vs Fases de Mezcla](#14-estados-vs-fases-de-mezcla)

---

## 1. Arquitectura de la Experiencia

### Modos de Pantalla

La experiencia se divide en 3 modos que se revelan progresivamente:

| Modo | Funcion | Se revela en |
|:-----|:--------|:-------------|
| **Coach** | Chat con el mentor. Bloque principal. Siempre visible desde STATE 0. | STATE 0 |
| **Session** | Mix Map, lista de Messengers, roles, routing. Evidencia estructural. | STATE 5 |
| **Tools** | Analyzers: Spectrum, LUFS, Vectorscope, Phase, Stereo. Evidencia tecnica. | STATE 7 |

### Elementos Persistentes

Desde STATE 0 hasta STATE 8, estos elementos NUNCA desaparecen:

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  Setup > Ref > Map > Coaching > Refine > Report  |  <- Siempre visible
|  Estado actual: ●●●○○○○○  42%                                    |
+------------------------------------------------------------------+
|                                                                   |
|  [COACH CHAT - contenido principal]                               |
|                                                                   |
|  (El contenido cambia segun el estado)                            |
|                                                                   |
+------------------------------------------------------------------+
| [CHAT INPUT]  Escribe un mensaje...                    [Enviar]  |  <- Siempre visible
+------------------------------------------------------------------+
```

La barra de progreso NUNCA desaparece. El chat input NUNCA desaparece.

### Bloques Colapsables

Cada tarea completada se convierte en un bloque colapsable dentro del chat:

```
>> Referencia
   ✓ Afrobeat Reference.wav - Analizada
   [Expandir para ver detalle]

>> Mapa de Sesion
   ✓ 12 tracks mapeados en 4 buses
   [Expandir para ver detalle]

>> Gain Staging
   ✓ Aprobado por el Coach
   [Expandir para ver detalle]
```

---

## 2. Elementos UI Referenciados

| ID | Elemento | Descripcion |
|:---|:---------|:------------|
| C0 | Chat principal | Burbujas de conversacion Coach/usuario |
| C1 | Chat input | Campo de texto + boton enviar |
| C2 | Avatar Coach | Robot metallic con ojos cyan |
| P0 | Progress bar | Barra superior con etapas y porcentaje |
| R0 | DropZone referencia | Zona de arrastre para archivos WAV/URL |
| R1 | Referencia colapsada | Bloque minimizado: icono check + nombre |
| R2 | Referencia expandida | Detalle: waveform, match %, dominios |
| M0 | Lista Messengers | Track list con health dots |
| M1 | Track resaltado | Track con glow cuando el Coach lo menciona |
| S0 | Mix Map | Arbol jerarquico de buses y pistas |
| S1 | Boton Confirmar Mapa | Accion para validar el routing |
| T0 | Analyzer: Spectrum | RTA con bandas de frecuencia |
| T1 | Analyzer: LUFS | Loudness meter |
| T2 | Analyzer: Phase Scope | Correlacion + vectorscope |
| T3 | Analyzer: Stereo Width | Ancho estereo |
| T4 | Analyzer: Vintage VU | VU meters analogicos |
| X0 | Eleccion Mix/Master | Dos botones: "Mezclar" / "Masterizar" |
| X1 | Eleccion Genero | Lista / selector de genero musical |
| Y0 | Reporte final | Overlay temporal con resumen de sesion |
| Y1 | Boton Exportar | Exportar reporte a HTML |
| Z0 | Boton "Volver al Coach" | Regresa del modo Tools/Session al chat |

---

## 3. STATE 0: WELCOME

### Cuando ocurre

Primera apertura de MixCoach. No hay sesion previa. No hay datos.

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  Setup > ... > Report                              |
|  ???                                                             |
+------------------------------------------------------------------+
|                                                                   |
|  [AVATAR]                                                         |
|                                                                   |
|  Hola.                                                            |
|                                                                   |
|  Soy MixCoach.                                                    |
|                                                                   |
|  Hoy voy a acompanarte durante toda tu mezcla.                    |
|                                                                   |
+------------------------------------------------------------------+
| [Escribe un mensaje...]                                [Enviar]  |
+------------------------------------------------------------------+
```

### Elementos visibles

| Elemento | Visible |
|:---------|:--------|
| C0 Chat principal | Solo el mensaje de bienvenida del Coach |
| C1 Chat input | Si |
| C2 Avatar Coach | Si, con animacion de saludo |
| P0 Progress bar | Si (estado: "Configurando - 0%") |
| Todo lo demas | NO |

### Elementos ocultos

Referencia, Messengers, Mix Map, Analyzers, Reporte. NADA existe aun.

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El usuario escribe "Hola" o cualquier mensaje | STATE 1: INTENTION |
| El usuario escribe "/start" | STATE 1: INTENTION |

### Reglas

- Si el usuario ya tiene nombre guardado (de sesion anterior), el Coach saluda con su nombre:
  "Hola Carlos. Bienvenido de vuelta."
- No mostrar NINGUN otro elemento. Ni referencia, ni botones, ni paneles.

---

## 4. STATE 1: INTENTION

### Cuando ocurre

Despues del saludo inicial, cuando el Coach pregunta que quiere hacer el usuario.

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  Setup > ... > Report                              |
|  Configurando - 5%                                               |
+------------------------------------------------------------------+
|                                                                   |
|  [AVATAR]                                                         |
|                                                                   |
|  [Burbuja Coach]                                                  |
|  Hola de nuevo.                                                   |
|                                                                   |
|  Que vamos a hacer hoy?                                           |
|                                                                   |
|  [Burbuja Coach]                                                  |
|  [ Mezclar ]  [ Masterizar ]                                      |
|                                                                   |
+------------------------------------------------------------------+
| [Escribe un mensaje...]                                [Enviar]  |
+------------------------------------------------------------------+
```

### Nuevos elementos que aparecen

| Elemento | Visible por primera vez |
|:---------|:-----------------------|
| X0 Eleccion Mix/Master | Aparecen dos botones como sugerencias clickeables |

### Elementos ocultos

Referencia, Messengers, Mix Map, Analyzers, Reporte. Siguen sin existir.

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El usuario hace clic en "Mezclar" | STATE 2: GENRE |
| El usuario hace clic en "Masterizar" | STATE 2: GENRE |
| El usuario escribe "Mezclar" o "Masterizar" | STATE 2: GENRE |

### Reglas

- El modo elegido (Mix/Master) cambia TODO el flujo descendente: targets, lenguaje, etapas, prioridades.
- Una vez elegido, el modo no se puede cambiar sin reiniciar sesion.
- El Coach confirma la eleccion: "Perfecto. Vamos a mezclar."

---

## 5. STATE 2: GENRE

### Cuando ocurre

Despues de elegir Mix o Master, el Coach pregunta el genero.

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  Setup > ... > Report                              |
|  Configurando - 10%                                              |
+------------------------------------------------------------------+
|                                                                   |
|  [Burbuja Coach]                                                  |
|  Excelente. Que genero vas a mezclar?                             |
|                                                                   |
|  [Sugerencias: Pop  Reggaeton  Rock  Electronic  HipHop  Otro]    |
|                                                                   |
+------------------------------------------------------------------+
| [Escribe un mensaje...]                                [Enviar]  |
+------------------------------------------------------------------+
```

### Nuevos elementos que aparecen

| Elemento | Visible por primera vez |
|:---------|:-----------------------|
| X1 Eleccion Genero | Chips/sugerencias de genero musical |

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El usuario selecciona un genero | STATE 3: REFERENCE |
| El usuario escribe un genero personalizado | STATE 3: REFERENCE |

### Reglas

- El genero elegido configura: targets de LUFS, perfiles espectrales, rangos de crest, balance tonal esperado.
- El Coach confirma: "Pop. Excelente genero. Conozco bien sus referencias."

---

## 6. STATE 3: REFERENCE

### Cuando ocurre

Despues de elegir el genero, el Coach pide una referencia.

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  Setup > Ref > ... > Report                        |
|  Configurando - 15%                                              |
+------------------------------------------------------------------+
|                                                                   |
|  [Burbuja Coach]                                                  |
|  Necesito una referencia para entender a que sonido apuntas.      |
|                                                                   |
|  [BLOQUE REFERENCIA - expandido]                                  |
|  +----------------------------------------------------------------+ |
|  | REFERENCIA                                                     | |
|  | Arrastra un archivo WAV/MP3 aqui     [ o pega un enlace ]     | |
|  |                                                                 | |
|  | [ Archivo ] [ URL ]                                            | |
|  |                                                                 | |
|  | [ Cancelar ]  [ Analizar ]                                     | |
|  +----------------------------------------------------------------+ |
|                                                                   |
+------------------------------------------------------------------+
| [Escribe un mensaje...]                                [Enviar]  |
+------------------------------------------------------------------+
```

### Nuevos elementos que aparecen

| Elemento | Visible por primera vez |
|:---------|:-----------------------|
| R0 DropZone referencia | Zona de arrastre con dos pestanas: Archivo / URL |
| R0.1 Boton "Analizar" | Procesa la referencia |

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El usuario carga archivo + hace clic en Analizar | STATE 3 -> Analizando -> STATE 4: MESSENGERS |
| El usuario pega URL + hace clic en Analizar | STATE 3 -> Analizando -> STATE 4: MESSENGERS |
| El usuario dice "No tengo referencia" | STATE 4: MESSENGERS (sin referencia) |

### Reglas

- Cuando la referencia se analiza, el bloque se colapsa a R1 (Referencia colapsada):
  ```
  ✓ Referencia
    Afrobeat Reference.wav - Analizada
    [Expandir]
  ```
- Si el usuario no tiene referencia, el Coach dice: "No pasa nada. Te guiare con targets genericos del genero."
- La referencia descargada de URL se cachea localmente.

---

## 7. STATE 4: MESSENGERS

### Cuando ocurre

Despues de la referencia (o sin ella), cuando el Coach necesita conocer la sesion.

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  Setup > Ref > Map > ... > Report                  |
|  Configurando - 25%                                              |
+------------------------------------------------------------------+
|                                                                   |
|  [Burbuja Coach]                                                  |
|  Ahora necesito conocer tu sesion. Inserta un Messenger           |
|  en cada pista para que pueda entender tu mezcla.                 |
|                                                                   |
|  [Burbuja Coach]                                                  |
|  Detectando...
|  Kick...
|  Bass...
|  Snare...
|  Voz...
|
|  [BLOQUE MESSENGERS - expandido]
|  +----------------------------------------------------------------+
|  | SESION 1 - PISTAS DETECTADAS                                   |
|  | [TIPO] [COLOR] [BUS]  [▼] [▲]                                 |
|  |                                                                 |
|  | DRUMS BUS                                                       |
|  |  o Kick        ████████░░ -6.2dB                               |
|  |  o Snare       ██████░░░░ -9.8dB                               |
|  |                                                                 |
|  | BASS BUS                                                        |
|  |  o 808 Bass    ████████░░ -8.1dB                               |
|  |                                                                 |
|  | VOCALS BUS                                                      |
|  |  o Voz         █████████░ -7.4dB                               |
|  +----------------------------------------------------------------+
|                                                                   |
+------------------------------------------------------------------+
| [Escribe un mensaje...]                                [Enviar]  |
+------------------------------------------------------------------+
```

### Nuevos elementos que aparecen

| Elemento | Visible por primera vez |
|:---------|:-----------------------|
| M0 Lista Messengers | Track list completa con health dots, niveles |
| M1 Track resaltado | Track con glow si el Coach lo menciona |

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El Coach confirma que tiene suficientes datos | STATE 5: MAPPING |
| El usuario confirma roles manualmente | STATE 5: MAPPING |

### Reglas

- Los Messengers aparecen DENTRO del chat, como un bloque. No en una pantalla separada.
- Los tracks se detectan automaticamente a medida que aparecen Messengers.
- El Coach puede resaltar un track: "Aun no identifico este track. Puedes decirme que es?"

---

## 8. STATE 5: MAPPING

### Cuando ocurre

Cuando todos los Messengers estan identificados y el Coach construye el mapa.

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  Setup > Ref > Map > ... > Report                  |
|  Mapa de Sesion - 35%                                            |
+------------------------------------------------------------------+
|                                                                   |
|  [Burbuja Coach]                                                  |
|  Aqui tienes el mapa completo de tu sesion.                       |
|  Confirma que el routing es correcto.                             |
|                                                                   |
|  [BLOQUE MESSENGERS - colapsado]
|  ✓ 12 pistas detectadas y identificadas
|  [Expandir]
|
|  ==> SESION: Aparece Mix Map por PRIMERA VEZ
|
|  +----------------------------------------------------------------+
|  | SESION                                                         |
|  |  +-- DRUMS BUS                                                 |
|  |  |    +-- Kick                                                 |
|  |  |    +-- Snare                                                |
|  |  |    +-- HiHat                                                |
|  |  +-- BASS BUS                                                  |
|  |  |    +-- 808 Bass                                             |
|  |  +-- VOCALS BUS                                                |
|  |  |    +-- Voz                                                  |
|  |  +-- MASTER                                                    |
|  |                                                                 |
|  | [Confirmar Mapa]                                               |
|  +----------------------------------------------------------------+
|                                                                   |
+------------------------------------------------------------------+
| [Escribe un mensaje...]                                [Enviar]  |
+------------------------------------------------------------------+
```

### Nuevos elementos que aparecen

| Elemento | Visible por primera vez |
|:---------|:-----------------------|
| S0 Mix Map | Arbol jerarquico de la sesion |
| S1 Boton Confirmar Mapa | Accion para validar |
| **MODO SESSION** | Aparece por primera vez como destino navegable |

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El usuario hace clic en "Confirmar Mapa" | STATE 6: COACHING |
| El usuario modifica routing y confirma | STATE 6: COACHING |

### Reglas

- El Mix Map es el primer elemento que no esta dentro del chat. Aparece en el area de contenido.
- Al confirmar, el bloque de Messengers y Mapa se colapsan:
  ```
  ✓ Mapa de Sesion
    12 tracks en 4 buses - Confirmado
    [Expandir]
  ```
- Aparece animacion de "NEW - Session Map Ready" cuando se desbloquea.

---

## 9. STATE 6: COACHING

### Cuando ocurre

El estado PRINCIPAL. Donde ocurre toda la interaccion de mezcla.

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  Gan Staging > Balance > EQ > Comp > Espacio > Ref |
|  Gain Staging - 15%                                               |
+------------------------------------------------------------------+
|                                                                   |
|  [BLOQUE REFERENCIA - colapsado]
|  ✓ Afrobeat Reference.wav
|  [Expandir]
|
|  [BLOQUE MESSENGERS - colapsado]
|  ✓ 12 pistas identificadas
|  [Expandir]
|
|  [BLOQUE MAPA - colapsado]
|  ✓ Mapa de sesion confirmado
|  [Expandir]
|
|  [Burbuja Coach]
|  Empecemos con el Gain Staging.
|
|  Revisando niveles...
|  La voz principal esta a -2.1dBFS. El target para Pop es -6dBFS.
|  Prueba bajar el gain de la voz unos 4dB.
|
|  [Track resaltado en la lista: Voz - BRILLO CYAN]
|
|  [Burbuja Coach]
|  Despues de eso, dime como suena y seguimos con el balance.
|
|  [Sugerencias: "Listo"  "Necesito ayuda"  "Muéstrame la evidencia"]
|
+------------------------------------------------------------------+
| [Escribe un mensaje...]                                [Enviar]  |
+------------------------------------------------------------------+
```

### Elementos visibles

| Elemento | Visible |
|:---------|:--------|
| C0 Chat principal | Si - ACTIVO. El Coach guia la mezcla |
| C1 Chat input | Si |
| C2 Avatar Coach | Si |
| P0 Progress bar | Si - Avanza por etapas de mezcla |
| R1 Referencia colapsada | Si - Bloque minimizado arriba del chat |
| M0 Lista Messengers | Si - Con health dots y track resaltado |
| S0 Mix Map | NO (a menos que el usuario navegue a el) |
| T0-T4 Analyzers | NO (a menos que el Coach los abra - ver STATE 7) |

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El usuario completa una etapa de mezcla | STATE 6 (siguiente sub-etapa) |
| El Coach dice "Mira la evidencia" + ofrece boton | STATE 7: EVIDENCE |
| El usuario hace clic en "Ver Analizador" | STATE 7: EVIDENCE |
| El usuario completa todas las etapas | STATE 8: REPORT |

### Sub-estados de Coaching

El estado COACHING contiene multiples sub-estados (etapas de mezcla):

| Etapa | Progreso | Coach accion |
|:------|:---------|:-------------|
| Gain Staging | 15% | Revisar niveles, resaltar tracks altos/bajos |
| Balance | 28% | Balance de faders entre tracks |
| EQ | 40% | Correccion tonal por track |
| Compresion | 52% | Dinamica, crest, transientes |
| Saturacion/Color | 60% | Armorica, caracter |
| Espacio/Reverb | 70% | Profundidad, ambiente |
| Estereo/Fase | 78% | Ancho, correlacion |
| Automatizacion | 85% | Movimiento, dinamica |
| Refinamiento vs Ref | 92% | Comparacion final |
| Revision Final | 98% | Aprobacion del Coach |

### Reglas de Coaching

- El Coach NUNCA muestra mas de un problema a la vez.
- Despues de cada recomendacion, el Coach espera respuesta del usuario.
- Si el usuario no responde en 30s, el Coach pregunta: "Necesitas ayuda con eso?"
- El Coach NUNCA avanza de etapa sin aprobacion del usuario.
- Cada etapa aprobada genera un bloque colapsable:
  ```
  ✓ Gain Staging
    Aprobado por el Coach - 3 ajustes realizados
    [Expandir]
  ```

---

## 10. STATE 7: EVIDENCE

### Cuando ocurre

Solo cuando el Coach necesita mostrar evidencia visual. No es un estado permanente.

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  EQ - 40%                                          |
+------------------------------------------------------------------+
|                                                                   |
|  [Burbuja Coach]                                                  |
|  Mira el espectro. El kick y el 808 compiten en 60Hz.             |
|  Observa el pico en la region Sub.                                |
|                                                                   |
|  ==> TOOLS: El Coach abre el Spectrum por PRIMERA VEZ
|
|  +----------------------------------------------------------------+
|  | 0dB                                                             |
|  |      ██                                                         |
|  |      ██  ██                                                     |
|  | -45dB ██  ██  ██                                               |
|  |      ██  ██  ██  ██                                            |
|  |      20Hz   200Hz   2kHz     20kHz                              |
|  |                                                                 |
|  | [Volver al Coach]                                               |
|  +----------------------------------------------------------------+
|                                                                   |
+------------------------------------------------------------------+
| [Escribe un mensaje...]                                [Enviar]  |
+------------------------------------------------------------------+
```

### Nuevos elementos que aparecen

| Elemento | Visible por primera vez |
|:---------|:-----------------------|
| T0-T4 Analyzers | El Coach abre el analizador especifico que necesita mostrar |
| Z0 Boton "Volver al Coach" | Aparece para regresar al chat |
| **MODO TOOLS** | Aparece como destino navegable |

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El usuario hace clic en "Volver al Coach" | STATE 6: COACHING |
| El usuario cierra el analizador | STATE 6: COACHING |

### Reglas

- Los analizadores NUNCA se abren solos. El Coach siempre los abre con un proposito.
- El Coach siempre explica QUE esta mostrando y POR QUE.
- El boton "Volver al Coach" es la accion primaria.
- El usuario tambien puede navegar a Tools manualmente desde cualquier estado > STATE 6.
- Cuando el usuario vuelve al Coach, el bloque de evidencia se colapsa:
  ```
  ✓ Spectrum - Evidencia mostrada
    [Expandir]
  ```

---

## 11. STATE 8: REPORT

### Cuando ocurre

Cuando el usuario completa todas las etapas o hace clic en "Finalizar Sesion".

### Que ve el usuario

```
+------------------------------------------------------------------+
| [PROGRESS BAR]  COMPLETADO - 100%                                 |
|  SESION COMPLETA                                                  |
+------------------------------------------------------------------+
|                                                                   |
|  [OVERLAY - Reporte reemplaza temporalmente el chat]              |
|                                                                   |
|  +----------------------------------------------------------------+
|  | FIN DE SESION - 29 junio 2026                                  |
|  |                                                                 |
|  | [ Emoji grande segun resultado ]                                |
|  |                                                                 |
|  | Excelente trabajo!                                              |
|  |                                                                 |
|  | Score: Muy buena                                               |
|  | Mejora vs inicio: +15%                                      |
|  |                                                                 |
|  | [Metricas] [Correcciones] [Referencia] [Progreso]               |
|  |                                                                 |
|  | +-- Gain  ████████░░                                            |
|  | +-- Tonal ██████░░░░                                            |
|  | +-- Dyn   █████████░                                            |
|  | +-- Spat  ███████░░░                                            |
|  | +-- Ref   ██████░░░░                                            |
|  |                                                                 |
|  | [Exportar Reporte HTML]   [Nueva Sesion]                        |
|  +----------------------------------------------------------------+
|                                                                   |
+------------------------------------------------------------------+
```

### Nuevos elementos que aparecen

| Elemento | Visible por primera vez |
|:---------|:-----------------------|
| Y0 Reporte final | Overlay completo que reemplaza el chat |
| Y1 Boton Exportar | Exportar a HTML |

### Transiciones

| Accion del usuario | Nuevo estado |
|:-------------------|:-------------|
| El usuario hace clic en "Nueva Sesion" | STATE 0: WELCOME |
| El usuario cierra el reporte | STATE 6: COACHING (para revision) |

### Reglas

- El reporte REEMPLAZA temporalmente el chat. No es una pestana aparte.
- El chat sigue existiendo. El usuario puede volver a el.
- El progreso se muestra cualitativamente (sin numeros):
  - "Excelente trabajo!"
  - "Buen trabajo, sigue asi"
  - "Vamos por buen camino"
  - "Sigamos trabajando"
- Los scores por dominio se muestran como barras visuales, NO como numeros.
- La mejora vs inicio se muestra como tendencia: "Mejoraste", "Estable", "Sigue intentando".

---

## 12. Diagrama de Estados

```
                  +-----------+
                  | STATE 0   |   WELCOME
                  | WELCOME   |   Solo chat + avatar
                  +-----+-----+
                        |
                 Usuario escribe
                        |
                        v
                  +-----------+
                  | STATE 1   |   INTENTION
                  | INTENTION |   Mix / Master choice
                  +-----+-----+
                        |
                 Usuario elige modo
                        |
                        v
                  +-----------+
                  | STATE 2   |   GENRE
                  | GENRE     |   Seleccion de genero
                  +-----+-----+
                        |
                 Usuario elige genero
                        |
                        v
                  +-----------+
                  | STATE 3   |   REFERENCE
                  | REFERENCE |   Aparece DropZone
                  +-----+-----+
                        |
                 Referencia cargada o saltada
                        |
                        v
                  +-----------+
                  | STATE 4   |   MESSENGERS
                  | MESSENGERS|   Aparece track list
                  +-----+-----+
                        |
                 Todos los tracks identificados
                        |
                        v
                  +-----------+
                  | STATE 5   |   MAPPING
                  | MAPPING   |   Aparece Mix Map + Modo Session
                  +-----+-----+
                        |
                 Mapa confirmado
                        |
                        v
            +---------------------------+
            |       STATE 6             |   COACHING (principal)
            |       COACHING            |   Chat activo, progreso por etapas
            |                           |
            |  +-- Gain Staging         |
            |  +-- Balance              |
            |  +-- EQ                   |
            |  +-- Compresion           |
            |  +-- Saturacion           |
            |  +-- Espacio              |
            |  +-- Estereo              |
            |  +-- Automatizacion       |
            |  +-- Refinamiento vs Ref  |
            |  +-- Revision Final       |
            +---+-----------------------+
                |                   |
                | Coach muestra     | Etapas completadas
                | evidencia         |
                v                   v
          +-----------+      +-----------+
          | STATE 7   |      | STATE 8   |
          | EVIDENCE  |      | REPORT    |
          | Tools se  |      | Overlay   |
          | abre      |      | final     |
          +-----------+      +-----------+
                |                   |
                | "Volver al        | "Nueva
                |  Coach"           |  Sesion"
                v                   v
          +-----------+      +-----------+
          | STATE 6   |      | STATE 0   |
          +-----------+      +-----------+
```

---

## 13. Reglas de Transicion

### Regla T1: Progresion Unidireccional

Los estados avanzan hacia adelante. Nunca se retrocede a un estado anterior EXCEPTO:
- STATE 7 -> STATE 6 (Volver al Coach desde evidencia)
- STATE 8 -> STATE 6 (Volver al chat desde reporte)

### Regla T2: No Saltar Estados

No se puede saltar de STATE 0 a STATE 6. Cada estado debe recorrerse en orden.
EXCEPCION: Si el usuario ya tiene una sesion guardada, puede restaurar en STATE 6.

### Regla T3: Estado Actual Visible

El estado actual siempre es visible en la barra de progreso.
Cada estado completado se marca con check en la barra.

### Regla T4: Bloque Colapsable por Estado Completado

Cada estado que se completa deja un bloque colapsable visible en el chat.
Los bloques se apilan en orden cronologico de arriba a abajo.

### Regla T5: Chat Siempre Presente

El chat NUNCA desaparece completamente. Incluso en STATE 7 (Evidence)
y STATE 8 (Report), el chat existe y es accesible.

### Regla T6: Herramientas Nunca Aparecen Solas

Ningun Tool o elemento de Session aparece sin que el Coach lo haya solicitado
o el estado actual lo requiera. EXCEPCION: El usuario puede navegar a
Tools o Session manualmente desde STATE 6 en adelante.

---

## 14. Estados vs Fases de Mezcla

No confundir los estados de UI con las fases de mezcla:

| Estado UI | Progreso UI | Fase de Mezcla |
|:----------|:------------|:---------------|
| STATE 0: WELCOME | 0% | - |
| STATE 1: INTENTION | 5% | - |
| STATE 2: GENRE | 10% | - |
| STATE 3: REFERENCE | 15% | - |
| STATE 4: MESSENGERS | 25% | Setup/inicio |
| STATE 5: MAPPING | 35% | Organizacion |
| STATE 6: COACHING | 35-95% | Gain Staging > Balance > EQ > Comp > Sat > Espacio > Estereo > Auto > Refinamiento |
| STATE 7: EVIDENCE | variable | (sub-estado de coaching) |
| STATE 8: REPORT | 100% | Finalizado |

---

*Documento de experiencia de usuario - MixCoach v1.0 - 29 junio 2026*
*Todo agente debe consultar este documento antes de proponer o implementar cualquier cambio en la interfaz.*
*Si un cambio contradice la maquina de estados definida aqui, el cambio debe replantearse.*
