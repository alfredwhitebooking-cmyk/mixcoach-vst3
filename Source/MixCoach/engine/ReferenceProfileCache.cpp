#include "ReferenceProfile.h"
#include "../../Common/types/LogHelper.h"
#include <juce_core/juce_core.h>
#include <cstdint>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceProfileCache Implementation
    // ═══════════════════════════════════════════════════════════════════════════

    ReferenceProfileCache::ReferenceProfileCache(const juce::File& cacheDirectory)
        : cacheDir_(cacheDirectory)
    {
        if (!cacheDir_.exists())
            cacheDir_.createDirectory();
    }

    juce::String ReferenceProfileCache::sanitizeKey(const juce::String& key)
    {
        // Hash simple: reemplazar caracteres no seguros para nombre de archivo
        juce::String safe;
        for (int i = 0; i < key.length(); ++i) {
            auto c = key[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-' || c == '_')
                safe += c;
            else
                safe += '_';
        }
        return safe;
    }

    juce::String ReferenceProfileCache::makeKey(const juce::String& filePath)
    {
        // Genera un hash SHA del filePath para usar como cache key
        juce::String path = filePath.trim();
        if (path.isEmpty()) return {};

        // Hash FNV-1a de 64 bits (rápido, portable, sin dependencias externas)
        uint64_t hash = 14695981039346656037ULL; // FNV_offset_basis
        auto utf8 = path.toUTF8();
        const char* data = utf8.getAddress();
        size_t len = utf8.sizeInBytes() - 1; // sin null terminator
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
            hash *= 1099511628211ULL; // FNV_prime
        }

        // Convertir a hex string de 16 chars
        juce::String hex;
        for (int i = 0; i < 16; ++i) {
            int nibble = (int)((hash >> (60 - i * 4)) & 0xFULL);
            hex += (nibble < 10) ? static_cast<juce::juce_wchar>('0' + nibble)
                                 : static_cast<juce::juce_wchar>('a' + nibble - 10);
        }
        return hex;
    }

    void ReferenceProfileCache::save(const juce::String& key, const ReferenceProfile& profile)
    {
        if (key.isEmpty() || !profile.valid) return;

        juce::String filename = sanitizeKey(key) + ".json";
        juce::File cacheFile  = cacheDir_.getChildFile(filename);

        if (!profile.saveToFile(cacheFile)) {
            LogHelper::writeToLog("[ReferenceProfileCache] Error guardando cache: " + key);
            return;
        }

        LogHelper::writeToLog("[ReferenceProfileCache] Cache guardado: " + key + " -> " + cacheFile.getFullPathName());
    }

    ReferenceProfile ReferenceProfileCache::load(const juce::String& key) const
    {
        if (key.isEmpty()) return ReferenceProfile{};

        juce::String filename = sanitizeKey(key) + ".json";
        juce::File cacheFile  = cacheDir_.getChildFile(filename);

        if (!cacheFile.existsAsFile()) return ReferenceProfile{};

        auto profile = ReferenceProfile::loadFromFile(cacheFile);
        if (profile.valid) {
            LogHelper::writeToLog("[ReferenceProfileCache] Cache cargado: " + key);
        }
        return profile;
    }

    bool ReferenceProfileCache::has(const juce::String& key) const
    {
        if (key.isEmpty()) return false;
        juce::String filename = sanitizeKey(key) + ".json";
        return cacheDir_.getChildFile(filename).existsAsFile();
    }

    void ReferenceProfileCache::remove(const juce::String& key)
    {
        if (key.isEmpty()) return;
        juce::String filename = sanitizeKey(key) + ".json";
        juce::File cacheFile  = cacheDir_.getChildFile(filename);
        if (cacheFile.existsAsFile()) {
            cacheFile.deleteFile();
            LogHelper::writeToLog("[ReferenceProfileCache] Cache eliminado: " + key);
        }
    }

    void ReferenceProfileCache::clearAll()
    {
        if (!cacheDir_.exists()) return;

        juce::Array<juce::File> results;
        cacheDir_.findChildFiles(results, juce::File::findFiles, false, "*.json");
        for (auto& f : results)
            f.deleteFile();

        LogHelper::writeToLog("[ReferenceProfileCache] Cache limpiado: " + juce::String(results.size()) + " archivos");
    }

} // namespace mixcoach
