#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  ReferenceAudioPlayer — Reproduce archivos de referencia de audio
//  (WAV, MP3, FLAC) desde un buffer en memoria.
//
//  ═══ HEADER AUTÓNOMO ═══
//  No incluye nada de MixCoach/Common — solo JUCE para formatos de audio.
//  Ideal para tests unitarios sin linkear todo el plugin.
//
//  ⚠ DISEÑO: Sin AudioTransportSource ni AudioFormatReaderSource.
//  El WAV se carga COMPLETO en memoria en playFile() (UI thread).
//  mixIntoBuffer() (audio thread) solo lee del buffer sin file I/O.
//  Esto elimina TODAS las race conditions y crashes en VST3/FL Studio.
// ═══════════════════════════════════════════════════════════════════════════

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#ifdef _WIN32
#include <excpt.h>
#endif

#include <memory>
#include <atomic>
#include <cstdint>
#include <vector>

namespace mixcoach {

    class ReferenceAudioPlayer
    {
    public:
        ReferenceAudioPlayer()  = default;
        ~ReferenceAudioPlayer() = default;

        void prepare(double sampleRate, int /*samplesPerBlock*/) { sampleRate_ = sampleRate; }

        void releaseResources() { clearBuffer(); }

        // ─── Cargar WAV desde disco (helper dentro de __try) ──────────────
        // ═══ REGLA DE ORO: NUNCA liberar buffers durante reproducción ═════
        //
        // El buffer se CREECE si el nuevo archivo es más grande (resize solo crece).
        // El buffer NUNCA se reduce (ni assign, ni clear, ni shrink_to_fit).
        //
        // ¿Por qué? La race condition clásica:
        //   Thread A (audio): refBufferL_[idx]  → leyendo buffer
        //   Thread B (UI):    refBufferL_.assign(nuevo) → LIBERA BUFFER VIEJO
        //   Thread A:         ACCESO A MEMORIA LIBERADA → CRASH
        //
        // Con resize() solo creciente: el buffer NUNCA se libera → NO HAY RACE.
        // El atómico bufferSize_ controla qué porción del buffer es válida.
        // El audio thread lee bufferSize_ atómicamente y nunca pasa del límite.
        bool loadFileFromDisk(const juce::String& filePath)
        {
            auto file = juce::File(filePath);
            if (!file.existsAsFile()) return false;

            juce::AudioFormatManager formatMgr;
            formatMgr.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(file));
            if (reader == nullptr) return false;

            currentFilePath_ = filePath;
            currentFileName_ = file.getFileName();

            const int64_t totalSamples = reader->lengthInSamples;
            const int numChannels      = reader->numChannels;
            if (totalSamples <= 0 || numChannels <= 0) return false;

            const int totalSamplesInt = static_cast<int>(totalSamples);

            // ═══ CRECER buffers si es necesario (NUNCA reduce) ════════════
            // resize() SOLO crece — si el buffer actual es más grande,
            // resize() no hace nada (no encoge, no libera memoria).
            // Esto elimina la race condition: el viejo buffer nunca se libera.
            if (static_cast<size_t>(totalSamplesInt) > refBufferL_.size()) {
                refBufferL_.resize(static_cast<size_t>(totalSamplesInt), 0.0f);
                refBufferR_.resize(static_cast<size_t>(totalSamplesInt), 0.0f);
            }

            // Leer samples directamente al buffer pre-asignado
            // ═══ FIX: Leer AMBOS canales estéreo en una sola llamada ═══════
            // Antes se leía channel 0 dos veces (una para L, otra para R),
            // lo que hacía que archivos estéreo sonaran como mono.
            if (numChannels >= 2) {
                float* stereoDest[2] = {refBufferL_.data(), refBufferR_.data()};
                reader->read(stereoDest, 2, 0, totalSamplesInt);
            }
            else if (numChannels == 1) {
                float* monoDest[1] = {refBufferL_.data()};
                reader->read(monoDest, 1, 0, totalSamplesInt);
                // Mono: copiar L → R
                std::copy(refBufferL_.begin(), refBufferL_.begin() + totalSamplesInt, refBufferR_.begin());
            }

            loadedTotalSamples_ = totalSamples;
            loadedSampleRate_   = reader->sampleRate;
            return true;
        }

