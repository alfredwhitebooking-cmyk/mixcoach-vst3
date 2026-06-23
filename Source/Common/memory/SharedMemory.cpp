#include "SharedMemory.h"
#include "../types/LogHelper.h"
#include <atomic>
#include <immintrin.h>  // _mm_pause()

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
        return true;

    mapName_ = mapName;

#ifdef _WIN32
    fileMapping_ = OpenFileMappingW(
        FILE_MAP_ALL_ACCESS, FALSE, mapName_.toWideCharPointer());

    bool isNew = false;
    if (fileMapping_ == nullptr) {
        fileMapping_ = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
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
        LogHelper::writeToLog("[SharedMemory] ERROR: No se pudo crear/abrir "
            "el file mapping (error " + juce::String(GetLastError()) + ")");
        return false;
    }

    fileView_ = MapViewOfFile(
        fileMapping_,
        FILE_MAP_ALL_ACCESS,
        0, 0,
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
        memset(block_, 0, sizeof(SharedMemoryBlock));
        block_->header.initialized = 1;
        block_->header.structVersion = SharedMemoryHeader::kCurrentStructVersion;
        block_->header.ownerCheck =
            static_cast<uint32_t>(juce::Time::getMillisecondCounter());
        LogHelper::writeToLog("[SharedMemory] CREADA nueva memoria "
            "compartida: " + mapName_ + " (structVersion=" +
            juce::String((int)SharedMemoryHeader::kCurrentStructVersion) + ")");
    } else {
        auto existingVersion = block_->header.structVersion;
        if (existingVersion != SharedMemoryHeader::kCurrentStructVersion) {
            LogHelper::writeToLog("[SharedMemory] VERSION MISMATCH: "
                "existente=" + juce::String((int)existingVersion) +
                " | esperada=" +
                juce::String((int)SharedMemoryHeader::kCurrentStructVersion) +
                " | RECREANDO...");
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
            isNew = true;

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

    int pauseCount = 0;
    while (InterlockedExchange(&block_->header.writeLock, 1) != 0) {
        if (GetTickCount64() - start > static_cast<ULONGLONG>(timeoutMs)) {
            LogHelper::writeToLog("[SharedMemory] TIMEOUT adquiriendo writeLock; se omite la operacion");
            return false;
        }
        if (pauseCount < 1000) {
            _mm_pause();
            ++pauseCount;
        } else {
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
    return InterlockedCompareExchange64(
        reinterpret_cast<volatile LONG64*>(&block_->header.changeCount),
        0, 0);
#else
    return block_->header.changeCount;
#endif
}

bool SharedMemoryManager::readSlot(int index, SharedSlotEntry& out) noexcept
{
    if (block_ == nullptr || index < 0 || index >= kSharedMaxSlots)
        return false;

    if (!acquireLock())
        return false;

    out = block_->slots[index];

    releaseLock();
    return true;
}

void SharedMemoryManager::writeSlot(int index, const SharedSlotEntry& entry) noexcept
{
    if (block_ == nullptr || index < 0 || index >= kSharedMaxSlots)
        return;

    if (!acquireLock())
        return;

    block_->slots[index] = entry;

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
            block_->slots[i] = entry;
            block_->slots[i].slotIndex = i;
            block_->slots[i].active = 1;

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

bool SharedMemoryManager::healthCheck() const noexcept
{
    if (block_ == nullptr)
        return false;

#ifdef _WIN32
    __try {
        volatile int dummy = block_->header.initialized;
        (void)dummy;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
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
