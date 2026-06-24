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
# Use CACHE variable so CI can override with: -DJUCE_ROOT=<path>
set(JUCE_ROOT "C:/MIXCOA~1/juce-8.0.13-windows/MixCoachAI/JUCE" CACHE PATH "Ruta a JUCE framework")

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
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/audio/AudioAnalysis.h
    Source/Common/audio/AudioAnalysis.cpp
    Source/Common/audio/DiagnosticBridge.h
    Source/Common/audio/DiagnosticBridge.cpp
    Source/Common/audio/LoudnessAnalyzer.h
    Source/Common/audio/LoudnessAnalyzer.cpp
    Source/Common/memory/SharedData.h
    Source/Common/memory/SharedData.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/memory/SharedAudioMemory.h
    Source/Common/memory/SharedAudioMemory.cpp
    Source/Common/memory/SharedAudioMemoryV2.h
    Source/Common/memory/SharedAudioMemoryV2.cpp
    Source/Common/types/LogHelper.h
)

# ─── Fuentes UI de Analyzers (refactorizadas de AnalyzersPanelComponent) ────
set(ANALYZERS_UI_SOURCES
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
    Source/MixCoach/UI/LUFSMeter.h
    Source/MixCoach/UI/LUFSMeter.cpp
    Source/MixCoach/UI/StereoVUMeter.h
    Source/MixCoach/UI/StereoVUMeter.cpp
    Source/MixCoach/UI/SpectrographComponent.h
    Source/MixCoach/UI/SpectrographComponent.cpp
    Source/MixCoach/UI/SpectrographDrawing.cpp
    Source/MixCoach/UI/SpectrographProcessor.cpp
    Source/MixCoach/UI/VectorscopeComponent.h
    Source/MixCoach/UI/VectorscopeComponent.cpp
    Source/MixCoach/UI/PlaylistComponent.h
    Source/MixCoach/UI/PlaylistComponent.cpp
    Source/MixCoach/UI/DividerBar.h
    Source/MixCoach/UI/MessengerListComponent.h
    Source/MixCoach/UI/MessengerListComponent.cpp
    Source/MixCoach/UI/MessengerListAdvice.h
    Source/MixCoach/UI/MessengerListAdvice.cpp
    Source/MixCoach/UI/MessengerListRoles.h
    Source/MixCoach/UI/MessengerListRoles.cpp
    Source/MixCoach/UI/MessengerListTelemetry.cpp
    Source/MixCoach/UI/MessengerListDrawing.cpp
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
    ${ANALYZERS_UI_SOURCES}
    Source/MixCoach/core/PluginProcessor.h
    Source/MixCoach/core/PluginProcessor.cpp
    Source/MixCoach/core/ReferenceAudioPlayer.h
    Source/MixCoach/core/PluginEditor.h
    Source/MixCoach/core/PluginEditor.cpp
    Source/MixCoach/core/PluginEditorTimer.cpp
    Source/MixCoach/core/PluginEditorBackground.cpp
    Source/MixCoach/engine/PhaseManager.h
    Source/MixCoach/engine/PhaseManager.cpp
    Source/MixCoach/engine/CoachEngine.h
    Source/MixCoach/engine/CoachEngine.cpp
    Source/MixCoach/engine/CoachEngineCommands.cpp
    Source/MixCoach/engine/CoachEngineCorrection.cpp
    Source/MixCoach/engine/CoachEngineIssues.cpp
    Source/MixCoach/engine/CoachEngineContext.cpp
    Source/MixCoach/engine/CoachEngineReference.cpp
    Source/MixCoach/engine/CoachEngineSetup.cpp
    Source/MixCoach/engine/CoachEngine_Analysis.cpp
    Source/MixCoach/engine/CoachEngine_Identity.cpp
    Source/MixCoach/engine/CoachEngine_Messages.cpp
    Source/MixCoach/engine/TrackRole.h
    Source/MixCoach/engine/SpectralProfiler.h
    Source/MixCoach/engine/SpectralProfiler.cpp
    Source/MixCoach/engine/SemanticComparator.h
    Source/MixCoach/engine/SemanticComparator.cpp
    Source/MixCoach/engine/ReferenceDrivenEngine.h
    Source/MixCoach/engine/ReferenceDrivenEngine.cpp
    Source/MixCoach/engine/DifferenceProfile.h
    Source/MixCoach/engine/DifferenceProfile.cpp
    Source/MixCoach/engine/CorrectionLearner.h
    Source/MixCoach/engine/CorrectionLearner.cpp
    Source/MixCoach/engine/MixPriorityEngine.h
    Source/MixCoach/engine/MixPriorityEngine.cpp
    Source/MixCoach/engine/PlanManager.h
    Source/MixCoach/engine/PlanManager.cpp
    Source/MixCoach/engine/MixScore.h
    Source/MixCoach/engine/MixScore.cpp
    Source/MixCoach/engine/TrackState.h
    Source/MixCoach/engine/TrackFeedCore.h
    Source/MixCoach/engine/TrackFeedCore.cpp
    Source/MixCoach/engine/WorkflowDetector.h
    Source/MixCoach/engine/WorkflowDetector.cpp
    Source/MixCoach/engine/AnalyzerInterpreter.h
    Source/MixCoach/engine/AnalyzerInterpreter.cpp
    Source/MixCoach/engine/SessionMap.cpp
    Source/MixCoach/audio/AudioAnalyzer.h
    Source/MixCoach/audio/AudioAnalyzer.cpp
    Source/MixCoach/audio/ReferenceAnalyzer.h
    Source/MixCoach/audio/ReferenceAnalyzer.cpp
    Source/MixCoach/UI/MixCoachTheme.h
    Source/MixCoach/UI/MainTabbedComponent.h
    Source/MixCoach/UI/MainTabbedComponent.cpp
    Source/MixCoach/UI/CoachChatComponent.h            Source/MixCoach/UI/CoachChatComponent.cpp
            Source/MixCoach/UI/MixMapComponent.cpp
    Source/MixCoach/UI/MasterMeterPanel.h
    Source/MixCoach/UI/MasterMeterPanel.cpp
    Source/MixCoach/UI/TrackDashboardComponent.h
    Source/MixCoach/UI/TrackDashboardComponent.cpp
    Source/MixCoach/UI/AnalyzersPanelComponent.h
    Source/MixCoach/UI/AnalyzersPanelComponent.cpp
    Source/MixCoach/UI/AnalyzersPanelDrawing.h
    Source/MixCoach/UI/MeterCard.h
    Source/MixCoach/UI/MeterCard.cpp
    Source/MixCoach/UI/MeterPanel.h
    Source/MixCoach/UI/MeterPanel.cpp
    Source/MixCoach/UI/VintageVUMeters.h
    Source/MixCoach/UI/VintageVUMeters.cpp
    Source/MixCoach/UI/RefToggle.h
    Source/MixCoach/UI/RefToggle.cpp
    Source/MixCoach/UI/ProfessionalAnalyzersComponent.h
    Source/MixCoach/UI/ProfessionalAnalyzersComponent.cpp
    Source/MixCoach/UI/VirtualBusesComponent.h
    Source/MixCoach/UI/VirtualBusesComponent.cpp
    Source/MixCoach/UI/ReferencePanelComponent.h
    Source/MixCoach/UI/ReferencePanelComponent.cpp
    Source/MixCoach/UI/ReferencePanelIcons.h
    Source/MixCoach/UI/ReferenceMatchPanel.h
    Source/MixCoach/UI/ReferenceMatchPanel.cpp
    Source/MixCoach/UI/DropZoneComponent.h
    Source/MixCoach/UI/DropZoneComponent.cpp
    Source/MixCoach/UI/StereoWidthMeter.h
    Source/MixCoach/UI/StereoWidthMeter.cpp
    Source/MixCoach/UI/CrestPanel.h
    Source/MixCoach/UI/AudioDNAComponent.h
    Source/MixCoach/UI/CrestPanel.cpp
    Source/MixCoach/UI/AudioDNAComponent.cpp
    Source/MixCoach/ai/AiCoachAdapter.h
    Source/MixCoach/ai/AiCoachAdapterPrompts.cpp
    Source/MixCoach/ai/AiCoachAdapterSession.cpp
    Source/MixCoach/ai/LlmClient.h
    Source/MixCoach/ai/LlmClient.cpp
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

target_sources(Messenger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Messenger/ui/ColourPresetStrip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Messenger/ui/ColourSwatch.cpp
)

target_include_directories(Messenger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_sources(Messenger PRIVATE
    ${COMMON_SOURCES}
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
    Source/Messenger/core/PluginProcessor.h
    Source/Messenger/core/PluginProcessor.cpp
    Source/Messenger/core/MessengerType.h
    Source/Messenger/ui/PluginEditor.h
    Source/Messenger/ui/PluginEditor.cpp
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

# ═══════════════════════════════════════════════════════════════════════════════
#  TESTS UNITARIOS
# ═══════════════════════════════════════════════════════════════════════════════

# ─── Test: SmoothValue (standalone, sin JUCE) ───────────────────────────────
add_executable(TestSmoothValue
    tests/TestSmoothValue.cpp
)
target_compile_definitions(TestSmoothValue PRIVATE TEST_SMOOTHVALUE_STANDALONE)
target_compile_options(TestSmoothValue PRIVATE /W4)
set_target_properties(TestSmoothValue PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: PhaseManager (con JUCE) ──────────────────────────────────────────
add_executable(TestPhaseManager
    tests/TestPhaseManager.cpp
    Source/MixCoach/engine/PhaseManager.h
    Source/MixCoach/engine/PhaseManager.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
)
target_include_directories(TestPhaseManager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestPhaseManager PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestPhaseManager PRIVATE /W4)
set_target_properties(TestPhaseManager PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: CoachEngine (motor de mentoría, con JUCE) ──────────────────────
add_executable(TestCoachEngine
    tests/TestCoachEngine.cpp
    Source/MixCoach/engine/CoachEngine.h
    Source/MixCoach/engine/CoachEngine.cpp
    Source/MixCoach/engine/CoachEngineCommands.cpp
    Source/MixCoach/engine/CoachEngineCorrection.cpp
    Source/MixCoach/engine/CoachEngineIssues.cpp
    Source/MixCoach/engine/CoachEngineContext.cpp
    Source/MixCoach/engine/CoachEngineReference.cpp
    Source/MixCoach/engine/CoachEngineSetup.cpp
    Source/MixCoach/engine/CoachEngine_Analysis.cpp
    Source/MixCoach/engine/CoachEngine_Identity.cpp
    Source/MixCoach/engine/CoachEngine_Messages.cpp
    Source/MixCoach/engine/PhaseManager.h
    Source/MixCoach/engine/PhaseManager.cpp
    Source/MixCoach/engine/TrackRole.h
    Source/MixCoach/engine/DomainGap.h
    Source/MixCoach/engine/SpectralProfiler.h
    Source/MixCoach/engine/SpectralProfiler.cpp
    Source/MixCoach/engine/SemanticComparator.h
    Source/MixCoach/engine/SemanticComparator.cpp
    Source/MixCoach/engine/PlanManager.h
    Source/MixCoach/engine/PlanManager.cpp
    Source/MixCoach/engine/ReferenceDrivenEngine.h
    Source/MixCoach/engine/ReferenceDrivenEngine.cpp
    Source/MixCoach/engine/MixScore.h
    Source/MixCoach/engine/MixScore.cpp
    Source/MixCoach/engine/TrackState.h
    Source/MixCoach/engine/AnalyzerInterpreter.h
    Source/MixCoach/engine/AnalyzerInterpreter.cpp
    Source/MixCoach/engine/MixPriorityEngine.h
    Source/MixCoach/engine/MixPriorityEngine.cpp
    Source/MixCoach/engine/SessionMap.cpp
    Source/MixCoach/engine/DifferenceProfile.h
    Source/MixCoach/engine/DifferenceProfile.cpp
    Source/MixCoach/engine/CorrectionLearner.h
    Source/MixCoach/engine/CorrectionLearner.cpp
    Source/MixCoach/engine/TrackFeedCore.h
    Source/MixCoach/engine/TrackFeedCore.cpp
    Source/MixCoach/engine/WorkflowDetector.h
    Source/MixCoach/engine/WorkflowDetector.cpp
    Source/MixCoach/audio/AudioAnalyzer.h
    Source/MixCoach/audio/AudioAnalyzer.cpp
    Source/MixCoach/audio/ReferenceAnalyzer.h
    Source/MixCoach/audio/ReferenceAnalyzer.cpp
    Source/MixCoach/UI/VectorscopeComponent.h
    Source/MixCoach/UI/VectorscopeComponent.cpp
    Source/MixCoach/UI/MixCoachTheme.h
    Source/Common/audio/AudioAnalysis.h
    Source/Common/audio/AudioAnalysis.cpp
    Source/Common/audio/LoudnessAnalyzer.h
    Source/Common/audio/LoudnessAnalyzer.cpp
    Source/Common/audio/DiagnosticBridge.h
    Source/Common/audio/DiagnosticBridge.cpp
    Source/Common/memory/SharedData.h
    Source/Common/memory/SharedData.cpp
    Source/Common/memory/SharedAudioMemory.h
    Source/Common/memory/SharedAudioMemory.cpp
    Source/Common/memory/SharedAudioMemoryV2.h
    Source/Common/memory/SharedAudioMemoryV2.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/LogHelper.h
)
target_include_directories(TestCoachEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestCoachEngine PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
    juce_audio_basics
    juce_dsp
)
target_compile_options(TestCoachEngine PRIVATE /W4)
set_target_properties(TestCoachEngine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: SpectralBands (dynamic sample rate computeSpectralBands) ──────
add_executable(TestSpectralBands
    tests/TestSpectralBands.cpp
    Source/Common/types/Constants.h
)
target_include_directories(TestSpectralBands PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestSpectralBands PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestSpectralBands PRIVATE /W4)
set_target_properties(TestSpectralBands PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: LUFSMeter (EBU R128 loudness meter UI) ────────────────────────
add_executable(TestLUFSMeter
    tests/TestLUFSMeter.cpp
    Source/MixCoach/UI/LUFSMeter.h
    Source/MixCoach/UI/LUFSMeter.cpp
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
)
target_include_directories(TestLUFSMeter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestLUFSMeter PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
)
target_compile_options(TestLUFSMeter PRIVATE /W4)
set_target_properties(TestLUFSMeter PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: SpectrographComponent (logarithmic spectrum analyzer UI) ────────
add_executable(TestSpectrographComponent
    tests/TestSpectrographComponent.cpp
    Source/MixCoach/UI/SpectrographComponent.h
    Source/MixCoach/UI/SpectrographComponent.cpp
    Source/MixCoach/UI/SpectrographDrawing.cpp
    Source/MixCoach/UI/SpectrographProcessor.cpp
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
    Source/MixCoach/UI/MixCoachTheme.h
    Source/Common/audio/DiagnosticBridge.h
    Source/Common/audio/DiagnosticBridge.cpp
)
target_include_directories(TestSpectrographComponent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestSpectrographComponent PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
)
target_compile_options(TestSpectrographComponent PRIVATE /W4)
set_target_properties(TestSpectrographComponent PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: VectorscopeComponent (circular phase vectorscope UI) ────────────
add_executable(TestVectorscopeComponent
    tests/TestVectorscopeComponent.cpp
    Source/MixCoach/UI/VectorscopeComponent.h
    Source/MixCoach/UI/VectorscopeComponent.cpp
    Source/Common/audio/DiagnosticBridge.h
    Source/Common/audio/DiagnosticBridge.cpp
)
target_include_directories(TestVectorscopeComponent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestVectorscopeComponent PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
)
target_compile_options(TestVectorscopeComponent PRIVATE /W4)
# ═══ MSB6006 mitigation: CL.exe crashea con STATUS_ACCESS_VIOLATION (-1073741819)
#     cuando compila translation units que incluyen juce_gui_basics.h en Release
#     con /O2. El frontend de CL.exe se queda sin memoria de pila al optimizar
#     las funciones inline de JUCE. Solución: /MP1 (serial) + /bigobj.
#     Mismo fix que TestExperienceLevel.
if(MSVC)
    target_compile_options(TestVectorscopeComponent PRIVATE
        /MP1
        /bigobj
    )
endif()
set_target_properties(TestVectorscopeComponent PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: SharedMemoryManager (init/close, register/read/release/lock) ──────────
add_executable(TestSharedMemory
    tests/TestSharedMemory.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
)
target_include_directories(TestSharedMemory PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestSharedMemory PRIVATE
    juce_core
)
target_compile_options(TestSharedMemory PRIVATE /W4)
set_target_properties(TestSharedMemory PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: SlotRegistry (modo local: register/release/update/iterate) ─────────
add_executable(TestSlotRegistry
    tests/TestSlotRegistry.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/LogHelper.h
)
target_include_directories(TestSlotRegistry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestSlotRegistry PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestSlotRegistry PRIVATE /W4)
set_target_properties(TestSlotRegistry PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: Stress Test — 50+ tracks (simulates FL Studio with 55 Messengers) ──
add_executable(TestStress50Tracks
    tests/TestStress50Tracks.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/LogHelper.h
)
target_include_directories(TestStress50Tracks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestStress50Tracks PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestStress50Tracks PRIVATE /W4)
set_target_properties(TestStress50Tracks PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── STRESS TEST: 128 slots (maximum capacity — worst case) ──────────────────
add_executable(TestStress128Slots
    tests/TestStress128Slots.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/LogHelper.h
)
target_include_directories(TestStress128Slots PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestStress128Slots PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestStress128Slots PRIVATE /W4)
set_target_properties(TestStress128Slots PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: SemanticComparator (análisis semántico por TrackRole) ─────────────────
add_executable(TestSemanticComparator
    tests/TestSemanticComparator.cpp
    Source/MixCoach/engine/TrackRole.h
    Source/MixCoach/engine/SpectralProfiler.h
    Source/MixCoach/engine/SpectralProfiler.cpp
    Source/MixCoach/engine/SemanticComparator.h
    Source/MixCoach/engine/SemanticComparator.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/audio/AudioAnalysis.h
    Source/Common/audio/AudioAnalysis.cpp
    Source/Common/audio/LoudnessAnalyzer.h
    Source/Common/audio/LoudnessAnalyzer.cpp
    Source/Common/memory/SharedData.h
    Source/Common/memory/SharedData.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/memory/SharedAudioMemory.h
    Source/Common/memory/SharedAudioMemory.cpp
    Source/Common/memory/SharedAudioMemoryV2.h
    Source/Common/memory/SharedAudioMemoryV2.cpp
    Source/Common/types/LogHelper.h
)
target_include_directories(TestSemanticComparator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestSemanticComparator PRIVATE
    juce_core
    juce_graphics
    juce_audio_basics
    juce_dsp
)
target_compile_options(TestSemanticComparator PRIVATE /W4)
set_target_properties(TestSemanticComparator PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: AudioAnalyzer (CORE — Master FFT, LUFS, correlation, RMS/Peak, vectorscope)
add_executable(TestAudioAnalyzer
    tests/TestAudioAnalyzer.cpp
    Source/MixCoach/audio/AudioAnalyzer.h
    Source/MixCoach/audio/AudioAnalyzer.cpp
    Source/MixCoach/UI/VectorscopeComponent.h
    Source/MixCoach/UI/VectorscopeComponent.cpp
    Source/MixCoach/UI/MixCoachTheme.h
    Source/Common/audio/AudioAnalysis.h
    Source/Common/audio/AudioAnalysis.cpp
    Source/Common/audio/LoudnessAnalyzer.h
    Source/Common/audio/LoudnessAnalyzer.cpp
    Source/Common/audio/DiagnosticBridge.h
    Source/Common/audio/DiagnosticBridge.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
)
target_include_directories(TestAudioAnalyzer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestAudioAnalyzer PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
    juce_audio_basics
    juce_dsp
)
target_compile_options(TestAudioAnalyzer PRIVATE /W4)
set_target_properties(TestAudioAnalyzer PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: FFT-based audio descriptors (transientRatio, crestPerBand, stereoWidthPerBand) ──
add_executable(TestAudioDescriptors
    tests/TestAudioDescriptors.cpp
)
target_include_directories(TestAudioDescriptors PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestAudioDescriptors PRIVATE
    juce_core
    juce_dsp
)
target_compile_options(TestAudioDescriptors PRIVATE /W4)
set_target_properties(TestAudioDescriptors PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: AiCoachAdapter ExperienceLevel (Novice/Intermediate/Advanced/Expert) ──
add_executable(TestExperienceLevel
    tests/TestExperienceLevel.cpp
    Source/MixCoach/ai/AiCoachAdapter.h
    Source/MixCoach/ai/AiCoachAdapterPrompts.cpp
    Source/MixCoach/ai/AiCoachAdapterSession.cpp
    Source/MixCoach/ai/LlmClient.h
    Source/MixCoach/ai/LlmClient.cpp
    Source/MixCoach/engine/CoachEngine.h
    Source/MixCoach/engine/CoachEngine.cpp
    Source/MixCoach/engine/CoachEngineCommands.cpp
    Source/MixCoach/engine/CoachEngineCorrection.cpp
    Source/MixCoach/engine/CoachEngineIssues.cpp
    Source/MixCoach/engine/CoachEngineContext.cpp
    Source/MixCoach/engine/CoachEngineReference.cpp
    Source/MixCoach/engine/CoachEngineSetup.cpp
    Source/MixCoach/engine/CoachEngine_Analysis.cpp
    Source/MixCoach/engine/CoachEngine_Identity.cpp
    Source/MixCoach/engine/CoachEngine_Messages.cpp
    Source/MixCoach/engine/PhaseManager.h
    Source/MixCoach/engine/PhaseManager.cpp
    Source/MixCoach/engine/TrackRole.h
    Source/MixCoach/engine/DomainGap.h
    Source/MixCoach/engine/SpectralProfiler.h
    Source/MixCoach/engine/SpectralProfiler.cpp
    Source/MixCoach/engine/SemanticComparator.h
    Source/MixCoach/engine/SemanticComparator.cpp
    Source/MixCoach/engine/PlanManager.h
    Source/MixCoach/engine/PlanManager.cpp
    Source/MixCoach/engine/ReferenceDrivenEngine.h
    Source/MixCoach/engine/ReferenceDrivenEngine.cpp
    Source/MixCoach/engine/MixScore.h
    Source/MixCoach/engine/MixScore.cpp
    Source/MixCoach/engine/TrackState.h
    Source/MixCoach/engine/AnalyzerInterpreter.h
    Source/MixCoach/engine/AnalyzerInterpreter.cpp
    Source/MixCoach/engine/MixPriorityEngine.h
    Source/MixCoach/engine/MixPriorityEngine.cpp
    Source/MixCoach/engine/SessionMap.cpp
    Source/MixCoach/engine/DifferenceProfile.h
    Source/MixCoach/engine/DifferenceProfile.cpp
    Source/MixCoach/engine/CorrectionLearner.h
    Source/MixCoach/engine/CorrectionLearner.cpp
    Source/MixCoach/engine/TrackFeedCore.h
    Source/MixCoach/engine/TrackFeedCore.cpp
    Source/MixCoach/engine/WorkflowDetector.h
    Source/MixCoach/engine/WorkflowDetector.cpp
    Source/MixCoach/audio/ReferenceAnalyzer.h
    Source/MixCoach/audio/ReferenceAnalyzer.cpp
    Source/MixCoach/audio/AudioAnalyzer.h
    Source/MixCoach/audio/AudioAnalyzer.cpp
    Source/MixCoach/UI/VectorscopeComponent.h
    Source/MixCoach/UI/VectorscopeComponent.cpp
    Source/MixCoach/UI/MixCoachTheme.h
    Source/MixCoach/UI/MessengerListComponent.h
    Source/MixCoach/UI/MessengerListComponent.cpp
    Source/MixCoach/UI/MessengerListAdvice.h
    Source/MixCoach/UI/MessengerListAdvice.cpp
    Source/MixCoach/UI/MessengerListRoles.h
    Source/MixCoach/UI/MessengerListRoles.cpp
    Source/MixCoach/UI/MessengerListTelemetry.cpp
    Source/MixCoach/UI/MessengerListDrawing.cpp
    Source/MixCoach/UI/DividerBar.h
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
    Source/Common/audio/AudioAnalysis.h
    Source/Common/audio/AudioAnalysis.cpp
    Source/Common/audio/LoudnessAnalyzer.h
    Source/Common/audio/LoudnessAnalyzer.cpp
    Source/Common/audio/DiagnosticBridge.h
    Source/Common/audio/DiagnosticBridge.cpp
    Source/Common/memory/SharedData.h
    Source/Common/memory/SharedData.cpp
    Source/Common/memory/SharedAudioMemory.h
    Source/Common/memory/SharedAudioMemory.cpp
    Source/Common/memory/SharedAudioMemoryV2.h
    Source/Common/memory/SharedAudioMemoryV2.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/LogHelper.h
)
target_include_directories(TestExperienceLevel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestExperienceLevel PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
    juce_audio_basics
    juce_dsp
)
target_compile_options(TestExperienceLevel PRIVATE /W4)
# TestExperienceLevel integra 50+ .cpp (engine + UI + ai + memory) con TrackTelemetry
# (alignas(32) + spectrum[512]). Bajo /MP y /W4 el compilador sufre MSB6006 con exit
# code -1073741819 (STATUS_ACCESS_VIOLATION por agotamiento de stack interno de CL.exe).
# Mitigaciones MSVC:
#   1) /d2ReduceOptimizedMaximumFramesInCFG      → reduce memoria del frontend
#   2) Forzar /MP1 (1 hilo por target)           → evita contención de memoria
#   3) /Zc:preprocessor off (no aplicado)        → no relevante aquí
# Usamos /MP1 + /bigobj (frames grandes) + reducción de memoria del frontend.
if(MSVC)
    target_compile_options(TestExperienceLevel PRIVATE
        /MP1        # serial compilation dentro de este target (libera RAM de CL.exe)
        /bigobj     # permite más secciones por .obj (TrackTelemetry es grande)
        /Zc:__cplusplus
    )
endif()
set_target_properties(TestExperienceLevel PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: ChatBubbleHeight (getBubbleHeight con valores concretos) ──
add_executable(TestChatBubbleHeight
    tests/TestChatBubbleHeight.cpp
)
target_include_directories(TestChatBubbleHeight PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestChatBubbleHeight PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestChatBubbleHeight PRIVATE /W4)
set_target_properties(TestChatBubbleHeight PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: PhaseCorrelationMeter (horizontal correlation bar with SmoothValue + color coding) ──
add_executable(TestPhaseCorrelationMeter
    tests/TestPhaseCorrelationMeter.cpp
    Source/MixCoach/UI/PhaseCorrelationMeter.h
    Source/MixCoach/UI/PhaseCorrelationMeter.cpp
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
    Source/MixCoach/UI/MixCoachTheme.h
    Source/Common/audio/DiagnosticBridge.h
    Source/Common/audio/DiagnosticBridge.cpp
)
target_include_directories(TestPhaseCorrelationMeter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestPhaseCorrelationMeter PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
)
target_compile_options(TestPhaseCorrelationMeter PRIVATE /W4)
set_target_properties(TestPhaseCorrelationMeter PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: StereoWidthMeter (stereo width bar with zones, marker animation, per-band) ──
add_executable(TestStereoWidthMeter
    tests/TestStereoWidthMeter.cpp
    Source/MixCoach/UI/StereoWidthMeter.h
    Source/MixCoach/UI/StereoWidthMeter.cpp
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
    Source/MixCoach/UI/MixCoachTheme.h
)
target_include_directories(TestStereoWidthMeter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestStereoWidthMeter PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
)
target_compile_options(TestStereoWidthMeter PRIVATE /W4)
set_target_properties(TestStereoWidthMeter PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: IPC Integration (SlotRegistry → Backup → SharedMemory pipeline) ──
add_executable(TestIPCIntegration
    tests/TestIPCIntegration.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/LogHelper.h
)
target_include_directories(TestIPCIntegration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestIPCIntegration PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestIPCIntegration PRIVATE /W4)
set_target_properties(TestIPCIntegration PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: TrackFeedCore (TrackState conversion, event generation, priority scoring) ──
add_executable(TestTrackFeedCore
    tests/TestTrackFeedCore.cpp
    Source/MixCoach/engine/TrackFeedCore.h
    Source/MixCoach/engine/TrackFeedCore.cpp
    Source/MixCoach/engine/TrackState.h
    Source/MixCoach/engine/TrackRole.h
    Source/Common/types/Types.h
    Source/Common/types/Constants.h
    Source/Common/types/LogHelper.h
    Source/Common/memory/SharedData.h
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SharedMemory.h
)
target_include_directories(TestTrackFeedCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestTrackFeedCore PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestTrackFeedCore PRIVATE /W4)
set_target_properties(TestTrackFeedCore PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: DifferenceProfile — toJson/fromJson round-trip, toTextSummary, build(), helpers ──
add_executable(TestDifferenceProfile
    tests/TestDifferenceProfile.cpp
    Source/MixCoach/engine/DomainGap.h
    Source/MixCoach/engine/DifferenceProfile.h
    Source/MixCoach/engine/DifferenceProfile.cpp
    Source/MixCoach/engine/ReferenceDrivenEngine.h
    Source/MixCoach/engine/ReferenceDrivenEngine.cpp
)
target_include_directories(TestDifferenceProfile PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestDifferenceProfile PRIVATE
    juce_core
    juce_graphics
    juce_audio_basics
)
target_compile_options(TestDifferenceProfile PRIVATE /W4)
set_target_properties(TestDifferenceProfile PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: ReferenceAudioPlayer (carga stereo/mono, playback, seek, volumen) ──
# ═══ HEADER AUTÓNOMO ═══ No necesita juce_audio_processors ni los .cpp del plugin.
# ReferenceAudioPlayer.h es 100% inline y solo depende de juce_core,
# juce_audio_basics (para AudioBuffer, FloatVectorOperations) y
# juce_audio_formats (para WavAudioFormat).
add_executable(TestReferenceAudioPlayer
    tests/TestReferenceAudioPlayer.cpp
    Source/MixCoach/core/ReferenceAudioPlayer.h
)
target_include_directories(TestReferenceAudioPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestReferenceAudioPlayer PRIVATE
    juce_core
    juce_audio_basics
    juce_audio_formats
)
target_compile_options(TestReferenceAudioPlayer PRIVATE /W4)
set_target_properties(TestReferenceAudioPlayer PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: ExpectedProfiles — Calibración de la base de conocimiento de roles ──
# Test header-only (TrackRole.h) con profiling visual de los 40+ perfiles.
add_executable(TestExpectedProfiles
    tests/TestExpectedProfiles.cpp
)
target_include_directories(TestExpectedProfiles PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestExpectedProfiles PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestExpectedProfiles PRIVATE /W4)
set_target_properties(TestExpectedProfiles PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: GenreProfiles — getGenreProfileDelta + getExpectedProfile(role, genre) ──
# Test header-only (TrackRole.h) con validación de deltas por género.
add_executable(TestGenreProfiles
    tests/TestGenreProfiles.cpp
)
target_include_directories(TestGenreProfiles PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestGenreProfiles PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestGenreProfiles PRIVATE /W4)
if(MSVC)
    target_compile_options(TestGenreProfiles PRIVATE
        /MP1
        /bigobj
        "$<$<CONFIG:Release>:/Od>"   # CL.exe crash con /O2 (STATUS_ACCESS_VIOLATION) en este test
    )
endif()
set_target_properties(TestGenreProfiles PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: MixPriorityEngine — score multidimensional (severity × role × domain × genre) ──
add_executable(TestMixPriorityEngine
    tests/TestMixPriorityEngine.cpp
    Source/MixCoach/engine/MixPriorityEngine.h
    Source/MixCoach/engine/MixPriorityEngine.cpp
    Source/MixCoach/engine/TrackRole.h
)
target_include_directories(TestMixPriorityEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestMixPriorityEngine PRIVATE
    juce_core
    juce_graphics
)
target_compile_options(TestMixPriorityEngine PRIVATE /W4)
set_target_properties(TestMixPriorityEngine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: MixScore — compute() con CoachEngine + AudioAnalyzer + SharedData ──
add_executable(TestMixScore
    tests/TestMixScore.cpp
    Source/MixCoach/engine/CoachEngine.h
    Source/MixCoach/engine/CoachEngine.cpp
    Source/MixCoach/engine/CoachEngineCommands.cpp
    Source/MixCoach/engine/CoachEngineCorrection.cpp
    Source/MixCoach/engine/CoachEngineIssues.cpp
    Source/MixCoach/engine/CoachEngineContext.cpp
    Source/MixCoach/engine/CoachEngineReference.cpp
    Source/MixCoach/engine/CoachEngineSetup.cpp
    Source/MixCoach/engine/CoachEngine_Analysis.cpp
    Source/MixCoach/engine/CoachEngine_Identity.cpp
    Source/MixCoach/engine/CoachEngine_Messages.cpp
    Source/MixCoach/engine/PhaseManager.h
    Source/MixCoach/engine/PhaseManager.cpp
    Source/MixCoach/engine/TrackRole.h
    Source/MixCoach/engine/DomainGap.h
    Source/MixCoach/engine/SpectralProfiler.h
    Source/MixCoach/engine/SpectralProfiler.cpp
    Source/MixCoach/engine/SemanticComparator.h
    Source/MixCoach/engine/SemanticComparator.cpp
    Source/MixCoach/engine/PlanManager.h
    Source/MixCoach/engine/PlanManager.cpp
    Source/MixCoach/engine/ReferenceDrivenEngine.h
    Source/MixCoach/engine/ReferenceDrivenEngine.cpp
    Source/MixCoach/engine/MixScore.h
    Source/MixCoach/engine/MixScore.cpp
    Source/MixCoach/engine/TrackState.h
    Source/MixCoach/engine/AnalyzerInterpreter.h
    Source/MixCoach/engine/AnalyzerInterpreter.cpp
    Source/MixCoach/engine/MixPriorityEngine.h
    Source/MixCoach/engine/MixPriorityEngine.cpp
    Source/MixCoach/engine/SessionMap.cpp
    Source/MixCoach/engine/DifferenceProfile.h
    Source/MixCoach/engine/DifferenceProfile.cpp
    Source/MixCoach/engine/CorrectionLearner.h
    Source/MixCoach/engine/CorrectionLearner.cpp
    Source/MixCoach/engine/TrackFeedCore.h
    Source/MixCoach/engine/TrackFeedCore.cpp
    Source/MixCoach/engine/WorkflowDetector.h
    Source/MixCoach/engine/WorkflowDetector.cpp
    Source/MixCoach/audio/AudioAnalyzer.h
    Source/MixCoach/audio/AudioAnalyzer.cpp
    Source/MixCoach/audio/ReferenceAnalyzer.h
    Source/MixCoach/audio/ReferenceAnalyzer.cpp
    Source/MixCoach/UI/VectorscopeComponent.h
    Source/MixCoach/UI/VectorscopeComponent.cpp
    Source/MixCoach/UI/MixCoachTheme.h
    Source/Common/audio/AudioAnalysis.h
    Source/Common/audio/AudioAnalysis.cpp
    Source/Common/audio/LoudnessAnalyzer.h
    Source/Common/audio/LoudnessAnalyzer.cpp
    Source/Common/audio/DiagnosticBridge.h
    Source/Common/audio/DiagnosticBridge.cpp
    Source/Common/memory/SharedData.h
    Source/Common/memory/SharedData.cpp
    Source/Common/memory/SharedAudioMemory.h
    Source/Common/memory/SharedAudioMemory.cpp
    Source/Common/memory/SharedAudioMemoryV2.h
    Source/Common/memory/SharedAudioMemoryV2.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/LogHelper.h
)
target_include_directories(TestMixScore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestMixScore PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
    juce_audio_basics
    juce_dsp
)
target_compile_options(TestMixScore PRIVATE /W4)
if(MSVC)
    target_compile_options(TestMixScore PRIVATE
        /MP1
        /bigobj
        /FS
        "$<$<CONFIG:Release>:/Od>"
    )
endif()
set_target_properties(TestMixScore PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Target: build + run all tests ──────────────────────────────────────────
add_custom_target(run_tests
    COMMAND $<TARGET_FILE:TestSmoothValue>
    COMMAND $<TARGET_FILE:TestPhaseManager>
    COMMAND $<TARGET_FILE:TestCoachEngine>
    COMMAND $<TARGET_FILE:TestIPCIntegration>
    COMMAND $<TARGET_FILE:TestSharedMemory>
    COMMAND $<TARGET_FILE:TestSlotRegistry>
    COMMAND $<TARGET_FILE:TestLUFSMeter>
    COMMAND $<TARGET_FILE:TestSpectralBands>
    COMMAND $<TARGET_FILE:TestSpectrographComponent>
    COMMAND $<TARGET_FILE:TestVectorscopeComponent>
    COMMAND $<TARGET_FILE:TestPhaseCorrelationMeter>
    COMMAND $<TARGET_FILE:TestStress50Tracks>
    COMMAND $<TARGET_FILE:TestStress128Slots>
    COMMAND $<TARGET_FILE:TestAudioAnalyzer>
    COMMAND $<TARGET_FILE:TestAudioDescriptors>
    COMMAND $<TARGET_FILE:TestReferenceAudioPlayer>
    COMMAND $<TARGET_FILE:TestStereoWidthMeter>
    COMMAND $<TARGET_FILE:TestTrackFeedCore>
    COMMAND $<TARGET_FILE:TestSemanticComparator>
    COMMAND $<TARGET_FILE:TestExpectedProfiles>
    COMMAND $<TARGET_FILE:TestGenreProfiles>
    COMMAND $<TARGET_FILE:TestMixPriorityEngine>
    COMMAND $<TARGET_FILE:TestMixScore>

    DEPENDS TestSmoothValue TestPhaseManager TestCoachEngine TestIPCIntegration
            TestSharedMemory TestSlotRegistry TestStress50Tracks TestStress128Slots
            TestLUFSMeter TestSpectralBands TestSpectrographComponent TestVectorscopeComponent
            TestPhaseCorrelationMeter
            TestStereoWidthMeter
            TestAudioAnalyzer
            TestAudioDescriptors
            TestReferenceAudioPlayer
            TestTrackFeedCore
            TestSemanticComparator
            TestExpectedProfiles
            TestGenreProfiles
            TestMixPriorityEngine
            TestMixScore

    COMMENT "Running all unit tests..."
    VERBATIM
)

# ─── Post-build: Deploy VST3s ───────────────────────────────────────────────
set(VST3_SYSTEM "C:/Program Files/Common Files/VST3")
set(DEPLOY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/deploy_vst3_postbuild.bat")

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

if(MSVC)
    target_link_options(MixCoach PRIVATE "$<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>")
    target_link_options(Messenger PRIVATE "$<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>")
endif()
