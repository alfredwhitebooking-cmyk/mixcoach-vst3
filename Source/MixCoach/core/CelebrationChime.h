#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  CelebrationChime — Genera un chime corto y agradable para celebrar
    //  correcciones exitosas. Se reproduce mezclándose en el buffer de salida
    //  desde el audio thread (processBlock).
    //
    //  El chime es un acorde mayor (C5+E5+G5 = 523+659+784 Hz) que suena
    //  durante ~400ms con fade-out exponencial. Suena como un "ding" festivo
    //  que refuerza la sensación de logro al completar una fase de coaching.
    //
    //  Thread-safety:
    //    • trigger()   — llama desde UI thread (NavigationShell)
    //    • mixIntoBuffer() — llama desde audio thread (PluginProcessor::processBlock)
    //    • Ambos usan std::atomic para estado compartido sin locks
    // ═══════════════════════════════════════════════════════════════════════════
    class CelebrationChime
    {
    public:
        CelebrationChime() = default;
        ~CelebrationChime() = default;

        /** Prepara el chime con el sample rate del proyecto.
            Genera un buffer stereo con el acorde sintetizado.
            @param sampleRate  Frecuencia de muestreo (ej: 48000) */
        void prepare(double sampleRate)
        {
            sampleRate_ = sampleRate;
            generateChime();
        }

        /** Dispara el chime. Resetea el playhead al inicio.
            Seguro para llamar desde cualquier thread. */
        void trigger()
        {
            if (chimeBuffer_.getNumSamples() <= 0) return;
            playIndex_.store(0, std::memory_order_release);
            isActive_.store(true, std::memory_order_release);
        }

        /** Mezcla el chime en el buffer de salida si está activo.
            @param outputBuffer  El buffer de audio de salida.
            @param volume        Factor de volumen (0.0-1.0). Default 0.35 para sutil. */
        void mixIntoBuffer(juce::AudioBuffer<float>& outputBuffer, float volume = 0.35f)
        {
            if (!isActive_.load(std::memory_order_acquire)) return;
            if (volume <= 0.0f) return;

            const int numOutChannels = outputBuffer.getNumChannels();
            const int numOutSamples  = outputBuffer.getNumSamples();
            if (numOutSamples <= 0 || numOutChannels <= 0) return;

            const int chimeSamples = chimeBuffer_.getNumSamples();
            if (chimeSamples <= 0) { isActive_.store(false); return; }

            int64_t idx = playIndex_.load(std::memory_order_acquire);
            if (idx >= chimeSamples) {
                isActive_.store(false, std::memory_order_release);
                return;
            }

            // Mezclar stereo: ambos canales leen del MISMO idx para
            // mantener la fase estéreo correcta (sin desync L/R).
            const float* chimeL = chimeBuffer_.getReadPointer(0);
            const float* chimeR = (chimeBuffer_.getNumChannels() > 1)
                                   ? chimeBuffer_.getReadPointer(1) : chimeL;

            for (int s = 0; s < numOutSamples && idx < chimeSamples; ++s, ++idx) {
                if (numOutChannels > 0) {
                    float* outL = outputBuffer.getWritePointer(0);
                    outL[s] += chimeL[idx] * volume;
                }
                if (numOutChannels > 1) {
                    float* outR = outputBuffer.getWritePointer(1);
                    outR[s] += chimeR[idx] * volume;
                }
            }

            // Actualizar playhead (ambos canales comparten el mismo idx)
            playIndex_.store(idx, std::memory_order_release);

            // Si terminó, marcar como inactivo
            if (idx >= chimeSamples) {
                isActive_.store(false, std::memory_order_release);
            }
        }

        /** Retorna true si el chime está sonando actualmente. */
        bool isPlaying() const noexcept
        {
            return isActive_.load(std::memory_order_acquire);
        }

        /** Libera recursos. */
        void releaseResources()
        {
            chimeBuffer_.setSize(1, 0);
            isActive_.store(false);
            playIndex_.store(0);
        }

    private:
        // ═══════════════════════════════════════════════════════════════════════
        //  generateChime — Sintetiza un acorde mayor como un breve "ding"
        //
        //  Frecuencias: C5 (523 Hz), E5 (659 Hz), G5 (784 Hz)
        //  Envolvente: ataque rápido (5ms) + decaimiento exponencial (~400ms)
        //  Efecto: las notas tienen un leve desfase de fase para sonido más rico
        // ═══════════════════════════════════════════════════════════════════════
        void generateChime()
        {
            if (sampleRate_ <= 0.0) return;

            const double durationSec = 0.4;  // 400ms
            const int numSamples = (int)(sampleRate_ * durationSec);
            if (numSamples <= 0) return;

            // Frecuencias del acorde mayor de Do
            const double freqs[] = { 523.25, 659.25, 783.99 };  // C5, E5, G5
            const double phases[] = { 0.0, 0.8, 1.7 };          // Desfase para sonido más rico

            // Crear buffer stereo
            chimeBuffer_.setSize(2, numSamples);
            chimeBuffer_.clear();

            for (int ch = 0; ch < 2; ++ch) {
                float* samples = chimeBuffer_.getWritePointer(ch);

                // Pequeña variación estéreo: el canal derecho tiene las notas
                // ligeramente detuned para simular un chorus natural
                double detune = (ch == 1) ? 1.003 : 1.0;  // +0.3% en el canal derecho

                for (int s = 0; s < numSamples; ++s) {
                    double t = (double)s / sampleRate_;

                    // Envolvente: ataque rápido + decaimiento exponencial
                    double envelope;
                    if (t < 0.005) {
                        // Ataque: 0→1 en 5ms
                        envelope = t / 0.005;
                    } else {
                        // Decaimiento exponencial: 1→0.01 en 400ms
                        double decayT = (t - 0.005) / 0.395;
                        envelope = std::exp(-decayT * 5.0);  // exp(-5) ≈ 0.007
                    }

                    // Sintetizar el acorde: suma de 3 senoidales
                    double sample = 0.0;
                    for (int n = 0; n < 3; ++n) {
                        double freq = freqs[n] * detune;
                        double phase = phases[n];
                        sample += std::sin(2.0 * juce::MathConstants<double>::pi * freq * t + phase);
                    }

                    // Normalizar (3 senoidales sumadas → dividir por 3)
                    sample /= 3.0;

                    // Aplicar envelope
                    samples[s] = (float)(sample * envelope);
                }
            }

            playIndex_.store(0, std::memory_order_release);
        }

        // ─── Estado atómico (audio-thread safe) ──────────────────────────────
        std::atomic<bool> isActive_{ false };
        std::atomic<int64_t> playIndex_{ 0 };

        // ─── Buffer del chime (se genera en prepare, solo lectura en audio thread) ──
        juce::AudioSampleBuffer chimeBuffer_;

        // ─── Sample rate del proyecto ────────────────────────────────────────
        double sampleRate_ = 0.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CelebrationChime)
    };

} // namespace mixcoach
