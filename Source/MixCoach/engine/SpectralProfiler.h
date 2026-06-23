#pragma once
#include <juce_core/juce_core.h>
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/types/Types.h"
#include "TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackSpectralProfile — Perfil espectral completo de una pista
    //  Computado a partir de TrackAudioResult + TrackTelemetry.
    //  Es la representación SEMÁNTICA de lo que la pista está sonando.
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackSpectralProfile
    {
        // ─── Niveles básicos ──────────────────────────────────────────
        float peakDb      = -100.0f;
        float rmsDb       = -100.0f;
        float crestDb     = 0.0f; // peak - rms (dB)
        float correlation = 0.0f;

        // ─── Descriptores de alto nivel (de AudioDNA) ─────────────────
        float transientRatio        = 0.0f;
        float crestPerBand[6]       = {0.0f};
        float stereoWidthPerBand[6] = {0.0f};

        // ─── Energía espectral absoluta por banda (dBFS) ─────────────
        // Computado desde bandEnergies[30] → 6 bandas promediadas.
        // Útil para EQ recommendations precisas con Hz + dB exactos.
        float bandLevelDb[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};

        // ─── Centroide espectral estimado (Hz) ──────────────────────
        // Indica si el sonido es "brillante" (>3kHz) u "oscuro" (<1kHz)
        float spectralCentroid = 0.0f;

        // ─── Frecuencia fundamental estimada (Hz) ─────────────────────
        // Estimación de la frecuencia más baja con energía significativa
        float fundamentalEstimate = 0.0f;

        // ─── Ancho espectral total (0-1) ─────────────────────────────
        // Promedio del stereo width a través de todas las bandas
        float avgStereoWidth = 0.0f;

        // ═══ Envelope descriptors (from bg worker envelope follower) ════════════
        float attackTimeMs   = 0.0f;    // Attack time in ms (0=silence, 1-5=percussive, 10+=slow)
        float releaseTimeMs  = 0.0f;    // Release/decay time in ms
        float sustainLevelDb = -100.0f; // Sustained body level in dBFS
        bool isPercussive    = false;   // attackTimeMs < 10ms && hasTransientCharacter
        bool isSustained     = false;   // releaseTimeMs > 200ms || (crestDb < 6.0f && releaseTimeMs > 100ms)

        // ═══ Mid/Side ratio per region (0.0 = puro mono, 1.0+ = stereo amplio) ═══
        // Computado desde midEnergyPerBand y sideEnergyPerBand del bg worker.
        // midSideRatio[r] = sideEnergy / midEnergy (lineal)
        // >0.5 significa que el Side tiene energia significativa → contenido estéreo
        float midSideRatio[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

        // ─── Indicador de rol inferido ───────────────────────────────
        // Basado en el perfil espectral + crest + transientRatio
        bool hasTransientCharacter = false; // crest > 12dB + transientRatio > 2.0
        bool hasSustainedCharacter = false; // crest < 6dB
        bool isBassHeavy           = false; // bandLevelDb bass > high + 6dB
        bool isBright              = false; // bandLevelDb[4] o [5] dominan sobre [2]
        bool isMonoCompatible      = true;  // avgStereoWidth < 0.3

        [[nodiscard]] bool hasData() const noexcept { return peakDb > -90.0f; }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  SpectralProfiler — Computa TrackSpectralProfile desde telemetría real
    // ═══════════════════════════════════════════════════════════════════════════
    class SpectralProfiler
    {
    public:
        /** Computa el perfil espectral completo desde un TrackAudioResult. */
        static TrackSpectralProfile computeProfile(const TrackAudioResult& result) noexcept;

        /** Computa el perfil espectral desde TrackTelemetry (para CoachEngine). */
        static TrackSpectralProfile computeProfile(const TrackTelemetry& telem) noexcept;

        /** Estima el centroide espectral desde bandLevelDb[6] (Hz aproximados).
            Usa energía real (dBFS→lineal) como peso, no crest factor. */
        static float estimateCentroid(const float bandLevelDb[6]) noexcept;

        /** Estima la frecuencia fundamental desde bandLevelDb[6] (Hz).
            Retorna la frecuencia central de la banda más grave con energía significativa. */
        static float estimateFundamental(const float bandLevelDb[6]) noexcept;

        /** Determina si el perfil tiene carácter transiente. */
        static bool hasTransientChar(float crest, float transientRatio) noexcept;

        /** Determina si la pista es "bass-heavy" (más energía en graves que agudos).
            Usa bandLevelDb (dBFS real), no crest factor. */
        static bool isBassHeavy(const float bandLevelDb[6]) noexcept;

        // ═══════════════════════════════════════════════════════════════════════
        //  INFERENCIA AUTOMATICA DE ROL
        //  Usa transientRatio, crestPerBand, bandLevelDb y stereoWidth
        //  para clasificar la pista en uno de los roles conocidos.
        //  Retorna Unknown si no puede determinar el rol con confianza.
        // ═══════════════════════════════════════════════════════════════════════
        /** Infiere el TrackRole mas probable desde el perfil espectral. */
        static TrackRole inferTrackRole(const TrackSpectralProfile& profile) noexcept;

        /** Infiere el TrackRole ajustado por genero musical.
            Primero ejecuta la inferencia base, luego aplica ajustes segun el genero.
            Ej: Reggaeton → Kick se convierte en ReggaetonKick, Trap → Snare se convierte en SnareTrap. */
        static TrackRole inferTrackRole(const TrackSpectralProfile& profile, const juce::String& genre) noexcept;

        // ─── Helpers de energia espectral (para arbol de decision) ──
        /** Energia en la region Sub+Bass (bandas 0-1). */
        static float subBassEnergy(const float bandLevelDb[6]) noexcept;
        /** Energia en la region Low-Mid + High-Mid (bandas 2-3). */
        static float midEnergy(const float bandLevelDb[6]) noexcept;
        /** Energia en la region Presence + Air (bandas 4-5). */
        static float highEnergy(const float bandLevelDb[6]) noexcept;
        /** Relacion Sub+High (indica si es brillante vs oscuro). */
        static float bassToMidRatio(const float bandLevelDb[6]) noexcept;
        /** Relacion High+Mid (indica presencia de agudos). */
        static float highToMidRatio(const float bandLevelDb[6]) noexcept;

        // Frecuencias centrales aproximadas de cada banda (Hz)
        // Sub(0)=94Hz, Bass(1)=281Hz, LoMid(2)=752Hz, HiMid(3)=2046Hz,
        // Pres(4)=5144Hz, Air(5)=12028Hz
        static constexpr float kBandCenterFreqs[6] = {94.0f, 281.0f, 752.0f, 2046.0f, 5144.0f, 12028.0f};

        // Nombres descriptivos de cada banda para mensajes EQ
        static constexpr const char* kBandNames[6] = {
            "Sub (94Hz)", "Bass (281Hz)", "LoMid (752Hz)", "HiMid (2kHz)", "Pres (5kHz)", "Air (12kHz)"};

    private:
    };

} // namespace mixcoach
