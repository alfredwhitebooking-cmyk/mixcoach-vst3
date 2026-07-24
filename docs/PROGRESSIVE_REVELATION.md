# Progressive Revelation Rules

## Principio fundamental

Ningún analizador, panel o herramienta aparece en la UI sin una **decisión del usuario** o un **paso narrativo del Director** que lo justifique explícitamente.

## Niveles de revelación (AnalysisScope)

| Scope | Cuándo se activa | Paneles permitidos | Ejemplo |
|-------|------------------|-------------------|---------|
| Setup | Durante onboarding (Welcome → MixMap) | Coach, Reference, Messengers | "Necesito que cargues tu referencia" abre Reference |
| Coaching | Durante el loop narrativo (GainStaging → MasterCheck) | Coach, Tools (solo analyzer de fase), Session | Director ejecuta ShowEvidence → abre analyzer contextual |
| Expert | Cuando el usuario explícitamente pide ver una herramienta | Coach, Tools (todos), Session, MixMap | Usuario clickea tab Tools o escribe "muéstrame el espectro" |
| Report | En la pantalla de reporte final | Coach, Report | NarrativeStep::Complete → Report |

## Reglas detalladas

### Regla 1: El Coach no revela analizadores en Setup

Durante onboarding (CoachRoomState < GainStaging), ningún mensaje del Coach puede revelar Tools.
El unico panel revelable es Reference (cuando el Coach pide cargar referencia) y Messengers (cuando detecta pistas).

### Regla 2: El Director decide qué analyzer se abre en Coaching

En el loop narrativo, solo el CoachingNarrativeDirector::doShowEvidence() puede abrir el analyzer correspondiente a la fase.
El analyzer revelado depende del ProblemType mapeado por EvidenceViewMapper, NO de keywords en el texto del Coach.

### Regla 3: El usuario siempre puede abrir Tools manualmente

El tab Tools siempre es accesible, pero al abrirlo manualmente se activa auto-return (5s) para volver al Coach.
En split-view coaching, Tools no es necesario: el analyzer contextual ya está visible en el panel derecho.

### Regla 4: El LLM no puede revelar paneles sin pasar por el Director

LlmCommandInterpreter::onRevealPanel solo se ejecuta si el Director está Idle (no hay ciclo narrativo activo).
Si el Director está ocupado, la petición se encola y se entrega al final del ciclo.

### Regla 5: Auto-return solo en modo Expert

startAutoReturn() solo se activa cuando el usuario cambia manualmente al tab Tools o Session durante coaching.
No se activa cuando el Director abre un analyzer contextual en el panel derecho (split-view).

## Implementación

1. AnalysisScope enum en PanelRevealManager.h
2. PanelRevealManager::validateReveal(scope, panelId) → bool
3. NavigationShell::revealPanel() llama validateReveal() antes de revelar
4. Director setea scope = Coaching cuando ejecuta doShowEvidence()
5. SceneManager setea scope = Setup durante onboarding
6. Panel menu manual se ejecuta siempre (siempre se puede acceder)
Documento creado: Mon Jul 20 17:09:49 GMT 2026
