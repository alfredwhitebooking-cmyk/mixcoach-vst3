#pragma once
#include <cstdint>
#include <cstring>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <string>

namespace mixcoach {

    // ─── Telemetría de cada pista (enviada por Messenger V2 → MixCoach) ───────
    // ULTRALIGERO: Solo RMS + Peak por canal estéreo (L/R).
    // El Messenger YA NO envía FFT/LUFS/correlación.
    // Esos análisis se hacen solo en el MASTER (MixCoach via AudioAnalyzer).
    struct alignas(32) TrackTelemetry
    {
        int64_t timestamp   = 0;       // μs
        float rms           = -100.0f; // dB (combinado, mayor de ambos canales)
        float peak          = -100.0f; // dB (combinado, mayor de ambos canales)
        float rmsLeft       = -100.0f; // dB (canal izquierdo)
        float rmsRight      = -100.0f; // dB (canal derecho)
        float peakLeft      = -100.0f; // dB (canal izquierdo)
        float peakRight     = -100.0f; // dB (canal derecho)
        float correlation   = 0.0f;    // -1 (out of phase) a +1 (in phase)
        juce::Colour colour = juce::Colours::grey;
        bool active         = false;
        int slotIndex       = -1; // Índice en SlotRegistry

        // ═══ 30-band spectral energy per slot (from backgroundRunLoop FFT) ═══
        float bandEnergies[30] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};

        // ═══ High-level audio descriptors (computed from raw audio in backgroundRunLoop) ═══
        float transientRatio  = 0.0f; // 0.0 = sustained/pad, >2.0 = transient-heavy (kick/snare)
        float crestFactor     = 0.0f; // Overall crest factor (peak - RMS) in dB
        float crestPerBand[6] = {
            0.0f}; // Crest factor in 6 broad frequency regions (Sub, Bass, LoMid, HiMid, Presence, Air)
        float stereoWidthPerBand[6] = {0.0f}; // 0.0 = mono/center, 1.0 = hard-panned stereo per band
        // ═══ Mid/Side energy per region (from bg worker Mid/Side decomposition) ═══
        float midEnergyPerBand[6]  = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        float sideEnergyPerBand[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        // ═══ Envelope descriptors (from bg worker envelope follower) ═══
        float attackTimeMs   = 0.0f;    // Estimated attack time in ms (0=silence, 5+=slow)
        float releaseTimeMs  = 0.0f;    // Estimated release/decay time in ms
        float sustainLevelDb = -100.0f; // Sustained body level in dBFS
        // ═══ LUFS — Momentary es aproximado por-pista ═══
        // lufsMomentary: Aproximacion per-pista usando RMS + K-weighting estimado
        //   desde bandEnergies[30]. Suficiente para comparaciones relativas (±2dB).
        // lufsShortTerm / lufsIntegrated: SOLO validos a nivel MASTER.
        //   Para LUFS exacto del master, usar AudioAnalyzer::get{Momentary|ShortTerm|Integrated}LUFS().
        float lufsMomentary  = -100.0f;
        float lufsShortTerm  = -100.0f;
        float lufsIntegrated = -100.0f;
        float spectrum[512]  = {0.0f};
    };

    // ─── Tipos de Bus (Ruteo) ────────────────────────────────────────────────────
    enum class BusType : int
    {
        None    = -1,
        Drums   = 0,
        Bass    = 1,
        Guitars = 2,
        Keys    = 3,
        Vocals  = 4,
        FX      = 5,
        Melody  = 6 // Instrumentos melódicos (piano, cuerdas, pads)
    };

    inline constexpr int kNumBuses = 7;

    inline constexpr const char* busNames[] = {"Bateria", "Bajo", "Guitarras", "Teclados", "Voces", "FX", "Melody"};

    // ─── Info de un slot (pista registrada) ──────────────────────────────────────
    // NOTA: trackName es char[64] (no std::string) para IPC via CreateFileMapping.
    struct SlotInfo
    {
        int slotIndex       = -1;
        char trackName[64]  = {0};
        juce::Colour colour = juce::Colours::grey;
        bool active         = false;
        bool stale          = false; // Sin telemetría por > 3s (Messenger desconectado)
        BusType bus         = BusType::None;
        int trackType       = -1;    // TrackType del Messenger (V7 Identity Layer)
        bool muted          = false; // Track muteado por el usuario (V8)
        bool soloed         = false; // Track en solo (V8)
        float faderDb       = 0.0f;  // Fader level en dB (V9)
        float panValue      = 0.0f;  // Pan -1.0 (izq) a +1.0 (der) (V9)

        // Acceso trackName como std::string
        [[nodiscard]] std::string getTrackName() const { return std::string(trackName); }

        void setTrackName(const std::string& name) { strncpy_s(trackName, sizeof(trackName), name.c_str(), _TRUNCATE); }

        void setTrackName(const char* name) { strncpy_s(trackName, sizeof(trackName), name, _TRUNCATE); }
    };

    // ─── Mensaje del mentor → chat ───────────────────────────────────────────────
    struct MentorMessage
    {
        enum class Type
        {
            Info,
            Tip,
            Warning,
            Achievement,
            Question
        };
        Type type = Type::Info;
        std::string text;
        std::string context; // ej. "Bajo", "Vocal", "Guitarra"
        int64_t timestamp = 0;
    };

