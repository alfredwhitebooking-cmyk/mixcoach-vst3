# MixCoach — Benchmark Frame a Frame

## Objetivo

Validar que la experiencia del plugin C++ en FL Studio coincida con el prototipo HTML (`MixCoach_Prototype.html`) en términos de **ritmo, timing, secuencia, y sensación subjetiva**, no de look pixel-perfect.

## Metodología de grabación

### Equipo necesario
- **OBS Studio** (gratuito) o **NVIDIA ShadowPlay** / **Windows Game Bar** (Win+G)
- Opcional: **DaVinci Resolve** o **Shotcut** para edición frame a frame

### Configuración de grabación
1. **Resolución:** 1920x1080
2. **FPS:** 60 fps (para análisis frame preciso — cada frame = ~16.67ms)
3. **Codec:** H.264
4. **Audio:** No necesario para este benchmark

### Grabación A: Prototipo HTML
1. Abrir `MixCoach_Prototype.html` en Chrome/Edge
2. Poner ventana en 1200x800 (presiona F12 → Device Toolbar → 1200x800)
3. Iniciar grabación
4. Completar flujo completo: Welcome → Mode → Genre → Reference → Prep → MixMap → Coaching (7 problemas) → Report
5. Detener grabación → guardar como `html_prototype.mp4`

### Grabación B: Plugin en FL Studio
1. Cargar MixCoach VST3 en FL Studio (mix de prueba con 8-16 pistas)
2. Poner plugin en 1200x800 (igual que el HTML)
3. Iniciar grabación
4. Completar el MISMO flujo: Welcome → Mode → Genre → Reference → Prep → MixMap → Coaching → Report
5. Detener grabación → guardar como `plugin_flstudio.mp4`

### Análisis frame a frame
1. Cargar ambos videos en editor (DaVinci/Shotcut)
2. Poner timeline a 60fps
3. Para cada ítem de QA: ir al frame exacto del evento en HTML, anotar timestamp
4. Ir al frame correspondiente en el plugin, anotar timestamp
5. Calcular diferencia Δ = |t_plugin - t_html|
6. **Criterio de aprobación:** Δ < 200ms (humano no percibe diferencia)

---

## ════════════════════════════════════════════════════════════
## QA #1: COACHING LOOP — 14 ítems críticos
## ════════════════════════════════════════════════════════════

### Q1.1: Detección de problema — "Clipping en Kick"

**Flujo HTML:** 
1. `showTyping(800)` → indicador \"IA escribiendo...\" visible
2. `addMessage('El Kick está clipeando...', 0)` → mensaje tipo coach aparece sin delay
3. `showTyping(800)` → 2º typing
4. `addEvidence('vu')` → system message aparece + panel se abre

| HTML timestamps (referencia) | Plugin | Diferencia |
|:----------------------------|:-------|:-----------|
| typing_start: 0ms | typing_start: ___ms | Δ: ___ms |
| msg_coach: +800ms | msg_coach: ___ms | Δ: ___ms |
| typing_2: +800ms | typing_2: ___ms | Δ: ___ms |
| system_evidence: +800ms | system_evidence: ___ms | Δ: ___ms |

**Aceptación:** Δ < 200ms en cada paso. Secuencia sin saltos.

### Q1.2: Problema EQ 2.5kHz — spectrum con highlight

**Flujo HTML:**
1. `showTyping(1200)` → typing largo, el más complejo
2. `addSystemMessage('🔍 Abriendo analizador espectral con marcador en 2.5 kHz...', 300)`
3. `addMessage('La Voz Principal y el Synth Pad están peleando...', 0)`
4. Canvas spectrum se pinta con línea roja en 2500Hz

| Item | HTML | Plugin | Δ |
|:-----|:----:|:------:|:-:|
| typing_start | 0ms | ___ms | ___ms |
| system_message | 1200+300=1500ms | ___ms | ___ms |
| coach_explain | 1500ms | ___ms | ___ms |
| spectrum_drawn | ~2500ms | ___ms | ___ms |

**Aceptación:** Highlight en 2500Hz visible. Delta < 200ms.

### Q1.3: Problema crest — gauge animado

**Flujo HTML:**
1. `showTyping(1000)`
2. `addSystemMessage('🔍 Abriendo Crest Meter...', 300)`
3. `showTyping(1200)` → explicación larga
4. `addMessage('El crest factor...', 0)`
5. Canvas gauge se pinta con aguja animada

