#pragma once
#include <cstdint>
#include <cstring>
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
    float       crestFactor = 0.0f;    // Peak/RMS ratio in dB
    float       sampleL     = 0.0f;     // Latest audio sample L (for vectorscope)
    float       sampleR     = 0.0f;     // Latest audio sample R (for vectorscope)
    float       spectrum[256]{};       // Magnitudes FFT (0…1)
    // LUFS (EBU R128 / ITU BS.1770)
    float       lufsIntegrated = -100.0f;
    float       lufsShortTerm  = -100.0f;
    float       lufsMomentary  = -100.0f;
    float       lufsTruePeak   = -100.0f;
    float       loudnessRange  = 0.0f;
    juce::Colour colour     = juce::Colours::grey;
    bool        active     = false;
    int         slotIndex  = -1;       // Índice en SlotRegistry
};

// ─── Tipos de Bus (Ruteo) ────────────────────────────────────────────────────
enum class BusType : int {
    None      = -1,
    Drums     = 0,
    Bass      = 1,
    Guitars   = 2,
    Keys      = 3,
    Vocals    = 4,
    FX        = 5
};

inline constexpr int kNumBuses = 6;

inline constexpr const char* busNames[] = {
    "Bateria",
    "Bajo",
    "Guitarras",
    "Teclados",
    "Voces",
    "FX"
};

// ─── Info de un slot (pista registrada) ──────────────────────────────────────
// NOTA: trackName es char[64] (no std::string) para ser compatible con
// memoria compartida (IPC entre procesos via CreateFileMapping).
// Usar getTrackName()/setTrackName() para acceso como std::string.
struct SlotInfo {
    int         slotIndex = -1;
    char        trackName[64] = {0};
    juce::Colour colour    = juce::Colours::grey;
    bool        active    = false;
    BusType     bus       = BusType::None;

    // Acceso trackName como std::string
    [[nodiscard]] std::string getTrackName() const { return std::string(trackName); }
    void setTrackName(const std::string& name) {
        strncpy_s(trackName, sizeof(trackName), name.c_str(), _TRUNCATE);
    }
    void setTrackName(const char* name) {
        strncpy_s(trackName, sizeof(trackName), name, _TRUNCATE);
    }
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
