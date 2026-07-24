# 08 — CHAT COMMANDED UI (ARQUITECTURA DE REVELACIÓN)

> **DOCUMENTO FUNDACIONAL — LECTURA OBLIGATORIA para cualquier IA que trabaje en MixCoach.**
> Define CÓMO y CUÁNDO aparecen los elementos UI. El Chat (Coach) es el único punto de entrada.
> Nada aparece sin que el Coach lo pida. Nada.
>
> **Versión:** 1.0 | **Última actualización:** 29 junio 2026
> **Autor:** Fundador + Buffy (Codebuff AI)

---

## 1. La Regla de Oro

> **El Chat comanda. La UI obedece.**
> **Ningún elemento UI existe antes de que el Coach lo necesite.**

### Corolarios

1. **No hay dashboard.** Hay un chat que guía.
2. **No hay tools.** Hay un coach que abre herramientas cuando las necesita.
3. **No hay session map.** Hay un coach que construye el mapa cuando los Messengers están listos.
4. **No hay referencia.** Hay un coach que pide una referencia cuando la necesita.

### Excepción Única

El usuario puede navegar manualmente a un panel **después** de que ha sido revelado por primera vez. Pero nunca antes.

---

## 2. Las Tres Reglas de Revelación

### Regla R1 — Keyword Reveal

Cuando el Coach envía un mensaje que contiene una keyword, el panel correspondiente se revela.

| Keyword | Panel que se revela |
|:--------|:--------------------|
| "referencia" / "reference" / "carga tu" | Reference Panel |
| "messenger" / "inserta" / "pista" / "track" | Messenger List |
| "mapa" / "routing" / "bus" / "ruteo" | Mix Map / Session Mode |
| "analizador" / "espectro" / "frequencia" / "analyzer" | Tools (Spectrum) |
| "fase" / "phase" / "correlación" / "vectorscope" | Tools (Phase Scope) |
| "LUFS" / "loudness" / "volumen" / "master meter" | Master Meter / Tools (LUFS) |
| "estereo" / "stereo" / "ancho" / "width" | Tools (Stereo Width) |
| "progreso" / "historial" / "sesión" / "avance" | Session Tab / Progress |
| "reporte" / "report" / "finalizar" / "terminar" | Report / EndOfSession |
| "vu" / "VU" / "analógico" / "vintage" | Tools (Vintage VU) |

### Regla R2 — Track Mention Reveal (Highlighting)

Cuando el Coach menciona un track específico, ese track se resalta en la Messenger List.

| Keyword en mensaje | Track que se resalta |
|:-------------------|:---------------------|
| "kick" | Kick slot → glow + scroll |
| "snare" | Snare slot → glow + scroll |
| "bass" / "808" | Bass slot → glow + scroll |
| "voz" / "vocal" / "voice" | Voz slot → glow + scroll |
| "hihat" / "hi-hat" / "hat" | HiHat slot → glow + scroll |
| *cualquier track name* | Match por nombre de slot |

**Comportamiento visual del highlight:**
- Borde glow del color del dominio del problema (gain=rojo, tonal=naranja, dynamics=amarillo, spatial=cyan)
- Scroll automático al slot en la Messenger List
- Tooltip con descripción corta del problema
- Si se mencionan múltiples tracks, todos se resaltan simultáneamente

### Regla R3 — SetupStep Reveal

Además de keywords, el estado del setup dialogue (SetupStep) controla revelaciones automáticas.

| SetupStep | Paneles que se revelan |
|:----------|:-----------------------|
| `NotStarted` | Solo Chat |
| `WaitingForName` | Chat + Input |
| `WaitingForMode` | Chat + Botones Mix/Master |
| `WaitingForReferenceFirst` | Chat + Reference Panel (DropZone) |
| `WaitingForGenre` | Chat + Chips de género |
| `WaitingForConfirm` | Chat + Messenger List |
| `Complete` | Chat + Messenger List + Master Meter |

---

## 3. Sistema PanelRevealManager

### 3.1 Arquitectura