**Aceptación:** Gauge animado visible. Crest value actualizado desde engine.

### Q1.4: Problema estéreo — vectorscope

**Flujo HTML:**
1. `showTyping(1000)` 
2. `addSystemMessage('🔍 Abriendo Vectorscope...', 300)`
3. `showTyping(1200)`
4. `addMessage('La correlación de 0.95...', 0)`
5. Canvas vectorscope se pinta

**Aceptación:** Vectorscope visible con correlación real. Δ < 200ms.

### Q1.5: Problema LUFS/Automation — loudness

**Flujo HTML:**
1. `showTyping(1000)`
2. `addSystemMessage('🔍 Monitoreando LUFS en tiempo real...', 300)`
3. `showTyping(1200)`
4. `addMessage('La voz se pierde en el coro...', 0)`
5. AutomationPanel con LUFS timeline

**Aceptación:** AutomationPanel visible con LUFS timeline y secciones.

### Q1.6: Master Check — comparación vs referencia

**Flujo HTML:**
1. `showTyping(1000)`
2. `addSystemMessage('🔍 Comparando con referencia...', 300)`
3. `showTyping(1200)`
4. `addMessage('Hay 3 dB de exceso en 40-100 Hz...', 0)`
5. Match score se muestra en panel

**Aceptación:** Panel MasterCheck con datos reales de DifferenceProfile.

### Q1.7: Usuario no toca ningún tab durante 7 problemas

**Verificación:** El split-view (chat 380px + evidencia) se mantiene visible todo el tiempo. El tab Tools nunca aparece de forma forzada.

| Fase | HTML | Plugin |
|:-----|:----:|:------:|
| Gain Staging | Split view | Split view? |
| Balance | Split view | Split view? |
| EQ | Split view | Split view? |
| Compression | Split view | Split view? |
| Space | Split view | Split view? |
| Automation | Split view | Split view? |
| Master Check | Split view | Split view? |

**Aceptación:** 7/7 fases en split-view. 0 cambios de tab manuales necesarios.

### Q1.8: Quick reply y option card producen mismo resultado

**Verificación:**
1. Hacer clic en tarjeta de opción (Native/Free/Premium)
2. Verificar que dispara el verify loop (corrección → verificación → celebrate)
3. Hacer clic en quick reply correspondiente
4. Verificar que produce EXACTAMENTE la misma secuencia

**Aceptación:** Misma secuencia verify en ambos casos. Mismo mensaje de confirmación.

### Q1.9: Verify fallido re-explica (max 3 retries)

**Verificación:**
1. Simular verify fallido (corrección no aplicada correctamente)
2. Plugin debe mostrar mensaje de re-explicación
3. Dar opciones de nuevo (no pasar automáticamente)
4. Después de 3 intentos, dar como parcial

**Aceptación:** RetryCount se incrementa. Mensaje re-explicativo aparece. Max 3 intentos.

### Q1.10: Input deshabilitado durante verify

**Verificación:**
1. Durante verify (mensajes 🔄, 👂), el input de chat debe estar deshabilitado (gris, no clicable)
2. Después de verify OK o celebrate, input se re-habilita

**Aceptación:** `chatInput.disabled = true` durante verify. Se re-habilita en celebrate/next.

### Q1.11: Option cards clicables

**Verificación:**
1. 3 tarjetas (Native/Free/Premium) son dibujadas inline en el chat
2. Hacer clic en cada una → dispara verify loop
3. Hit-test funciona en toda el área de la tarjeta

**Aceptación:** 3 tarjetas responden a clic. Cada una dispara verify con opción correcta.

### Q1.12: 7 pasos por problema

**Verificación:** Cada problema sigue exactamente:
1. Detect (typing + mensaje inicial)
2. ShowEvidence (system message + panel se abre)
3. Explain (mensaje coach con explicación)
4. ShowOptions (3 tarjetas aparecen)
5. Verify (🔄 → 👂 → ✅)
6. Celebrate (🎉 mensaje + robot animación)
7. Next (⭐ XP + transición)

**Aceptación:** 7/7 pasos por problema. Orden correcto. Ningún paso saltado.

### Q1.13: Delta real (Δ -2.3 dB en Kick) en verify

**Verificación:** El mensaje de verify debe incluir:
- Nombre del track: "Kick"
- Valor antes: "Peak: -12.5 dBFS" 
- Valor después: "Peak: -15.2 dBFS"
- Delta: "Δ -2.7 dB"

