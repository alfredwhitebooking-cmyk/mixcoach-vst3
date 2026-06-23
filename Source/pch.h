// ═══════════════════════════════════════════════════════════════════════════
//  pch.h — Precompiled Header para MixCoach / Messenger
//
//  ⚠️  NO incluir headers de módulos JUCE (juce_core, juce_gui_basics, etc.)
//     porque JUCE compila sus .cpp con JUCE_IMPLEMENT_MODULE, y si el header
//     ya fue procesado por el PCH, el include guard impide que el .cpp lo
//     procese de nuevo, causando "#error Incorrect use of JUCE cpp file".
//
//  Solo incluir headers del proyecto que sean ESTABLES y ligeros:
//  - Types.h (definiciones de tipos)
//  - Constants.h (constantes)
//  - LogHelper.h (logging)
//  EVITAR: headers que incluyan módulos JUCE (casi todos los demás).
// ═══════════════════════════════════════════════════════════════════════════

#pragma once

#ifdef __cplusplus

// ─── Headers ligeros del proyecto (no incluyen módulos JUCE) ─────────────
#include <Constants.h>
#include <Types.h>
#include <LogHelper.h>

#endif // __cplusplus
