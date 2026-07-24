#include "SharedMemory.h"
#include "../types/LogHelper.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <immintrin.h> // _mm_pause()

#ifdef _WIN32
#include <windows.h>
#else
#error "SharedMemory solo soportado en Windows (CreateFileMapping)"
#endif

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers de sesión
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String generateSessionGUID()
    {
        // Genera un GUID único: "MixCoach_<timestamp-hex>_<random16-hex>"
        uint32_t now = juce::Time::getMillisecondCounter();
        uint32_t rng = static_cast<uint32_t>(now ^ (uint32_t)(intptr_t)&now); // entropy from stack
        rng ^= static_cast<uint32_t>(rand() ^ (uint32_t)time(nullptr));

        char buf[48];
        snprintf(buf, sizeof(buf), "MixCoach_%08X%08X", now, rng);
        return juce::String(buf);
    }

    juce::String makeSlotShmName(const juce::String& guid)
    {
        return "Local\\" + guid + "_Slots";
    }

    juce::String makeAudioShmName(const juce::String& guid)
    {
        return "Local\\" + guid + "_Audio";
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionDiscovery — Negociación de GUID entre procesos
    // ═══════════════════════════════════════════════════════════════════════════

    SessionDiscovery::SessionDiscovery() = default;

    SessionDiscovery::~SessionDiscovery()
    {
        if (block_ != nullptr) {
            block_->active = 0; // Señal de muerte
        }
        closeDiscovery();
    }

    bool SessionDiscovery::initialize()
    {
        if (block_ != nullptr) return true;

#ifdef _WIN32
        fileMapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                          nullptr,
                                          PAGE_READWRITE,
                                          0,
                                          sizeof(SessionDiscoveryBlock),
                                          juce::String(kSessionDiscoveryName).toWideCharPointer());

        if (fileMapping_ == nullptr) {
            LogHelper::writeToLog("[SessionDiscovery] ERROR: Cannot create/open discovery shm (error "
                                  + juce::String(GetLastError()) + ")");
            return false;
        }

        fileView_ = MapViewOfFile(fileMapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SessionDiscoveryBlock));

        if (fileView_ == nullptr) {
            LogHelper::writeToLog("[SessionDiscovery] ERROR: MapViewOfFile failed (" + juce::String(GetLastError())
                                  + ")");
            CloseHandle(fileMapping_);
            fileMapping_ = nullptr;
            return false;
        }

        block_ = static_cast<SessionDiscoveryBlock*>(fileView_);
        LogHelper::writeToLog("[SessionDiscovery] Inicializado");
        return true;
#else
        return false;