**Aceptación:** Mensaje con delta real desde SlotRegistry. No texto simulado/placeholder.

### Q1.14: LLM respeta ritmo del Director

**Verificación:** 
1. Enviar un mensaje al chat durante una secuencia de coaching
2. La respuesta del LLM debe encolarse hasta que el Director termine su ciclo
3. No debe interrumpir la secuencia typing → explain → options

**Aceptación:** Mensaje aparece DESPUÉS de que el ciclo actual termina. No interrumpe.

---

## ════════════════════════════════════════════════════════════
## QA #2: SETUP — 8 ítems
## ════════════════════════════════════════════════════════════

### Q2.1: Welcome → Modo → Género → Referencia → Prep → MixMap → Coaching

**Flujo HTML (8 pasos):**

| Paso | HTML timing | Plugin | Δ |
|:-----|:-----------:|:------:|:-:|
| Welcome visible | 0ms | ___ms | ___ms |
| Nombre ingresado + click | ~1000ms | ___ms | ___ms |
| Mode screen visible | 0ms (transición inmediata) | ___ms | ___ms |
| Mix mode clicked | ~1000ms | ___ms | ___ms |
| Genre screen visible | 400ms (robotNod delay) | ___ms | ___ms |
| Genre selected | ~1200ms | ___ms | ___ms |
| Reference screen visible | 400ms (auto-advance) | ___ms | ___ms |
| File dropped + analyzed | ~5000ms (5 stages x 700ms) | ___ms | ___ms |
| Prep screen visible | 400ms (auto-advance) | ___ms | ___ms |
| Checklist completed + confirm | ~4000ms | ___ms | ___ms |
| MixMap visible | 400ms (auto-advance) | ___ms | ___ms |
| Map confirmed | ~1000ms | ___ms | ___ms |
| Coaching visible (Gain Staging) | 400ms (auto-advance) | ___ms | ___ms |

**Aceptación:** 8 pasos completos. Cada transición con auto-advance 400ms.

### Q2.2: Robot nod en cada transición

| Transición | HTML | Plugin |
|:-----------|:----:|:------:|
| Welcome→Mode | robotNod() | setAvatarNod(500)? |
| Mode→Genre | robotNod() | setAvatarNod(400)? |
| Genre→Reference | robotNod() | setAvatarNod(400)? |
| Reference→Prep | robotNod() | setAvatarNod(400)? |
| Prep→MixMap | robotNod() | setAvatarNod(600)? |
| MixMap→Coaching | robotCelebrate() | setAvatarExpression(Happy)? |

**Aceptación:** Todas las transiciones tienen animación de robot visible.

### Q2.3: Prep confirm disabled hasta checklist completo

**Verificación:**
1. Botón "Listo, continuar" aparece gris/disabled al principio
2. Al completar los 4 items del checklist, botón se activa
3. Sin clics en botón disabled

**Aceptación:** Botón correctamente deshabilitado hasta completar checklist.

### Q2.4: Reference analysis muestra 5 stages

**Flujo HTML:**
- Stage 0: "Leyendo archivo..." → barra 15%
- Stage 1: "Analizando espectro..." → barra 35%  
- Stage 2: "Calculando LUFS..." → barra 55%
- Stage 3: "Analizando rango dinámico..." → barra 75%
- Stage 4: "✓ Completado" → barra 100%

Cada stage: 700ms → total ~3500ms.

**Aceptación:** 5 stages visibles. Progreso continuo. Transición a Prep automática.

### Q2.5: Transición uniforme con delay 400ms

**Verificación:** Todas las transiciones entre pasos de setup tienen exactamente 400ms de delay con robotNod() visible.

**Aceptación:** Medir timing entre pantallas. Δ < 100ms del target 400ms.

### Q2.6: SetupFadeAnim timing 300ms

**Verificación:** La animación de fade-in entre pantallas de setup dura exactamente 300ms (18 frames a 60fps) con ease-out quad.

**Aceptación:** SetupFadeAnim::kFrames = 18, kFadeInStart = 0.3. Timing coincide con CSS (transition: all .3s).

### Q2.7: TabBar oculta hasta GainStaging

**Verificación:** 
1. Welcome, Mode, Genre, Reference, Prep, MixMap: TabBar invisible
2. Al entrar a GainStaging: TabBar aparece

