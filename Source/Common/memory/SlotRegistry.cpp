#include "SlotRegistry.h"
#include "../types/Constants.h"
#include "../types/LogHelper.h"
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>
#include <atomic>

namespace mixcoach {

// ─── Logging de diagnóstico ───────────────────────────────────────────────────
static void logSlot(const juce::String& action, int slotIndex, const juce::String& detail = {})
{
    auto msg = "[SlotRegistry] " + action + " slot=" + juce::String(slotIndex);
    if (detail.isNotEmpty())
        msg += " | " + detail;
    LogHelper::writeToLog(msg);
}

namespace {

float readBackupFloat(juce::FileInputStream& fis)
{
    const int raw = fis.readIntBigEndian();
    float val = 0.0f;
    std::memcpy(&val, &raw, sizeof(val));
    return val;
}

TrackTelemetry buildTelemetryFromSharedEntry(const SharedSlotEntry& entry, int slotIndex)
{
    TrackTelemetry telem;
    telem.timestamp    = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
    telem.slotIndex    = slotIndex;
    telem.active       = true;
    telem.peakLeft     = entry.peakLeft;
    telem.peakRight    = entry.peakRight;
    telem.rmsLeft      = entry.rmsLeft;
    telem.rmsRight     = entry.rmsRight;
    telem.correlation  = entry.correlation;
    telem.crestFactor  = entry.crestFactor;
    telem.sampleL      = entry.sampleL;
    telem.sampleR      = entry.sampleR;

    if (entry.fftTimestamp > 0)
    {
        const auto now = juce::Time::getMillisecondCounter();
        const auto fftAge = now - static_cast<int64_t>(entry.fftTimestamp / 1000);
        if (fftAge < 500)
        {
            std::copy(std::begin(entry.fftMagnitudes),
                      std::end(entry.fftMagnitudes),
                      std::begin(telem.spectrum));
        }
    }

    telem.lufsIntegrated = entry.lufsIntegrated;
    telem.lufsShortTerm  = entry.lufsShortTerm;
    telem.lufsMomentary  = entry.lufsMomentary;
    telem.lufsTruePeak   = entry.lufsTruePeak;
    telem.loudnessRange  = entry.loudnessRange;
    return telem;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
//  File-based backup helpers
//  Cada slot se guarda en: %%TEMP%%/MixCoach_SlotBackup/slot_N.bin
//  Formato: [magic:4] [version:4] [slotIndex:4] [active:4] [bus:4] [colourARGB:4] [trackName:64]
// ═══════════════════════════════════════════════════════════════════════════
static constexpr uint32_t kSlotFileMagic   = 0x4D534C54; // "MSLT"
static constexpr uint32_t kSlotFileVersion = 2; // V2 incluye telemetría (peaks, RMS, LUFS)
static constexpr uint32_t kSlotFileVersionV1 = 1; // Legacy (solo registro)

static juce::File getSlotBackupDir()
{
    // ═══ FIX: Usar %LOCALAPPDATA% en vez de %TEMP% ═══════════════════
    // %TEMP% puede ser volátil o no existir en algunos contextos de VST3.
    // %LOCALAPPDATA% es persistente, siempre accesible, y no se limpia
    // automáticamente. Usamos la subcarpeta MixCoach/SlotBackup.
    //
    // En Windows: C:\Users\<usuario>\AppData\Local\MixCoach\SlotBackup
    // En macOS:   ~/Library/Application Support/MixCoach/SlotBackup
    // En Linux:   ~/.local/share/MixCoach/SlotBackup
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MixCoach")
        .getChildFile("SlotBackup");
}

static juce::File getSlotBackupFile(int slotIndex)
{
    return getSlotBackupDir().getChildFile("slot_" + juce::String(slotIndex) + ".bin");
}

// ─── Archivo marcador de metadatos (slot_N.meta) ────────────────────────────
// Este archivo es la CLAVE para latencia instantánea. Es un archivo de 0 bytes
// que SOLO se actualiza cuando cambian los METADATOS (nombre/color/bus).
// NUNCA se actualiza por telemetría. Así pollTelemetryFromBackups() puede
// verificar si hubo cambios de metadatos con solo un stat() del meta file,
// SIN abrir el backup file (que cambia constantemente por telemetría ~32ms).
//
// Flujo:
//   1. saveSlotToBackupFile() escribe backup file + TOUCH meta file
//   2. pollTelemetryFromBackups() checkea meta file modTime primero
//   3. Si meta file no cambió → skip (0 I/O real, solo stat())
//   4. Si meta file cambió → abrir y leer backup file completo
static juce::File getSlotBackupMetaFile(int slotIndex)
{
    return getSlotBackupDir().getChildFile("slot_" + juce::String(slotIndex) + ".meta");
}

// ─── Offset de campos de telemetría en archivo V2 ───────────────────────────
// V2 layout: [magic:4] [version:4] [slotIndex:4] [active:4] [bus:4]
//            [colourARGB:4] [trackName:64]
//            [peakLeft:4] [peakRight:4] [rmsLeft:4] [rmsRight:4]
//            [correlation:4] [crestFactor:4] [sampleL:4] [sampleR:4]
//            [lufsIntegrated:4] [lufsShortTerm:4]
//            [lufsMomentary:4] [lufsTruePeak:4] [loudnessRange:4]
// V1 size = 88, V2 size = 88 + 72 = 160
static constexpr int kSlotFileV1Size = 88;
static constexpr int kSlotFileV2Size = 140; // 88 (V1) + 13*4 (telemetría)
static constexpr int kSlotFileTelemetryOffset = 88; // empieza después de V1

static_assert(kSlotFileV1Size == 8 + 4 + 4 + 4 + 4 + 64, "V1 size mismatch");
static_assert(kSlotFileV2Size == kSlotFileV1Size + 13 * 4, "V2 size mismatch");

void SlotRegistry::saveSlotToBackupFile(int slotIndex, const SlotInfo& info)
{
    if (slotIndex < 0 || slotIndex >= kMaxSlots) return;

    auto dir = getSlotBackupDir();
    if (!dir.exists())
        dir.createDirectory();

    // Escribir a archivo temporal primero, luego renombrar (atómico en Windows)
    auto tmpFile = dir.getChildFile("_tmp_" + juce::String(slotIndex) + ".bin");
    auto finalFile = getSlotBackupFile(slotIndex);

    {
        juce::FileOutputStream fos(tmpFile);
        if (!fos.openedOk()) return;

        // ─── V2: Campos de registro (compatibles con V1) ───────────────────────
        fos.writeIntBigEndian(static_cast<int>(kSlotFileMagic));
        fos.writeIntBigEndian(static_cast<int>(kSlotFileVersion));
        fos.writeIntBigEndian(info.slotIndex);
        fos.writeIntBigEndian(info.active ? 1 : 0);
        fos.writeIntBigEndian(static_cast<int>(info.bus));
        fos.writeIntBigEndian(static_cast<int>(info.colour.getARGB()));

        // trackName: hasta 64 bytes, relleno con ceros
        size_t nameLen = strnlen_s(info.trackName, 64);
        fos.write(info.trackName, static_cast<int>(nameLen));
        for (size_t i = nameLen; i < 64; ++i)
            fos.writeByte(0);

        // ─── V2: Campos de telemetría (valores por defecto si no hay datos) ────
        // Estos se actualizan via updateSlotBackupTelemetry() desde el audio thread
        auto writeFloat = [&](float val) {
            int intVal;
            std::memcpy(&intVal, &val, sizeof(intVal));
            fos.writeIntBigEndian(intVal);
        };
        auto zero = 0.0f;
        auto neg100 = -100.0f;
        writeFloat(neg100); // peakLeft
        writeFloat(neg100); // peakRight
        writeFloat(neg100); // rmsLeft
        writeFloat(neg100); // rmsRight
        writeFloat(1.0f);  // correlation
        writeFloat(zero);  // crestFactor
        writeFloat(zero);  // sampleL
        writeFloat(zero);  // sampleR
        writeFloat(neg100); // lufsIntegrated
        writeFloat(neg100); // lufsShortTerm
        writeFloat(neg100); // lufsMomentary
        writeFloat(neg100); // lufsTruePeak
        writeFloat(zero);  // loudnessRange

        fos.flush();
    }

    // Renombrar atómicamente
    if (finalFile.exists())
        finalFile.deleteFile();
    tmpFile.moveFileTo(finalFile);

    // ─── Touch meta file (marcador de cambio de metadatos) ───────────────
    // Escribir un solo byte para que el archivo exista con un timestamp
    // actualizado. pollTelemetryFromBackups() checkea este archivo para
    // saber si necesita releer el backup file.
    auto metaFile = getSlotBackupMetaFile(slotIndex);
    juce::FileOutputStream metaFos(metaFile);
    if (metaFos.openedOk()) {
        metaFos.writeByte(0);  // 1 byte, cualquier valor — solo importa el timestamp
        metaFos.flush();
    }
}

// ─── Actualizar SOLO telemetría en backup file existente ───────────────────
// Esta función se llama DESDE EL AUDIO THREAD (processBlock del Messenger).
// El archivo se abre en modo append-overwrite: solo se sobreescriben los
// bytes de telemetría (offset 88 en adelante), sin tocar nombre/color/bus.
// Esto minimiza el tiempo de I/O en el audio thread.
void SlotRegistry::updateSlotBackupTelemetry(int slotIndex,
                                               float peakLeft, float peakRight,
                                               float rmsLeft, float rmsRight,
                                               float correlation, float crestFactor,
                                               float sampleL, float sampleR,
                                               float lufsIntegrated, float lufsShortTerm,
                                               float lufsMomentary, float lufsTruePeak,
                                               float loudnessRange)
{
    if (slotIndex < 0 || slotIndex >= kMaxSlots) return;

    // Verificar que el archivo exista y tenga el tamaño V2 correcto
    auto file = getSlotBackupFile(slotIndex);
    if (!file.existsAsFile())
        return; // Backup no existe (aún no registrado) — no podemos actualizar

    if (file.getSize() < static_cast<juce::int64>(kSlotFileV2Size))
        return; // Tamaño incorrecto (no tiene telemetría V2)

    // Abrir el archivo en modo lectura/escritura y saltar directamente al
    // offset de telemetría.
    juce::FileOutputStream fos(file);
    if (!fos.openedOk()) return;

    // Mover el cursor al offset donde empiezan los campos de telemetría
    if (!fos.setPosition(kSlotFileTelemetryOffset))
        return;

    // Escribir cada campo de telemetría en orden secuencial
    auto writeFloat = [&](float val) {
        int intVal;
        std::memcpy(&intVal, &val, sizeof(intVal));
        fos.writeIntBigEndian(intVal);
    };

    writeFloat(peakLeft);
    writeFloat(peakRight);
    writeFloat(rmsLeft);
    writeFloat(rmsRight);
    writeFloat(correlation);
    writeFloat(crestFactor);
    writeFloat(sampleL);
    writeFloat(sampleR);
    writeFloat(lufsIntegrated);
    writeFloat(lufsShortTerm);
    writeFloat(lufsMomentary);
    writeFloat(lufsTruePeak);
    writeFloat(loudnessRange);

    fos.flush();
}

void SlotRegistry::removeSlotBackupFile(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kMaxSlots) return;
    auto file = getSlotBackupFile(slotIndex);
    if (file.exists())
        file.deleteFile();
    auto meta = getSlotBackupMetaFile(slotIndex);
    if (meta.exists())
        meta.deleteFile();
}

int SlotRegistry::loadSlotsFromBackupFiles(bool forceOverwrite)
{
    auto dir = getSlotBackupDir();
    if (!dir.exists()) return 0;

    juce::Array<juce::File> files;
    dir.findChildFiles(files, juce::File::findFiles, false, "slot_*.bin");

    int found = 0;
    for (auto& file : files)
    {
        juce::FileInputStream fis(file);
        if (!fis.openedOk()) continue;

        auto magic = static_cast<uint32_t>(fis.readIntBigEndian());
        if (magic != kSlotFileMagic) continue;

        auto version = static_cast<uint32_t>(fis.readIntBigEndian());
        if (version != kSlotFileVersion && version != kSlotFileVersionV1) continue;

        int slotIndex  = fis.readIntBigEndian();
        int active     = fis.readIntBigEndian();
        int bus        = fis.readIntBigEndian();
        auto colourARGB = static_cast<uint32_t>(fis.readIntBigEndian());

        char trackName[64] = {0};
        int bytesRead = fis.read(trackName, 64);
        if (bytesRead < 0) continue;

        if (active && slotIndex >= 0 && slotIndex < kMaxSlots)
        {
            auto& local = slots_[slotIndex];
            bool wasActive = local.active;

            // ─── SIEMPRE sobrescribir si forceOverwrite está activo ───────
            // Esto es CRÍTICO: aunque el slot ya esté activo (por shared memory),
            // los backup files tienen datos MÁS RECIENTES (telemetría actualizada
            // en cada processBlock del Messenger) y deben prevalecer.
            if (!wasActive || forceOverwrite)
            {
                local.slotIndex = slotIndex;
                local.active    = true;
                local.bus       = static_cast<BusType>(bus);
                local.colour    = juce::Colour(colourARGB);
                strncpy_s(local.trackName, sizeof(local.trackName), trackName, _TRUNCATE);

                // ─── V2: Leer telemetría si está disponible ───────────────
                if (version == kSlotFileVersion)
                {
                    auto readFloat = [&]() -> float {
                        if (fis.getNumBytesRemaining() < 4) return -100.0f;
                        int intVal = fis.readIntBigEndian();
                        float result;
                        std::memcpy(&result, &intVal, sizeof(result));
                        return result;
                    };

                    float peakL   = readFloat();
                    float peakR   = readFloat();
                    float rmsL    = readFloat();
                    float rmsR    = readFloat();
                    float corr    = readFloat();
                    float crest   = readFloat();
                    float sL      = readFloat();
                    float sR      = readFloat();
                    float lufsInt = readFloat();
                    float lufsST  = readFloat();
                    float lufsMom = readFloat();
                    float lufsTP  = readFloat();
                    float lr      = readFloat();

                    // Push telemetría al buffer local para que la UI la vea
                    TrackTelemetry telem;
                    telem.timestamp    = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
                    telem.slotIndex    = slotIndex;
                    telem.active       = true;
                    telem.peakLeft     = peakL;
                    telem.peakRight    = peakR;
                    telem.rmsLeft      = rmsL;
                    telem.rmsRight     = rmsR;
                    telem.correlation  = corr;
                    telem.crestFactor  = crest;
                    telem.sampleL      = sL;
                    telem.sampleR      = sR;
                    telem.lufsIntegrated = lufsInt;
                    telem.lufsShortTerm  = lufsST;
                    telem.lufsMomentary  = lufsMom;
                    telem.lufsTruePeak   = lufsTP;
                    telem.loudnessRange  = lr;
                    telemetry_[slotIndex].push(telem);
                }

                ++found;

                if (!wasActive)
                {
                    logSlot("BACKUP_LOAD_NEW", slotIndex,
                        "name=" + juce::String(local.trackName)
                        + " bus=" + juce::String(static_cast<int>(local.bus))
                        + " colour=" + local.colour.toDisplayString(false));
                    if (onSlotRegistered) onSlotRegistered(slotIndex);
                }
                else
                {
                    logSlot("BACKUP_LOAD_OVERWRITE", slotIndex,
                        "name=" + juce::String(local.trackName)
                        + " | forceOverwrite=true");
                    if (onSlotChanged) onSlotChanged(slotIndex);
                }
            }
        }
    }

    if (found > 0)
    {
        logSlot("BACKUP_LOAD_DONE", -1, "found=" + juce::String(found)
            + " forceOverwrite=" + (forceOverwrite ? "SI" : "NO"));
        localChangeCount_ += found;
        everSynced_ = true;
    }

    return found;
}

// ═══════════════════════════════════════════════════════════════════════════
//  End of file-based backup helpers
// ═══════════════════════════════════════════════════════════════════════════


const juce::Colour SlotRegistry::kSlotColours[8] = {
    juce::Colour(0xFFFF0000), // red
    juce::Colour(0xFF0000FF), // blue
    juce::Colour(0xFF00FF00), // green
    juce::Colour(0xFFFFA500), // orange
    juce::Colour(0xFF800080), // purple
    juce::Colour(0xFF00FFFF), // cyan
    juce::Colour(0xFFFFFF00), // yellow
    juce::Colour(0xFFFF00FF)  // magenta
};

SlotRegistry::SlotRegistry()
    : nextSlot_{0}
{
    for (auto& slot : slots_) {
        slot.slotIndex = -1;
        slot.active    = false;
    }
}

// ─── Helpers de shared memory ────────────────────────────────────────────────

void SlotRegistry::readFromShared(int slotIndex)
{
    if (shm_ == nullptr || slotIndex < 0 || slotIndex >= kMaxSlots)
        return;

    SharedSlotEntry entry;
    if (!shm_->readSlot(slotIndex, entry))
        return;

    auto& local = slots_[slotIndex];
    local.slotIndex = entry.slotIndex;
    local.active    = entry.active != 0;
    local.bus       = static_cast<BusType>(entry.bus);
    local.colour    = juce::Colour(entry.colourARGB);
    strncpy_s(local.trackName, sizeof(local.trackName), entry.trackName, _TRUNCATE);
}

bool SlotRegistry::syncFromShared()
{
    if (shm_ == nullptr) return false;

    auto sharedCC = shm_->getChangeCount();
    if (sharedCC == lastSharedChangeCount_)
        return false; // Sin cambios

    // Actualizar changeCount local al valor compartido
    lastSharedChangeCount_ = sharedCC;
    localChangeCount_ = sharedCC;
    everSynced_ = true;

    // Leer todos los slots activos desde shared memory
    for (int i = 0; i < kMaxSlots; ++i) {
        SharedSlotEntry entry;
        if (shm_->readSlot(i, entry) && entry.active) {
            // ─── Slot activo en shared memory ────────────────────────────
            auto& local = slots_[i];
            bool wasActive = local.active;

            // ═══ CRITICAL FIX: Preservar bus/name/color del backup ═══════
            // Misma lógica que forceFullSyncFromShm(): si el slot ya tenía
            // datos válidos de backup y shared memory tiene BusType::None,
            // NO sobreescribir los metadatos. Solo actualizar telemetría.
            bool hasValidBackup = wasActive && local.bus != BusType::None;
            bool shmHasValidBus = static_cast<BusType>(entry.bus) != BusType::None;

            local.slotIndex = entry.slotIndex;
            local.active    = true;

            bool metadataChanged = false;

            if (shmHasValidBus || !hasValidBackup)
            {
                metadataChanged = wasActive &&
                    (local.bus != static_cast<BusType>(entry.bus) ||
                     local.colour.getARGB() != entry.colourARGB ||
                     std::strncmp(local.trackName, entry.trackName, sizeof(local.trackName)) != 0);

                local.bus       = static_cast<BusType>(entry.bus);
                local.colour    = juce::Colour(entry.colourARGB);
                strncpy_s(local.trackName, sizeof(local.trackName), entry.trackName, _TRUNCATE);
            }
            // else: preservar bus/name/color del backup (más confiable)

            telemetry_[i].push(buildTelemetryFromSharedEntry(entry, i));

        } else if (slots_[i].active) {
            // Slot liberado en shared memory
            logSlot("SYNC_RELEASED", i, "name=" + juce::String(slots_[i].trackName));
            slots_[i].active = false;
            slots_[i].slotIndex = -1;
            slots_[i].trackName[0] = '\0';
            if (onSlotReleased) onSlotReleased(i);
        }
    }

    return true;
}

void SlotRegistry::pollTelemetryFromShared()
{
    if (shm_ == nullptr)
        return;

    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (! slots_[i].active)
            continue;

        SharedSlotEntry entry;
        if (! shm_->readSlot(i, entry) || ! entry.active)
            continue;

        telemetry_[i].push(buildTelemetryFromSharedEntry(entry, i));
    }
}

void SlotRegistry::pollTelemetryFromBackups()
{
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (! slots_[i].active)
            continue;

        auto file = getSlotBackupFile(i);
        if (! file.existsAsFile()
            || file.getSize() < static_cast<juce::int64>(kSlotFileV2Size))
            continue;

        // ═══ Check META file (slot_N.meta) — solo cambia con metadatos ═══
        // El backup file (slot_N.bin) se actualiza CADA ~32ms por telemetría
        // (updateSlotBackupTelemetry desde audio thread). No podemos usarlo
        // para detectar cambios de metadatos.
        // El meta file SOLO se actualiza cuando saveSlotToBackupFile() escribe
        // metadatos nuevos (nombre/color/bus). Si no cambió, skip total.
        // Esto significa 0 I/O real en ~59 de cada 60 frames.
        //
        // Fallback: si el meta file no existe (migración desde versión anterior),
        // sigue leyendo el backup file completo para no congelar la telemetría.
        bool metaExists = false;
        int64_t metaMod = 0;
        auto metaFile = getSlotBackupMetaFile(i);
        if (metaFile.existsAsFile()) {
            metaExists = true;
            metaMod = metaFile.getLastModificationTime().toMilliseconds();
            if (metaMod > 0 && metaMod == lastMetaModTimeMs_[i])
                continue;  // Metadatos no cambiaron — skip (0 I/O real)
        }
        // else: sin meta file (migración) → leer backup completo (legacy)

        juce::FileInputStream fis(file);
        if (! fis.openedOk())
            continue;

        // ─── Leer METADATOS (inicio del archivo) ────────────────────────
        // CRÍTICO: Cuando el Messenger cambia nombre/color/bus, escribe el
        // backup file completo (via saveSlotToBackupFile). Pero pollTelemetry-
        // FromBackups() solo leía telemetría (offset 88), ignorando metadatos.
        // MixCoach NUNCA se enteraba de los cambios hasta reiniciar.
        auto magic = static_cast<uint32_t>(fis.readIntBigEndian());
        if (magic != kSlotFileMagic)
            continue;

        auto ver = static_cast<uint32_t>(fis.readIntBigEndian());
        if (ver != kSlotFileVersion && ver != kSlotFileVersionV1)
            continue;

        int slotIdx = fis.readIntBigEndian();
        int active  = fis.readIntBigEndian();
        int bus     = fis.readIntBigEndian();
        auto colourARGB = static_cast<uint32_t>(fis.readIntBigEndian());

        char trackName[64] = {0};
        int bytesRead = fis.read(trackName, 64);
        if (bytesRead < 0) continue;

        if (slotIdx != i || !active)
            continue;

        // ─── Actualizar metadatos en slots_[i] ─────────────────────────
        {
            auto& local = slots_[i];
            bool changed = false;

            if (local.bus != static_cast<BusType>(bus)) {
                local.bus = static_cast<BusType>(bus);
                changed = true;
            }
            if (local.colour.getARGB() != colourARGB) {
                local.colour = juce::Colour(colourARGB);
                changed = true;
            }
            if (std::strncmp(local.trackName, trackName, sizeof(local.trackName)) != 0) {
                strncpy_s(local.trackName, sizeof(local.trackName), trackName, _TRUNCATE);
                changed = true;
            }

            if (changed && onSlotChanged)
                onSlotChanged(i);
        }

        // ─── Saltar a telemetría y leer ─────────────────────────────────
        if (! fis.setPosition(kSlotFileTelemetryOffset))
            continue;

        TrackTelemetry telem;
        telem.timestamp   = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        telem.slotIndex   = i;
        telem.active      = true;
        telem.peakLeft    = readBackupFloat(fis);
        telem.peakRight   = readBackupFloat(fis);
        telem.rmsLeft     = readBackupFloat(fis);
        telem.rmsRight    = readBackupFloat(fis);
        telem.correlation = readBackupFloat(fis);
        telem.crestFactor = readBackupFloat(fis);
        telem.sampleL     = readBackupFloat(fis);
        telem.sampleR     = readBackupFloat(fis);
        telem.lufsIntegrated = readBackupFloat(fis);
        telem.lufsShortTerm  = readBackupFloat(fis);
        telem.lufsMomentary  = readBackupFloat(fis);
        telem.lufsTruePeak   = readBackupFloat(fis);
        telem.loudnessRange  = readBackupFloat(fis);

        telemetry_[i].push(telem);

        // Cachear timestamp del meta file (si existe) para saltar en el
        // próximo poll. El meta file SOLO cambia con metadatos, así que
        // saltamos hasta que el usuario vuelva a cambiar nombre/color/bus.
        if (metaExists)
            lastMetaModTimeMs_[i] = metaMod;
    }
}