        void playFile(const juce::String& filePath)
        {
            if (isPlaying() && currentFilePath_ == filePath) {
                pause();
                return;
            }

            // ═══ PASO 1: Cerrar puerta atómica (audio thread ve isPlaying_=false) ══
            // Los vectores aún tienen datos del archivo anterior, pero el audio thread
            // no los toca porque isPlaying_=false. Esto es INTENCIONAL: evitamos
            // la race condition donde el audio thread accede a un vector mientras
            // lo reasignamos.
            isPlaying_.store(false);
            playingRefIndex_.store(-1);
            clearBuffer();

            // ═══ PASO 2: __try protege el file I/O + allocaciones grandes ═════
            // loadFileFromDisk() tiene objetos C++ (AudioFormatManager, unique_ptr...)
            // pero como está en una función SEPARADA, MSVC no emite C2712.
            // En playFile() NO hay objetos C++ locales con destructor, solo:
            // - llamadas a funciones miembro
            // - atomic stores
            // - __try/__except
            bool loadOk = false;
            __try {
                loadOk = loadFileFromDisk(filePath);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                // SEH capturado — NO tocar heap (sin stop(), sin strings)
                sehSafeStop();
                return;
            }

            if (!loadOk) {
                // Carga falló sin SEH (archivo no existe, formato no soportado)
                return;
            }

            // ═══ PASO 3: Publicar buffers atómicamente (ORDEN CRÍTICO) ═══════
            // 1. Metadatos (tamaño, sample rate)
            // 2. Índice de lectura = 0
            // 3. isPlaying_ = true
            //
            // El audio thread lee en orden inverso: isPlaying_ → readIndex_ → bufferSize_
            // Los acquire/release garantizan visibilidad total sin race conditions.
            bufferSize_.store(loadedTotalSamples_, std::memory_order_release);
            bufferSampleRate_.store(loadedSampleRate_, std::memory_order_release);
            readIndex_.store(0, std::memory_order_release);
            isPlaying_.store(true, std::memory_order_release);
        }

        void playFileAtPath(const juce::String& filePath, int refIndex)
        {
            playFile(filePath);
            playingRefIndex_.store(refIndex);
        }

        void pause() { isPlaying_.store(false); }

        void stop()
        {
            isPlaying_.store(false);
            readIndex_.store(0);
            playingRefIndex_.store(-1);
            currentFilePath_.clear();
            currentFileName_.clear();
            clearBuffer();
        }

        // ─── SEH-safe: solo atomic, sin heap ────────────────────────────────
        // Llama pause() en vez de stop() desde handlers SEH.
        // stop() usa juce::String::clear() que puede AV en heap corrupto.
        // pause() solo setea isPlaying_=false (atomic, 100% seguro).
        void sehSafeStop() noexcept
        {
            isPlaying_.store(false, std::memory_order_release);
            playingRefIndex_.store(-1, std::memory_order_release);
        }

        // ─── Seek / Transport ───────────────────────────────────────────────
        void setPosition(double seconds)
        {
            double sr = bufferSampleRate_.load(std::memory_order_acquire);
            if (sr <= 0.0) return;
            int64_t bSize = bufferSize_.load(std::memory_order_acquire);
            int64_t idx   = static_cast<int64_t>(juce::jmax(0.0, seconds) * sr);
            idx           = (bSize > 0) ? juce::jmin(idx, bSize - 1) : idx;
            readIndex_.store(idx, std::memory_order_release);
        }

        double getPosition() const noexcept
        {
            double sr = bufferSampleRate_.load(std::memory_order_acquire);
            if (sr <= 0.0) return 0.0;
            return static_cast<double>(readIndex_.load(std::memory_order_acquire)) / sr;
        }

        double getLength() const noexcept
        {
            double sr = bufferSampleRate_.load(std::memory_order_acquire);
            if (sr <= 0.0) return 0.0;
            return static_cast<double>(bufferSize_.load(std::memory_order_acquire)) / sr;
        }

        double getProgress() const noexcept
        {
            int64_t bSize = bufferSize_.load(std::memory_order_acquire);
            if (bSize <= 0) return 0.0;
            return static_cast<double>(readIndex_.load(std::memory_order_acquire)) / static_cast<double>(bSize);
        }

        // ─── Estado ──────────────────────────────────────────────────────────
        bool isPlaying() const noexcept { return isPlaying_.load(); }

        int getPlayingRefIndex() const noexcept { return playingRefIndex_.load(); }

        juce::String getCurrentFileName() const { return currentFileName_; }

        // ─── Acceso al buffer cargado (para Fase 3: reusar en analyzeReferenceFile) ─
        /** Buffer L del archivo cargado (read-only). */
        const float* getRefBufferL() const noexcept { return refBufferL_.data(); }

        /** Buffer R del archivo cargado (read-only). */
        const float* getRefBufferR() const noexcept { return refBufferR_.data(); }

