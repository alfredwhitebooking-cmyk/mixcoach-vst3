#include "CoachEngine.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionMapEntry — toJson / fromJson
    // ═══════════════════════════════════════════════════════════════════════════

    void SessionMapEntry::toJson(juce::DynamicObject& obj) const
    {
        obj.setProperty("trackName", trackName);
        obj.setProperty("roleName", roleName);
        obj.setProperty("slotIndex", slotIndex);
        obj.setProperty("busType", busType);
        obj.setProperty("trackType", trackType);
        obj.setProperty("confidence", static_cast<double>(confidence));
        obj.setProperty("peakDb", static_cast<double>(peakDb));
        obj.setProperty("hasSignal", hasSignal);
    }

    SessionMapEntry SessionMapEntry::fromJson(const juce::DynamicObject& obj)
    {
        SessionMapEntry entry;
        if (obj.hasProperty("trackName")) entry.trackName = obj.getProperty("trackName").toString();
        if (obj.hasProperty("roleName")) entry.roleName = obj.getProperty("roleName").toString();
        if (obj.hasProperty("slotIndex")) entry.slotIndex = static_cast<int>(obj.getProperty("slotIndex"));
        if (obj.hasProperty("busType")) entry.busType = static_cast<int>(obj.getProperty("busType"));
        if (obj.hasProperty("trackType")) entry.trackType = static_cast<int>(obj.getProperty("trackType"));
        if (obj.hasProperty("confidence"))
            entry.confidence = static_cast<float>(static_cast<double>(obj.getProperty("confidence")));
        if (obj.hasProperty("peakDb"))
            entry.peakDb = static_cast<float>(static_cast<double>(obj.getProperty("peakDb")));
        if (obj.hasProperty("hasSignal")) entry.hasSignal = obj.getProperty("hasSignal");
        return entry;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionMapCategory — toJson / fromJson
    // ═══════════════════════════════════════════════════════════════════════════

    void SessionMapCategory::toJson(juce::DynamicObject& obj) const
    {
        obj.setProperty("name", name);
        obj.setProperty("emoji", emoji);

        juce::Array<juce::var> trackArr;
        for (const auto& track : tracks) {
            auto trackObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
            track.toJson(*trackObj);
            trackArr.add(juce::var(trackObj));
        }
        obj.setProperty("tracks", trackArr);
    }

    SessionMapCategory SessionMapCategory::fromJson(const juce::DynamicObject& obj)
    {
        SessionMapCategory cat;
        if (obj.hasProperty("name")) cat.name = obj.getProperty("name").toString();
        if (obj.hasProperty("emoji")) cat.emoji = obj.getProperty("emoji").toString();

        if (obj.hasProperty("tracks")) {
            auto trackArr = obj.getProperty("tracks").getArray();
            if (trackArr != nullptr) {
                for (int i = 0; i < trackArr->size(); ++i) {
                    auto trackObj = (*trackArr)[i].getDynamicObject();
                    if (trackObj != nullptr) cat.tracks.push_back(SessionMapEntry::fromJson(*trackObj));
                }
            }
        }
        return cat;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionMap — toJson / fromJson / toText
    // ═══════════════════════════════════════════════════════════════════════════

    void SessionMap::toJson(juce::DynamicObject& obj) const
    {
        obj.setProperty("version", 1);
        obj.setProperty("timestampUs", static_cast<int64_t>(timestampUs));
        obj.setProperty("totalTracks", totalTracks);

        juce::Array<juce::var> catArr;
        for (const auto& cat : categories) {
            auto catObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
            cat.toJson(*catObj);
            catArr.add(juce::var(catObj));
        }
        obj.setProperty("categories", catArr);
    }

    SessionMap SessionMap::fromJson(const juce::DynamicObject& obj)
    {
        SessionMap map;
        if (obj.hasProperty("timestampUs")) map.timestampUs = obj.getProperty("timestampUs");
        if (obj.hasProperty("totalTracks")) map.totalTracks = obj.getProperty("totalTracks");

        if (obj.hasProperty("categories")) {
            auto catArr = obj.getProperty("categories").getArray();
            if (catArr != nullptr) {
                for (int i = 0; i < catArr->size(); ++i) {
                    auto catObj = (*catArr)[i].getDynamicObject();
                    if (catObj != nullptr) map.categories.push_back(SessionMapCategory::fromJson(*catObj));
                }
            }
        }
        return map;
    }

    juce::String SessionMap::toText() const
    {
        if (!hasData()) return "";

        juce::String map;
        map += "\xF0\x9F\x97\xBA **SESSION MAP** \xE2\x80\x94 " + juce::String(totalTracks) + " track"
               + (totalTracks != 1 ? "s" : "") + "\n\n";

        int totalShown = 0;
        for (const auto& cat : categories) {
            if (cat.tracks.empty()) continue;

            // Header de categoría
            map += cat.emoji + "  **" + cat.name + "** (" + juce::String((int)cat.tracks.size()) + ")\n";

            for (int e = 0; e < (int)cat.tracks.size(); ++e) {
                const auto& entry = cat.tracks[e];
                bool isLast       = (e == (int)cat.tracks.size() - 1);

                // Conector: ├── o └──
                map += isLast ? "  \xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 " : "  \xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 ";

                // Rol
                if (entry.roleName.isNotEmpty()) map += "\xE2\x97\x8F " + entry.roleName;
                else
                    map += "[EMPTY] Unknown";

                // Nombre de pista (si no está duplicado en el rol)
                if (!entry.trackName.containsIgnoreCase(entry.roleName)) map += "  [RIGHT] " + entry.trackName;

                // Indicador de nivel
                if (entry.hasSignal) {
                    if (entry.peakDb > -0.5f) map += "  \xF0\x9F\x94\xB4";
                    else if (entry.peakDb > -3.0f)
                        map += "  \xF0\x9F\x9F\xA1";
                    else if (entry.peakDb > -10.0f)
                        map += "  \xF0\x9F\x9F\xA2";
                    else if (entry.peakDb > -30.0f)
                        map += "  \xE2\x9A\xAA";
                }
                else {
                    map += "  \xF0\x9F\x94\x95";
                }

                map += "\n";
                totalShown++;
            }
            map += "\n";
        }

        // Resumen al pie
        map += "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90"
           "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90"
           "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90"
           "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90"
           "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90"
           "\xE2\x95\x90\xE2\x95\x90\n"
           "[CHART] " + juce::String(totalShown) + " tracks organizados. "
           "Usa **/map** para refrescar o **/next** para continuar.";

        return map;
    }

} // namespace mixcoach