```
CoachEngine::sendMessage(text)
  ↓
PanelRevealManager::processMessage(text)
  ├── detecta keywords → revealPanel(panelId)
  ├── detecta track names → highlightTrack(slotIndex)
  └── actualiza locked state en NavigationShell

PanelRevealManager
  ├── revealedPanels_: std::set<PanelId>  // Paneles ya revelados
  ├── activeHighlights_: std::vector<int> // Slots actualmente resaltados
  ├── keywordMap_: std::map<string, PanelId>  // Keywords → panels
  ├── trackNameCache_: // Track names → slot indices
  │
  ├── revealPanel(PanelId id)
  │   ├── marca panel como revelado
  │   ├── si es primera vez: animación "NEW" + crossfade reveal
  │   └── NavigationShell::setTabLocked(panel, false)
  │
  ├── highlightTrack(int slotIndex)
  │   ├── scroll al slot en MessengerList
  │   ├── aplica glow según dominio del problema
  │   └── muestra tooltip con descripción
  │
  └── clearHighlights()
      └── remueve todos los glows al enviar nuevo mensaje
```

### 3.2 Estados de un Panel

| Estado | Significado | Acción del usuario |
|:-------|:------------|:-------------------|
| `Hidden` | No revelado aún, no existe para el usuario | No puede acceder |
| `Revealed` | El Coach lo mencionó, panel visible | Puede verlo e interactuar |
| `Active` | Panel actualmente visible en el content area | Interacción completa |
| `Dismissed` | El usuario cerró o el Coach cambió de tema | Sigue revelado, accesible desde menú |

### 3.3 Menú de Paneles Revelados

Una vez que un panel ha sido revelado, debe haber una forma de acceder a él:

```
[☰] → Menú de paneles revelados
  ├── 📁 Reference          (revelado)
  ├── 🎚 Messengers         (revelado)
  ├── 🗺 Mix Map            (revelado)
  ├── 📊 Analyzers          (revelado)
  ├── 📈 Progress           (aún no revelado)
  └── 📋 Report             (aún no revelado)
```

Los paneles NO revelados aparecen atenuados con un candado 🔒 y tooltip: "El Coach te guiará aquí cuando sea el momento."

---

## 4. Sistema TrackHighlightManager

### 4.1 Flujo de Highlighting

```
1. CoachEngine genera mensaje: "El KICK tiene demasiados graves..."
2. TrackHighlightManager::processMessage(text)
3.   → Detecta "KICK" en trackNameCache_
4.   → Obtiene slotIndex del Kick
5.   → CoachEngine tiene Issue(s) para ese slot
6.   → Aplica glow ROJO (gain) + scroll automático
7.   → Show tooltip: "Kick: +4dB sobre target de género"
8.   → Si hay Issues de otros dominios: multi-glow

9. Usuario ajusta el Kick

10. CoachEngine detecta mejora
11. → TrackHighlightManager::clearHighlights()
12. → Coach envía nuevo mensaje (sin highlight previo)
```

### 4.2 Colores de Highlight por Dominio

| Dominio | Color glow | Significado |
|:--------|:-----------|:------------|
| `gain` | 🔴 Rojo `#EF4444` | Nivel incorrecto |
| `tonal` | 🟠 Naranja `#F97316` | EQ/Balance tonal |
| `dynamics` | 🟡 Amarillo `#F59E0B` | Compresión/Rango dinámico |
| `spatial` | 🔵 Cyan `#00B7FF` | Fase/Estéreo/Ancho |
| `masking` | 🟣 Púrpura `#A855F7` | Enmascaramiento entre tracks |

### 4.3 Multi-track Highlighting

El Coach puede resaltar múltiples tracks en un solo mensaje:

```
"El KICK y el BASS están peleando en 60Hz."
→ Kick slot resaltado (🔴 gain domain)
→ Bass slot resaltado (🟠 tonal domain)
→ Ambos con scroll al más crítico
```

---

## 5. Mapa de Revelación Completo

### 5.1 FLUJO COMPLETO paso a paso