// ─── Sincronización forzada completa (ignora changeCount) ───────────────────────
// Llamar cuando MixCoach se conecta por primera vez a shared memory que
// ya puede tener Messengers activos (ignora lastSharedChangeCount_).
//
// Estrategia DUAL:
//   1. Sincronizar desde shared memory (si disponible)
//   2. SIEMPRE cargar backup files adicionalmente
//      → Los backup files contienen slots que algún Messenger registró,
//        incluso si el Messenger ya no está activo o la shared memory
//        entre DLLs separadas falló.
//      → loadSlotsFromBackupFiles() solo carga slots que NO están activos
//        localmente, por lo que no hay riesgo de duplicados.
int SlotRegistry::forceFullSync()
{
    int totalFound = 0;

    // 1. SIEMPRE cargar desde archivos de backup primero
    //    Esto es CRÍTICO para detectar Messengers cuya shared memory
    //    entre DLLs separadas (MixCoach.vst3 / Messenger.vst3) falló,
    //    pero que sí escribieron su archivo de backup.
    //    Con forceOverwrite=true, los datos de backup (nombre, color, bus,
    //    y telemetría) SIEMPRE prevalecen sobre shared memory.
    //    Esto asegura que MixCoach siempre tenga los datos más recientes
    //    independientemente del estado de la memoria compartida.
    int foundFromBackup = loadSlotsFromBackupFiles(true);
    totalFound += foundFromBackup;

    // 2. Sincronizar desde shared memory DESPUÉS del backup
    //    ⚠ ORDEN CRÍTICO: forceFullSyncFromShm() DEBE correr al final porque
    //    incluye datos FFT que NO están en los backup files. Si el backup
    //    corre después, pushea un TrackTelemetry sin FFT (todos ceros) que
    //    SOBREESCRIBE el entry bueno de shared memory, dejando el spectrograph
    //    sin datos (negro).
    //    
    //    El backup es importante para metadatos (nombre, color) pero NUNCA
    //    debe sobreescribir telemetría fresca de shared memory.
    if (shm_ != nullptr) {
        int foundFromShm = forceFullSyncFromShm();
        totalFound += foundFromShm;
    }

    return totalFound;
}

