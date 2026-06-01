#include "SharedMemory.h"
#include "../types/LogHelper.h"
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#error "SharedMemory solo soportado en Windows (CreateFileMapping)"
#endif

namespace mixcoach {

// ─── Constructor / Destructor ───────────────────────────────────────────────

SharedMemoryManager::SharedMemoryManager() = default;

SharedMemoryManager::~SharedMemoryManager()
{
    close();
}

// ─── Inicializar ────────────────────────────────────────────────────────────

bool SharedMemoryManager::initialize(const juce::String& mapName)
{
    if (block_ != nullptr)
        return true; // Ya inicializado

    mapName_ = mapName;

#ifdef _WIN32
    // Intentar abrir memoria compartida existente primero
    fileMapping_ = OpenFileMappingW(
        FILE_MAP_ALL_ACCESS, FALSE, mapName_.toWideCharPointer());

    bool isNew = false;
    if (fileMapping_ == nullptr) {
        // No existe → crear una nueva
        fileMapping_ = CreateFileMappingW(
            INVALID_HANDLE_VALUE,  // Usar memoria paginada del sistema
            nullptr,               // Seguridad por defecto
            PAGE_READWRITE,
            0,                     // Tamaño (high-order)
            sizeof(SharedMemoryBlock), // Tamaño (low-order)
            mapName_.toWideCharPointer());
        isNew = true;
    }

    if (fileMapping_ == nullptr) {
        LogHelper::writeToLog("[SharedMemory] ERROR: No se pudo crear/abrir "
            "el file mapping (error " + juce::String(GetLastError()) + ")");
        return false;
    }

    // Mapear la vista
    fileView_ = MapViewOfFile(
        fileMapping_,
        FILE_MAP_ALL_ACCESS,
        0, 0,  // Offset
        sizeof(SharedMemoryBlock));

    if (fileView_ == nullptr) {
        LogHelper::writeToLog("[SharedMemory] ERROR: No se pudo mapear "
            "la vista (error " + juce::String(GetLastError()) + ")");
        CloseHandle(fileMapping_);
        fileMapping_ = nullptr;
        return false;
    }

    block_ = static_cast<SharedMemoryBlock*>(fileView_);

    if (isNew) {
        // Inicializar bloque (solo el primer proceso que lo crea)
        memset(block_, 0, sizeof(SharedMemoryBlock));
        block_->header.initialized = 1;
        block_->header.structVersion = SharedMemoryHeader::kCurrentStructVersion;
        block_->header.ownerCheck =
            static_cast<uint32_t>(juce::Time::getMillisecondCounter());
        LogHelper::writeToLog("[SharedMemory] CREADA nueva memoria "
            "compartida: " + mapName_ + " (structVersion=" +
            juce::String((int)SharedMemoryHeader::kCurrentStructVersion) + ")");
    } else {
        // ─── VERIFICACIÓN CRÍTICA: version del struct ────────────────────
        // Si el structVersion no coincide con el actual, significa que la
        // memoria compartida fue creada por una versión anterior del plugin
        // con un SharedSlotEntry de diferente tamaño. Acceder a campos en
        // offsets incorrectos causaría access violation → cerrar y recrear.
        auto existingVersion = block_->header.structVersion;
        if (existingVersion != SharedMemoryHeader::kCurrentStructVersion) {
            LogHelper::writeToLog("[SharedMemory] VERSION MISMATCH: "
                "existente=" + juce::String((int)existingVersion) +
                " | esperada=" +
                juce::String((int)SharedMemoryHeader::kCurrentStructVersion) +
                " | RECREANDO...");

            // Cerrar mapping viejo y crear uno nuevo con tamaño correcto
            close();

            fileMapping_ = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                sizeof(SharedMemoryBlock),
                mapName_.toWideCharPointer());

            if (fileMapping_ == nullptr) {
                LogHelper::writeToLog("[SharedMemory] ERROR: No se pudo "
                    "recrear el file mapping (error " +
                    juce::String(GetLastError()) + ")");
                return false;
            }

            fileView_ = MapViewOfFile(
                fileMapping_,
                FILE_MAP_ALL_ACCESS,
                0, 0,
                sizeof(SharedMemoryBlock));

            if (fileView_ == nullptr) {
                LogHelper::writeToLog("[SharedMemory] ERROR: No se pudo "
                    "mapear la nueva vista (error " +
                    juce::String(GetLastError()) + ")");
                CloseHandle(fileMapping_);
                fileMapping_ = nullptr;
                return false;
            }

            block_ = static_cast<SharedMemoryBlock*>(fileView_);
            memset(block_, 0, sizeof(SharedMemoryBlock));
            block_->header.initialized = 1;
            block_->header.structVersion = SharedMemoryHeader::kCurrentStructVersion;
            block_->header.ownerCheck =
                static_cast<uint32_t>(juce::Time::getMillisecondCounter());
            isNew = true; // tratar como nuevo

            LogHelper::writeToLog("[SharedMemory] RECREADA con version "
                + juce::String((int)SharedMemoryHeader::kCurrentStructVersion));
        } else {
            LogHelper::writeToLog("[SharedMemory] ABIERTA memoria "
                "compartida existente: " + mapName_ + " (structVersion=" +
                juce::String((int)existingVersion) + ")");
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
    if (block_ == nullptr)
        return false;

    auto start = GetTickCount64();

    // Spinlock: esperar hasta que writeLock sea 0, luego ponerlo en 1
    while (InterlockedExchange(&block_->header.writeLock, 1) != 0) {
        if (GetTickCount64() - start > static_cast<ULONGLONG>(timeoutMs)) {
            LogHelper::writeToLog("[SharedMemory] TIMEOUT adquiriendo writeLock; se omite la operacion");
            return false;
        }
        // Yield opcional para no quemar CPU en espera corta
        Sleep(0);
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
    return InterlockedCompareExchange64(
        reinterpret_cast<volatile LONG64*>(&block_->header.changeCount),
        0, 0); // No cambia, solo lectura
#else
    return block_->header.changeCount;
#endif
}

bool SharedMemoryManager::readSlot(int index, SharedSlotEntry& out) const noexcept
{
    if (block_ == nullptr || index < 0 || index >= kSharedMaxSlots)
        return false;

    // Copia segura del slot (no necesita lock para lectura porque
    // las escrituras de un slot son atómicas en x64 para este tamaño)
    out = block_->slots[index];
    return true;
}

void SharedMemoryManager::writeSlot(int index, const SharedSlotEntry& entry) noexcept
{
    if (block_ == nullptr || index < 0 || index >= kSharedMaxSlots)
        return;

    if (!acquireLock())
        return;

    block_->slots[index] = entry;

    // Incrementar changeCount atómicamente
#ifdef _WIN32
    InterlockedIncrement64(
        reinterpret_cast<volatile LONG64*>(&block_->header.changeCount));
#else
    block_->header.changeCount++;
#endif

    releaseLock();
}

int SharedMemoryManager::registerSlot(const SharedSlotEntry& entry) noexcept
{
    if (block_ == nullptr) return -1;

    if (!acquireLock())
        return -1;

    int assigned = -1;
    for (int i = 0; i < kSharedMaxSlots; ++i) {
        if (block_->slots[i].active == 0) {
            // Escribir la entrada en este slot
            block_->slots[i] = entry;
            block_->slots[i].slotIndex = i;
            block_->slots[i].active = 1;

            // Incrementar changeCount
#ifdef _WIN32
            InterlockedIncrement64(
                reinterpret_cast<volatile LONG64*>(&block_->header.changeCount));
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
    if (block_ == nullptr || index < 0 || index >= kSharedMaxSlots)
        return;

    if (!acquireLock())
        return;

    memset(&block_->slots[index], 0, sizeof(SharedSlotEntry));
    block_->slots[index].slotIndex = -1;

#ifdef _WIN32
    InterlockedIncrement64(
        reinterpret_cast<volatile LONG64*>(&block_->header.changeCount));
#else
    block_->header.changeCount++;
#endif

    releaseLock();
}

// ─── Health check ─────────────────────────────────────────────────────────────
// Verifica que el mapeo de memoria es accesible sin crashear.
// Usa SEH (Structured Exception Handling) en Windows para capturar
// access violations si el mapping fue invalidado.
bool SharedMemoryManager::healthCheck() const noexcept
{
    if (block_ == nullptr)
        return false;

#ifdef _WIN32
    __try {
        // Lectura simple para verificar que la página está accesible
        volatile int dummy = block_->header.initialized;
        (void)dummy;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Access violation — el mapping no es accesible
        return false;
    }
#else
    return block_ != nullptr;
#endif
}

// ─── Reintentar conexión ─────────────────────────────────────────────────────
// Cierra el mapping actual y lo reabre. Útil cuando healthCheck() falla
// o cuando la inicialización inicial fue diferida.
bool SharedMemoryManager::reconnect()
{
    // Cerrar mapping actual si existe
    close();

    // Pequeña pausa para asegurar que los handles del OS se liberan
    juce::Thread::sleep(50);

    // Reabrir
    return initialize(mapName_);
}

} // namespace mixcoach