```
PASO | Coach dice                                    | Qué se revela                   | Tracks resaltados
─────┼───────────────────────────────────────────────┼─────────────────────────────────┼─────────────────
  1  | "¡Hola! ¿Cómo te llamas?"                    | Nada (solo chat)                | -
  2  | "¡Genial, Alex! ¿Mix o Master?"              | Botones Mezcla/Mastering        | -
  3  | "Carga tu referencia aquí..."                | 🔓 REFERENCE PANEL              | -
  4  | "He analizado tu referencia..."              | (panel reference se actualiza)  | -
  5  | "Inserta Messengers en tus pistas..."        | 🔓 MESSENGER LIST               | -
  6  | "Aquí tienes el mapa de sesión..."           | 🔓 MIX MAP / SESSION MODE       | -
  7  | "El KICK tiene demasiados graves..."         | -                               | 🔴 Kick
  8  | "La SNARE suena apagada..."                  | -                               | 🟠 Snare
  9  | "Activo el espectro para que veas..."        | 🔓 TOOLS (Spectrum)             | -
 10  | "Mira la fase del BASS..."                   | 🔓 TOOLS (Phase Scope)          | 🔵 Bass
 11  | "Has progresado bastante..."                 | 🔓 SESSION / Progress           | -
 12  | "¿Listo para el reporte?"                    | 🔓 REPORT / EndOfSession        | -
```

### 5.2 VISIBILIDAD ACUMULATIVA

```
           Chat  Ref  Msgrs  Map  Tools  Sess  Rprt
PASO  1:    ✅    ❌    ❌    ❌    ❌    ❌    ❌
PASO  2:    ✅    ❌    ❌    ❌    ❌    ❌    ❌
PASO  3:    ✅    ✅    ❌    ❌    ❌    ❌    ❌
PASO  5:    ✅    ✅    ✅    ❌    ❌    ❌    ❌
PASO  6:    ✅    ✅    ✅    ✅    ❌    ❌    ❌
PASO  9:    ✅    ✅    ✅    ✅    ✅    ❌    ❌
PASO 11:    ✅    ✅    ✅    ✅    ✅    ✅    ❌
PASO 12:    ✅    ✅    ✅    ✅    ✅    ✅    ✅
```

---

## 6. Integración con CoachRoomState

El `PanelRevealManager` se integra con el `CoachRoomState` existente:

```cpp
// NavigationShell actualizado con PanelRevealManager
class NavigationShell {
    PanelRevealManager revealManager_;
    
    // Cuando el Coach envía un mensaje:
    void onCoachMessage(const juce::String& text) {
        auto reveals = revealManager_.processMessage(text);
        for (auto& panel : reveals) {
            revealPanel(panel);
        }
        auto highlights = revealManager_.highlightTracks(text);
        for (auto& slot : highlights) {
            highlightTrack(slot);
        }
    }
    
    // Revela un panel (primera vez o toggle)
    void revealPanel(PanelId id) {
        if (id == PanelId::Reference) {
            tabBar_.setTabLocked(TabBarComponent::Reference, false);
            // Mostrar animación "NEW - Reference Ready"
        }
        if (id == PanelId::Tools) {
            tabBar_.setTabLocked(TabBarComponent::Tools, false);
            // Mostrar animación "NEW - Tools Unlocked"
        }
    }
};
```

---

## 7. Implementación Técnica

### 7.1 Archivos a crear

| Archivo | Propósito |
|:--------|:----------|
| `Source/MixCoach/engine/PanelRevealManager.h` | Manager de revelación de paneles |
| `Source/MixCoach/engine/PanelRevealManager.cpp` | Implementación con keyword matching |
| `Source/MixCoach/engine/TrackHighlightManager.h` | Manager de highlighting de tracks |
| `Source/MixCoach/engine/TrackHighlightManager.cpp` | Implementación con fuzzy match |

### 7.2 Archivos a modificar

| Archivo | Cambio |
|:--------|:-------|
| `NavigationShell.h/.cpp` | Integrar PanelRevealManager, conectar onCoachMessage |
| `TabBarComponent.h/.cpp` | Añadir PanelId enum (Reference, Session, Tools) consistente |
| `CoachChatComponent.cpp` | Conectar highlightTrackMention mejorado |
| `MessengerListComponent.cpp` | Aceptar highlight externo con glow por dominio |

### 7.3 Keyword Detector — Diseño

