#include "UrlDownloader.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    UrlDownloader::UrlDownloader()
    {
        // Crear directorio temporal para descargas
        tempDir_ = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("MixCoach_URLDownloads");
        tempDir_.createDirectory();
    }

    UrlDownloader::~UrlDownloader()
    {
        cancel();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  isYtDlpAvailable — Busca yt-dlp en PATH o en ubicaciones conocidas
    // ═══════════════════════════════════════════════════════════════════════════
    bool UrlDownloader::isYtDlpAvailable()
    {
        return getYtDlpPath().isNotEmpty();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getYtDlpPath — Encuentra el ejecutable de yt-dlp
    //  Busca en:
    //    1. PATH del sistema (where/which yt-dlp)
    //    2. Directorio del ejecutable (bundled)
    //    3. Directorio de documentos MixCoach
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String UrlDownloader::getYtDlpPath()
    {
        // ─── 1. Buscar en PATH del sistema ──────────────────────────────────
        {
            juce::ChildProcess proc;
#ifdef JUCE_WINDOWS
            proc.start("where yt-dlp 2>nul");
#else
            proc.start("which yt-dlp 2>/dev/null");
#endif
            if (proc.waitForProcessToFinish(2000)) {
                juce::String output = proc.readAllProcessOutput().trim();
                if (output.isNotEmpty()) {
                    auto lines = juce::StringArray::fromLines(output);
                    if (lines.size() > 0) {
                        juce::File f(lines[0].trim());
                        if (f.existsAsFile()) return f.getFullPathName();
                    }
                }
            }
        }

        // ─── 2. Buscar bundled con el ejecutable ────────────────────────────
        {
            auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                              .getParentDirectory();
            auto bundled = exeDir.getChildFile("yt-dlp.exe");
            if (bundled.existsAsFile()) return bundled.getFullPathName();
            bundled = exeDir.getChildFile("yt-dlp");
            if (bundled.existsAsFile()) return bundled.getFullPathName();
        }

        // ─── 3. Buscar en Documents/MixCoach/Tools/ (instalación manual) ────
        {
            auto toolsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                .getChildFile("MixCoach")
                                .getChildFile("Tools");
            auto tools = toolsDir.getChildFile("yt-dlp.exe");
            if (tools.existsAsFile()) return tools.getFullPathName();
            tools = toolsDir.getChildFile("yt-dlp");
            if (tools.existsAsFile()) return tools.getFullPathName();
        }

        // ─── 4. Buscar en ubicaciones de package managers ─────────────────
        // Nota: winget instala yt-dlp en PATH, ya cubierto por el paso 1.
        // Aquí solo buscamos package managers que NO añaden al PATH por defecto.
#ifdef JUCE_WINDOWS
        {
            // scoop (Scoop package manager — a veces no añade al PATH)
            auto scoopDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                                .getChildFile("scoop")
                                .getChildFile("apps")
                                .getChildFile("yt-dlp")
                                .getChildFile("current");
            auto scoopExe = scoopDir.getChildFile("yt-dlp.exe");
            if (scoopExe.existsAsFile()) return scoopExe.getFullPathName();

            // chocolatey (paquete yt-dlp, shim en tools)
            auto chocoDir = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                                .getChildFile("chocolatey")
                                .getChildFile("lib")
                                .getChildFile("yt-dlp")
                                .getChildFile("tools");
            auto chocoExe = chocoDir.getChildFile("yt-dlp.exe");
            if (chocoExe.existsAsFile()) return chocoExe.getFullPathName();

            // pip user install (Windows: %APPDATA%\Python\Scripts)
            auto pipDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("Python")
                              .getChildFile("Scripts");
            auto pipExe = pipDir.getChildFile("yt-dlp.exe");
            if (pipExe.existsAsFile()) return pipExe.getFullPathName();

            // npm global install (Windows: %APPDATA%\npm)
            auto npmDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("npm");
            auto npmExe = npmDir.getChildFile("yt-dlp.exe");
            if (npmExe.existsAsFile()) return npmExe.getFullPathName();
        }
#else
        // macOS / Linux: package managers comunes
        {
            // Homebrew Intel
            auto brewDir = juce::File("/usr/local/bin");
            auto brewExe = brewDir.getChildFile("yt-dlp");
            if (brewExe.existsAsFile()) return brewExe.getFullPathName();
            // Homebrew Apple Silicon
            brewDir = juce::File("/opt/homebrew/bin");
            brewExe = brewDir.getChildFile("yt-dlp");
            if (brewExe.existsAsFile()) return brewExe.getFullPathName();
            // pip global
            auto pipDir = juce::File("/usr/local/bin");
            auto pipExe = pipDir.getChildFile("yt-dlp");
            if (pipExe.existsAsFile()) return pipExe.getFullPathName();
            // apt (Linux)
            auto aptDir = juce::File("/usr/bin");
            auto aptExe = aptDir.getChildFile("yt-dlp");
            if (aptExe.existsAsFile()) return aptExe.getFullPathName();
        }
#endif

        return {};
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  fetchMetadata — Extrae título, artista y duración usando yt-dlp --print
    //
    //  Ejecuta: yt-dlp --print title --print channel --print duration
    //           --print duration_string --skip-download URL
    //  Las 4 líneas de output se mapean a: title, channel, duration, durationString
    // ═══════════════════════════════════════════════════════════════════════════
    bool UrlDownloader::fetchMetadata(const juce::String& url)
    {
        juce::String ytDlpPath = getYtDlpPath();
        if (ytDlpPath.isEmpty()) {
            LogHelper::writeToLog("[UrlDownloader] fetchMetadata: yt-dlp no encontrado");
            return false;
        }

        juce::String urlTrimmed = url.trim();
        if (urlTrimmed.isEmpty()) {
            LogHelper::writeToLog("[UrlDownloader] fetchMetadata: URL vacia");
            return false;
        }

        // ─── Construir comando --print ────────────────────────────────────────
        // yt-dlp --print title --print channel --print duration
        //         --print duration_string --no-warnings --no-playlist --skip-download URL
        juce::String cmd = "\"" + ytDlpPath + "\"";
        cmd += " --print title";                           // Línea 1: título
        cmd += " --print channel";                         // Línea 2: artista/canal
        cmd += " --print duration";                        // Línea 3: duración en segundos
        cmd += " --print duration_string";                 // Línea 4: duración formateada (ej: "3:45")
        cmd += " --no-warnings";
        cmd += " --no-playlist";
        cmd += " --skip-download";                         // NO descargar, solo metadata
        cmd += " \"" + urlTrimmed + "\"";

        LogHelper::writeToLog("[UrlDownloader] Obteniendo metadata: " + cmd);

        // ─── Ejecutar yt-dlp sincrónicamente ─────────────────────────────────
        juce::ChildProcess proc;
        if (!proc.start(cmd)) {
            LogHelper::writeToLog("[UrlDownloader] fetchMetadata: no se pudo iniciar yt-dlp");
            return false;
        }

        if (!proc.waitForProcessToFinish(30000)) { // 30s timeout para metadata
            proc.kill();
            LogHelper::writeToLog("[UrlDownloader] fetchMetadata: timeout (30s)");
            return false;
        }

        // ─── Parsear output ──────────────────────────────────────────────────
        juce::String output = proc.readAllProcessOutput().trim();
        if (output.isEmpty()) {
            LogHelper::writeToLog("[UrlDownloader] fetchMetadata: output vacio");
            return false;
        }

        // Las primeras 4 líneas no vacías son: title, channel, duration, duration_string
        auto lines = juce::StringArray::fromLines(output);
        if (lines.size() < 4) {
            LogHelper::writeToLog("[UrlDownloader] fetchMetadata: formato inesperado ("
                                  + juce::String(lines.size()) + " lineas)");
            return false;
        }

        metadata_.title           = lines[0].trim();
        metadata_.channel         = lines[1].trim();
        metadata_.durationSeconds = lines[2].trim().getDoubleValue();
        metadata_.durationString  = lines[3].trim();
        metadata_.valid           = true;

        // ─── Duración formateada si yt-dlp no la devolvió ────────────────────
        if (metadata_.durationString.isEmpty() && metadata_.durationSeconds > 0.0) {
            int totalSecs = static_cast<int>(metadata_.durationSeconds);
            int mins = totalSecs / 60;
            int secs = totalSecs % 60;
            metadata_.durationString = juce::String(mins) + ":"
                                       + juce::String(secs).paddedLeft('0', 2);
        }

        LogHelper::writeToLog("[UrlDownloader] Metadata: \"" + metadata_.title
                              + "\" por " + metadata_.channel
                              + " (" + metadata_.durationString + ")");

        // ─── Notificar al callback ───────────────────────────────────────────
        if (onMetadataFetched) onMetadataFetched(metadata_);

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  sanitizeForFilename — Quita caracteres inválidos para Windows
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String UrlDownloader::sanitizeForFilename(const juce::String& s)
    {
        if (s.isEmpty()) return "desconocido";

        juce::String result = s.trim();
        const juce::String invalidChars = "<>:\"/\\|?*";
        for (int i = 0; i < result.length(); ++i) {
            if (invalidChars.containsChar(result[i])) {
                result = result.replaceSection(i, 1, "_");
            }
        }

        // Limitar longitud para evitar paths demasiado largos
        if (result.length() > 80)
            result = result.substring(0, 80).trim();

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  downloadURL — Inicia la descarga de audio desde una URL
    //
    //  FLUJO MEJORADO:
    //    1. Obtiene metadata (título, artista) usando --print antes de descargar
    //    2. Nombra el archivo como "Título - Artista.wav"
    //    3. El callback onMetadataFetched se dispara para que la UI muestre info
    //    4. El callback onProgress reporta el progreso real desde yt-dlp
    // ═══════════════════════════════════════════════════════════════════════════
    bool UrlDownloader::downloadURL(const juce::String& url)
    {
        if (isDownloading_) {
            LogHelper::writeToLog("[UrlDownloader] Ya hay una descarga en curso");
            return false;
        }

        juce::String ytDlpPath = getYtDlpPath();
        if (ytDlpPath.isEmpty()) {
            LogHelper::writeToLog("[UrlDownloader] ERROR: yt-dlp no encontrado");
            if (onComplete) onComplete({});
            return false;
        }

        url_ = url.trim();
        if (url_.isEmpty()) {
            LogHelper::writeToLog("[UrlDownloader] ERROR: URL vacia");
            if (onComplete) onComplete({});
            return false;
        }

        // ─── Paso 1: Obtener metadata si no está cacheada ────────────────────
        metadata_.valid = false; // Reset para forzar refetch
        fetchMetadata(url_);

        // ─── Paso 2: Preparar directorio temporal ────────────────────────────
        tempDir_.deleteRecursively();
        tempDir_.createDirectory();

        // ─── Paso 3: Generar nombre de archivo con metadata ──────────────────
        if (metadata_.valid && metadata_.title.isNotEmpty()) {
            juce::String safeTitle   = sanitizeForFilename(metadata_.title);
            juce::String safeChannel = sanitizeForFilename(metadata_.channel);
            outputFilePath_ = tempDir_.getChildFile(safeTitle + " - " + safeChannel + ".wav").getFullPathName();
            LogHelper::writeToLog("[UrlDownloader] Archivo nombrado con metadata: \""
                                  + safeTitle + " - " + safeChannel + ".wav\"");
        } else {
            // Fallback: timestamp si no hay metadata
            juce::String timestamp = juce::String(juce::Time::getCurrentTime().toMilliseconds());
            outputFilePath_ = tempDir_.getChildFile("reference_" + timestamp + ".wav").getFullPathName();
            LogHelper::writeToLog("[UrlDownloader] Sin metadata, usando timestamp");
        }

        // ─── Paso 4: Construir comando yt-dlp ────────────────────────────────
        // yt-dlp -x --audio-format wav --audio-quality 0 -o "Song - Artist.wav" --no-playlist URL
        juce::String cmd = "\"" + ytDlpPath + "\"";
        cmd += " -x";                                    // Extraer audio
        cmd += " --audio-format wav";                     // Convertir a WAV
        cmd += " --audio-quality 0";                      // Mejor calidad
        cmd += " -o \"" + outputFilePath_ + "\"";         // Output path con metadata
        cmd += " --no-playlist";                          // No descargar playlist
        cmd += " --no-warnings";                          // Menos output
        cmd += " --progress";                             // Mostrar progreso
        cmd += " \"" + url_ + "\"";                       // URL

        LogHelper::writeToLog("[UrlDownloader] Iniciando descarga: " + cmd);

        // Iniciar proceso
        process_ = std::make_unique<juce::ChildProcess>();
        if (!process_->start(cmd)) {
            LogHelper::writeToLog("[UrlDownloader] ERROR: No se pudo iniciar yt-dlp");
            process_ = nullptr;
            if (onComplete) onComplete({});
            return false;
        }

        isDownloading_ = true;
        progress_ = 0.0f;
        processStartTime_ = juce::Time::getCurrentTime();
        lastOutputSize_ = 0;

        // Iniciar timer para monitorear progreso
        startTimer(kProgressCheckIntervalMs);

        LogHelper::writeToLog("[UrlDownloader] Descarga iniciada para: " + url_);
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  cancel — Cancela la descarga en curso
    // ═══════════════════════════════════════════════════════════════════════════
    void UrlDownloader::cancel()
    {
        if (!isDownloading_) return;

        stopTimer();

        if (process_ != nullptr) {
            process_->kill();
            process_ = nullptr;
        }

        // Limpiar archivo parcial
        juce::File(outputFilePath_).deleteFile();

        isDownloading_ = false;
        progress_ = 0.0f;
        LogHelper::writeToLog("[UrlDownloader] Descarga cancelada");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Monitorea el progreso de la descarga
    // ═══════════════════════════════════════════════════════════════════════════
    void UrlDownloader::timerCallback()
    {
        if (!isDownloading_ || process_ == nullptr) {
            stopTimer();
            return;
        }

        // Timeout check
        auto elapsed = juce::Time::getCurrentTime() - processStartTime_;
        if (elapsed.inSeconds() > kMaxDownloadTimeSecs) {
            LogHelper::writeToLog("[UrlDownloader] Timeout: descarga excedio "
                                  + juce::String(kMaxDownloadTimeSecs) + "s");
            failDownload("Timeout de descarga");
            return;
        }

        // Verificar si el proceso terminó
        if (!process_->isRunning()) {
            int exitCode = process_->getExitCode();
            process_ = nullptr;
            stopTimer();

            if (exitCode == 0) {
                // Verificar que el archivo existe
                juce::File outputFile(outputFilePath_);
                if (outputFile.existsAsFile() && outputFile.getSize() > 1024) {
                    LogHelper::writeToLog("[UrlDownloader] Descarga completada: "
                                          + outputFilePath_ + " ("
                                          + juce::String(outputFile.getSize() / 1024) + " KB)");
                    progress_ = 1.0f;
                    if (onProgress) onProgress(1.0f);
                    finishDownload(outputFilePath_);
                } else {
                    LogHelper::writeToLog("[UrlDownloader] ERROR: Archivo no encontrado o muy pequeno");
                    failDownload("Archivo descargado no valido");
                }
            } else {
                LogHelper::writeToLog("[UrlDownloader] ERROR: yt-dlp exit code " + juce::String(exitCode));
                failDownload("yt-dlp termino con error (codigo " + juce::String(exitCode) + ")");
            }
            return;
        }

        // Leer output del proceso para estimar progreso
        checkProcessOutput();

        // Estimar progreso basado en tamaño del archivo parcial
        juce::File partialFile(outputFilePath_);
        if (partialFile.existsAsFile()) {
            int64_t fileSize = partialFile.getSize();
            // Asumimos que un archivo de referencia típico es ~10-30 MB
            // Usamos 20 MB como estimación para el progreso
            constexpr int64_t kEstimatedSize = 20 * 1024 * 1024;
            float fileProgress = juce::jmin(1.0f, (float)fileSize / (float)kEstimatedSize);
            progress_ = juce::jmax(progress_, fileProgress * 0.9f);
        } else {
            // Sin archivo aún, progreso basado en tiempo
            float timeProgress = juce::jmin(0.9f, (float)elapsed.inSeconds() / 30.0f);
            progress_ = juce::jmax(progress_, timeProgress * 0.3f);
        }

        if (onProgress) onProgress(progress_);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  checkProcessOutput — Lee el output de yt-dlp para extraer progreso
    // ═══════════════════════════════════════════════════════════════════════════
    void UrlDownloader::checkProcessOutput()
    {
        if (process_ == nullptr) return;

        // Leer todo el output disponible (sin bloqueo)
        juce::String output = process_->readAllProcessOutput();
        if (output.isEmpty()) return;

        // Buscar líneas de progreso tipo: "[download]  45.2% of ~15.34MiB"
        auto lines = juce::StringArray::fromLines(output);
        for (auto& line : lines) {
            line = line.trim();
            if (line.contains("[download]") && line.contains("%")) {
                // Extraer porcentaje
                auto percentStr = line.fromFirstOccurrenceOf("[download]", false, false).trim();
                auto endPct = percentStr.indexOf("%");
                if (endPct > 0) {
                    juce::String pct = percentStr.substring(0, endPct).trim();
                    float pctVal = pct.getFloatValue();
                    if (pctVal > 0.0f && pctVal <= 100.0f) {
                        progress_ = pctVal / 100.0f;
                        // Loggear cada ~10%
                        int pctInt = (int)pctVal;
                        if (pctInt % 10 == 0 && pctInt > (int)(lastOutputSize_ / 1000)) {
                            LogHelper::writeToLog("[UrlDownloader] Progreso: " + juce::String(pctInt) + "%");
                            lastOutputSize_ = pctInt * 1000LL;
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  finishDownload — Completar descarga exitosamente
    // ═══════════════════════════════════════════════════════════════════════════
    void UrlDownloader::finishDownload(const juce::String& filePath)
    {
        isDownloading_ = false;
        process_ = nullptr;
        stopTimer();

        if (onComplete) onComplete(filePath);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  failDownload — Manejar error de descarga
    // ═══════════════════════════════════════════════════════════════════════════
    void UrlDownloader::failDownload(const juce::String& error)
    {
        isDownloading_ = false;
        process_ = nullptr;
        stopTimer();

        // Limpiar archivo parcial
        juce::File(outputFilePath_).deleteFile();

        LogHelper::writeToLog("[UrlDownloader] Error: " + error);

        if (onComplete) onComplete({}); // Empty = error
    }

} // namespace mixcoach
