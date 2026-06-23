#include "SharedAudioMemory.h"
#include "../types/LogHelper.h"
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#error "SharedAudioMemory solo soportado en Windows (CreateFileMapping)"
#endif

namespace mixcoach {

    // ─── Tamaño total del bloque compartido ─────────────────────────────────────
    static constexpr size_t kAudioMemSize = sizeof(SharedAudioBlock);

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor / Destructor
    // ═══════════════════════════════════════════════════════════════════════════

    SharedAudioMemory::~SharedAudioMemory()
    {
        close();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Inicializar
    // ═══════════════════════════════════════════════════════════════════════════
    bool SharedAudioMemory::initialize(const juce::String& mapName)
    {
        if (block_ != nullptr) return true;

        mapName_ = mapName;

#ifdef _WIN32
        // Intentar abrir mapping existente primero
        fileMapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName_.toWideCharPointer());

        bool isNew = false;
        if (fileMapping_ == nullptr) {
            // Crear nuevo mapping
            fileMapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                              nullptr,
                                              PAGE_READWRITE,
                                              0,
                                              static_cast<DWORD>(kAudioMemSize),
                                              mapName_.toWideCharPointer());

            if (fileMapping_ != nullptr) {
                isNew = (GetLastError() != ERROR_ALREADY_EXISTS);
            }
        }

        if (fileMapping_ == nullptr) {
            LogHelper::writeToLog(
                "[SharedAudioMemory] ERROR: No se pudo crear/abrir "
                "el file mapping (error "
                + juce::String(GetLastError()) + ")");
            return false;
        }

        fileView_ = MapViewOfFile(fileMapping_, FILE_MAP_ALL_ACCESS, 0, 0, kAudioMemSize);

        if (fileView_ == nullptr) {
            LogHelper::writeToLog(
                "[SharedAudioMemory] ERROR: No se pudo mapear "
                "la vista (error "
                + juce::String(GetLastError()) + ")");
            CloseHandle(fileMapping_);
            fileMapping_ = nullptr;
            return false;
        }

        block_ = static_cast<SharedAudioBlock*>(fileView_);

        if (isNew) {
            // Inicializar todo a cero
            memset(block_, 0, kAudioMemSize);
            block_->header.initialized = 1;
            LogHelper::writeToLog("[SharedAudioMemory] CREADO nuevo mapping de audio: " + mapName_ + " ("
                                  + juce::String((int)(kAudioMemSize / 1024)) + " KB)");
        }
        else {
            LogHelper::writeToLog("[SharedAudioMemory] ABIERTO mapping de audio existente: " + mapName_);
        }

        return true;
#else
        LogHelper::writeToLog("[SharedAudioMemory] ERROR: Solo soportado en Windows");
        return false;
#endif
    }

    void SharedAudioMemory::close()
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

    // ═══════════════════════════════════════════════════════════════════════════
    //  API de escritura (Messenger → audio thread)
    // ═══════════════════════════════════════════════════════════════════════════
    void SharedAudioMemory::writeSamples(int slotIndex, const float* data, int numSamples) noexcept
    {
        if (block_ == nullptr || slotIndex < 0 || slotIndex >= kMaxAudioSlots || data == nullptr || numSamples <= 0)
            return;

        auto& slot = block_->slots[slotIndex];

        // Leer writePos una sola vez al inicio
        int64_t wp = slot.writePos;

        // Escribir todos los samples en el buffer circular
        for (int i = 0; i < numSamples; ++i) slot.buffer[(wp + i) % kAudioBufferSize] = data[i];

        // _WriteBarrier() asegura que TODAS las escrituras del buffer ocurran
        // ANTES de que el lector vea el nuevo writePos.
        // Esto es una barrera de compilador (no emite instrucciones en x86/x64).
#if defined(_MSC_VER)
        _WriteBarrier();
#endif

        // Publicar nuevo writePos con un solo incremento atómico
        slot.writePos = wp + numSamples;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  API de lectura (MixCoach → background thread / timer UI)
    // ═══════════════════════════════════════════════════════════════════════════
    int SharedAudioMemory::readSamples(int slotIndex, float* outData, int maxSamples) noexcept
    {
        if (block_ == nullptr || slotIndex < 0 || slotIndex >= kMaxAudioSlots || outData == nullptr || maxSamples <= 0)
            return 0;

        auto& slot = block_->slots[slotIndex];

        // 1. Leer writePos (el productor escribió datos y luego incrementó writePos)
        int64_t wp = slot.writePos;

        // 2. _ReadBarrier: evita que el compilador lea el buffer ANTES de writePos.
        //    El productor hace: buffer[i] = dato → _WriteBarrier() → writePos++
        //    El consumidor debe: leer writePos → _ReadBarrier() → leer buffer[i]
        //    En x86/x64 no hay reordenamiento Load-Load, pero es correcto tenerlo.
#if defined(_MSC_VER)
        _ReadBarrier();
#endif

        int64_t rp        = slot.readPos;
        int64_t available = wp - rp;
        if (available <= 0) return 0;

        // Protección contra overrun: si el productor escribió más que el tamaño
        // del buffer sin que hayamos leído, limitamos a kAudioBufferSize para
        // evitar leer datos corruptos (sobreescritos por wrap-around).
        if (available > kAudioBufferSize) available = kAudioBufferSize;

        int64_t toRead = (std::min)(available, static_cast<int64_t>(maxSamples));

        for (int64_t i = 0; i < toRead; ++i) outData[i] = slot.buffer[(rp + i) % kAudioBufferSize];

        // Publicar nuevo readPos
        slot.readPos = rp + toRead;

        return static_cast<int>(toRead);
    }

    int SharedAudioMemory::peekSamples(int slotIndex, float* outData, int maxSamples) const noexcept
    {
        if (block_ == nullptr || slotIndex < 0 || slotIndex >= kMaxAudioSlots || outData == nullptr || maxSamples <= 0)
            return 0;

        auto& slot = block_->slots[slotIndex];

        int64_t wp = slot.writePos;
#if defined(_MSC_VER)
        _ReadBarrier();
#endif
        int64_t rp = slot.readPos;

        int64_t available = wp - rp;
        if (available <= 0) return 0;

        if (available > kAudioBufferSize) available = kAudioBufferSize;

        int64_t toRead = (std::min)(available, static_cast<int64_t>(maxSamples));

        for (int64_t i = 0; i < toRead; ++i) outData[i] = slot.buffer[(rp + i) % kAudioBufferSize];

        return static_cast<int>(toRead);
    }

    bool SharedAudioMemory::hasData(int slotIndex) const noexcept
    {
        if (block_ == nullptr || slotIndex < 0 || slotIndex >= kMaxAudioSlots) return false;

        // Solo comparación de posiciones — no hay buffer reads que proteger.
        auto& slot = block_->slots[slotIndex];
        return slot.writePos != slot.readPos;
    }

    int SharedAudioMemory::available(int slotIndex) const noexcept
    {
        if (block_ == nullptr || slotIndex < 0 || slotIndex >= kMaxAudioSlots) return 0;

        auto& slot   = block_->slots[slotIndex];
        int64_t diff = slot.writePos - slot.readPos;
        if (diff > kAudioBufferSize) diff = kAudioBufferSize;
        return static_cast<int>(diff);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Health check & reconexión
    // ═══════════════════════════════════════════════════════════════════════════
    bool SharedAudioMemory::healthCheck() const noexcept
    {
        if (block_ == nullptr) return false;

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

    bool SharedAudioMemory::reconnect()
    {
        close();
        juce::Thread::sleep(50);
        return initialize(mapName_);
    }

} // namespace mixcoach
