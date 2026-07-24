#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  UrlDownloader — Descarga audio desde URLs (YouTube, Spotify, etc.)
    //
    //  Usa yt-dlp externo para descargar audio y convertirlo a WAV.
    //  Flujo:
    //    1. Detectar yt-dlp en PATH o bundled
    //    2. Ejecutar: yt-dlp -x --audio-format wav --output "temp.wav" "URL"
    //    3. Reportar progreso vía callback
    //    4. Devolver ruta del archivo descargado
    //
    //  Uso:
    //    UrlDownloader downloader;
    //    downloader.downloadURL("https://youtu.be/xxx",
    //        [](const juce::String& result) {
    //            if (result.isEmpty()) // error
    //            else // result = path to downloaded WAV
    //        },
    //        [](float progress) {
    //            // progress 0.0-1.0
    //        });
    // ═══════════════════════════════════════════════════════════════════════════
    class UrlDownloader : private juce::Timer
    {
    public:
        UrlDownloader();
        ~UrlDownloader() override;

            // ─── Track Metadata — extraído con --print antes de descargar ───────
        struct TrackMetadata
        {
            juce::String title;            // Título de la canción
            juce::String channel;          // Nombre del artista/canal
            double durationSeconds = 0.0;  // Duración en segundos
            juce::String durationString;   // Duración formateada (ej: "3:45")
            bool valid = false;            // true si se obtuvo metadata exitosamente

            /** Retorna "Título - Artista" o solo el título si no hay artista. */
            [[nodiscard]] juce::String displayName() const noexcept
            {
                if (title.isNotEmpty() && channel.isNotEmpty())
                    return title + " - " + channel;
                if (title.isNotEmpty())
                    return title;
                if (channel.isNotEmpty())
                    return channel;
                return "Referencia";
            }
        };

        // ─── Callbacks ───────────────────────────────────────────────────────
        /** Se llama cuando la descarga termina.
            @param filePath  Ruta al archivo WAV descargado, o vacío si error */
        std::function<void(const juce::String& filePath)> onComplete;

        /** Se llama periódicamente con el progreso de la descarga (0.0 - 1.0). */
        std::function<void(float progress)> onProgress;

        /** Se llama cuando se obtiene la metadata de la URL (antes de descargar). */
        std::function<void(const TrackMetadata& metadata)> onMetadataFetched;

        // ─── API pública ────────────────────────────────────────────────────

        /** Extrae metadata de una URL usando yt-dlp --print.
            Se ejecuta sincrónicamente — llamar desde background thread.
            Normalmente se llama automáticamente desde downloadURL() en primera
            llamada, pero puede llamarse antes para obtener solo la metadata
            sin descargar.
            @param url  URL de YouTube, Spotify, etc.
            @return true si la metadata se obtuvo exitosamente */
        bool fetchMetadata(const juce::String& url);

        /** Descarga audio desde una URL.
            Si fetchMetadata() no se ha llamado antes, se llama automáticamente
            al inicio de downloadURL(). Usa el título y artista para nombrar
            el archivo: "Título - Artista.wav"
            @param url  URL de YouTube, Spotify, Apple Music, etc.
            @return true si se pudo iniciar la descarga */
        bool downloadURL(const juce::String& url);

        /** Cancela la descarga en curso. */
        void cancel();

        /** Retorna true si hay una descarga en curso. */
        [[nodiscard]] bool isDownloading() const noexcept { return isDownloading_; }

        /** Retorna el progreso actual (0.0 - 1.0). */
        [[nodiscard]] float getProgress() const noexcept { return progress_; }

        /** Retorna la metadata cacheadel último fetchMetadata() o downloadURL(). */
        [[nodiscard]] const TrackMetadata& getMetadata() const noexcept { return metadata_; }

        /** Verifica si yt-dlp está disponible en el sistema.
            @return true si se encontró yt-dlp */
        static bool isYtDlpAvailable();

        /** Retorna la ruta al ejecutable de yt-dlp. */
        static juce::String getYtDlpPath();

    private:
        void timerCallback() override;

        void checkProcessOutput();
        void finishDownload(const juce::String& filePath);
        void failDownload(const juce::String& error);

        /** Limpia caracteres inválidos para nombre de archivo en Windows. */
        static juce::String sanitizeForFilename(const juce::String& s);

        // ─── Estado ─────────────────────────────────────────────────────────
        bool isDownloading_ = false;
        float progress_ = 0.0f;
        juce::String outputFilePath_;
        juce::String url_;
        TrackMetadata metadata_;

        // ─── Child process ──────────────────────────────────────────────────
        std::unique_ptr<juce::ChildProcess> process_;
        juce::Time processStartTime_;
        int64_t lastOutputSize_ = 0;

        // ─── Temp directory ─────────────────────────────────────────────────
        juce::File tempDir_;

        static constexpr int kProgressCheckIntervalMs = 200;
        static constexpr int kMaxDownloadTimeSecs = 300; // 5 min timeout

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UrlDownloader)
    };

} // namespace mixcoach
