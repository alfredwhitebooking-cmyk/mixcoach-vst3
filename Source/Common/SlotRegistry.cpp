#include "SlotRegistry.h"
#include "LogHelper.h"
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
    std::strncpy(local.trackName, entry.trackName, sizeof(local.trackName) - 1);
    local.trackName[sizeof(local.trackName) - 1] = '\0';
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

    // Leer todos los slots activos desde shared memory
    for (int i = 0; i < kMaxSlots; ++i) {
        SharedSlotEntry entry;
        if (shm_->readSlot(i, entry) && entry.active) {
            // ─── Slot activo en shared memory ────────────────────────────
            auto& local = slots_[i];
            bool wasActive = local.active;

            // Sincronizar datos de registro
            const bool metadataChanged =
                wasActive &&
                (local.slotIndex != entry.slotIndex ||
                 local.bus != static_cast<BusType>(entry.bus) ||
                 local.colour.getARGB() != entry.colourARGB ||
                 std::strncmp(local.trackName, entry.trackName, sizeof(local.trackName)) != 0);

            local.slotIndex = entry.slotIndex;
            local.active    = true;
            local.bus       = static_cast<BusType>(entry.bus);
            local.colour = juce::Colour(entry.colourARGB);
            std::strncpy(local.trackName, entry.trackName, sizeof(local.trackName) - 1);
            local.trackName[sizeof(local.trackName) - 1] = '\0';

            if (!wasActive) {
                logSlot("SYNC_NEW", i, "name=" + juce::String(local.trackName));
                if (onSlotRegistered) onSlotRegistered(i);
            } else if (metadataChanged) {
                logSlot("SYNC_CHANGED", i, "name=" + juce::String(local.trackName));
                if (onSlotChanged) onSlotChanged(i);
            }

            // ─── Sincronizar telemetría desde shared memory ──────────────
            // Esto es CRÍTICO: los Messengers escriben telemetría en shared
            // memory via updateSharedTelemetry(), y MixCoach (otro proceso)
            // necesita leer esos datos en su TelemetryBuffer local.
            TrackTelemetry telem;
            telem.timestamp    = juce::Time::getMillisecondCounter() * 1000;
            telem.slotIndex    = entry.slotIndex;
            telem.active       = true;
            telem.peakLeft     = entry.peakLeft;
            telem.peakRight    = entry.peakRight;
            telem.rmsLeft      = entry.rmsLeft;
            telem.rmsRight     = entry.rmsRight;
            telem.correlation  = entry.correlation;
            telem.crestFactor  = entry.crestFactor;
            telem.sampleL      = entry.sampleL;
            telem.sampleR      = entry.sampleR;

            // Copiar datos FFT si hay datos recientes (< 500ms)
            if (entry.fftTimestamp > 0) {
                auto now = juce::Time::getMillisecondCounter();
                auto fftAge = now - static_cast<int64_t>(entry.fftTimestamp / 1000);
                if (fftAge < 500) {
                    std::copy(std::begin(entry.fftMagnitudes),
                              std::end(entry.fftMagnitudes),
                              std::begin(telem.spectrum));
                }
            }

            // Copiar datos LUFS desde shared memory
            telem.lufsIntegrated = entry.lufsIntegrated;
            telem.lufsShortTerm  = entry.lufsShortTerm;
            telem.lufsMomentary  = entry.lufsMomentary;
            telem.lufsTruePeak   = entry.lufsTruePeak;
            telem.loudnessRange  = entry.loudnessRange;

            // Empujar a la TelemetryBuffer local
            telemetry_[i].push(telem);

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
        std::strncpy(entry.trackName, trackName.c_str(), kSharedTrackNameLen - 1);
        entry.trackName[kSharedTrackNameLen - 1] = '\0';

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
            std::strncpy(entry.trackName, name.c_str(), kSharedTrackNameLen - 1);
            entry.trackName[kSharedTrackNameLen - 1] = '\0';
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
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
            std::strncpy(entry.trackName, slots_[slotIndex].trackName, kSharedTrackNameLen - 1);
            entry.trackName[kSharedTrackNameLen - 1] = '\0';
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
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
            std::strncpy(entry.trackName, slots_[slotIndex].trackName, kSharedTrackNameLen - 1);
            entry.trackName[kSharedTrackNameLen - 1] = '\0';
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
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
        for (int fi = 0; fi < 256; ++fi) {
            if (fftMagnitudes[fi] > 0.001f) {
                hasSpectrum = true;
                break;
            }
        }
        if (hasSpectrum) {
            std::copy(fftMagnitudes, fftMagnitudes + 256, entry.fftMagnitudes);
            entry.fftTimestamp = juce::Time::getMillisecondCounter() * 1000;
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
