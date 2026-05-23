cmake_minimum_required(VERSION 3.22)

project(MixCoach
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "MixCoach - Sistema Inteligente de Mentoría para Mezcla"
)

# ─── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# ─── JUCE Integration ─────────────────────────────────────────────────────────
set(JUCE_ROOT "C:/MIX COACH SYSTEM/juce-8.0.13-windows/MixCoachAI/JUCE")

if(NOT EXISTS "${JUCE_ROOT}/CMakeLists.txt")
    message(WARNING "JUCE no encontrado en ${JUCE_ROOT}. Configura: -DJUCE_ROOT=<ruta>")
endif()

add_subdirectory(${JUCE_ROOT} JUCE)

# ─── Módulos JUCE compartidos ──────────────────────────────────────────────
set(MIXCOACH_JUCE_MODULES
    juce_audio_basics
    juce_audio_devices
    juce_audio_formats
    juce_audio_processors
    juce_audio_utils
    juce_core
    juce_data_structures
    juce_dsp
    juce_events
    juce_graphics
    juce_gui_basics
    juce_gui_extra
)

# ─── Fuentes comunes (compartidas entre plugins) ─────────────────────────────
set(COMMON_SOURCES
    Source/Common/Constants.h
    Source/Common/Types.h
    Source/Common/AudioAnalysis.h
    Source/Common/AudioAnalysis.cpp
    Source/Common/SharedData.h
    Source/Common/SharedData.cpp
    Source/Common/TelemetryData.h
    Source/Common/SlotRegistry.h
    Source/Common/SlotRegistry.cpp
    Source/Common/SharedMemory.h
    Source/Common/SharedMemory.cpp
    Source/Common/LogHelper.h
)

# ═══════════════════════════════════════════════════════════════════════════════
#  PLUGIN 1: MixCoach (El Cerebro — Canal Master)
# ═══════════════════════════════════════════════════════════════════════════════
juce_add_plugin(MixCoach
    PLUGIN_MANUFACTURER_CODE  MixC
    PLUGIN_CODE               MCch
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "MixCoach"
    COMPANY_NAME              "MixCoachLabs"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CAN_REPLACE_VST2     FALSE
)

target_link_libraries(MixCoach PRIVATE
    ${MIXCOACH_JUCE_MODULES}
)