**Aceptación:** TabBar invisible en setup. Visible en coaching.

### Q2.8: Onboarding completo en <3 min

**Tiempo total HTML (estimado):** ~22s sin interacción + tiempo de usuario.
**Criterio:** Con usuario rápido (~5s por decisión) = ~60s. Con usuario lento (~15s) = ~120s. Total < 3 min.

**Aceptación:** Plugin permite completar onboarding en <3 minutos con 0 clicks en tabs.

---

## ════════════════════════════════════════════════════════════
## QA #3: POLISH — 8 ítems
## ════════════════════════════════════════════════════════════

### Q3.1: Typing indicator visible entre mensajes del coach

**Verificación:** El indicador "MixCoach is typing..." con 3 dots animados aparece entre cada mensaje del coach. Duración visible.

**HTML timing:** showTyping(800) = 800ms antes del mensaje inicial. showTyping(1200) = 1200ms antes de explicaciones largas.

**Aceptación:** Typing indicator visible. Timing NarrativeTiming::kDetectMs (800ms), kExplainMs (1200ms), etc.

### Q3.2: Burbujas entran con fade+slide

**Verificación:** Cada burbuja nueva aparece con:
- Alpha: 0 → 1 (fade in)
- TranslateY: 8px → 0px (slide up)
- Scale: 0.97 → 1.0
- Duración: 250ms
- Easing: ease-out quad

**Aceptación:** ChatBubble::animateIn=true, kAnimDurationMs=250, kAnimSlidePx=8, kAnimStartScale=0.97. 14 refs en CoachChatComponent.

### Q3.3: XP burst en barra superior

**Verificación:** Tras cada verify exitoso:
1. System message "⚡ +45 XP · +12% progreso" aparece en chat
2. Animación de XP burst en PhaseProgressBar (barra superior)
3. La animación es visible (burbuja flotante con +XP)

**Aceptación:** `navShell_.getPhaseProgressBar().triggerXpBurst(45)` llamado desde `doCelebrateStep()`. XP visible en barra superior.

### Q3.4: Partículas/orbes visibles en fondo

**Verificación:** El fondo tiene:
- 25 partículas flotando (colores: purple, cyan, green, pink)
- 3 orbes glow (purple, cyan, pink) con blur 80px
- Movimiento suave y continuo

**Aceptación:** BackgroundEffectsComponent activo. Partículas y orbes visibles.

### Q3.5: Reporte con datos reales

**Verificación:**
1. Match Score: valor real desde ReferenceProfile / DifferenceProfile
2. Tags "Has aprendido": fases completadas durante la sesión
3. Mejora vs baseline: datos reales desde CorrectionLearner
4. Botón "Nueva sesión": resetea todo el estado

**Aceptación:** EndOfSessionComponent con MixScore::compute(), drawUserCheckmarks(), onNewSession().

### Q3.6: Scroll suave en chat (300ms ease-out quad)

**Verificación:** Cuando un nuevo mensaje aparece y el chat hace scroll al fondo, la animación dura ~300ms con ease-out quad (no salto instantáneo).

**Aceptación:** advanceSmoothScroll() presente. SmoothScrollState::kDurationFrames = 18 (~300ms). 10 refs en CoachChatComponent.

### Q3.7: Glass simulado (gradiente + borde)

**Verificación:** Los paneles en coaching tienen:
- Fondo semi-transparente (alpha ~0.85)
- Borde sutil con alpha 0.15
- Esquinas redondeadas (8px cr)

**Aceptación:** MixCoachTheme::bgPanel().withAlpha(0.85f) + MixCoachTheme::border().withAlpha(0.15f).

### Q3.8: Hover lift en cards (mode, genre)

**Verificación:** 
- Mode cards: hover → translateY(-4px) con transición 0.3s CSS cubic-bezier
- Genre cards: hover → translateY(-2px) con transición 0.25s
- Option cards: hover → translateY(-3px)

**Aceptación:** Cards responden con lift animation al hover. Timing coincide con CSS.

---

## ════════════════════════════════════════════════════════════
## QA #4: REPORTE FINAL — 4 ítems
## ════════════════════════════════════════════════════════════

### Q4.1: Match Score real desde engine

**Valor HTML:** `state.matchScore = 92` (hardcodeado en demo)
**Valor plugin:** Debe venir de `MixScore::compute()` o `ReferenceProfile::compareToReference()`