#endif
    }

    void SessionDiscovery::publishGUID(const juce::String& guid)
    {
        if (block_ == nullptr) return;

        memset(block_->sessionGUID, 0, sizeof(block_->sessionGUID));
        juce::String guidStr = guid.substring(0, sizeof(block_->sessionGUID) - 1);
        strncpy(block_->sessionGUID, guidStr.toRawUTF8(), sizeof(block_->sessionGUID) - 1);
        block_->timestampMs = juce::Time::getMillisecondCounter();
        block_->active      = 1;
        block_->protocolVer = 1;

        LogHelper::writeToLog("[SessionDiscovery] GUID publicado: " + guid);
    }

    juce::String SessionDiscovery::readGUID() const
    {
        if (block_ == nullptr || block_->active == 0) return {};

        // Leer el GUID del bloque compartido
        juce::String guid = juce::String::fromUTF8(block_->sessionGUID);
        if (guid.isNotEmpty()) return guid;

        return {};
    }

    void SessionDiscovery::closeSession()
    {
        if (block_ != nullptr) {
            block_->active = 0;
            memset(block_->sessionGUID, 0, sizeof(block_->sessionGUID));
        }
    }

    void SessionDiscovery::closeDiscovery()
    {
#ifdef _WIN32
        if (fileView_ != nullptr) {
            UnmapViewOfFile(fileView_);
            fileView_ = nullptr;
        }
        if (fileMapping_ != nullptr) {
            CloseHandle(fileMapping_);
            fileMapping_ = nullptr;
        }
        block_ = nullptr;
#endif
    }

    // ─── Constructor / Destructor ───────────────────────────────────────────────

    SharedMemoryManager::SharedMemoryManager() = default;

    SharedMemoryManager::~SharedMemoryManager()
    {
        close();
    }

    // ─── Inicializar ────────────────────────────────────────────────────────────

    bool SharedMemoryManager::initialize(const juce::String& mapName)
    {
        if (block_ != nullptr) return true;

        mapName_ = mapName;

#ifdef _WIN32
        fileMapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName_.toWideCharPointer());

        bool isNew = false;
        if (fileMapping_ == nullptr) {
            fileMapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                              nullptr,
                                              PAGE_READWRITE,
                                              0,
                                              sizeof(SharedMemoryBlock),
                                              mapName_.toWideCharPointer());
            if (fileMapping_ != nullptr) {
                isNew = (GetLastError() != ERROR_ALREADY_EXISTS);
            }
        }

        if (fileMapping_ == nullptr) {
            LogHelper::writeToLog(
                "[SharedMemory] ERROR: No se pudo crear/abrir "
                "el file mapping (error "
                + juce::String(GetLastError()) + ")");
            return false;
        }

        fileView_ = MapViewOfFile(fileMapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryBlock));

        if (fileView_ == nullptr) {
            LogHelper::writeToLog(
                "[SharedMemory] ERROR: No se pudo mapear "
                "la vista (error "
                + juce::String(GetLastError()) + ")");
            CloseHandle(fileMapping_);
            fileMapping_ = nullptr;
            return false;
        }

        block_ = static_cast<SharedMemoryBlock*>(fileView_);

        if (isNew) {
            memset(block_, 0, sizeof(SharedMemoryBlock));
            block_->header.initialized   = 1;
            block_->header.structVersion = SharedMemoryHeader::kCurrentStructVersion;
            block_->header.ownerCheck    = static_cast<uint32_t>(juce::Time::getMillisecondCounter());
            LogHelper::writeToLog(
                "[SharedMemory] CREADA nueva memoria "
                "compartida: "
                + mapName_ + " (structVersion=" + juce::String((int)SharedMemoryHeader::kCurrentStructVersion) + ")");
        }
        else {
            auto existingVersion = block_->header.structVersion;
            if (existingVersion != SharedMemoryHeader::kCurrentStructVersion) {
                LogHelper::writeToLog(
                    "[SharedMemory] VERSION MISMATCH: "
                    "existente="
                    + juce::String((int)existingVersion) + " | esperada="
                    + juce::String((int)SharedMemoryHeader::kCurrentStructVersion) + " | RECREANDO...");
                close();

                fileMapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                                  nullptr,
                                                  PAGE_READWRITE,
                                                  0,
                                                  sizeof(SharedMemoryBlock),
                                                  mapName_.toWideCharPointer());

                if (fileMapping_ == nullptr) {
                    LogHelper::writeToLog(
                        "[SharedMemory] ERROR: No se pudo "
                        "recrear el file mapping (error "
                        + juce::String(GetLastError()) + ")");
                    return false;
                }

                fileView_ = MapViewOfFile(fileMapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryBlock));

                if (fileView_ == nullptr) {
                    LogHelper::writeToLog(
                        "[SharedMemory] ERROR: No se pudo "
                        "mapear la nueva vista (error "
                        + juce::String(GetLastError()) + ")");
                    CloseHandle(fileMapping_);
                    fileMapping_ = nullptr;
                    return false;
                }

                block_ = static_cast<SharedMemoryBlock*>(fileView_);
                memset(block_, 0, sizeof(SharedMemoryBlock));
                block_->header.initialized   = 1;
                block_->header.structVersion = SharedMemoryHeader::kCurrentStructVersion;
                block_->header.ownerCheck    = static_cast<uint32_t>(juce::Time::getMillisecondCounter());
                isNew                        = true;

                LogHelper::writeToLog("[SharedMemory] RECREADA con version "
                                      + juce::String((int)SharedMemoryHeader::kCurrentStructVersion));
            }
            else {
                LogHelper::writeToLog(
                    "[SharedMemory] ABIERTA memoria "
                    "compartida existente: "
                    + mapName_ + " (structVersion=" + juce::String((int)existingVersion) + ")");
            }
        }

        return true;
#else
        LogHelper::writeToLog("[SharedMemory] ERROR: Solo soportado en Windows");
        return false;
#endif
    }

    void SharedMemoryManager::close()
    {
#ifdef _WIN32
        if (fileView_ != nullptr) {
            UnmapViewOfFile(fileView_);
            fileView_ = nullptr;
        }
        if (fileMapping_ != nullptr) {
            CloseHandle(fileMapping_);
            fileMapping_ = nullptr;
        }
        block_ = nullptr;
#endif
    }

    // ─── Operaciones atómicas ────────────────────────────────────────────────────

    bool SharedMemoryManager::acquireLock(int timeoutMs)
    {
#ifdef _WIN32
        if (block_ == nullptr) return false;

        auto start = GetTickCount64();

        int pauseCount = 0;
        while (InterlockedExchange(&block_->header.writeLock, 1) != 0) {
            if (GetTickCount64() - start > static_cast<ULONGLONG>(timeoutMs)) {
                LogHelper::writeToLog("[SharedMemory] TIMEOUT adquiriendo writeLock; se omite la operacion");
                return false;
            }
            if (pauseCount < 1000) {
                _mm_pause();
                ++pauseCount;
            }
            else {
                Sleep(0);
            }
        }
        return true;
#else
        return true;
#endif
    }

    void SharedMemoryManager::releaseLock()
    {
#ifdef _WIN32
        InterlockedExchange(&block_->header.writeLock, 0);
#endif
    }

    uint64_t SharedMemoryManager::getChangeCount() const noexcept
    {
        if (block_ == nullptr) return 0;
#ifdef _WIN32
        return InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(&block_->header.changeCount), 0, 0);
