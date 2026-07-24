# 07 - COACH ROOM VISION

> **Vision obligatoria para la experiencia principal de MixCoach.**
> Este documento fija la direccion del producto: MixCoach debe sentirse como un amigo/mentor dentro del DAW que entiende la sesion, guia por etapas y demuestra con datos por que cada decision tiene sentido.
>
> Si una propuesta de UI, IA, flujo o arquitectura contradice este documento, la propuesta esta equivocada.
>
> **Version:** 2.0 | **Ultima actualizacion:** 29 junio 2026
> **Complemento obligatorio:** `workspace_memory/08_CHAT_COMMANDED_UI.md` (Arquitectura de Revelacion)

---

## 1. La Vision Central

MixCoach no debe sentirse como una coleccion de paneles tecnicos.

MixCoach debe sentirse como:

> **Un amigo de mezcla dentro del DAW que recibe al productor, aprende quien es, entiende que quiere lograr, conoce la sesion completa, guia cada etapa de la mezcla o masterizacion, explica por que algo falla, muestra evidencia visual y celebra el progreso hacia una referencia profesional.**

El usuario debe sentir:

> "Tengo un ingeniero conmigo. Entiende mi sesion, me explica lo importante, me guia al siguiente paso y me muestra que estoy mejorando."

---

## 2. Estructura Principal De Experiencia

La experiencia se organiza en tres areas principales:

| Area | Funcion | Filosofia |
|:-----|:--------|:----------|
| **Tab 1 - Coach Room** | Conversacion, bienvenida, referencia, sesion, Messengers, siguiente paso | El centro emocional y educativo |
| **Tab 2 - Analyzer Lab** | Medidores y analizadores para verificar lo que dice el coach | Evidencia visual, no producto principal |
| **Tab 3 - Progress Journey** | Progreso, etapas, aprendizaje, cercania a referencia, continuidad | Retencion, motivacion y amistad de mezcla |

La conversacion manda. Los analizadores apoyan. El progreso motiva.

---

## 3. Tab 1 - Coach Room

Tab 1 es la pantalla principal del producto.

Debe estar siempre orientada a una relacion humana entre usuario y coach.

### Debe incluir siempre

- Bienvenida / continuidad de sesion.
- Chat con el coach.
- Estado de modo: Mix o Master.
- Genero actual.
- Etapa actual.
- Referencia activa.
- Cercania a referencia.
- Panel de referencias: archivo + enlace.
- Lista de Messengers detectados.
- Track resaltado cuando el coach habla de un problema concreto.
- Mensaje visible del problema en el track afectado.
- Siguiente accion clara.

### No debe sentirse como

- Un formulario.
- Un dashboard tecnico.
- Una lista de metricas.
- Un sistema que regana al usuario.
- Un auto-mixer.

Debe sentirse como una conversacion guiada.

---

## 4. Flujo Canonico Para Usuario Nuevo

El flujo de usuario nuevo debe ser:

1. **Bienvenida**
   - El coach saluda.
   - Se presenta como mentor de mezcla.
   - Pregunta el nombre del usuario.
   - Guarda el nombre para futuras sesiones.

2. **Intencion**
   - Pregunta: "Que vamos a hacer hoy: mezclar o masterizar?"
   - Cambia a **modo Mix** o **modo Master**.

3. **Genero**
   - Pregunta que genero se va a mezclar o masterizar.
   - Usa los archivos internos de conocimiento de genero para entender targets, balance, dinamica, tono, low-end, voces, pegada y referencias tipicas.

4. **Referencia**
   - Pide una referencia en dos formas:
     - Archivo de audio.
     - Enlace.
   - La referencia da dos puntos de vista:
     - Analisis tecnico real.
     - Contexto musical/intencional.

5. **Interpretacion De Referencia**
   - El coach explica la referencia en lenguaje humano.
   - Ejemplo: "Esta cancion tiene un low-end compacto, voz al frente y drums secos. No vamos a copiarla, vamos a buscar esa energia y pegada."

6. **Preparacion De Sesion**
   - El coach sugiere preparar la sesion:
     - Cargar stems o archivos.
     - Ordenar tracks.
     - Rutear.
     - Colorear.
     - Nombrar correctamente.