// ─── forceFullSync interno (solo shared memory) ────────────────────────────
int SlotRegistry::forceFullSyncFromShm()
{
    if (shm_ == nullptr) return 0;

    int found = 0;
    for (int i = 0; i < kMaxSlots; ++i)
    {
        SharedSlotEntry entry;
        if (!shm_->readSlot(i, entry)) continue;

        if (entry.active)
        {
            auto& local = slots_[i];
            bool wasActive = local.active;

            // ═══ CRITICAL FIX: Preservar bus/name/color de backup files ═══
            // La shared memory entre DLLs separadas (MixCoach.vst3 ↔ Messenger.vst3)
            // NO es confiable. Frecuentemente devuelve BusType::None (-1) incluso
            // cuando el Messenger registró un bus válido. Los backup files son el
            // canal CONFIABLE para metadatos (nombre, color, bus).
            //
            // Si el slot ya está activo (cargado desde backup) y shared memory
            // tiene BusType::None, PRESERVAMOS los datos del backup.
            // Solo sobreescribimos si shared memory tiene un bus VÁLIDO (≥0).
            bool hasValidBackup = wasActive && local.bus != BusType::None;
            bool shmHasValidBus = static_cast<BusType>(entry.bus) != BusType::None;

            local.slotIndex = entry.slotIndex;
            local.active    = true;

            if (shmHasValidBus || !hasValidBackup)
            {
                local.bus       = static_cast<BusType>(entry.bus);
                local.colour    = juce::Colour(entry.colourARGB);
                strncpy_s(local.trackName, sizeof(local.trackName), entry.trackName, _TRUNCATE);
            }
            // else: preservar bus/name/color del backup (más confiable)

            // Sincronizar telemetría
            TrackTelemetry telem;
            telem.timestamp   = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
            telem.slotIndex   = entry.slotIndex;
            telem.active      = true;
            telem.peakLeft    = entry.peakLeft;
            telem.peakRight   = entry.peakRight;
            telem.rmsLeft     = entry.rmsLeft;
            telem.rmsRight    = entry.rmsRight;
            telem.correlation = entry.correlation;
            telem.crestFactor = entry.crestFactor;
            telem.sampleL     = entry.sampleL;
            telem.sampleR     = entry.sampleR;

            // ═══ FIX: Copiar datos FFT desde shared memory ═══════════════
            // Si el Messenger escribió datos FFT recientes (< 500ms), copiarlos
            // al TelemetryBuffer local para que el spectrograph los muestre.
            if (entry.fftTimestamp > 0) {
                auto now = juce::Time::getMillisecondCounter();
                auto fftAge = now - static_cast<int64_t>(entry.fftTimestamp / 1000);
                if (fftAge < 500) {
                    std::copy(std::begin(entry.fftMagnitudes),
                              std::end(entry.fftMagnitudes),
                              std::begin(telem.spectrum));
                }
            }

            telem.lufsIntegrated = entry.lufsIntegrated;
            telem.lufsShortTerm  = entry.lufsShortTerm;
            telem.lufsMomentary  = entry.lufsMomentary;
            telem.lufsTruePeak   = entry.lufsTruePeak;
            telem.loudnessRange  = entry.loudnessRange;
            telemetry_[i].push(telem);

            ++found;

            if (!wasActive)
            {
                logSlot("FORCE_SYNC_NEW", i, "name=" + juce::String(local.trackName)
                    + " colour=" + local.colour.toDisplayString(false));
                if (onSlotRegistered) onSlotRegistered(i);
            }
        }
        else if (slots_[i].active)
        {
            // Slot liberado en shared memory
            logSlot("FORCE_SYNC_RELEASED", i);
            slots_[i].active = false;
            slots_[i].slotIndex = -1;
            slots_[i].trackName[0] = '\0';
            if (onSlotReleased) onSlotReleased(i);
        }
    }

    // Actualizar lastSharedChangeCount_ para evitar que syncFromShared()
    // re-procese los mismos slots inmediatamente
    if (shm_ != nullptr) {
        lastSharedChangeCount_ = shm_->getChangeCount();
        localChangeCount_      = lastSharedChangeCount_;
        everSynced_            = true;
    }

    logSlot("FORCE_SYNC_DONE", -1, "found=" + juce::String(found));
    return found;
}