```cpp
class KeywordDetector {
public:
    struct Match {
        PanelId panel;      // Panel a revelar
        int slotIndex = -1; // Track a resaltar (-1 = none)
        int domain    = -1; // Dominio del problema (-1 = none)
    };
    
    // Procesa un mensaje y devuelve matches
    std::vector<Match> process(const juce::String& text);
    
private:
    // Keywords → Panel mapping
    struct KeywordRule {
        const char* keyword;
        PanelId panel;
        bool requireExactWord; // true = "referencia" no "referencial"
    };
    
    // Track names cacheados de SlotRegistry
    std::vector<juce::String> trackNames_;
};
```

### 7.4 Estructura de PanelId

```cpp
enum class PanelId : uint8_t {
    Coach,      // Siempre visible (default)
    Reference,  // Se revela con "referencia", "carga tu..."
    Messengers, // Se revela con "messenger", "pista", "track"
    MixMap,     // Se revela con "mapa", "routing", "bus"
    Tools,      // Se revela con "analizador", "espectro", "fase"
    Session,    // Se revela con "progreso", "historial"
    Report,     // Se revela con "reporte", "finalizar"
    
    Count
};
```

---

## 8. Plan de Implementación (Fases)

### Fase 1 — Fundación (Estimado: 2-3 sesiones)
- [ ] Crear `PanelRevealManager.h/.cpp` con keyword detection básico
- [ ] Crear `TrackHighlightManager.h/.cpp` con highlight por keyword
- [ ] Añadir `PanelId` enum a `NavigationShell`
- [ ] Integrar en `NavigationShell::onCoachMessage()`
- [ ] Conectar con `CoachChatComponent::highlightTrackMention()` existente

### Fase 2 — UI de Revelación (Estimado: 2 sesiones)
- [ ] Animación "NEW" cuando un panel se revela por primera vez
- [ ] Menú "☰ Paneles" con estado de revelación
- [ ] Candado 🔒 en paneles no revelados
- [ ] Tooltip "El Coach te guiará aquí cuando sea el momento"
- [ ] Botón "Volver al Coach" en cada modo

### Fase 3 — Highlighting Avanzado (Estimado: 2 sesiones)
- [ ] Multi-track highlighting simultáneo
- [ ] Glow por dominio del problema (rojo/naranja/amarillo/cyan)
- [ ] Auto-scroll suave al slot resaltado
- [ ] Tooltip con descripción del problema
- [ ] Clear highlights automático al enviar nuevo mensaje del Coach

### Fase 4 — Integración Total (Estimado: 2-3 sesiones)
- [ ] Conectar con `CoachEngine::SetupStep` para revelaciones automáticas
- [ ] Conectar con `TrackAdvice` para colores de dominio
- [ ] Persistencia de paneles revelados entre sesiones
- [ ] Pruebas de integración con el flujo completo de bienvenida
- [ ] Documentación final y cleanup

---

## 9. Reglas para Futuras IAs

1. **El chat es el centro.** Cualquier propuesta que desplace el chat del centro visual o funcional está equivocada.
2. **Nada aparece sin orden del Coach.** Si un panel se muestra sin que el Coach lo haya mencionado, es un bug.
3. **Los highlights tienen color por dominio.** Nunca usar un solo color para todos los problemas.
4. **El menú de paneles solo muestra lo revelado.** No mostrar paneles bloqueados como "próximamente".
5. **No hay scores numéricos.** El Coach nunca dice "tu score es 72". Dice "vas por buen camino".
6. **La referencia es un norte, no una copia.** El Coach nunca dice "copia esto".
7. **El Coach celebra, no juzga.** Los highlights son para guiar, no para castigar.
8. **Multi-track es importante.** El Coach puede mencionar varios tracks a la vez. Todos deben resaltarse.
9. **El highlighting no persiste.** Cada nuevo mensaje del Coach limpia los highlights anteriores.
10. **El PanelRevealManager es el dueño de la visibilidad.** Ningún otro componente decide qué panel se muestra.

---

*Documento fundacional de Chat-Commanded UI — MixCoach v1.0 — 29 junio 2026*
*Este documento debe ser leído por cualquier IA antes de modificar la UI.*
*Si un cambio contradice las reglas aquí definidas, el cambio debe rechazarse.*
