#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "MixCoachTheme.h"
#include "ReferenceMatchPanel.h"
#include "DropZoneComponent.h"
#include "SmoothValue.h"

namespace mixcoach {

// ─── Información de archivo de audio extraída ──────────────────────────────
struct AudioFileInfo {
    double  durationSeconds = 0.0;
    int     sampleRate      = 0;
    int     bitDepth        = 0;
    bool    valid           = false;
};

// ─── Estado de análisis de la IA ───────────────────────────────────────────
enum class AnalysisStatus {
    Pending,
    Analyzing,
    Ready,
    Error
};

// ─── Referencia de mezcla (archivo o enlace) ───────────────────────────────
struct MixReference {
    enum class Type { File, URL };
    Type        type;
    juce::String name;
    juce::String path;       // Ruta de archivo o URL
    juce::Colour colour;
    int64_t     addedTime;
    AudioFileInfo audioInfo; // Metadata del archivo (solo File)
    AnalysisStatus analysisStatus = AnalysisStatus::Pending;

    // ─── Para URLs: indica que el título real se está obteniendo vía HTTP ───
    bool isFetchingTitle = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  ReferencePanelComponent — Panel de referencias unificado
//  Tab 1: REFERENCIAS — drop zone + lista unificada (archivos + URLs)
//  Tab 2: NOTAS — editor de texto libre
// ═══════════════════════════════════════════════════════════════════════════
class ReferencePanelComponent : public juce::Component,
                              private juce::Timer
{
public:
    ReferencePanelComponent();
    ~ReferencePanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void addFileReference(const juce::String& filePath);
    void addURLReference(const juce::String& url);
    void removeReference(int index);
    void clearReferences();

    [[nodiscard]] int getNumReferences() const { return static_cast<int>(references_.size()); }
    [[nodiscard]] const MixReference& getReference(int index) const { return references_[index]; }

    std::function<void()> onReferencesChanged;
    std::function<void(const juce::String&)> onFileReferenceAdded;
    std::function<void(const juce::String&, const juce::String&)> onURLReferenceAdded;
    std::function<void(int refIndex)> onPlayReference;
    /** Callback cuando el usuario selecciona una referencia para comparacion. */
    std::function<void(int refIndex)> onReferenceSelected;
    /** Callback para seek: en segundos. */
    std::function<void(double)> onSeekReference;

    /** Callback cuando el usuario selecciona una seccion (indice, -1 = global). */
    std::function<void(int sectionIndex)> onSectionSelected;
    /** Callback cuando el usuario hace clic en una seccion para ir a esa posicion. */
    std::function<void(double)> onSectionSeekTo;

    // ─── Reference-Driven Mode toggle ─────────────────────────────────────
    /** Activa/desactiva el Reference-Driven Mode. */
    void setReferenceDrivenMode(bool enabled);
    /** Retorna true si el Reference-Driven Mode está activo. */
    [[nodiscard]] bool isReferenceDrivenMode() const noexcept { return referenceDrivenMode_; }
    /** Callback cuando el usuario cambia el toggle. */
    std::function<void(bool)> onReferenceDrivenModeToggled;

    // ─── Reference progress (match % contra referencia) ──────────────────
    /** Actualiza el progreso de matching contra la referencia. */
    void setReferenceProgress(float currentMatch, float delta);

    // ─── Context menu actions for REF MODE toggle ─────────────────────────
    enum class RefModeMenuAction { ConfigureGaps, ViewHistory, ResetProgress };
    /** Callback cuando el usuario selecciona una accion del menu contextual. */
    std::function<void(RefModeMenuAction)> onRefModeMenuAction;

    /** Actualiza la posicion de reproduccion desde el engine (timer ~10Hz). */
    void updatePlaybackPosition(double positionSeconds, double totalSeconds);

    void setPlayingRefIndex(int index) { playingRefIndex_ = index; repaint(); }
    int  getPlayingRefIndex() const { return playingRefIndex_; }
    void setReferenceAnalysisStatus(int index, AnalysisStatus status);

    // ─── Match Panel (spectral + LUFS comparison) ──────────────────────
    /** Actualiza los datos de matching y refresca el panel visual. */
    void timerCallback() override;

    void updateMatchData(const DifferenceProfile& data);
    /** Retorna el panel de matching para acceso externo. */
    ReferenceMatchPanel& getMatchPanel() noexcept { return matchPanel_; }

    // ─── Section info struct (público) ─────────────────────────────────────
    struct SectionInfo {
        juce::String label;
        float startSeconds = 0.0f;
        float endSeconds = 0.0f;
    };

    /** Recibe datos de secciones desde el engine y refresca la UI. */
    void setSectionData(const std::vector<SectionInfo>& sections, int activeIndex);

    // ─── Persistencia ────────────────────────────────────────────────────
    std::vector<juce::String> getFilePaths() const;
    std::vector<juce::String> getURLs() const;
    void restoreFromPaths(const std::vector<juce::String>& filePaths,
                          const std::vector<juce::String>& urls);

private:
    // ─── Tabs ──────────────────────────────────────────────────────────────
    enum class ActiveTab { References, Notes };
    ActiveTab activeTab_ = ActiveTab::References;

    juce::Rectangle<int> refsTabBounds_;
    juce::Rectangle<int> notesTabBounds_;

    void switchTab(ActiveTab tab);

    // ─── Components ────────────────────────────────────────────────────────
    ReferenceMatchPanel matchPanel_;
    DropZoneComponent dropZone_;
    juce::TextEditor  notesEditor_;
    juce::Label       referenceCountLabel_;
    juce::Label       emptyLabel_;

    // ─── Playback bar ──────────────────────────────────────────────────────
    juce::Slider      seekSlider_;
    juce::Label       positionLabel_;
    double            playbackPosition_{0.0};
    double            playbackTotal_{0.0};
    bool              isDraggingSeek_{false};

    // ─── Section selector ──────────────────────────────────────────────────
    std::vector<SectionInfo> sections_;
    int activeSection_ = -1; // -1 = fingerprint global
    std::vector<juce::Rectangle<int>> sectionBtnBounds_;
    void drawSectionSelector(juce::Graphics& g, juce::Rectangle<int> bounds);

    std::vector<MixReference> references_;

    // ─── Audio metadata helper ─────────────────────────────────────────────
    static AudioFileInfo readAudioFileInfo(const juce::File& file);

    // ─── Section layout bounds ─────────────────────────────────────────────
    juce::Rectangle<int> addRefHeaderBounds_;  // "AGREGAR REFERENCIA"
    juce::Rectangle<int> listHeaderBounds_;    // "REFERENCIAS ({n})"
    int listDividerY_ = 0;                     // Y position of divider line

    // ─── Indice de la referencia activa para el match panel ──────────────
    int matchRefIndex_ = -1;

    // ─── Display ──────────────────────────────────────────────────────────
    void refreshDisplay();

    // ─── Reference-Driven Mode state ────────────────────────────────────
    bool referenceDrivenMode_ = false;
    float referenceProgressRaw_ = 0.0f;   // 0.0-1.0 match percentage (raw for text)
    float referenceDeltaRaw_ = 0.0f;      // + = improving, - = worsening (raw for text)

    // SmoothValue animators for the progress bar fill and delta arrow
    SmoothValue progressFillSmooth_{ 0.0f, 150.0f, 800.0f };
    SmoothValue deltaSmooth_{ 0.0f, 300.0f, 800.0f };

    // ─── Toggle and progress bar bounds ──────────────────────────────────
    juce::Rectangle<int> refModeToggleBounds_;
    juce::Rectangle<int> refProgressBounds_;
    void drawReferenceRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                          const MixReference& ref, int index, int visualIndex);