// ─── Registro / Liberación ──────────────────────────────────────────────────

int SlotRegistry::registerSlot(const std::string& trackName, const juce::Colour& colour, BusType bus)
{
    int assignedSlot = -1;

    // 1. Registrar en shared memory PRIMERO si está disponible para obtener el índice canónico.
    //    Así todos los procesos ven el mismo índice para esta pista.
    if (shm_ != nullptr) {
        SharedSlotEntry entry;
        entry.active    = 1;
        entry.bus       = static_cast<int>(bus);
        entry.colourARGB = colour.getARGB();
        entry.slotIndex = -1; // será asignado por registerSlot()
        strncpy_s(entry.trackName, kSharedTrackNameLen, trackName.c_str(), _TRUNCATE);

        assignedSlot = shm_->registerSlot(entry);
        if (assignedSlot < 0) {
            logSlot("REGISTER_FAIL", -1, "Shared memory llena");
            return -1;
        }
        logSlot("REGISTER_SHARED", assignedSlot,
            "name=" + juce::String(trackName) +
            " | colour=" + colour.toDisplayString(false));
    }

    // 2. Si no hay shared memory, asignar localmente (standalone)
    if (assignedSlot < 0) {
        for (int i = 0; i < kMaxSlots; ++i) {
            if (!slots_[i].active) {
                assignedSlot = i;
                break;
            }
        }
    }

    // 3. Registrar localmente con el índice canónico
    if (assignedSlot >= 0 && assignedSlot < kMaxSlots) {
        auto i = assignedSlot;
        slots_[i].slotIndex = i;
        slots_[i].setTrackName(trackName);
        slots_[i].colour = colour;
        slots_[i].bus    = bus;
        slots_[i].active = true;
        ++localChangeCount_;
        logSlot("REGISTER", i, "name=" + juce::String(trackName) +
            " | colour=" + colour.toDisplayString(false) +
            " | bus=" + juce::String(static_cast<int>(bus)));

        if (onSlotRegistered) onSlotRegistered(i);
        
        // Backup a archivo (siempre, incluso si shared memory está disponible)
        saveSlotToBackupFile(i, slots_[i]);
        
        return i;
    }

    logSlot("REGISTER_FAIL", -1, "No hay espacio libre");
    return -1;
}