**Verificación:** El valor mostrado en EndOfSessionComponent coincide con el calculado por el engine.

**Aceptación:** MatchScore no es placeholder. Es dato real del engine.

### Q4.2: Tags "Has aprendido" dinámicos

**HTML:** Muestra tags: "Gain Staging", "Balance", "EQ", "Compresión", "Espacio", "Master Check"
(6 tags = 6 fases completadas)

**Verificación:** El plugin debe mostrar tags de las fases que el usuario completó realmente durante la sesión.

**Aceptación:** Tags dinámicos basados en fases completadas (no hardcodeados).

### Q4.3: Mejora vs baseline

**HTML:** Muestra ítems:
- "Balance" (antes: 65%, después: 82% → +17%)
- "Tonalidad" (+22%)
- "Imagen Estéreo" (+35%)
- "Rango Dinámico" (+28%)

**Verificación:** Plugin debe mostrar datos de mejora basados en CorrectionLearner / perfil antes/después.

**Aceptación:** Datos reales de mejora, no placeholders.

### Q4.4: Botón "Nueva sesión" funcional

**HTML:** Restablece:
- state.xp = 0, state.progress = 0
- state.phase = 0, state.coachedPhases = new Set()
- Limpia chatMessages.innerHTML
- Resetea referencia, prep, mixmap
- Va a screen 'welcome'

**Verificación:** Botón en EndOfSessionComponent resetea TODO el estado y vuelve a Welcome.

**Aceptación:** Reset completo. Sin leaks de estado anterior.

---

## ════════════════════════════════════════════════════════════
## TIMING EXACTO DEL PROTOTIPO HTML
## ════════════════════════════════════════════════════════════

### setTimout delays (ms)
```
 300  → micro-pause (animación de dots)
 400  → transición auto-advance + robotNod
 500  → animación de robot nod/celebrate corta
 600  → typing corto (confirmaciones rápidas)
 1000 → typing medio (mensajes de evidencia)
 1600 → typing largo (celebrate, transiciones mayores)
```

### showTyping durations (ms)
```
 600  → typing muy corto (system message)
 800  → typing inicial de problema (DETECT)
 1000 → typing estándar (evidence, opciones)
 1200 → typing explicativo (EXPLAIN — explicaciones pedagógicas)
 1400 → typing largo
 1500 → typing verify (ESCUCHANDO)
 1800 → typing más largo (verify con escucha)
```

### CSS animation durations (s)
```
 0.3  → msgIn (entrada de burbuja), fade entre screens
 0.5  → robotNod
 1.4  → typingBounce (3 dots) 
 1.5  → earPulse, robotCelebrate
 2.0  → robotThinking
 3.0  → robotFloat
 8.0  → orbFloat (orbe glow)
```

### CSS transition durations (s)
```
 0.2  → botones (hover lift)
 0.25 → genre cards (hover)
 0.3  → mode cards, option cards, todo lo demás
```

### Secuencia de 7 problemas (tiempos exactos)
| Problema | Detect | Evidence | Explain | Options | Verify | Celebrate | XP | Total |
|:---------|:------:|:--------:|:-------:|:-------:|:------:|:---------:|:--:|:-----:|
| Gain Staging | 800ms | 800+300=1100ms | 1200ms | 800ms | 1500+1200+1800=4500ms | 1000ms | 500ms | ~9.9s |
| Balance | 800ms | 1100ms | 1200ms | 800ms | 4500ms | 1000ms | 500ms | ~9.9s |
| EQ | 800ms | 1100ms | 1200ms | 800ms | 4500ms | 1000ms | 500ms | ~9.9s |
| Compresión | 800ms | 1100ms | 1200ms | 800ms | 4500ms | 1000ms | 500ms | ~9.9s |
| Espacio | 800ms | 1100ms | 1200ms | 800ms | 4500ms | 1000ms | 500ms | ~9.9s |
| Automatización | 800ms | 1100ms | 1200ms | 800ms | 4500ms | 1000ms | 500ms | ~9.9s |
| Master Check | 800ms | 1100ms | 1200ms | 800ms | 4500ms | 1000ms | 500ms | ~9.9s |

**Total del loop de 7 problemas:** ~70s (sin contar tiempo de usuario para elegir opciones)

---

## ════════════════════════════════════════════════════════════
## PLANTILLA DE REPORTE
## ════════════════════════════════════════════════════════════

