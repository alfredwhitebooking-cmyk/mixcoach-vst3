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
set(JUCE_ROOT "C:/MIXCOA~1/juce-8.0.13-windows/MixCoachAI/JUCE")

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
    Source/Common/memory/SharedData.h
    Source/Common/memory/SharedData.cpp
    Source/Common/types/TelemetryData.h
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
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
    Source/MixCoach/UI/VectorscopeComponent.h
    Source/MixCoach/UI/VectorscopeComponent.cpp
    Source/MixCoach/UI/PhaseCorrelationMeter.h
    Source/MixCoach/UI/PhaseCorrelationMeter.cpp
    Source/MixCoach/UI/CrestHistogram.h
    Source/MixCoach/UI/CrestHistogram.cpp
    Source/MixCoach/UI/PlaylistComponent.h
    Source/MixCoach/UI/PlaylistComponent.cpp
    Source/MixCoach/UI/VerticalGradientMeter.h
    Source/MixCoach/UI/VerticalGradientMeter.cpp
    Source/MixCoach/UI/MeterComponent.h
    Source/MixCoach/UI/MeterComponent.cpp
    Source/MixCoach/UI/AnalogVUMeter.h
    Source/MixCoach/UI/AnalogVUMeter.cpp
    Source/MixCoach/UI/VUMetersPanel.h
    Source/MixCoach/UI/VUMetersPanel.cpp
    Source/MixCoach/UI/PhaseScopePanel.h
    Source/MixCoach/UI/PhaseScopePanel.cpp
    Source/MixCoach/UI/DividerBar.h
    Source/MixCoach/UI/MessengerListComponent.h
    Source/MixCoach/UI/MessengerListComponent.cpp
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
    Source/MixCoach/core/PluginEditor.h
    Source/MixCoach/core/PluginEditor.cpp
    Source/MixCoach/engine/PhaseManager.h
    Source/MixCoach/engine/PhaseManager.cpp
    Source/MixCoach/engine/CoachEngine.h
    Source/MixCoach/engine/CoachEngine.cpp
    Source/MixCoach/audio/AudioAnalyzer.h
    Source/MixCoach/audio/AudioAnalyzer.cpp
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
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
    Source/Messenger/core/PluginProcessor.h
    Source/Messenger/core/PluginProcessor.cpp
    Source/Messenger/ui/PluginEditor.h
    Source/Messenger/ui/PluginEditor.cpp
    Source/Messenger/ui/VUMeter.h
    Source/Messenger/ui/VUMeter.cpp
    Source/Messenger/ui/WaveformView.h
    Source/Messenger/ui/WaveformView.cpp
    Source/Messenger/ui/CircularGauge.h
    Source/Messenger/ui/CircularGauge.cpp
    Source/Messenger/ui/ColourPresetStrip.h
    Source/Messenger/ui/ColourPresetStrip.cpp
    Source/Messenger/ui/ColourSwatch.h
    Source/Messenger/ui/ColourSwatch.cpp
    Source/Messenger/ui/StereoMeter.h
    Source/Messenger/ui/StereoMeter.cpp
    Source/Messenger/telemetry/TelemetryCollector.h
    Source/Messenger/telemetry/TelemetryCollector.cpp
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
    Source/Common/types/TelemetryData.h
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
    Source/MixCoach/engine/PhaseManager.h
    Source/MixCoach/engine/PhaseManager.cpp
    Source/Common/memory/SharedData.h
    Source/Common/memory/SharedData.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/TelemetryData.h
    Source/Common/types/LogHelper.h
)
target_include_directories(TestCoachEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestCoachEngine PRIVATE
    juce_core
    juce_graphics
    juce_audio_basics
)
target_compile_options(TestCoachEngine PRIVATE /W4)
set_target_properties(TestCoachEngine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: TelemetryCollector DSP pipeline (Peak, RMS, FFT, LUFS, Correlation) ──
add_executable(TestTelemetryDSP
    tests/TestTelemetryDSP.cpp
    Source/Messenger/telemetry/TelemetryCollector.h
    Source/Messenger/telemetry/TelemetryCollector.cpp
    Source/Common/types/Types.h
    Source/Common/types/Constants.h
    Source/Common/types/TelemetryData.h
)
target_include_directories(TestTelemetryDSP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestTelemetryDSP PRIVATE
    juce_core
    juce_graphics
    juce_audio_basics
    juce_dsp
)
target_compile_options(TestTelemetryDSP PRIVATE /W4)
set_target_properties(TestTelemetryDSP PROPERTIES
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
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
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
set_target_properties(TestVectorscopeComponent PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# ─── Test: PhaseCorrelationMeter (horizontal correlation bar UI) ──────────
add_executable(TestPhaseCorrelationMeter
    tests/TestPhaseCorrelationMeter.cpp
    Source/MixCoach/UI/PhaseCorrelationMeter.h
    Source/MixCoach/UI/PhaseCorrelationMeter.cpp
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
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

# ─── Test: MeterComponent (compact meter panel UI) ─────────────────────────
add_executable(TestMeterComponent
    tests/TestMeterComponent.cpp
    Source/MixCoach/UI/VerticalGradientMeter.h
    Source/MixCoach/UI/VerticalGradientMeter.cpp
    Source/MixCoach/UI/MeterComponent.h
    Source/MixCoach/UI/MeterComponent.cpp
    Source/MixCoach/UI/SmoothValue.h
    Source/MixCoach/UI/SmoothValue.cpp
    Source/Common/types/TelemetryData.h
)
target_include_directories(TestMeterComponent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestMeterComponent PRIVATE
    juce_core
    juce_graphics
    juce_gui_basics
)
target_compile_options(TestMeterComponent PRIVATE /W4)
set_target_properties(TestMeterComponent PROPERTIES
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
    Source/Common/types/TelemetryData.h
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

# ─── Test: IPC Integration (SlotRegistry → Backup → SharedMemory pipeline) ──
add_executable(TestIPCIntegration
    tests/TestIPCIntegration.cpp
    Source/Common/memory/SlotRegistry.h
    Source/Common/memory/SlotRegistry.cpp
    Source/Common/memory/SharedMemory.h
    Source/Common/memory/SharedMemory.cpp
    Source/Common/types/Constants.h
    Source/Common/types/Types.h
    Source/Common/types/TelemetryData.h
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

# ─── Target: build + run all tests ──────────────────────────────────────────
add_custom_target(run_tests
    COMMAND $<TARGET_FILE:TestSmoothValue>
    COMMAND $<TARGET_FILE:TestPhaseManager>
    COMMAND $<TARGET_FILE:TestCoachEngine>
    COMMAND $<TARGET_FILE:TestIPCIntegration>
    COMMAND $<TARGET_FILE:TestSharedMemory>
    COMMAND $<TARGET_FILE:TestSlotRegistry>
    COMMAND $<TARGET_FILE:TestTelemetryDSP>
    COMMAND $<TARGET_FILE:TestLUFSMeter>
    COMMAND $<TARGET_FILE:TestSpectrographComponent>
    COMMAND $<TARGET_FILE:TestVectorscopeComponent>
    COMMAND $<TARGET_FILE:TestPhaseCorrelationMeter>
    COMMAND $<TARGET_FILE:TestMeterComponent>
    DEPENDS TestSmoothValue TestPhaseManager TestCoachEngine TestTelemetryDSP TestIPCIntegration
            TestSharedMemory TestSlotRegistry
            TestLUFSMeter TestSpectrographComponent TestVectorscopeComponent
            TestPhaseCorrelationMeter TestMeterComponent
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
