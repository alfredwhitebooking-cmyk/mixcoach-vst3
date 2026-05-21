#pragma once
#include <cstdint>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <string>

namespace mixcoach {

// ─── Telemetría de cada pista (enviada por Messenger → MixCoach) ────────────
struct alignas(64) TrackTelemetry {
    int64_t       timestamp = 0;        // μs, Mono
    float       peakLeft   = -100.0f;  // dB
    float       peakRight  = -100.0f;
    float       rmsLeft    = -100.0f;
    float       rmsRight   = -100.0f;
    float       correlation= 1.0f;     // Phase correlation −1…1
    float       spectrum[256]{};       // Magnitudes FFT (0…1)
    juce::Colour colour     = juce::Colours::grey;
    bool        active     = false;
    int         slotIndex  = -1;       // Índice en SlotRegistry
};

// ─── Info de un slot (pista registrada) ──────────────────────────────────────
struct SlotInfo {
    int         slotIndex = -1;
    std::string trackName;
    juce::Colour colour    = juce::Colours::grey;
    bool        active    = false;
};

// ─── Mensaje del mentor → chat ───────────────────────────────────────────────
struct MentorMessage {
    enum class Type { Info, Tip, Warning, Achievement, Question };
    Type        type   = Type::Info;
    std::string text;
    std::string context;   // ej. "Bajo", "Vocal", "Guitarra"
    int64_t     timestamp = 0;
};

// ─── Fases de mentoría predefinidas ──────────────────────────────────────────
enum class MentorPhase {
    Welcome       = 0,
    GainStaging   = 1,
    Organisation  = 2,
    TonalBalance  = 3,
    Dynamics      = 4,
    Spatial       = 5
};

// ─── Logros de gamificación ──────────────────────────────────────────────────
enum class Achievement {
    FirstTrack     = 0,
    FiveTracks     = 1,
    TenTracks      = 2,
    FullMix        = 3,
    PhaseMaster    = 4,
    DynamicControl = 5,
    GainGod        = 6
};

// Arrays auxiliares (C++17 inline)
inline constexpr const char* achievementNames[] = {
    "Primera Pista 🎯",
    "5 Pistas 🌟",
    "10 Pistas 💫",
    "Mezcla Completa 🏆",
    "Maestro de Fase 🔮",
    "Control Dinámico ⚡",
    "Dios del Gain 🛐"
};

inline constexpr const char* phaseNames[] = {
    "Bienvenida",
    "Gain Staging",
    "Organización",
    "Balance Tonal",
    "Dinámica",
    "Espacialidad"
};

} // namespace mixcoach