### Instrucciones
1. Grabar HTML: `html_prototype.mp4`
2. Grabar Plugin: `plugin_flstudio.mp4`  
3. Para cada ítem, marcar:
   - ✅ PASA (Δ < 200ms, comportamiento idéntico)
   - 🟡 PARCIAL (difiere pero funcionalmente equivalente)
   - ❌ FALLA (no existe o comportamiento muy diferente)
   - 📝 Notas adicionales

| # | QA Item | Categoría | Resultado | Δ (ms) | Notas |
|:-:|:--------|:---------:|:---------:|:------:|:------|
| 1 | Detección clipping | Coaching | ___ | ___ | |
| 2 | EQ highlight 2500Hz | Coaching | ___ | ___ | |
| 3 | Crest gauge animado | Coaching | ___ | ___ | |
| 4 | Vectorscope | Coaching | ___ | ___ | |
| 5 | LUFS/Automation | Coaching | ___ | ___ | |
| 6 | Master Check match | Coaching | ___ | ___ | |
| 7 | Sin tabs en 7 problemas | Coaching | ___ | — | |
| 8 | QR = Card | Coaching | ___ | — | |
| 9 | Retry max 3 | Coaching | ___ | — | |
| 10 | Input disabled | Coaching | ___ | — | |
| 11 | Cards clicables | Coaching | ___ | — | |
| 12 | 7 pasos por problema | Coaching | ___ | — | |
| 13 | Delta real dB | Coaching | ___ | — | |
| 14 | LLM respeta ritmo | Coaching | ___ | — | |
| 15 | 8 pasos setup | Setup | ___ | — | |
| 16 | Robot nod transiciones | Setup | ___ | — | |
| 17 | Prep btn disabled | Setup | ___ | — | |
| 18 | Reference 5 stages | Setup | ___ | — | |
| 19 | Transición uniforme 400ms | Setup | ___ | ___ | |
| 20 | SetupFadeAnim 300ms | Setup | ___ | ___ | |
| 21 | TabBar oculta en setup | Setup | ___ | — | |
| 22 | Onboarding <3 min | Setup | ___ | — | |
| 23 | Typing indicator | Polish | ___ | ___ | |
| 24 | Burbujas fade+slide | Polish | ___ | ___ | |
| 25 | XP burst barra | Polish | ___ | — | |
| 26 | Partículas/orbes | Polish | ___ | — | |
| 27 | Reporte datos reales | Polish | ___ | — | |
| 28 | Scroll suave chat | Polish | ___ | ___ | |
| 29 | Glass simulado | Polish | ___ | — | |
| 30 | Hover lift cards | Polish | ___ | — | |
| 31 | Match Score real | Reporte | ___ | — | |
| 32 | Tags "Has aprendido" | Reporte | ___ | — | |
| 33 | Mejora vs baseline | Reporte | ___ | — | |
| 34 | Botón Nueva sesión | Reporte | ___ | — | |

### Scoring final
```
✅ PASAN: ___/34 (target: 30/34 = 88%)
🟡 PARCIALES: ___/34
❌ FALLAN: ___/34
Δ promedio: ___ ms (target: <200ms)

Resultado: ___%
```

---

## Resumen de tiempos del prototipo HTML

| Componente | HTML | Plugin target | Estado |
|:-----------|:----:|:-------------:|:------:|
| msgIn animación | 250ms fade+slide+scale | ChatBubble::kAnimDurationMs=250 | ✅ |
| Robot nod | 500ms | setAvatarNod(500) | ✅ |
| Robot celebrate | 1500ms | Director.doCelebrateStep + setAvatarExpression | ✅ |
| Robot thinking | continuo | ChatMessageSequencer.showTyping | ✅ |
| XP burst | instantáneo + badge update | PhaseProgressBar.triggerXpBurst | ✅ |
| Scroll suave | scroll-behavior: smooth (~300ms) | SmoothScrollState::kDurationFrames=18 | ✅ |
| Setup fade | transition: all 0.3s | SetupFadeAnim::kFrames=18 (~300ms) | ✅ |
| Phase dots | active + done + pulse | PhaseProgressBar setDotActive | ✅ |
| Option cards | 3 tiers hover lift | PluginSuggestionCard draw | ✅ |
| Reference analysis | 5 stages x 700ms | ReferenceAnalysisProgressCard 5 stages | ✅ |
| Progress bar | gradient, 0.6s ease | PhaseProgressBar gradient | ✅ |