7. **Insertar Messengers**
   - El coach pide insertar Messenger en cada track.
   - Cada Messenger aporta identidad:
     - Nombre.
     - Color.
     - Rol.
     - Ruta/bus.
     - Audio RAW.

8. **Mapa De Sesion**
   - El coach construye el mapa.
   - Ahora sabe quien es quien: Kick, 808, snare, vocal, synth, FX, buses, master.

9. **Etapa 1: Gain Staging**
   - El primer trabajo real es gain staging.
   - El coach detecta tracks muy altos o muy bajos.
   - Resalta el track afectado en la lista de Messengers.
   - Explica por que importa.
   - Da una accion exacta.

10. **Etapas De Mezcla**
    - El usuario sigue una mezcla por etapas.
    - El coach aprueba cada etapa antes de avanzar.

---

## 5. Etapas De Mezcla

El coach debe guiar la mezcla como proceso profesional, no como respuestas sueltas.

Orden sugerido:

1. Preparacion de sesion.
2. Identidad de tracks.
3. Mapa de sesion.
4. Referencia.
5. Gain staging.
6. Balance inicial.
7. EQ correctivo.
8. Compresion.
9. Saturacion / color.
10. Espacio, reverb y delay.
11. Estereo y fase.
12. Automatizacion / movimiento.
13. Refinamiento contra referencia.
14. Revision final.
15. Cierre y progreso.

Cada etapa debe tener:

- Que estamos haciendo.
- Por que importa.
- Que track o bus esta involucrado.
- Que accion exacta sigue.
- Como vamos a verificarlo.
- Aprobacion del coach antes de avanzar.

---

## 6. Modo Mix Y Modo Master

El coach debe preguntar si el usuario quiere mezclar o masterizar.

### Modo Mix

El coach se enfoca en:

- Sesion completa.
- Stems.
- Roles.
- Routing.
- Balance.
- Gain staging.
- EQ.
- Compresion.
- Saturacion.
- Profundidad.
- Estereo.
- Relacion entre tracks.

### Modo Master

El coach se enfoca en:

- Mix stereo final.
- LUFS.
- True peak.
- Tonal balance global.
- Dinamica.
- Estereo global.
- Traduccion.
- Comparacion contra referencia.
- Preparacion para distribucion.

El modo elegido cambia targets, lenguaje, prioridades y etapas.

---

## 7. Referencia Siempre Visible

La cercania a la referencia debe estar siempre visible en la experiencia principal.

Esto es central para motivacion y confianza.

### Regla

La referencia no es una copia exacta. Es un norte.

El coach debe repetir la filosofia:

> "No buscamos copiar la referencia. Buscamos entender por que funciona y acercarnos a su energia, balance y pegada."

### Indicador Permanente

Tab 1 debe mostrar siempre:

- Referencia activa.
- Cercania general.
- Tendencia.
- Foco actual.
- Dominio principal a mejorar.

Ejemplo:

```text
Referencia: Bad Bunny - Titi Me Pregunto
Cercania: 72% - Buen camino
Tendencia: +7% desde el ultimo ajuste
Foco actual: voz y low-end
```

### Lenguaje Motivador

No usar lenguaje de castigo.

Rangos sugeridos:

| Cercania | Texto |
|:---------|:------|
| 0-30% | Estamos construyendo la base |
| 30-55% | Ya hay direccion |
| 55-75% | Buen camino |
| 75-90% | Muy cerca |
| 90%+ | Listo para revision final |

Si la cercania baja:

> "Nos alejamos un poco de la referencia, pero no pasa nada. Probablemente el cambio fue demasiado fuerte. Vamos a ajustarlo."

Si la cercania sube:

> "Bien. Ese ajuste acerco la mezcla a la referencia. La voz ahora se sienta mejor con el beat."

### Dimensiones De Comparacion

La cercania no debe ser solo un numero. Debe tener contexto:

- Tonal balance.
- Low-end.
- Voz / presencia.
- Dinamica.
- LUFS / energia.
- Estereo / fase.
- Pegada / impacto.
- Profundidad.

El numero motiva. La explicacion ensena.

---

## 8. Messengers Como Mapa Vivo