        /** Numero de samples por canal del buffer cargado. */
        int64_t getLoadedBufferSize() const noexcept { return loadedTotalSamples_; }

        /** Numero de canales del archivo cargado (1=mono, 2=estereo). */
        int getLoadedNumChannels() const noexcept { return loadedNumChannels_; }

        /** Sample rate del archivo cargado. */
        double getLoadedSampleRate() const noexcept { return loadedSampleRate_; }

        // ─── Mezclar en buffer de salida (audio thread) ─────────────────────
        void mixIntoBuffer(juce::AudioBuffer<float>& outputBuffer, float volume = 0.5f)
        {
            // ═══ SIN File I/O, SIN JUCE Transport, SIN locks ═══════════════
            // Solo lectura de buffers en memoria + posición atómica.
            // Esto NO puede crashear porque no hay recursos externos.
            if (!isPlaying_.load()) return;

            const int numChannels = outputBuffer.getNumChannels();
            const int numSamples  = outputBuffer.getNumSamples();

            if (numSamples <= 0 || numChannels <= 0) return;

            if (refBufferL_.empty()) {
                isPlaying_.store(false);
                return;
            }

            // ─── Leer posición actual atómicamente ──────────────────────────
            // ORDEN: Primero isPlaying_ (gate), luego readIndex_, luego bufferSize_.
            // Esto garantiza que si playFile() está escribiendo, vemos isPlaying_=false
            // y salimos antes de tocar los buffers.
            int64_t idx   = readIndex_.load(std::memory_order_acquire);
            int64_t bSize = bufferSize_.load(std::memory_order_acquire);

            if (idx >= bSize) {
                // Llegó al final
                isPlaying_.store(false, std::memory_order_release);
                playingRefIndex_.store(-1, std::memory_order_release);
                return;
            }

            int64_t samplesToCopy      = juce::jmin(static_cast<int64_t>(numSamples), bSize - idx);
            const int samplesToCopyInt = static_cast<int>(samplesToCopy);

            // ─── Copiar samples usando FloatVectorOperations (RÁPIDO) ───────
            if (samplesToCopyInt > 0) {
                for (int ch = 0; ch < numChannels && ch < 2; ++ch) {
                    const float* src = (ch == 0) ? &refBufferL_[static_cast<size_t>(idx)]
                                                 : &refBufferR_[static_cast<size_t>(idx)];

                    float* writePtr = outputBuffer.getWritePointer(ch);
                    juce::FloatVectorOperations::addWithMultiply(writePtr, src, volume, samplesToCopyInt);
                }
            }

            // ─── Avanzar posición de lectura ────────────────────────────────
            readIndex_.store(idx + samplesToCopy, std::memory_order_release);

            // Si no copiamos todo (final del archivo), marcar como terminado
            if (samplesToCopy < numSamples) {
                isPlaying_.store(false, std::memory_order_release);
                playingRefIndex_.store(-1, std::memory_order_release);
            }
        }

    private:
        void clearBuffer()
        {
            // ═══ PUBLICAR VACIADO (ORDEN CRÍTICO) ═══════════════════════════
            // El audio thread lee bufferSize_ después de isPlaying_.
            // Al ponerlo a 0 PRIMERO, el audio thread ve buffer vacío y sale.
            bufferSize_.store(0, std::memory_order_release);
            bufferSampleRate_.store(0.0, std::memory_order_release);
            isPlaying_.store(false, std::memory_order_release);
            readIndex_.store(0, std::memory_order_release);
        }

        // ─── Buffer de audio completo en memoria ─────────────────────────────
        std::vector<float> refBufferL_; // Canal izquierdo
        std::vector<float> refBufferR_; // Canal derecho

        // ═══ Cache de carga (set por loadFileFromDisk, leido por playFile) ══
        int64_t loadedTotalSamples_ = 0;
        double loadedSampleRate_    = 0.0;
        int loadedNumChannels_      = 0;

        // ═══ CAMPOS ATÓMICOS (thread-safe UI ↔ Audio, sin locks) ══════════
        // Todos se leen desde mixIntoBuffer() (audio thread) con memory_order_acquire
        // y se escriben desde playFile()/stop() (UI thread) con memory_order_release.
        std::atomic<int64_t> bufferSize_{0}; // Samples POR canal
        std::atomic<double> bufferSampleRate_{0.0};
        std::atomic<int64_t> readIndex_{0};
        std::atomic<bool> isPlaying_{false};
        std::atomic<int> playingRefIndex_{-1};

        double sampleRate_ = 44100.0;
        juce::String currentFilePath_;
        juce::String currentFileName_;
    };

} // namespace mixcoach