#else
        return block_->header.changeCount;
#endif
    }

    bool SharedMemoryManager::readSlot(int index, SharedSlotEntry& out) noexcept
    {
        if (block_ == nullptr || index < 0 || index >= kSharedMaxSlots) return false;

        if (!acquireLock()) return false;

        out = block_->slots[index];

        releaseLock();
        return true;
    }

    void SharedMemoryManager::writeSlot(int index, const SharedSlotEntry& entry) noexcept
    {
        if (block_ == nullptr || index < 0 || index >= kSharedMaxSlots) return;

        if (!acquireLock()) return;

        block_->slots[index] = entry;

#ifdef _WIN32
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&block_->header.changeCount));
#else
        block_->header.changeCount++;
#endif

        releaseLock();
    }

    int SharedMemoryManager::registerSlot(const SharedSlotEntry& entry) noexcept
    {
        if (block_ == nullptr) return -1;

        if (!acquireLock()) return -1;

        int assigned = -1;
        for (int i = 0; i < kSharedMaxSlots; ++i) {
            if (block_->slots[i].active == 0) {
                block_->slots[i]           = entry;
                block_->slots[i].slotIndex = i;
                block_->slots[i].active    = 1;

#ifdef _WIN32
                InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&block_->header.changeCount));
#else
                block_->header.changeCount++;
#endif
                assigned = i;
                break;
            }
        }

        releaseLock();
        return assigned;
    }

    void SharedMemoryManager::releaseSlot(int index) noexcept
    {
        if (block_ == nullptr || index < 0 || index >= kSharedMaxSlots) return;

        if (!acquireLock()) return;

        memset(&block_->slots[index], 0, sizeof(SharedSlotEntry));
        block_->slots[index].slotIndex = -1;

#ifdef _WIN32
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&block_->header.changeCount));
#else
        block_->header.changeCount++;
#endif

        releaseLock();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  V10: Heartbeat lock-free (sin spinlock)
    // ═══════════════════════════════════════════════════════════════════════════

    void SharedMemoryManager::writeHeartbeat(int slotIndex, int64_t timestampMs) noexcept
    {
        if (block_ == nullptr || slotIndex < 0 || slotIndex >= kSharedMaxSlots) return;

#ifdef _WIN32
        // ═══ Lock-free: sin spinlock, sin advertencia de timeout ═══════════
        // InterlockedExchange64 es atómico en x64 y no bloquea.
        // El audio thread de Messenger llama esto ~94 veces/segundo/pista.
        InterlockedExchange64(reinterpret_cast<volatile LONG64*>(&block_->slotHeartbeats[slotIndex]),
                              static_cast<LONG64>(timestampMs));
#else
        block_->slotHeartbeats[slotIndex] = timestampMs;
#endif
    }

    int64_t SharedMemoryManager::readHeartbeat(int slotIndex) const noexcept
    {
        if (block_ == nullptr || slotIndex < 0 || slotIndex >= kSharedMaxSlots) return 0;

#ifdef _WIN32
        // Lectura atómica con barrera de memoria (acquire semantics)
        return InterlockedCompareExchange64(
            reinterpret_cast<volatile LONG64*>(const_cast<int64_t*>(&block_->slotHeartbeats[slotIndex])),
            0, 0);
#else
        return block_->slotHeartbeats[slotIndex];
#endif
    }

    // Helper SEH puro: no puede tener objetos C++ con destructor (C2712).
    static bool probeSharedBlock(const volatile int* p) noexcept
    {
        __try {
            volatile int dummy = *p;
            (void)dummy;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool SharedMemoryManager::healthCheck() const noexcept
    {
        if (block_ == nullptr) return false;

#ifdef _WIN32
        bool ok = probeSharedBlock(&block_->header.initialized);
        if (!ok) {
            char buf[96];
            snprintf(buf, sizeof(buf), "[SharedMemory] healthCheck SEH (block probe failed)");
            LogHelper::writeToLog(buf);
        }
        return ok;
#else
        return block_ != nullptr;
#endif
    }

    bool SharedMemoryManager::reconnect()
    {
        close();
        juce::Thread::sleep(50);
        return initialize(mapName_);
    }

} // namespace mixcoach
