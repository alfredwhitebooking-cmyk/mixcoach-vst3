# MixCoach Agent Brief

This repository is intentionally optimized for AI-assisted development with Codex,
Antigravity, Freebuff-style workflows, and human developers. Read this file first,
then follow the links below only as needed.

## Product Goal

MixCoach is a real-time mixing mentor, not an audio processor.

The user wants two VST3 plugins:

- **Messenger**: one lightweight plugin per track. It listens, measures, and sends
  telemetry. It should stay very cheap on CPU and never change the audio.
- **MixCoach**: one plugin on the master bus. It receives all Messenger telemetry,
  shows professional analyzers, and guides the engineer with practical mentoring.

The north star is: **Engineer + MixCoach as a team**. The AI should teach, suggest,
organize, and validate decisions without taking control away from the engineer.

## Visual North Star

The UI reference images are source of truth for the desired look and workflow:

- `UI_REFERENCES/Messenger.png`
- `UI_REFERENCES/MixCoach_Tab1_AICoach.png`
- `UI_REFERENCES/MixCoach_Tab2_Analyzers.png`

Before making UI changes, inspect those images and read:

- `workspace_memory/visual_design.md`
- `workspace_memory/component_map.md`

The intended UI is a dark, professional audio tool:

- Deep black/blue-black canvas with subtle panel separation.
- Purple brand headers and active states.
- Cyan for AI/spectrum accents.
- Bus colors must remain consistent: drums purple, bass blue, guitars orange,
  keys teal, vocals pink, master red.
- Dense, scan-friendly layouts. No marketing page, no decorative hero, no
  generic dashboard style.
- The reference images beat any vague text description.

## Architecture In One Pass

- `Source/Common/`: shared types, audio helpers, logging, IPC, shared memory,
  slot registry, backup file fallback.
- `Source/Messenger/`: per-track plugin, telemetry collector, compact UI.
- `Source/MixCoach/`: master plugin, coaching engine, phase manager, analyzers,
  main UI.
- `tests/`: C++ and Python tests.
- `scripts/`: validation and deploy helpers.
- `workspace_memory/`: durable project memory for agents.
- `AI_CONTEXT.md`: consolidated long-form context.

Critical data flow:

1. Messenger analyzes audio in `processBlock()`.
2. Messenger writes telemetry via shared memory and backup files.
3. MixCoach syncs slots and telemetry.
4. UI analyzers update from `SlotRegistry`.
5. `CoachEngine` and `PhaseManager` generate mentoring messages.

## Commands Agents Should Prefer

Use the lowercase build directory only: `build/`.

```powershell
# Save a project checkpoint before risky work
.\scripts\ProjectCheckpoint.ps1 -Action Save -Name "before_ui_refactor"

# Build Release VST3s and deploy when possible
.\build.ps1

# List and restore deployed VST3 backups
.\DeployVST3.ps1 -Action ListBackups
.\DeployVST3.ps1 -Action Restore -Backup latest -Force

# Build without deploy
.\build.ps1 -NoDeploy

# Full validation helper
.\scripts\validate.ps1

# CMake test target
cmake --build build --config Release --target run_tests
```

If FL Studio is open, deploy to `C:\Program Files\Common Files\VST3\` can fail
because the VST3 DLLs may be locked.

## Backup And Restore

Use project checkpoints before risky AI edits, large UI rewrites, IPC work, or
dependency/build changes.

```powershell
# Create a zip checkpoint of the useful workspace files
.\scripts\ProjectCheckpoint.ps1 -Action Save -Name "before_big_change"

# List available checkpoints
.\scripts\ProjectCheckpoint.ps1 -Action List

# Restore a checkpoint folder or zip over the current workspace
.\scripts\ProjectCheckpoint.ps1 -Action Restore -Checkpoint "C:\Proyectos\MixCoach\workspace_backups\project_checkpoints\YYYYMMDD_HHMMSS_name" -Force
```

The checkpoint script excludes `.git/`, `build/`, `.vs/`, `.vscode/`, and
`workspace_backups/`. It includes untracked project files such as UI references.
Before restoring, it automatically saves an emergency `pre_restore_*` checkpoint.

VST3 deploy backups are separate from project checkpoints. They let you roll back
only the installed plugins in `C:\Program Files\Common Files\VST3\`:

```powershell
.\DeployVST3.ps1 -Action ListBackups
.\DeployVST3.ps1 -Action Restore -Backup latest -Force
.\DeployVST3.ps1 -Action Restore -Backup "20260531_202451" -Force
```

## Current Verified State

As of 2026-05-31:

- Release test executables build successfully.
- Direct execution of the C++ test suites passes: 12 suites, 0 failures.
- `run_tests` uses target file generator expressions, so Visual Studio
  multi-config builds run the Release executables from `build/tests/Release/`.
- The project still needs real FL Studio validation for load-order and IPC
  reconnection scenarios.

Known caution:

- Shared memory and backup files may behave differently inside FL Studio process
  isolation than in unit tests.
- The backup file directory is `%LOCALAPPDATA%/MixCoach/SlotBackup/`.
- PCH is disabled because JUCE module implementation conflicts were seen.
- Ninja Release builds have had MSVC C1001 issues; prefer Visual Studio/MSBuild.

## High-Risk Files

Be extra careful with:

- `Source/Common/types/Types.h`
- `Source/Common/types/Constants.h`
- `Source/Common/memory/SharedMemory.h`
- `Source/Common/memory/SharedData.h`
- `Source/Common/memory/SlotRegistry.h`
- `Source/MixCoach/core/PluginProcessor.h`
- `Source/Messenger/core/PluginProcessor.h`
- `Source/MixCoach/UI/MixCoachTheme.h`

Touch these only with focused tests and a clear reason.

## Editing Rules

- Keep audio-thread code allocation-free and fast.
- Do not put UI logic in engine/audio modules.
- Do not put audio or IPC side effects in `paint()`.
- Use `MixCoachTheme` and shared constants for colors.
- Add focused tests for risky behavior changes.
- Prefer small, local changes that match existing JUCE/C++ patterns.

## Best Starting Points By Task

- UI visual work: `workspace_memory/visual_design.md`, then `Source/MixCoach/UI/`.
- Messenger UI: `Source/Messenger/ui/PluginEditor.cpp`.
- Analyzers: `Source/MixCoach/UI/*Meter*`, `SpectrographComponent`,
  `VectorscopeComponent`, `PhaseScopePanel`.
- Mentoring logic: `Source/MixCoach/engine/CoachEngine.*` and
  `PhaseManager.*`.
- IPC/debugging: `Source/Common/memory/SlotRegistry.*`,
  `SharedMemory.*`, `SharedData.*`, plus `IPC_CONTRACT.md`.
- Build/deploy: `build.ps1`, `scripts/validate.ps1`, `CMakeLists.txt`.

## What Done Means

A change is done when:

1. It preserves the product goal: mentor/analyzer, not audio processor.
2. It respects the UI references if visual.
3. It builds with MSBuild/CMake.
4. Relevant tests pass, or any unrun tests are clearly called out.
5. The next agent can understand the change from code and this project memory.