target_include_directories(MixCoach PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_sources(MixCoach PRIVATE
    ${COMMON_SOURCES}
    Source/MixCoach/PluginProcessor.h
    Source/MixCoach/PluginProcessor.cpp
    Source/MixCoach/PluginEditor.h
    Source/MixCoach/PluginEditor.cpp
    Source/MixCoach/PhaseManager.h
    Source/MixCoach/PhaseManager.cpp
    Source/MixCoach/CoachEngine.h
    Source/MixCoach/CoachEngine.cpp
    Source/MixCoach/AudioAnalyzer.h
    Source/MixCoach/AudioAnalyzer.cpp
    Source/MixCoach/UI/MixCoachTheme.h
    Source/MixCoach/UI/MainTabbedComponent.h
    Source/MixCoach/UI/MainTabbedComponent.cpp
    Source/MixCoach/UI/CoachChatComponent.h
    Source/MixCoach/UI/CoachChatComponent.cpp
    Source/MixCoach/UI/TrackDashboardComponent.h
    Source/MixCoach/UI/TrackDashboardComponent.cpp
    Source/MixCoach/UI/AnalyzersPanelComponent.h
    Source/MixCoach/UI/AnalyzersPanelComponent.cpp
    Source/MixCoach/UI/VirtualBusesComponent.h
    Source/MixCoach/UI/VirtualBusesComponent.cpp
    Source/MixCoach/UI/ReferencePanelComponent.h
    Source/MixCoach/UI/ReferencePanelComponent.cpp
)

# ═══════════════════════════════════════════════════════════════════════════════
#  PLUGIN 2: Messenger (Los Oídos — Pistas Individuales)
# ═══════════════════════════════════════════════════════════════════════════════
juce_add_plugin(Messenger
    PLUGIN_MANUFACTURER_CODE  MixC
    PLUGIN_CODE               Msgr
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "Messenger"
    COMPANY_NAME              "MixCoachLabs"
    DESCRIPTION               "Nodo de telemetria MixCoach - Picos, RMS, FFT, Fase"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CAN_REPLACE_VST2     FALSE
)

target_link_libraries(Messenger PRIVATE
    ${MIXCOACH_JUCE_MODULES}
)

target_include_directories(Messenger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_sources(Messenger PRIVATE
    ${COMMON_SOURCES}
    Source/Messenger/PluginProcessor.h
    Source/Messenger/PluginProcessor.cpp
    Source/Messenger/PluginEditor.h
    Source/Messenger/PluginEditor.cpp
    Source/Messenger/TelemetryCollector.h
    Source/Messenger/TelemetryCollector.cpp
)

# ─── Propiedades de compilación ──────────────────────────────────────────────
set_target_properties(MixCoach Messenger PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Forzar desactivacion de VST2 (JUCE 8 bug: VST3_CAN_REPLACE_VST2 no funciona via juce_add_plugin)
target_compile_definitions(MixCoach PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_definitions(Messenger PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_definitions(MixCoach_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_definitions(Messenger_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_definitions(MixCoach PUBLIC JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)
target_compile_definitions(Messenger PUBLIC JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)
target_compile_definitions(MixCoach_VST3 PRIVATE JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)
target_compile_definitions(Messenger_VST3 PRIVATE JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

# ─── Post-build: Copiar VST3 a C:\Program Files\Common Files\VST3 ──────────
# UNA SOLA ubicacion. FL Studio detecta duplicados si el mismo UID aparece en
# dos carpetas diferentes, creando entradas con sufijos -1, _2, etc.
#
# Usamos un script batch (.bat) externo en vez de comandos inline para evitar
# problemas de escaping de comillas con cmd.exe.
# El batch siempre retorna 0 incluso si la copia falla (VST3 bloqueado por DAW).
#
# Para deploy manual: ejecutar DeployVST3.ps1 (despues de cerrar el DAW)
set(VST3_SYSTEM "C:/Program Files/Common Files/VST3")
set(DEPLOY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/deploy_vst3_postbuild.bat")

# ├── Post-build MixCoach ────────────────────────────────────────────────
add_custom_command(TARGET MixCoach_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_SYSTEM}"
    COMMAND "${DEPLOY_SCRIPT}"
        "${CMAKE_COMMAND}"
        "${CMAKE_BINARY_DIR}/MixCoach_artefacts/$<CONFIG>/VST3/MixCoach.vst3"
        "${VST3_SYSTEM}/MixCoach.vst3"
    COMMENT "Deploy MixCoach.vst3"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

# ├── Post-build Messenger ───────────────────────────────────────────────
add_custom_command(TARGET Messenger_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_SYSTEM}"
    COMMAND "${DEPLOY_SCRIPT}"
        "${CMAKE_COMMAND}"
        "${CMAKE_BINARY_DIR}/Messenger_artefacts/$<CONFIG>/VST3/Messenger.vst3"
        "${VST3_SYSTEM}/Messenger.vst3"
    COMMENT "Deploy Messenger.vst3"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

# ─── Target deploy_vst3 manual ────────────────────────────────────────
# Uso: cmake --build build --target deploy_vst3 (cerrar FL Studio primero)
# Tambien se puede usar: .\DeployVST3.ps1
add_custom_target(deploy_vst3
    COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_SYSTEM}"
    COMMAND "${DEPLOY_SCRIPT}"
        "${CMAKE_COMMAND}"
        "${CMAKE_BINARY_DIR}/MixCoach_artefacts/$<CONFIG>/VST3/MixCoach.vst3"
        "${VST3_SYSTEM}/MixCoach.vst3"
    COMMAND "${DEPLOY_SCRIPT}"
        "${CMAKE_COMMAND}"
        "${CMAKE_BINARY_DIR}/Messenger_artefacts/$<CONFIG>/VST3/Messenger.vst3"
        "${VST3_SYSTEM}/Messenger.vst3"
    # Limpiar duplicados viejos (sufijos -1, _2)
    COMMAND powershell -Command "@('MixCoach_2.vst3','MixCoach-2.vst3','MixCoach-1.vst3','Messenger_2.vst3','Messenger-2.vst3','Messenger-1.vst3') | %% { $$p = Join-Path '${VST3_SYSTEM}' $$_; if (Test-Path $$p) { Remove-Item $$p -Recurse -Force -ErrorAction SilentlyContinue; Write-Host ('[Cleanup] ' + $$_) } }"
    COMMENT "Desplegar VST3 manualmente (cerrar FL Studio primero)"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

# Optimización para MSVC
if(MSVC)
    target_compile_options(MixCoach PRIVATE /W4 /permissive- /MP)
    target_compile_options(Messenger PRIVATE /W4 /permissive- /MP)
endif()

# Flags de linker para Release: eliminar símbolos de depuración y minimizar tamaño
if(MSVC)
    target_link_options(MixCoach PRIVATE "$<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>")
    target_link_options(Messenger PRIVATE "$<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>")
endif()