    // Click / Drag handling
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    // Row bounds for hit testing
    std::vector<juce::Rectangle<int>> visibleRowBounds_;
    std::vector<juce::Rectangle<int>> playBtnBounds_;
    std::vector<juce::Rectangle<int>> deleteBtnBounds_;
    bool wasHovering_ = false;
    int playingRefIndex_ = -1;

    // ─── Drag & drop reorder ───────────────────────────────────────────────
    bool isDraggingRow_ = false;
    int  dragSourceVisualIdx_ = -1;     // visual index being dragged
    int  dragSourceArrayIdx_ = -1;      // array index being dragged
    int  dropTargetVisualIdx_ = -1;     // visual index where to insert before
    juce::Point<int> mouseDownPos_;
    void drawDropIndicator(juce::Graphics& g, juce::Rectangle<int> rowBounds, bool above);

    // ═══ URL Title Extraction — Async helpers ═══════════════════════════════
    /** Actualiza el nombre de una referencia (usado por el async title fetcher). */
    void updateReferenceName(int refIndex, const juce::String& newName);

    /** Extrae el título de una URL vía HTTP (bloqueante — ejecutar en background).
        Retorna la string vacía si falla. */
    static juce::String fetchURLTitle(const juce::String& url, int timeoutMs = 4000);

    /** Parsea el título de YouTube/Spotify del HTML. */
    static juce::String parseHTMLTitle(const juce::String& html, const juce::String& url);

    /** Extrae el nombre de una URL de SoundCloud (artista/tema del path). */
    static juce::String extractSoundCloudName(const juce::String& url);

    /** Extrae un nombre descriptivo de una URL (para SoundCloud: track name;
        para otros: intenta fetch HTTP). Retorna string vacía si no puede. */
    static juce::String extractURLName(const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferencePanelComponent)
};

} // namespace mixcoach