void SlotRegistry::releaseSlot(int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        logSlot("RELEASE", slotIndex, "name=" + juce::String(slots_[slotIndex].trackName));

        // Liberar de shared memory si está disponible
        if (shm_ != nullptr) {
            shm_->releaseSlot(slotIndex);
        }

        slots_[slotIndex].active    = false;
        slots_[slotIndex].slotIndex = -1;
        slots_[slotIndex].trackName[0] = '\0';
        slots_[slotIndex].colour = juce::Colours::grey;
        slots_[slotIndex].bus = BusType::None;
        audioBuffers_[slotIndex].reset();
        ++localChangeCount_;
        if (onSlotReleased) onSlotReleased(slotIndex);
        
        // Eliminar backup de archivo
        removeSlotBackupFile(slotIndex);
    } else {
        logSlot("RELEASE_INVALID", slotIndex, "SlotIndex fuera de rango");
    }
}

void SlotRegistry::setActive(int slotIndex, bool active)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        slots_[slotIndex].active = active;
}

// ─── Consultas ──────────────────────────────────────────────────────────────

int SlotRegistry::activeCount() const noexcept
{
    int count = 0;
    for (const auto& slot : slots_) {
        if (slot.active) ++count;
    }
    return count;
}

SlotInfo SlotRegistry::getSlotInfo(int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        return slots_[slotIndex];
    return SlotInfo{};
}