La lista de Messengers debe estar visible en Tab 1 porque el usuario necesita identificar la sesion.

Cuando el coach hable de un track:

- Ese track debe resaltarse.
- El problema debe aparecer en la card del track.
- El estado debe ser claro.
- El usuario debe poder ubicarlo rapido.

Ejemplo:

```text
Coach: El 808 esta muy alto contra la referencia.

Messenger list:
> 808 Bass - Problema: +3 dB sobre target de genero/ref
```

Esto conecta conversacion con accion real.

---

## 9. Tab 2 - Analyzer Lab

Tab 2 existe para dar credibilidad.

Cuando el coach diga algo, el usuario debe poder verlo.

Ejemplos:

- "El sonido esta muy abierto en estereo. Observa el Phase Scope."
- "Kick y 808 se pisan en graves. Mira el spectrum."
- "El master esta cerca de clipping. Mira el meter."
- "La referencia tiene mas presencia. Mira la comparacion tonal."

Los analizadores no deben competir con el coach.

Funcionan como evidencia visual:

> El usuario confia porque observa que el coach no miente.

---

## 10. Tab 3 - Progress Journey

Tab 3 existe para motivar, retener y construir relacion.

No debe sentirse como una calificacion fria.

Debe mostrar:

- Etapa actual.
- Etapas completadas.
- Cercania a referencia.
- Tendencia de mejora.
- Conceptos aprendidos.
- Problemas resueltos.
- Que queda pendiente.
- Continuidad de sesion.
- Proximo paso.

Ejemplo:

```text
Hoy completaste:
- Organizacion de sesion
- Mapa de mezcla
- Gain staging inicial

Estas aprendiendo:
- Como dejar headroom
- Como separar kick y 808
- Como usar una referencia

Siguiente:
EQ correctivo en voz principal
```

La retencion viene de sentir progreso real y compania.

---

## 11. Relacion Emocional Con El Usuario

MixCoach debe ser confiable, cercano y profesional.

El usuario debe sentir que el coach:

- Lo recuerda.
- Sabe en que etapa va.
- Entiende su genero.
- Entiende su referencia.
- Conoce todos sus tracks.
- No juzga.
- Explica.
- Celebra avances.
- Corrige con respeto.
- Le da confianza.

Objetivo emocional:

> El usuario debe querer volver porque siente que tiene un amigo de mezcla dentro del DAW.

---

## 12. Reglas Para Futuras IAs

Toda IA que trabaje en MixCoach debe respetar estas reglas:

1. No convertir MixCoach en un dashboard tecnico.
2. No esconder el chat en una seccion secundaria.
3. No separar la referencia del flujo principal.
4. No quitar la lista de Messengers de la experiencia principal.
5. No mostrar numeros sin explicacion.
6. No mostrar cercania a referencia como juicio o perfeccionismo.
7. No hacer que el usuario interprete medidores para saber que hacer.
8. No crear nuevas pantallas que rompan el flujo de mentor.
9. No permitir que la UI decida diagnosticos.
10. No permitir que el LLM invente metricas.
11. No avanzar de etapa sin explicar por que.
12. No saturar al usuario con muchos problemas a la vez.

---

## 13. Definicion De Exito

Esta vision se cumple cuando un usuario nuevo puede:

1. Abrir MixCoach.
2. Ser recibido por el coach.
3. Presentarse y ser recordado.
4. Elegir Mix o Master.
5. Elegir genero.
6. Cargar referencia por archivo y enlace.
7. Preparar su sesion guiado por el coach.
8. Insertar Messengers.
9. Ver su mapa de sesion.
10. Seguir una mezcla por etapas.
11. Ver que tan cerca va de la referencia.
12. Ver el track exacto que necesita atencion.
13. Ver evidencia en analizadores.
14. Recibir aprobacion de etapa.
15. Ver progreso y aprendizaje.
16. Sentir que MixCoach es su amigo de mezcla dentro del DAW.

---

## 14. Frase Marcada En Piedra

> **MixCoach es un amigo de mezcla dentro del DAW: te recibe, entiende tu meta, conoce tu sesion, compara contra tu referencia, te guia por etapas, te explica por que algo falla, te muestra evidencia y celebra tu progreso sin quitarte el control creativo.**