    // ─── MODOS DE OPERACIÓN (V4 Dual Mode) ───────────────────────────────────────
    // Mix Mode:   Construcción de mezcla con Messengers, roles, buses, gain staging, EQ
    // Master Mode: Finalización de mezcla terminada, solo Master Listener, destino específico
    enum class CoachMode : uint8_t
    {
        Mix    = 0, // Modo mezcla (default): Messengers + roles + buses + procesamiento progresivo
        Master = 1  // Modo masterización: solo master, sin Messengers, destino + LUFS targets
    };

    // ─── Destinos de Master Mode ─────────────────────────────────────────────────
    // Cada destino tiene targets específicos de LUFS, True Peak y Loudness Range
    enum class MasterDestination : uint8_t
    {
        Spotify          = 0, // -14 LUFS, -1 dBTP
        AppleMusic       = 1, // -16 LUFS, -1 dBTP
        YouTube          = 2, // -13 LUFS, -1 dBTP
        SoundCloud       = 3, // -8 LUFS, -0.5 dBTP
        StreamingGeneral = 4, // -14 LUFS, -1 dBTP (Spotify/Apple/YT genérico)
        CD               = 5  // -9 LUFS, -0.1 dBTP
    };

    // ─── Fases de mentoría (V5 Progresión Granular) ───────────────────────────────
    // Mix Mode: 7 fases progresivas del flujo de mezcla
    // Master Mode: mismas 7 fases con descripciones enfocadas en masterización
    enum class MentorPhase
    {
        Organizacion = 0, // Setup, detectar pistas, asignar roles, nombrar, colorear, agrupar en buses
        GainStaging  = 1, // Ajustar niveles: gain staging, clipping, headroom, sin EQ ni compresión
        Balance      = 2, // Balance de mezcla: faders, paneo, niveles relativos entre instrumentos
        EQ           = 3, // Balance tonal: EQ, filtros, carving espectral por pista y bus
        Compresion   = 4, // Dinámica: compresión, saturación, control de crest factor y transientes
        Espacio      = 5, // Espacio y profundidad: reverb, delay, ancho estéreo, automatización
        MasterCheck  = 6  // Verificación final: comparar con referencia, LUFS target, veredicto
    };

    // ─── Helper: Retorna el target LUFS integrado para un destino de masterización
    inline float getDestinationLUFS(MasterDestination dest) noexcept
    {
        switch (dest) {
            case MasterDestination::Spotify:
                return -14.0f;
            case MasterDestination::AppleMusic:
                return -16.0f;
            case MasterDestination::YouTube:
                return -13.0f;
            case MasterDestination::SoundCloud:
                return -8.0f;
            case MasterDestination::StreamingGeneral:
                return -14.0f;
            case MasterDestination::CD:
                return -9.0f;
            default:
                return -14.0f;
        }
    }

    // ─── Helper: Retorna el True Peak máximo para un destino
    inline float getDestinationTruePeak(MasterDestination dest) noexcept
    {
        switch (dest) {
            case MasterDestination::Spotify:
                return -1.0f;
            case MasterDestination::AppleMusic:
                return -1.0f;
            case MasterDestination::YouTube:
                return -1.0f;
            case MasterDestination::SoundCloud:
                return -0.5f;
            case MasterDestination::StreamingGeneral:
                return -1.0f;
            case MasterDestination::CD:
                return -0.1f;
            default:
                return -1.0f;
        }
    }

    inline constexpr const char* destinationNames[] = {
        "Spotify", "Apple Music", "YouTube", "SoundCloud", "Streaming General", "CD"};

    // ─── Logros de gamificación (MASTER VISION v3) ────────────────────────────────
    enum class Achievement
    {
        FirstTrack     = 0, // Primera pista identificada
        FiveTracks     = 1, // 5 pistas con rol asignado
        TenTracks      = 2, // 10 pistas organizadas en buses
        FullMap        = 3, // Mapa de mezcla completado
        PhaseMaster    = 4, // Completar las 7 fases
        FirstReference = 5, // Primera referencia cargada
        MixComplete    = 6  // Sesión completa (todas las fases completadas)
    };

    // Arrays auxiliares (C++17 inline)
    inline constexpr const char* achievementNames[] = {
        "Primera Pista 🎯",      // FirstTrack
        "5 Pistas 🌟",           // FiveTracks
        "10 Pistas 💫",          // TenTracks
        "Mapa Completo 🗺️",      // FullMap
        "Maestro de Fase 🔮",    // PhaseMaster
        "Primera Referencia 📀", // FirstReference
        "Mezcla Completa 🏆"     // MixComplete
    };

    inline constexpr const char* phaseNames[] = {
        "Organización", // Organizacion
        "Gain Staging", // GainStaging
        "Balance",      // Balance
        "EQ",           // EQ
        "Compresión",   // Compresion
        "Espacio",      // Espacio
        "Master Check"  // MasterCheck
    };

    // ─── Fases de Master Mode (nombres para la UI)
    inline constexpr const char* masterPhaseNames[] = {
        "Organización",      // Organizacion
        "Ganancia",          // GainStaging
        "Balance Espectral", // Balance
        "EQ",                // EQ
        "Dinámica",          // Compresion
        "Estéreo",           // Espacio
        "Veredicto"          // MasterCheck
    };


} // namespace mixcoach