juce::Colour SlotRegistry::getSlotColour(int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        return slots_[slotIndex].colour;
    return juce::Colours::grey;
}

void SlotRegistry::forEachActive(std::function<void(const SlotInfo&)> callback) const
{
    for (const auto& slot : slots_) {
        if (slot.active) callback(slot);
    }
}

// ─── Actualizaciones ────────────────────────────────────────────────────────

void SlotRegistry::updateSlotName(int slotIndex, const std::string& name)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        logSlot("UPDATE_NAME", slotIndex,
            "old=" + juce::String(slots_[slotIndex].trackName) +
            " | new=" + juce::String(name));
        slots_[slotIndex].setTrackName(name);
        ++localChangeCount_;

        // Sync a shared memory
        if (shm_ != nullptr) {
            SharedSlotEntry entry;
            shm_->readSlot(slotIndex, entry);
            entry.slotIndex = slotIndex;
            entry.active    = 1;
            entry.bus       = static_cast<int>(slots_[slotIndex].bus);
            entry.colourARGB = slots_[slotIndex].colour.getARGB();
            strncpy_s(entry.trackName, kSharedTrackNameLen, name.c_str(), _TRUNCATE);
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
        
        // Backup a archivo
        saveSlotToBackupFile(slotIndex, slots_[slotIndex]);
    }
}

