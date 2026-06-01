#pragma once
#include <juce_core/juce_core.h>
#include "../types/Types.h"
#include "SlotRegistry.h"
#include "SharedMemory.h"

namespace mixcoach {

// ─── Datos compartidos entre plugins ────────────────────────────────────────
// Gestiona SharedMemoryManager (IPC entre procesos) + SlotRegistry local.
// Cuando se usa como VST3 en un DAW, SharedMemoryManager sincroniza los datos
// entre MixCoach y todos los Messengers via CreateFileMapping.
class SharedData : public juce::ReferenceCountedObject
{
public:
    // ─── Singleton thread-safe ──────────────────────────────────────────
    // Retorna la instancia única de SharedData.
    //
    // NOTA: Durante el escaneo VST3 en algunos DAWs (FL Studio, Cubase),
    // CreateFileMapping puede fallar. El constructor maneja esa falla
    // degradando a modo local (sin IPC). Usar isAvailable() para verificar.
    static SharedData& getInstance()
    {
        static SharedData instance;
        return instance;
    }

    // ─── Singleton seguro contra excepciones ───────────────────────────
    // Retorna nullptr si la inicialización falló (útil en lambdas/callbacks
    // que no pueden lanzar excepciones, como onTrackSelected en la UI).
    static SharedData* safeGetInstance() noexcept
    {
        try {
            return &getInstance();
        } catch (const std::exception&) {
            return nullptr;
        } catch (...) {
            return nullptr;
        }
    }

    SharedData() noexcept;
    ~SharedData() override;

    // Verificar que la instancia está en estado usable
    [[nodiscard]] bool isAvailable() const noexcept { return shmInitialized_; }

    // Acceso al registro de slots
    SlotRegistry& getSlotRegistry() noexcept { return slotRegistry_; }
    const SlotRegistry& getSlotRegistry() const noexcept { return slotRegistry_; }

    // Acceso al gestor de memoria compartida
    SharedMemoryManager& getSharedMemory() noexcept { return *shm_; }
    bool isSharedMemoryAvailable() const noexcept { return shm_ != nullptr && shm_->isInitialized(); }

    // ─── Reintentar inicialización de shared memory ───────────────────────
    // Llamar cuando la inicialización inicial falló para reintentar abrir
    // el file mapping. Retorna true si se pudo conectar.
    bool retryInitSharedMemory();

    // Fase activa de mentoría
    void setCurrentPhase(MentorPhase phase) noexcept { currentPhase_ = phase; }
    MentorPhase getCurrentPhase() const noexcept { return currentPhase_; }

    // Mensajes del chat
    void pushMessage(const MentorMessage& msg);
    [[nodiscard]] int getMessageCount() const noexcept;
    [[nodiscard]] MentorMessage getMessage(int index) const;

private:
    SlotRegistry slotRegistry_;
    std::unique_ptr<SharedMemoryManager> shm_;
    MentorPhase currentPhase_{MentorPhase::Welcome};
    static constexpr int kMaxMessages = 256;
    std::array<MentorMessage, kMaxMessages> messages_{};
    int messageCount_{0};
    bool shmInitialized_{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedData)
};

} // namespace mixcoach
