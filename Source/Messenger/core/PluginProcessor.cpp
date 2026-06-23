#include "PluginProcessor.h"
#include "../ui/PluginEditor.h"
#include "../../Common/types/LogHelper.h"
#include "../../Common/name/NameInferrer.h"

namespace mixcoach {

    // ─── Contador de nombres por defecto (incrementa por instancia) ─────────────
    static int defaultTrackCounter = 0;

    // ─── Constructor: ABSOLUTAMENTE NADA que pueda crashear ─────────────────────
    // Durante escaneo VST3: ni SharedData, ni archivos, ni FFT.
    // Todo se inicializa LAZY en ensureSlotRegistered() / prepareToPlay().
    MessengerAudioProcessor::MessengerAudioProcessor() :
        AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        trackName_ = "Pista " + juce::String(++defaultTrackCounter);
    }

    MessengerAudioProcessor::~MessengerAudioProcessor()
    {
        slotIndex_      = -1;
        slotRegistered_ = false;
        sharedData_     = nullptr;
    }

    // ─── Inicialización lazy (no en constructor!) ─────────────────────────────
    void MessengerAudioProcessor::ensureSlotRegistered()
    {
        if (slotIndex_ >= 0 && sharedData_ != nullptr) return;

        try {
            if (!sharedData_) sharedData_ = &SharedData::getInstance();

            if (!sharedData_) return;

            auto& registry = sharedData_->getSlotRegistry();
            slotIndex_     = registry.registerSlot(trackName_.toStdString(), trackColour_, busAssignment_);

            if (slotIndex_ >= 0) {
                slotRegistered_ = true;
                // ═══ V7 Identity Layer: sync trackType inmediatamente ═══
                syncTrackTypeToRegistry();
            }
        }
        catch (...) {
            sharedData_ = nullptr;
            slotIndex_  = -1;
        }
    }

    void MessengerAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
    {
        prepared_ = true;
        try {
            ensureSlotRegistered();
        }
        catch (...) {
        }
    }

    void MessengerAudioProcessor::releaseResources() {}

    // ═══════════════════════════════════════════════════════════════════════════
    //  processBlock — SENSOR PURO (V3: solo transmite audio RAW + identidad)
    // ═══════════════════════════════════════════════════════════════════════════
    // 1. Audio pasa INTACTO (passthrough, ni un cálculo por muestra)
    // 2. Escribe audio MONO al SharedAudioMemory (IPC cross-process) para que
    //    MixCoach lo lea desde su propio proceso y analice desde el Master
    // 3. Mantiene heartbeat + identidad en shared memory
    // 4. SIN RMS, SIN Peak, SIN FFT, SIN LUFS, SIN nada — eso lo hace MixCoach
    void MessengerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        if (!prepared_) return;

        auto numSamples  = buffer.getNumSamples();
        auto numChannels = buffer.getNumChannels();

        // ─── Asegurar slot registrado (solo la primera vez) ──────────────
        if (!slotRegistered_) {
            ensureSlotRegistered();
            if (!slotRegistered_ || !sharedData_) return;
        }

        // ─── Audio Signal Detection V11 (ultraligero) ─────────────────────
        // Detecta cuando el audio empieza a fluir (transicion silencio->senal).
        // CPU ultrabajo: revisa 1 de cada 64 samples (~1.5% de los samples).
        // NO hace FFT, NO procesa — solo detecta presencia de senal.
        if (audioSignalDetected_.load(std::memory_order_relaxed) == SignalState::Waiting) {
            for (int ch = 0; ch < numChannels && ch < 2; ++ch) {
                const float* channelData = buffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; i += 64) {
                    if (std::abs(channelData[i]) > 0.0005f) {
                        audioSignalDetected_.store(SignalState::Detected, std::memory_order_relaxed);
                        LogHelper::writeToLog("[Messenger-Signal] Slot " + juce::String(slotIndex_)
                                              + " audio detectado!");
                        break;
                    }
                }
                if (audioSignalDetected_.load(std::memory_order_relaxed) == SignalState::Detected) break;
            }
        }

        // ─── Escribir audio RAW al SharedAudioMemory (IPC cross-process)
        // MixCoach lee este buffer desde su propio proceso para analizar
        // pistas individuales. Usamos shared memory separada para no
        // contaminar el mapping de identidad con datos de audio.
        if (!muted_.load(std::memory_order_relaxed) && slotIndex_ >= 0 && numChannels > 0) {
            // Write audio data. If stereo enabled, write left and right channels separately using SharedAudioMemoryV2.
            if (useStereo_) {
                const float* left  = buffer.getReadPointer(0);
                const float* right = (numChannels > 1) ? buffer.getReadPointer(1) : nullptr;
                // Use sharedData_->getAudioMemoryV2() for stereo IPC
                if (right) {
                    // Write both channels
                    sharedData_->getAudioMemoryV2().writeStereoSamples(slotIndex_, left, right, numSamples);
                }
                else {
                    // Mono input: duplicate to both channels
                    sharedData_->getAudioMemoryV2().writeStereoSamples(slotIndex_, left, left, numSamples);
                }
            }
            else {
                // Existing mono path
                const float* ch0 = buffer.getReadPointer(0);
                const float* ch1 = (numChannels > 1) ? buffer.getReadPointer(1) : nullptr;
                // Pre-mezclar a mono para reducir throughput a la mitad
                if (ch1) {
                    for (int i = 0; i < numSamples; i += 64) {
                        int chunkEnd = (std::min)(i + 64, numSamples);
                        float temp[64];
                        for (int j = i; j < chunkEnd; ++j) temp[j - i] = (ch0[j] + ch1[j]) * 0.5f;
                        sharedData_->getAudioMemory().writeSamples(slotIndex_, temp, chunkEnd - i);
                    }
                }
                else {
                    for (int i = 0; i < numSamples; i += 64) {
                        int chunkEnd = (std::min)(i + 64, numSamples);
                        sharedData_->getAudioMemory().writeSamples(slotIndex_, ch0 + i, chunkEnd - i);
                    }
                }
            }
        }

        // ─── Heartbeat (cada ~85ms a 48kHz)
        // Señal de vida: MixCoach detecta si el Messenger sigue activo
        lastHeartbeatMs_.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);

        // ─── Mantener slot activo en shared memory
        if (!muted_.load(std::memory_order_relaxed)) {
            auto& registry = sharedData_->getSlotRegistry();
            registry.setActive(slotIndex_, true);
        }
    }

    juce::AudioProcessorEditor* MessengerAudioProcessor::createEditor()
    {
        return new MessengerAudioProcessorEditor(*this);
    }

    // ─── APIs de identidad ─────────────────────────────────────────────────────

    void MessengerAudioProcessor::setTrackName(const juce::String& newName)
    {
        trackName_ = newName;
        try {
            if (slotIndex_ >= 0 && sharedData_)
                sharedData_->getSlotRegistry().updateSlotName(slotIndex_, newName.toStdString());
        }
        catch (...) {
        }
    }

    void MessengerAudioProcessor::setTrackType(TrackType type)
    {
        trackType_       = type;
        trackTypePinned_ = true; // Marcar que el usuario lo selecciono manualmente
        // Auto-asignar color y bus sugeridos según el tipo seleccionado
        setTrackColour(getTrackTypeColour(type));
        setBusAssignment(getTrackTypeBus(type));
        // ═══ V7 Identity Layer: sync trackType a SlotRegistry ═══
        syncTrackTypeToRegistry();
    }

    // ═══ V7 Identity Layer: Sincroniza el TrackType del Messenger al SlotRegistry ═══
    // Esto permite que MixCoach lea el tipo explícito que seleccionó el usuario,
    // sin tener que inferirlo desde el nombre o el espectro.
    void MessengerAudioProcessor::syncTrackTypeToRegistry()
    {
        try {
            if (slotIndex_ >= 0 && sharedData_) {
                auto& registry = sharedData_->getSlotRegistry();
                registry.updateSlotTrackType(slotIndex_, static_cast<int>(trackType_));
            }
        }
        catch (...) {
        }
    }

    // ═══ Name Auto-Suggestion V10: Sugerir TrackType desde el nombre ──────────
    // Usa NameInferrer (Common) para detectar palabras clave multilingüe (EN+ES)
    // y mapear nombres comunes a TrackType.
    // ═══════════════════════════════════════════════════════════════════════════

    TrackType MessengerAudioProcessor::suggestTrackTypeFromName(const juce::String& rawName) noexcept
    {
        auto kw = NameInferrer::detectKeywords(rawName);
        if (kw.isEmpty) return TrackType::None;

        // ─── Árbol de decisión → TrackType ────────────────────────────────

        // Kick 808
        if (kw.has808Kick || (kw.has808 && kw.hasKick)) return TrackType::ReggaetonKick;

        // Kick
        if (kw.hasKick) return TrackType::Kick;

        // Snare
        if (kw.hasSnare) return TrackType::Snare;

        // 808 Bass
        if (kw.has808Bass || (kw.has808 && kw.hasBass)) return TrackType::Bass808;
        if (kw.has808) return TrackType::Bass808;

        // HiHat
        if (kw.hasHiHat) return TrackType::HiHat;

        // Tom
        if (kw.hasTom) return TrackType::Tom;

        // Clap
        if (kw.hasClap) return TrackType::Percussion;

        // Crash / Ride
        if (kw.hasCrash || kw.hasRide) return TrackType::Percussion;

        // Percusión general
        if (kw.hasPerc) return TrackType::Percussion;

        // Bass general
        if (kw.hasBass || kw.hasSub) return TrackType::BassDI;

        // Vocals
        if (kw.hasLeadVocal) return TrackType::LeadVocal;
        if (kw.hasBackVocal) return TrackType::DoubleVocal;
        if (kw.hasAdlib) return TrackType::Adlibs;
        if (kw.hasVocal) return TrackType::LeadVocal;

        // Guitar
        if (kw.hasGuitar) return TrackType::Guitar;

        // Piano / Keys
        if (kw.hasPiano) return TrackType::Piano;

        // Synth
        if (kw.hasSynth && kw.hasStrings) return TrackType::Strings;
        if (kw.hasSynth) return TrackType::SynthLead;

        // Strings
        if (kw.hasStrings) return TrackType::Strings;

        // Room / Overheads
        if (kw.hasRoom) return TrackType::Room;

        // FX
        if (kw.hasRiser) return TrackType::Risers;
        if (kw.hasImpact) return TrackType::Impacts;
        if (kw.hasAmbient) return TrackType::Ambience;

        // Drum Bus
        if (kw.hasDrumBus) return TrackType::Overheads;

        // Reggaeton general
        if (kw.hasReggaeton) return TrackType::ReggaetonKick;

        return TrackType::None;
    }

    // ═══ Name Auto-Fill V11: Auto-llenar nombre desde TrackType inferido ──────
    bool MessengerAudioProcessor::autoFillNameFromTrackType(TrackType suggestedType)
    {
        if (suggestedType == TrackType::None) return false;

        // Solo auto-llenar si el nombre sigue siendo el generico "Pista X"
        if (!hasDefaultName()) return false;

        // Obtener nombre legible del TrackType (ej: TrackType::Kick -> "Kick")
        juce::String typeName = juce::String(getTrackTypeName(suggestedType));
        if (typeName.isEmpty() || typeName == "\xE2\x80\x94") return false;

        LogHelper::writeToLog("[Messenger-Identity-V11] Slot " + juce::String(slotIndex_) + " auto-nombrado: \""
                              + trackName_ + "\" -> \"" + typeName + "\"");

        // Establecer el nuevo nombre (esto tambien sincroniza a SlotRegistry)
        setTrackName(typeName);
        return true;
    }

    // ═══ Name Auto-Suggestion V10: setTrackTypeAutoSuggested ──────────────────
    void MessengerAudioProcessor::setTrackTypeAutoSuggested(TrackType type)
    {
        if (type == TrackType::None) return;

        trackType_ = type;
        // NO marcar trackTypePinned_ — esto fue auto-sugerido, no seleccion manual
        setTrackColour(getTrackTypeColour(type));
        setBusAssignment(getTrackTypeBus(type));
        syncTrackTypeToRegistry();
    }

    // ═══ Feedback Loop V9: Leer TrackType inferido por MixCoach desde shared memory ═══
    // MixCoach escribe el TrackType inferido al slot via updateSlotTrackType().
    // Este método lee ese cambio desde shared memory directamente y actualiza
    // el estado local. Retorna true si hubo cambio (para que el editor actualice UI).
    // Solo aplica si el usuario NO ha fijado manualmente el tipo (trackTypePinned_).
    bool MessengerAudioProcessor::syncTrackTypeFromSharedMemory()
    {
        if (slotIndex_ < 0 || !sharedData_) return false;

        // Si el usuario selecciono manualmente, no sobrescribir
        if (trackTypePinned_) return false;

        try {
            // Leer directamente de shared memory (cross-process)
            auto& shm = sharedData_->getSharedMemory();
            SharedSlotEntry entry;
            if (shm.readSlot(slotIndex_, entry)) {
                int newType = entry.trackType;
                // Solo actualizar si hay un TrackType valido y es diferente
                if (newType >= 0 && newType != static_cast<int>(trackType_)) {
                    trackType_ = static_cast<TrackType>(newType);
                    // Actualizar color y bus segun el nuevo tipo
                    setTrackColour(getTrackTypeColour(trackType_));
                    setBusAssignment(getTrackTypeBus(trackType_));

                    // ═══ Name Auto-Fill V11: si el nombre es "Pista X", auto-llenar ─
                    // Cuando MixCoach infiere un TrackType (Kick, Snare, etc.),
                    // auto-llenamos el nombre para que el usuario no tenga que
                    // escribirlo manualmente.
                    autoFillNameFromTrackType(trackType_);

                    return true;
                }
            }
        }
        catch (...) {
        }

        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setFaderDb / setPanValue — Escribe fader/pan a shared memory (V9)
    // ═══════════════════════════════════════════════════════════════════════════

    void MessengerAudioProcessor::setFaderDb(float db)
    {
        faderDb_ = db;
        try {
            if (slotIndex_ >= 0 && sharedData_) sharedData_->getSlotRegistry().updateSlotFaderDb(slotIndex_, db);
        }
        catch (...) {
        }
    }

    void MessengerAudioProcessor::setPanValue(float pan)
    {
        panValue_ = pan;
        try {
            if (slotIndex_ >= 0 && sharedData_) sharedData_->getSlotRegistry().updateSlotPanValue(slotIndex_, pan);
        }
        catch (...) {
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setMuted / setSoloed — Escribe estado de mute/solo a shared memory (V8)
    // ═══════════════════════════════════════════════════════════════════════════

    void MessengerAudioProcessor::setMuted(bool mute)
    {
        muted_.store(mute, std::memory_order_relaxed);
        try {
            if (slotIndex_ >= 0 && sharedData_) sharedData_->getSlotRegistry().updateSlotMuted(slotIndex_, mute);
        }
        catch (...) {
        }
    }

    void MessengerAudioProcessor::setSoloed(bool solo)
    {
        soloed_.store(solo, std::memory_order_relaxed);
        try {
            if (slotIndex_ >= 0 && sharedData_) sharedData_->getSlotRegistry().updateSlotSoloed(slotIndex_, solo);
        }
        catch (...) {
        }
    }

    void MessengerAudioProcessor::setTrackColour(const juce::Colour& newColour)
    {
        trackColour_ = newColour;
        try {
            if (slotIndex_ >= 0 && sharedData_) sharedData_->getSlotRegistry().updateSlotColour(slotIndex_, newColour);
        }
        catch (...) {
        }
    }

    void MessengerAudioProcessor::setBusAssignment(BusType bus)
    {
        busAssignment_ = bus;
        try {
            if (slotIndex_ >= 0 && sharedData_) sharedData_->getSlotRegistry().updateSlotBus(slotIndex_, bus);
        }
        catch (...) {
        }
    }

    // ─── Estado persistente ─────────────────────────────────────────────────────

    static constexpr int kStateVersion = 2;

    void MessengerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
    {
        try {
            juce::MemoryOutputStream mos(destData, false);
            // Version marker para detectar formato V1 vs V2
            mos.writeInt(kStateVersion);
            auto nameStr = trackName_.toStdString();
            auto nameLen = static_cast<int>(nameStr.length());
            mos.writeInt(nameLen);
            if (nameLen > 0) mos.write(nameStr.data(), nameLen);
            mos.writeInt(static_cast<int>(trackType_));
            mos.writeInt(static_cast<int>(trackColour_.getARGB()));
            mos.writeInt(static_cast<int>(busAssignment_));
            mos.writeInt(muted_ ? 1 : 0);
            mos.writeInt(soloed_ ? 1 : 0);
        }
        catch (...) {
        }
    }

    void MessengerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
    {
        if (sizeInBytes < 4) return;
        try {
            juce::MemoryInputStream mis(data, sizeInBytes, false);

            int version = mis.readInt();

            if (version == kStateVersion) {
                // ─── Formato V2 (Messenger V2) ───────────────────────────────
                auto nameLen = mis.readInt();
                if (nameLen > 0 && nameLen <= 256 && mis.getNumBytesRemaining() >= nameLen) {
                    std::vector<char> nameBuf(nameLen + 1, 0);
                    mis.read(nameBuf.data(), nameLen);
                    trackName_ = juce::String::fromUTF8(nameBuf.data());
                }
                if (mis.getNumBytesRemaining() >= 4) {
                    trackType_ = static_cast<TrackType>(mis.readInt());
                    if (trackType_ != TrackType::None) trackTypePinned_ = true; // Restaurar estado de pin del usuario
                }
                if (mis.getNumBytesRemaining() >= 4)
                    trackColour_ = juce::Colour(static_cast<juce::uint32>(mis.readInt()));
                if (mis.getNumBytesRemaining() >= 4) busAssignment_ = static_cast<BusType>(mis.readInt());
                if (mis.getNumBytesRemaining() >= 4) muted_.store(mis.readInt() != 0, std::memory_order_relaxed);
            }
            else {
                // ─── Formato V1 (Messenger V1 legacy) ────────────────────────
                // Formato: [slotIndex:4][nameLen:4][name:N][colour:4][bus:4][muted:4]
                // El primer int es slotIndex (no version). Lo descartamos.
                if (sizeInBytes >= 8) {
                    auto nameLen = mis.readInt();
                    if (nameLen > 0 && nameLen <= 256 && mis.getNumBytesRemaining() >= nameLen) {
                        std::vector<char> nameBuf(nameLen + 1, 0);
                        mis.read(nameBuf.data(), nameLen);
                        trackName_ = juce::String::fromUTF8(nameBuf.data());
                    }
                    if (mis.getNumBytesRemaining() >= 4)
                        trackColour_ = juce::Colour(static_cast<juce::uint32>(mis.readInt()));
                    if (mis.getNumBytesRemaining() >= 4) busAssignment_ = static_cast<BusType>(mis.readInt());
                    if (mis.getNumBytesRemaining() >= 4) muted_.store(mis.readInt() != 0, std::memory_order_relaxed);
                    // trackType_ defaults to None (legacy Messenger no tenía tipo)
                    // soloed_ defaults to false (legacy no tenía)
                }
            }

            // Leer soloed_ si hay datos suficientes (V8+)
            if (mis.getNumBytesRemaining() >= 4) {
                soloed_.store(mis.readInt() != 0, std::memory_order_relaxed);
            }

            // Sync metadata with slot registry if slot is registered
            if (slotIndex_ >= 0 && sharedData_) {
                auto& registry = sharedData_->getSlotRegistry();
                registry.updateSlotName(slotIndex_, trackName_.toStdString());
                registry.updateSlotColour(slotIndex_, trackColour_);
                registry.updateSlotBus(slotIndex_, busAssignment_);
                registry.updateSlotMuted(slotIndex_, muted_.load(std::memory_order_relaxed));
                registry.updateSlotSoloed(slotIndex_, soloed_.load(std::memory_order_relaxed));
            }
        }
        catch (...) {
        }
    }

} // namespace mixcoach

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mixcoach::MessengerAudioProcessor();
}