void SlotRegistry::updateSlotColour(int slotIndex, const juce::Colour& colour)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        logSlot("UPDATE_COLOUR", slotIndex,
            "old=" + slots_[slotIndex].colour.toDisplayString(false) +
            " | new=" + colour.toDisplayString(false));
        slots_[slotIndex].colour = colour;
        ++localChangeCount_;

        // Sync a shared memory
        if (shm_ != nullptr) {
            SharedSlotEntry entry;
            shm_->readSlot(slotIndex, entry);
            entry.slotIndex = slotIndex;
            entry.active    = 1;
            entry.bus       = static_cast<int>(slots_[slotIndex].bus);
            entry.colourARGB = colour.getARGB();
            strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
        
        // Backup a archivo
        saveSlotToBackupFile(slotIndex, slots_[slotIndex]);
    }
}

void SlotRegistry::updateSlotBus(int slotIndex, BusType bus)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        logSlot("UPDATE_BUS", slotIndex,
            "old=" + juce::String(static_cast<int>(slots_[slotIndex].bus)) +
            " | new=" + juce::String(static_cast<int>(bus)));
        slots_[slotIndex].bus = bus;
        ++localChangeCount_;

        // Sync a shared memory
        if (shm_ != nullptr) {
            SharedSlotEntry entry;
            shm_->readSlot(slotIndex, entry);
            entry.slotIndex = slotIndex;
            entry.active    = 1;
            entry.bus       = static_cast<int>(bus);
            entry.colourARGB = slots_[slotIndex].colour.getARGB();
            strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
        
        // Backup a archivo
        saveSlotToBackupFile(slotIndex, slots_[slotIndex]);
    }
}

// ─── Telemetría ─────────────────────────────────────────────────────────────

TelemetryBuffer& SlotRegistry::getTelemetry(int slotIndex)
{
    return telemetry_[slotIndex];
}

const TelemetryBuffer& SlotRegistry::getTelemetry(int slotIndex) const
{
    return telemetry_[slotIndex];
}

AudioRingBuffer& SlotRegistry::getAudioBuffer(int slotIndex)
{
    return audioBuffers_[slotIndex];
}

const AudioRingBuffer& SlotRegistry::getAudioBuffer(int slotIndex) const
{
    return audioBuffers_[slotIndex];
}

// ─── Telemetría compartida ───────────────────────────────────────────────────

void SlotRegistry::updateSharedTelemetry(int slotIndex,
                                          float peakLeft, float peakRight,
                                          float rmsLeft, float rmsRight,
                                          float correlation, float crestFactor,
                                          float sampleL, float sampleR,
                                          const float* fftMagnitudes,
                                          float lufsIntegrated, float lufsShortTerm,
                                          float lufsMomentary, float lufsTruePeak,
                                          float loudnessRange)
{
    // Solo escribir a shared memory (si está disponible)
    // Los backup files se escriben desde el Messenger directamente
    // con throttling para evitar I/O excesivo en el audio thread.
    if (shm_ == nullptr || slotIndex < 0 || slotIndex >= kMaxSlots)
        return;

    SharedSlotEntry entry;
    if (!shm_->readSlot(slotIndex, entry))
        return;

    entry.peakLeft    = peakLeft;
    entry.peakRight   = peakRight;
    entry.rmsLeft     = rmsLeft;
    entry.rmsRight    = rmsRight;
    entry.correlation = correlation;
    entry.crestFactor = crestFactor;
    entry.sampleL     = sampleL;
    entry.sampleR     = sampleR;
    entry.telemetryTimestamp = static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes() * 1000.0);

    // Escribir datos FFT si se proporcionan (opcional, ~cada 40ms)
    if (fftMagnitudes != nullptr) {
        bool hasSpectrum = false;
        for (int fi = 0; fi < kNumSpectrumBins; ++fi) {
            if (fftMagnitudes[fi] > 0.001f) {
                hasSpectrum = true;
                break;
            }
        }
        if (hasSpectrum) {
            std::copy(fftMagnitudes, fftMagnitudes + kNumSpectrumBins, entry.fftMagnitudes);
            // ═══ FIX: Overflow de uint32 ═══════════════════════════════
            // getMillisecondCounter() retorna uint32_t. Multiplicar por 1000
            // como uint32_t desborda tras ~1.2h de uptime, corrompiendo
            // fftTimestamp. Al leerlo en forceFullSyncFromShm(), el chequeo
            // fftAge < 500 falla SIEMPRE y el FFT nunca se copia.
            // Cast a int64_t ANTES de multiplicar para evitar el overflow.
            entry.fftTimestamp = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        }
    }

    // Escribir datos LUFS en shared memory
    entry.lufsIntegrated = lufsIntegrated;
    entry.lufsShortTerm  = lufsShortTerm;
    entry.lufsMomentary  = lufsMomentary;
    entry.lufsTruePeak   = lufsTruePeak;
    entry.loudnessRange  = loudnessRange;

    shm_->writeSlot(slotIndex, entry);
}

// ─── ChangeCount ────────────────────────────────────────────────────────────

uint64_t SlotRegistry::getChangeCount() const noexcept
{
    if (shm_ != nullptr) {
        // En modo shared, retornar el contador de shared memory
        // (incluye cambios hechos por otros procesos)
        return shm_->getChangeCount();
    }
    return localChangeCount_;
}

juce::String SlotRegistry::defaultTrackName()
{
    static std::atomic<int> counter{0};
    return "Pista " + juce::String(++counter);
}

} // namespace mixcoach
