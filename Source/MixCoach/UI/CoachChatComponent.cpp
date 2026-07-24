#include "CoachChatComponent.h"
#include "TrackProblemBuilder.h"
#include "../engine/CoachEngine.h"
#include "../engine/TrackRole.h"
#include "../../Common/types/Constants.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers — Text measurement without JUCE 8 TextLayout + diagnostic logging
    // ═══════════════════════════════════════════════════════════════════════════
    namespace {

        // ─── writeChatLog — Escribe directamente a MixCoach_Crash.log (SIN LogHelper)
        //     LogHelper requiere inicialización via setLogFile(), que puede no ocurrir
        //     si el plugin se inicializa con retry. Esta función escribe al mismo archivo
        //     que use el sistema de crash logging, garantizando que los mensajes SIEMPRE
        //     lleguen a un archivo.
        //     Formato: [timestamp] [CHAT] mensaje
        static void writeChatLog(const juce::String& msg)
        {
            try {
                auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("MixCoach_Logs")
                                   .getChildFile("MixCoach_Crash.log");
                logFile.getParentDirectory().createDirectory();
                juce::FileOutputStream fos(logFile, true);
                if (fos.openedOk()) {
                    fos << "[" << juce::Time::getCurrentTime().toString(true, true) << "] [CHAT] " << msg << "\n";
                    fos.flush();
                }
            }
            catch (const std::exception& e) {
                juce::ignoreUnused(e);
                // Intencionalmente silencioso — no podemos loggear un fallo del sistema de logging
            }
        }

        static float getTextWidth(const juce::Font& font, const juce::String& text)
        {
            if (text.isEmpty()) return 0.0f;
            juce::GlyphArrangement ga;
            ga.addLineOfText(font, text, 0.0f, 0.0f);
            return ga.getBoundingBox(0, ga.getNumGlyphs(), true).getWidth();
        }

        static float measureWrappedHeight(const juce::String& text, const juce::Font& font, float maxWidth)
        {
            if (text.isEmpty()) return font.getHeight();

            auto paragraphs = juce::StringArray::fromLines(text);
            float totalH    = 0.0f;
            float lineH     = font.getHeight();
            float spaceW    = getTextWidth(font, " ");

            for (int p = 0; p < paragraphs.size(); ++p) {
                auto words  = juce::StringArray::fromTokens(paragraphs[p], " ", "");
                float lineW = 0.0f;

                for (int i = 0; i < words.size(); ++i) {
                    float wordW = getTextWidth(font, words[i]);

                    if (lineW + (lineW > 0.0f ? spaceW : 0.0f) + wordW > maxWidth && lineW > 0.0f) {
                        totalH += lineH + 2.0f;
                        lineW = 0.0f;
                        --i;
                        continue;
                    }

                    lineW += (lineW > 0.0f ? spaceW : 0.0f) + wordW;
                }

                totalH += lineH + 2.0f;

                if (p < paragraphs.size() - 1) totalH += lineH * 0.5f;
            }

            return totalH;
        }

        // ═══════════════════════════════════════════════════════════════════════════
        //  MarkdownSegment + parseMarkdown — Tokenización básica de **bold**
        //  DEFINIDOS ANTES de measureMarkdownHeight para que el compilador vea el tipo completo
        // ═══════════════════════════════════════════════════════════════════════════
        struct MarkdownSegment
        {
            juce::String text;
            bool isBold;
        };

        /** Divide el texto en segmentos: normal y bold según **bloque** */
        static std::vector<MarkdownSegment> parseMarkdown(const juce::String& text)
        {
            std::vector<MarkdownSegment> segments;
            if (text.isEmpty()) return segments;

            int pos = 0;
            while (pos < text.length()) {
                int openIdx = text.indexOf(pos, "**");
                if (openIdx < 0) {
                    // Resto del texto sin markers
                    segments.push_back({text.substring(pos), false});
                    break;
                }
                // Texto antes del marker
                if (openIdx > pos) segments.push_back({text.substring(pos, openIdx), false});

                // Buscar cierre **
                int closeIdx = text.indexOf(openIdx + 2, "**");
                if (closeIdx < 0) {
                    // No hay cierre — tratar resto como normal
                    segments.push_back({text.substring(pos), false});
                    break;
                }
                // Texto bold
                segments.push_back({text.substring(openIdx + 2, closeIdx), true});
                pos = closeIdx + 2;
            }

            return segments;
        }

        /** Mide la altura real del texto markdown con word-wrap, midiendo palabras bold
            con la fuente boldened para obtener wrap EXACTO en vez de un multiplicador fijo. */
        static float measureMarkdownHeight(const juce::String& text,
                                            const juce::Font& baseFont,
                                            float maxWidth)
        {
            if (text.isEmpty()) return baseFont.getHeight();

            auto segments = parseMarkdown(text);
            if (segments.empty()) return baseFont.getHeight();

            juce::Font boldFont = baseFont.boldened();

            float lineH  = baseFont.getHeight();
            float spaceW = getTextWidth(baseFont, " ");
            float totalH = 0.0f;

            // Build word list from segments
            struct Word { juce::String text; bool isBold; };
            std::vector<Word> words;
            for (auto& seg : segments) {
                if (seg.text.isEmpty()) continue;
                auto tokens = juce::StringArray::fromTokens(seg.text, " ", "");
                for (int i = 0; i < tokens.size(); ++i) {
                    if (tokens[i].isNotEmpty()) words.push_back({tokens[i], seg.isBold});
                }
            }

            if (words.empty()) return lineH + 2.0f;

            // Word-wrap con medición bold-aware (exactamente como drawMarkdownText)
            int wordIdx = 0;
            while (wordIdx < (int)words.size()) {
                float lineW  = 0.0f;
                int startIdx = wordIdx;

                while (wordIdx < (int)words.size()) {
                    auto& w       = words[wordIdx];
                    juce::Font& f = w.isBold ? boldFont : const_cast<juce::Font&>(baseFont);
                    float wordW   = getTextWidth(f, w.text);
                    float sep     = (lineW > 0.0f && startIdx < wordIdx) ? spaceW : 0.0f;

                    if (lineW + sep + wordW > maxWidth && startIdx < wordIdx) break;

                    lineW += sep + wordW;
                    wordIdx++;
                }

                if (startIdx == wordIdx) wordIdx = startIdx + 1;

                totalH += lineH + 2.0f;
            }

            return totalH;
        }

        // ═══════════════════════════════════════════════════════════════════════════
        //  drawMarkdownText — Text wrapping con soporte markdown básico (**bold**)
        //  Tokeniza el texto por marcadores **, alternando entre normal y bold.
        //  NOTA: MarkdownSegment + parseMarkdown están definidos más arriba en este archivo.
        // ═══════════════════════════════════════════════════════════════════════════
        /** Dibuja texto con word-wrap, usando boldened font para segmentos **bold** */
        static void drawMarkdownText(juce::Graphics& g,
                                     const juce::String& text,
                                     const juce::Font& baseFont,
                                     const juce::Colour& colour,
                                     juce::Rectangle<float> area)
        {
            if (text.isEmpty()) return;

            auto segments = parseMarkdown(text);
            if (segments.empty()) return;

            juce::Font boldFont = baseFont.boldened();

            float maxW   = area.getWidth();
            float x      = area.getX();
            float y      = area.getY();
            float lineH  = baseFont.getHeight();
            float spaceW = getTextWidth(baseFont, " ");

            // Word-wrap: construimos línea por línea con segmentos
            // Para simplificar, mezclamos todo en una línea de marcadores
            // y hacemos wrap a nivel de palabra
            struct Word
            {
                juce::String text;
                bool isBold;
            };

            std::vector<Word> words;
            for (auto& seg : segments) {
                if (seg.text.isEmpty()) continue;
                // Dividir segmento en palabras
                auto tokens = juce::StringArray::fromTokens(seg.text, " ", "");
                for (int i = 0; i < tokens.size(); ++i) {
                    if (tokens[i].isNotEmpty()) words.push_back({tokens[i], seg.isBold});
                }
            }

            // Renderizar word-wrap con medición bold-aware
            int wordIdx = 0;
            while (wordIdx < (int)words.size()) {
                float lineW  = 0.0f;
                int startIdx = wordIdx;

                // Medir cuántas palabras caben en esta línea
                while (wordIdx < (int)words.size()) {
                    auto& w       = words[wordIdx];
                    juce::Font& f = w.isBold ? boldFont : const_cast<juce::Font&>(baseFont);
                    float wordW   = getTextWidth(f, w.text);
                    float sep     = (lineW > 0.0f && startIdx < wordIdx) ? spaceW : 0.0f;

                    if (lineW + sep + wordW > maxW && startIdx < wordIdx) break;

                    lineW += sep + wordW;
                    wordIdx++;
                }

                if (startIdx == wordIdx) {
                    // Una palabra más ancha que maxW — dibujar de todas formas
                    wordIdx = startIdx + 1;
                }

                // Dibujar la línea con los segmentos correspondientes
                float drawX = x;
                for (int i = startIdx; i < wordIdx; ++i) {
                    auto& w = words[i];
                    g.setFont(w.isBold ? boldFont : baseFont);
                    g.setColour(w.isBold ? colour.brighter(0.3f) : colour);

                    float wordW = w.isBold ? getTextWidth(boldFont, w.text) : getTextWidth(baseFont, w.text);
                    juce::Rectangle<float> wordRect(drawX, y, wordW + 1.0f, lineH);
                    g.drawText(w.text, wordRect, juce::Justification::centredLeft);

                    drawX += wordW;
                    if (i < wordIdx - 1) drawX += spaceW;
                }

                y += lineH + 2.0f;
            }
        }

        // ─── AM/PM timestamp formatter ─────────────────────────────────────────────
        static juce::String formatTimestamp()
        {
            auto now          = juce::Time::getCurrentTime();
            int hours         = now.getHours();
            int mins          = now.getMinutes();
            juce::String ampm = (hours < 12) ? "AM" : "PM";
            if (hours == 0) hours = 12;
            else if (hours > 12)
                hours -= 12;
            return juce::String(hours) + ":" + (mins < 10 ? "0" : "") + juce::String(mins) + " " + ampm;
        }

    } // anonymous namespace

    // ═══════════════════════════════════════════════════════════════════════════
    //  ChatMessagesComponent — Renderiza burbujas de chat
    // ═══════════════════════════════════════════════════════════════════════════

    /** Ease-out quad: suave, aceleración decreciente. */
    static float easeOutQuad(float t) noexcept
    {
        return 1.0f - (1.0f - t) * (1.0f - t);
    }

    ChatMessagesComponent::ChatMessagesComponent()
    {
        setOpaque(false);
        startTimerHz(60);
    }

    void ChatMessagesComponent::addMessage(const juce::String& text, const juce::String& tag)
    {
        ChatBubble bubble;
        bubble.text      = text;
        bubble.tag       = tag;
        bubble.isUser    = false;
        bubble.isSystem  = false;
        bubble.timestamp = formatTimestamp();                bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();
        messages_.push_back(bubble);
        startTimerHz(60);
        {
            juce::String preview = text.substring(0, 80);
            preview              = preview.replace("\n", " ");
            writeChatLog("[ChatBubble] addMessage (coach): \"" + preview + "\" (len=" + juce::String(text.length())
                         + ")");
        }
        resized();
        repaint();
    }

    void ChatMessagesComponent::addUserMessage(const juce::String& text)
    {
        ChatBubble bubble;
        bubble.text      = text;
        bubble.isUser    = true;
        bubble.isSystem  = false;
        bubble.timestamp = formatTimestamp();
        bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();
        messages_.push_back(bubble);
        startTimerHz(60);
        {
            juce::String preview = text.substring(0, 80);
            preview              = preview.replace("\n", " ");
            writeChatLog("[ChatBubble] addUserMessage: \"" + preview + "\" (len=" + juce::String(text.length()) + ")");
        }
        resized();
        repaint();
    }

    void ChatMessagesComponent::addSystemMessage(const juce::String& text)
    {
        ChatBubble bubble;
        bubble.text      = text;
        bubble.isUser    = false;
        bubble.isSystem  = true;
        bubble.timestamp = formatTimestamp();
        bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();
        messages_.push_back(bubble);
        startTimerHz(60);
        {
            juce::String preview = text.substring(0, 80);
            preview              = preview.replace("\n", " ");
            writeChatLog("[ChatBubble] addSystemMessage: \"" + preview + "\" (len=" + juce::String(text.length())
                         + ")");
        }
        resized();
        repaint();
    }

    void ChatMessagesComponent::addTrackGroupCard(const TrackProblemGroup& group)
    {
        ChatBubble bubble;
        bubble.text = {};
        bubble.isUser = false;
        bubble.isSystem = false;
        bubble.isTrackGroupCard = true;
        bubble.trackGroup = group;
        bubble.trackProblems = group.tracks;
        bubble.timestamp = formatTimestamp();
        bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();
        messages_.push_back(bubble);
        startTimerHz(60);
        {
            writeChatLog("[ChatBubble] addTrackGroupCard: \"" + group.groupName
                         + "\" tracks=" + juce::String((int)group.tracks.size()));
        }
        resized();
        repaint();
    }

    void ChatMessagesComponent::addReverbCard(const ReverbCardData& data)
    {
        ChatBubble bubble;
        bubble.isReverbCard = true;
        bubble.reverbData = data;
        bubble.timestamp = formatTimestamp();
        bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();
        messages_.push_back(bubble);
        startTimerHz(60);

        {
            writeChatLog("[ChatBubble] addReverbCard: genre=\"" + data.genre
                         + "\" tracks=" + juce::String((int)data.trackNames.size()));
        }
        resized();
        repaint();
    }

    void ChatMessagesComponent::addPluginSuggestionCard(const PluginSuggestionGroup& group)
    {
        ChatBubble bubble;
        bubble.isPluginSuggestionCard = true;
        bubble.pluginSuggestionData = group;
        bubble.timestamp = formatTimestamp();
        bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();
        messages_.push_back(bubble);
        startTimerHz(60);
        {
            writeChatLog("[ChatBubble] addPluginSuggestionCard: \"" + group.problemTitle
                         + "\" tiers=" + juce::String((int)group.suggestions.size()));
        }
        resized();
        repaint();
    }

    void ChatMessagesComponent::addCorrectionCard(const CorrectionCardData& data)
    {
        ChatBubble bubble;
        bubble.isCorrectionCard = true;
        bubble.correctionData = data;
        bubble.timestamp = formatTimestamp();
        bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();
        messages_.push_back(bubble);
        startTimerHz(60);
        {
            writeChatLog("[ChatBubble] addCorrectionCard: \"" + data.problemTitle
                         + "\" track=\"" + data.trackName
                         + "\" action=\"" + data.action + "\"");
        }
        resized();
        repaint();
    }

    void ChatMessagesComponent::addMasterCheckCard(const MasterCheckCardData& data)
    {
        ChatBubble bubble;
        bubble.isMasterCheckCard = true;
        bubble.masterCheckData = data;
        bubble.timestamp = formatTimestamp();
        bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();
        messages_.push_back(bubble);
        startTimerHz(60);
        {
            writeChatLog("[ChatBubble] addMasterCheckCard: score="
                         + juce::String(data.matchScore, 2)
                         + " gaps=" + juce::String((int)data.gaps.size()));
        }
        resized();
        repaint();
    }

    void ChatMessagesComponent::clear()
    {
        messages_.clear();
        isTyping_ = false;
        repaint();
    }

    int ChatMessagesComponent::getNumSystemMessages() const noexcept
    {
        int count = 0;
        for (auto& m : messages_)
            if (m.isSystem) ++count;
        return count;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Streaming — Texto incremental del LLM
    // ═══════════════════════════════════════════════════════════════════════════

    void ChatMessagesComponent::startStreamingMessage()
    {
        ChatBubble bubble;
        bubble.text      = {}; // Vacío — se llenará con appendToStreamingMessage
        bubble.tag       = {};
        bubble.isUser    = false;
        bubble.timestamp = formatTimestamp();
        bubble.animateIn = true;
        bubble.animStartMs = (int)juce::Time::getMillisecondCounter();

        streamingMessageIndex_ = (int)messages_.size();
        messages_.push_back(bubble);
        streamingActive_  = true;
        streamingStartMs_ = juce::Time::getMillisecondCounter();
        startTimerHz(60);

        resized();
        repaint();
    }

    void ChatMessagesComponent::appendToStreamingMessage(const juce::String& text)
    {
        if (!streamingActive_ || streamingMessageIndex_ < 0 || streamingMessageIndex_ >= (int)messages_.size()) return;

        messages_[streamingMessageIndex_].text += text;
        resized();
        repaint();
    }

    void ChatMessagesComponent::finalizeStreamingMessage()
    {
        if (!streamingActive_) return;

        streamingActive_       = false;
        streamingMessageIndex_ = -1;
        resized();
        repaint();
    }

    void ChatMessagesComponent::setTypingIndicator(bool isTyping)
    {
        if (isTyping != isTyping_) {
            isTyping_ = isTyping;
            if (isTyping) typingStartMs_ = juce::Time::getMillisecondCounter();
            resized();
            repaint();
        }
    }

    int ChatMessagesComponent::getTotalHeight() const
    {
        float total = 6.0f; // top padding (compacto)

        if (messages_.empty()) {
            // Welcome card height
            total += 160.0f;
            total += 6.0f;
        }
        else {
            float maxW = (float)getWidth();
            for (auto& msg : messages_) {
                // Saltar mensajes del sistema si están ocultos
                if (msg.isSystem && !showSystemMessages_) continue;
                float bh = getBubbleHeight(msg, maxW);
                total += bh;
                total += 6.0f; // spacing compacto
            }
        }

        // Typing indicator
        if (isTyping_) total += 36.0f;

        // Bottom padding
        total += 6.0f;

        int result = (int)std::ceil(total);
        return result;
    }

    void ChatMessagesComponent::resized()
    {
        int h = getTotalHeight();
        setSize(getWidth(), h);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getBubbleHeight — Calcula altura de una burbuja según su texto
    // ═══════════════════════════════════════════════════════════════════════════
    float ChatMessagesComponent::getBubbleHeight(const ChatBubble& bubble, float maxWidth) const
    {
        // Throttle logging: solo loggear cada 100 llamadas para no inundar el log
        static int heightLogCounter = 0;
        bool shouldLogHeight        = (++heightLogCounter % 100 == 1);
        float bubbleMaxW            = maxWidth * (bubble.isUser ? 0.75f : 0.85f);
        bubbleMaxW                  = juce::jlimit(120.0f, 350.0f, bubbleMaxW);

        float textW = bubbleMaxW - 12.0f; // 6px padding cada lado
        juce::Font font(juce::FontOptions(12.5f));

        float textH = measureMarkdownHeight(bubble.text, font, textW);

        // ─── Mensajes del sistema: altura JUSTA, midiendo con el MISMO texto que se renderiza
        //     drawSystemMessage renderiza "🛠 " + msg.text con drawMarkdownText.
        //     Usamos el mismo prefijo para que la medición coincida EXACTAMENTE con el render.
        //     El padding mínimo es 2px para que el texto no quede pegado al borde.
        if (bubble.isSystem) {
            float resultH;
            juce::Font sysFont(juce::FontOptions(8.5f));
            float sysTextW           = maxWidth - 32.0f; // coincide con drawSystemMessage
            juce::String displayText = juce::String(juce::CharPointer_UTF8("\xE2\x96\xB8 ")) + bubble.text;
            float sysTextH = measureMarkdownHeight(displayText, sysFont, sysTextW);
            resultH = juce::jmax(14.0f, sysTextH + 2.0f); // 2px padding mínimo
            if (shouldLogHeight) {
                juce::String preview = bubble.text.substring(0, 60).replace("\n", " ");
                writeChatLog("[Height] SYSTEM maxW=" + juce::String(maxWidth, 0) + " textW=" + juce::String(sysTextW, 1)
                             + " textH=" + juce::String(sysTextH, 1) + " result=" + juce::String(resultH, 1) + " txt=\""
                             + preview + "\"");
            }
            return resultH;
        }

        // ─── Track group card: altura fija basada en tracks
        if (bubble.isTrackGroupCard) {
            const float cardH = 40.0f + bubble.trackGroup.tracks.size() * 22.0f;
            return juce::jmax(60.0f, cardH);
        }

        // ─── Reverb card: altura fija (header + tracks + decay graph + params + footer)
        if (bubble.isReverbCard) {
            const float headerH = 24.0f;
            const float tracksH = juce::jmin(18.0f * (float)bubble.reverbData.trackNames.size(), 54.0f);
            const float graphH = 52.0f;
            const float paramsH = 20.0f;
            const float footerH = 16.0f;
            const float padding = 16.0f;
            return juce::jmax(130.0f, headerH + tracksH + graphH + paramsH + footerH + padding);
        }

        // ─── Plugin suggestion card: altura dinámica por tiers
        if (bubble.isPluginSuggestionCard) {
            const float headerH = 28.0f;
            const float badgeH = bubble.pluginSuggestionData.trackName.isNotEmpty() ? 22.0f : 0.0f;
            const float sugH = 25.0f * (float)bubble.pluginSuggestionData.suggestions.size();
            const float footerH = 16.0f;
            const float padding = 12.0f;
            return juce::jmax(80.0f, headerH + badgeH + sugH + footerH + padding);
        }

        // ─── Master Check card: altura dinámica por gaps
        if (bubble.isMasterCheckCard) {
            const float headerH = 24.0f;
            const float scoreH = 40.0f;
            const float gapsLabelH = 16.0f;
            const float gapRowH = 22.0f * (float)bubble.masterCheckData.gaps.size();
            const float recH = bubble.masterCheckData.overallRecommendation.isNotEmpty() ? 24.0f : 0.0f;
            const float padding = 12.0f;
            return juce::jmax(100.0f, headerH + scoreH + gapsLabelH + gapRowH + recH + padding);
        }

        // ─── Correction card height
        if (bubble.isCorrectionCard) {
            const float headerH = 28.0f;
            const float actionH = 18.0f;
            const float statusH = bubble.correctionData.isResolved() ? 22.0f : 0.0f;
            const float buttonH = (bubble.correctionData.status < CorrectionCardData::Status::Verified) ? 28.0f : 0.0f;
            const float padding = 14.0f;
            return juce::jmax(85.0f, headerH + actionH + statusH + buttonH + padding);
        }

        // ─── Tag height: solo si hay tag (la mayoría de mensajes NO tienen tag)
        //     Esto ahorra 18px por burbuja en mensajes sin tag.
        bool hasBold = bubble.text.contains("**");
        float tagH = (!bubble.isUser && !bubble.tag.isEmpty()) ? 18.0f : 0.0f;
        // ─── Padding: innerPad(6px) × 2 + 2px reserva timestamp
        float bubbleH = textH + tagH + 14.0f;
        float resultH = juce::jmax(44.0f, bubbleH);

        if (shouldLogHeight) {
            juce::String preview = bubble.text.substring(0, 60).replace("\n", " ");
            juce::String type    = bubble.isUser ? "USER" : "COACH";
            writeChatLog("[Height] " + type + " maxW=" + juce::String(maxWidth, 0) + " textW=" + juce::String(textW, 1)
                         + " textH=" + juce::String(textH, 1) + " tagH=" + juce::String(tagH, 1)
                         + " result=" + juce::String(resultH, 1) + " bold=" + (hasBold ? "Y" : "N") + " tag=\""
                         + bubble.tag + "\"" + " txt=\"" + preview + "\"");
        }

        return resultH;
    }

    /** Helper: aplica animación de entrada a un ChatBubble.
        Retorna el offset Y, escala, y alpha. */
    static float applyBubbleAnimation(juce::Graphics& g, const ChatBubble& msg,
                                       float& outAlpha, float& outScale)
    {
        outAlpha = 1.0f;
        outScale = 1.0f;
        if (!msg.animateIn) return 0.0f;

        int64_t now = juce::Time::getMillisecondCounter();
        int64_t elapsed = now - msg.animStartMs;
        float t = juce::jmin(1.0f, (float)elapsed / (float)ChatBubble::kAnimDurationMs);
        float eased = easeOutQuad(t);

        outAlpha = eased;
        outScale = ChatBubble::kAnimStartScale + (1.0f - ChatBubble::kAnimStartScale) * eased;
        return ChatBubble::kAnimSlidePx * (1.0f - eased);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Dibuja welcome card o burbujas + typing indicator
    // ═══════════════════════════════════════════════════════════════════════════
void ChatMessagesComponent::paint(juce::Graphics& g)
{
    // ═══ FASE 5: Limpiar cache de hit-test para option cards ═══════════
    pluginCardHitAreas_.clear();

    float y    = 6.0f;
    float maxW = (float)getWidth();

    if (messages_.empty()) {
            // ─── Welcome card ────────────────────────────────────────────────
            drawWelcomeCard(g, juce::Rectangle<float>(6.0f, y, maxW - 12.0f, 160.0f));
            y += 166.0f;
        }
        else {
            int visibleCount = 0;
            for (int i = 0; i < (int)messages_.size(); ++i) {
                auto& msg = messages_[i];

                // Saltar mensajes del sistema si están ocultos
                if (msg.isSystem && !showSystemMessages_) continue;

                float bubbleH    = getBubbleHeight(msg, maxW);
                float bubbleMaxW = juce::jlimit(120.0f, 350.0f, maxW * (msg.isUser ? 0.75f : 0.85f));

                // ─── Aplicar animación de entrada (slide + fade + scale) ──
                float bubbleAlpha = 1.0f;
                float bubbleScale = 1.0f;
                float animOffsetY = applyBubbleAnimation(g, msg, bubbleAlpha, bubbleScale);
                float drawY = y + animOffsetY;

                bool isAnimating = (bubbleAlpha < 1.0f || bubbleScale < 1.0f);
                if (isAnimating) {
                    g.saveState();
                    g.setOpacity(bubbleAlpha);
                    // Scale transform from center of bubble
                    float halfW = bubbleMaxW * 0.5f;
                    float cx = msg.isUser ? (maxW - bubbleMaxW - 6.0f + halfW) : (6.0f + halfW);
                    g.addTransform(juce::AffineTransform::scale(bubbleScale, bubbleScale, cx, drawY + bubbleH * 0.5f));
                }

                if (msg.isSystem) {
                    // ─── Mensajes del sistema: compactos, sin burbuja ──────
                    drawSystemMessage(g, {12.0f, drawY, maxW - 24.0f, bubbleH}, msg);
                }
                else if (msg.isUser) {
                    float x = maxW - bubbleMaxW - 6.0f;
                    drawUserBubble(g, {x, drawY, bubbleMaxW, bubbleH}, msg);
                }
                else {
                    // Restaurar alpha para el resto de las cards (ya se hereda)

                    // ─── Para mensaje en streaming, dibujar con cursor ──────
                    if (streamingActive_ && i == streamingMessageIndex_) {
                        drawCoachBubbleStreaming(g, {6.0f, y, bubbleMaxW, bubbleH}, msg);
                    }
                    else if (msg.isTrackGroupCard) {
                        // ─── Inline track group card rendering (self-contained) ──
                        const auto& group = msg.trackGroup;
                        float cardW = maxW - 12.0f;
                        float cr = 8.0f;
                        auto cardBounds = juce::Rectangle<float>(6.0f, y, cardW, bubbleH);

                        // Container shadow
                        g.setColour(juce::Colours::black.withAlpha(0.15f));
                        g.fillRoundedRectangle(cardBounds.expanded(1.0f, 2.0f), cr);
                        // Container background
                        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.92f));
                        g.fillRoundedRectangle(cardBounds, cr);
                        // Container border
                        g.setColour(MixCoachTheme::border().withAlpha(0.25f));
                        g.drawRoundedRectangle(cardBounds, cr, 0.6f);

                        auto area = cardBounds.reduced(6, 4);

                        // ─── Header with accent bar ────────────────────────
                        auto header = area.removeFromTop(20);
                        // Accent bar
                        g.setColour(group.colour.withAlpha(0.5f));
                        g.fillRoundedRectangle(
                            juce::Rectangle<float>(area.getX(), header.getY() + 2,
                                                    3.0f, header.getHeight() - 4), 1.5f);
                        // Icon + group name
                        g.setFont(juce::Font(juce::FontOptions(11.0f)));
                        g.drawText(group.icon, header.removeFromLeft(18),
                                   juce::Justification::centredLeft);
                        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
                        g.setColour(MixCoachTheme::textBright());
                        g.drawText("Grupo: " + group.groupName, header.removeFromLeft(110),
                                   juce::Justification::centredLeft);
                        // Track count badge
                        auto badgeArea = header.removeFromLeft(56);
                        g.setColour(group.colour.withAlpha(0.12f));
                        g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
                        g.setColour(group.colour);
                        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
                        g.drawText(juce::String((int)group.tracks.size()) + " pistas",
                                   badgeArea, juce::Justification::centred);
                        area.removeFromTop(2);

                        // ─── Track rows ────────────────────────────────────
                        for (auto& t : group.tracks) {
                            auto row = area.removeFromTop(20);

                            // Severity dot
                            float dotR = 2.5f;
                            juce::Colour sevColour;
                            if (t.severity >= 0.8f) sevColour = MixCoachTheme::error();
                            else if (t.severity >= 0.4f) sevColour = MixCoachTheme::warning();
                            else sevColour = MixCoachTheme::success();
                            g.setColour(sevColour);
                            g.fillEllipse(row.getX() + 7 - dotR, row.getCentreY() - dotR,
                                          dotR * 2.0f, dotR * 2.0f);

                            // Track name
                            auto textArea = row.removeFromLeft(
                                juce::jmin(90.0f, row.getWidth() * 0.35f));
                            g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
                            g.setColour(MixCoachTheme::textPrimary());
                            juce::String displayName = t.roleName.isNotEmpty()
                                                           ? t.roleName : t.trackName;
                            g.drawText(displayName, textArea.translated(10, 0),
                                       juce::Justification::centredLeft);

                            // Problem badge
                            auto badgeR = row.removeFromLeft(
                                juce::jmin(row.getWidth() * 0.45f, 90.0f));
                            g.setColour(sevColour.withAlpha(0.10f));
                            g.fillRoundedRectangle(badgeR, 3.0f);
                            g.setColour(sevColour);
                            g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
                            g.drawText(t.problemType, badgeR.reduced(2, 0),
                                       juce::Justification::centred);

                            // Plugin pill (first suggestion only)
                            if (!t.pluginSuggestions.empty()) {
                                area.removeFromTop(1);
                                auto sugArea = area.removeFromTop(16);
                                auto& sug = t.pluginSuggestions[0];
                                auto pill = sugArea.withWidth(sugArea.getWidth() * 0.9f).reduced(1, 0);
                                g.setColour(MixCoachTheme::accent().withAlpha(0.10f));
                                g.fillRoundedRectangle(pill, 3.0f);
                                g.setColour(MixCoachTheme::accent().withAlpha(0.25f));
                                g.drawRoundedRectangle(pill, 3.0f, 0.3f);
                                g.setFont(juce::Font(juce::FontOptions(6.0f)).boldened());
                                g.setColour(MixCoachTheme::accent());
                                g.drawText(sug.pluginName, pill.reduced(2, 0),
                                           juce::Justification::centredLeft);
                            }
                            area.removeFromTop(1);
                        }

                        // ─── Footer ────────────────────────────────────────
                        if (area.getHeight() >= 16) {
                            auto footerArea = area.removeFromTop(16);
                            g.setFont(juce::Font(juce::FontOptions(7.0f)));
                            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
                            g.drawText("Revisa estas pistas para mejorar tu mezcla.",
                                       footerArea.withWidth(footerArea.getWidth() - 70),
                                       juce::Justification::centredLeft);
                        }
                    }
                    else if (msg.isReverbCard) {
                        // ─── Inline reverb card rendering ──
                        float cardW = maxW - 12.0f;
                        auto cardBounds = juce::Rectangle<float>(6.0f, y, cardW, bubbleH);
                        drawReverbCard(g, cardBounds, msg);
                    }
                else if (msg.isPluginSuggestionCard) {
                    // ─── Inline plugin suggestion card ──
                    float cardW = maxW - 12.0f;
                    auto cardBounds = juce::Rectangle<float>(6.0f, y, cardW, bubbleH);
                    // ═══ FASE 5: Pasar cache de hit-test para option cards clicables ═══
                    drawPluginSuggestionCard(g, cardBounds, msg, &pluginCardHitAreas_);
                }
                    else if (msg.isMasterCheckCard) {
                        // ─── Inline master check card ──
                        float cardW = maxW - 12.0f;
                        auto cardBounds = juce::Rectangle<float>(6.0f, y, cardW, bubbleH);
                        drawMasterCheckCard(g, cardBounds, msg);
                    }
                    else if (msg.isCorrectionCard) {
                        // ─── Inline correction card ──
                        float cardW = maxW - 12.0f;
                        auto cardBounds = juce::Rectangle<float>(6.0f, y, cardW, bubbleH);
                        drawCorrectionCard(g, cardBounds, msg);
                    }
                    else {
                        drawCoachBubble(g, {6.0f, y, bubbleMaxW, bubbleH}, msg);
                    }
                }
                juce::String type = msg.isSystem ? "SYS" : (msg.isUser ? "USR" : "COA");
                if (visibleCount % 5 == 0 && visibleCount < 10) {
                    juce::String preview = msg.text.substring(0, 40).replace("\n", " ");
                    writeChatLog("[Paint] idx=" + juce::String(i) + " type=" + type + " y=" + juce::String(y, 1)
                                 + " h=" + juce::String(bubbleH, 1) + " txt=\"" + preview + "\"");
                }
                // Restaurar alpha y transform para la siguiente burbuja
                if (isAnimating)
                    g.restoreState();

                visibleCount++;
                y += bubbleH + 6.0f;
            }
            if (visibleCount > 10) {
                writeChatLog("[Paint] " + juce::String(visibleCount) + " bubbles, total y=" + juce::String(y, 1));
            }
        }

        // ─── Typing indicator ────────────────────────────────────────────────
        if (isTyping_) {
            drawTypingIndicator(g, juce::Rectangle<float>(6.0f, y + 2.0f, 120.0f, 28.0f));
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawWelcomeCard — Tarjeta de bienvenida premium cuando el chat está vacío
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessagesComponent::drawWelcomeCard(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const float cr = 12.0f;

        // ─── Shadow profundo ──────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRoundedRectangle(bounds.expanded(1.5f, 2.0f), cr);

        // ─── Glow púrpura exterior ─────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
        g.fillRoundedRectangle(bounds.expanded(3.0f, 3.5f), cr + 2.0f);

        // ─── Subtle dot grid background ─────────────────────────────────────
        for (int dotX = (int)bounds.getX() + 24; dotX < (int)bounds.getRight(); dotX += 28) {
            for (int dotY = (int)bounds.getY() + 24; dotY < (int)bounds.getBottom(); dotY += 28) {
                g.setColour(MixCoachTheme::accent().withAlpha(0.025f));
                g.fillEllipse((float)dotX - 0.6f, (float)dotY - 0.6f, 1.2f, 1.2f);
            }
        }

        // ─── Fondo con gradiente premium ───────────────────────────────────
        juce::ColourGradient bgGrad(juce::Colour(0xDD2A1048),
                                    bounds.getX(),
                                    bounds.getY(),
                                    juce::Colour(0xDD081020),
                                    bounds.getX(),
                                    bounds.getBottom(),
                                    false);
        bgGrad.addColour(0.5f, juce::Colour(0xDD181A38));
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(bounds, cr);

        // ─── Glass highlight (2 capas de profundidad) ───────────────────────
        auto glassTop = bounds.withHeight(bounds.getHeight() * 0.3f);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.09f),
                                       glassTop.getX(),
                                       glassTop.getY(),
                                       juce::Colour(0x00000000),
                                       glassTop.getX(),
                                       glassTop.getBottom(),
                                       false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassTop, cr);

        // Second glass layer (shorter, brighter)
        auto glassPeak = glassTop.withHeight(glassTop.getHeight() * 0.4f);
        juce::ColourGradient glassPeakGrad(juce::Colours::white.withAlpha(0.12f),
                                           glassPeak.getX(),
                                           glassPeak.getY(),
                                           juce::Colour(0x00000000),
                                           glassPeak.getX(),
                                           glassPeak.getBottom(),
                                           false);
        g.setGradientFill(glassPeakGrad);
        g.fillRoundedRectangle(glassPeak, cr);

        // ─── Borde ───────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.20f));
        g.drawRoundedRectangle(bounds, cr, 0.8f);

        // ─── Corner accent decorations ─────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.10f));
        // Top-left
        float cl = bounds.getX() + 6.0f;
        float ct = bounds.getY() + 6.0f;
        g.drawVerticalLine((int)cl, ct, ct + 16.0f);
        g.drawHorizontalLine((int)ct, cl, cl + 16.0f);
        // Bottom-right
        float crx = bounds.getRight() - 6.0f;
        float cby = bounds.getBottom() - 6.0f;
        g.drawVerticalLine((int)crx, cby - 16.0f, cby);
        g.drawHorizontalLine((int)cby, crx - 16.0f, crx);

        auto area = bounds.reduced(14, 12);

        // ─── Icono robot — 40px con double-ring glow ────────────────────────
        auto iconArea = area.removeFromTop(40);
        float iconCx  = iconArea.getCentreX();
        float iconCy  = iconArea.getY() + 20.0f;

        // Outer glow ring
        g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
        g.fillEllipse(iconCx - 24.0f, iconCy - 24.0f, 48.0f, 48.0f);
        // Mid ring
        g.setColour(MixCoachTheme::accent().withAlpha(0.10f));
        g.fillEllipse(iconCx - 20.0f, iconCy - 20.0f, 40.0f, 40.0f);
        // Core circle
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.fillEllipse(iconCx - 16.0f, iconCy - 16.0f, 32.0f, 32.0f);
        // Border rings
        g.setColour(MixCoachTheme::accent().withAlpha(0.20f));
        g.drawEllipse(iconCx - 20.0f, iconCy - 20.0f, 40.0f, 40.0f, 0.4f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.35f));
        g.drawEllipse(iconCx - 16.0f, iconCy - 16.0f, 32.0f, 32.0f, 0.6f);
        // Icono: estrella de 4 puntas dibujada con Graphics
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.7f));
        auto starBounds = juce::Rectangle<float>(iconCx - 8.0f, iconCy - 8.0f, 16.0f, 16.0f);
        g.fillRoundedRectangle(starBounds, 4.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.3f));
        g.drawRoundedRectangle(starBounds, 4.0f, 1.0f);

        area.removeFromTop(4);

        // ─── Título con glow sutil ───────────────────────────────────────────
        auto titleArea = area.removeFromTop(22);
        // Subtle text glow
        g.setFont(juce::Font(juce::FontOptions(14.5f)).boldened());
        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.setFont(juce::Font(juce::FontOptions(14.5f)).boldened());
        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.drawText("Welcome to MixCoach!", titleArea.translated(0, 1), juce::Justification::centred);
        g.setColour(MixCoachTheme::textBright());
        g.drawText("Welcome to MixCoach!", titleArea, juce::Justification::centred);

        // ─── Decorative line under title ─────────────────────────────────────
        float lineCx = titleArea.getCentreX();
        float lineY  = (float)titleArea.getBottom() + 2.0f;
        float lineW  = 50.0f;
        juce::ColourGradient lineGrad(MixCoachTheme::accent().withAlpha(0.25f),
                                      lineCx,
                                      lineY,
                                      MixCoachTheme::accent().withAlpha(0.0f),
                                      lineCx + lineW,
                                      lineY,
                                      false);
        lineGrad.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.12f));
        g.setGradientFill(lineGrad);
        g.drawHorizontalLine((int)lineY, lineCx - lineW, lineCx + lineW);

        area.removeFromTop(4);

        // ─── Subtítulo ────────────────────────────────────────────────────────
        auto subArea = area.removeFromTop(16);
        g.setFont(juce::Font(juce::FontOptions(9.5f)));
        g.setColour(MixCoachTheme::textDim());
        g.drawText("Your AI assistant for professional-sounding mixes.", subArea, juce::Justification::centred);

        area.removeFromTop(4);

        // ─── Badge de fase premium ────────────────────────────────────────────
        auto badgeArea = area.removeFromTop(22);
        auto badgeRect = badgeArea.withSizeKeepingCentre(150, 20).toFloat();
        // Glow exterior
        g.setColour(MixCoachTheme::warning().withAlpha(0.06f));
        g.fillRoundedRectangle(badgeRect.expanded(4.0f, 3.0f), 11.0f);
        // Fondo con gradiente
        juce::ColourGradient badgeGrad(MixCoachTheme::warning().withAlpha(0.10f),
                                       badgeRect.getX(),
                                       badgeRect.getY(),
                                       MixCoachTheme::warning().withAlpha(0.05f),
                                       badgeRect.getX(),
                                       badgeRect.getBottom(),
                                       false);
        badgeGrad.addColour(0.5f, MixCoachTheme::warning().withAlpha(0.08f));
        g.setGradientFill(badgeGrad);
        g.fillRoundedRectangle(badgeRect, 10.0f);
        // Borde
        g.setColour(MixCoachTheme::warning().withAlpha(0.40f));
        g.drawRoundedRectangle(badgeRect, 10.0f, 0.5f);
        // Texto
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
        g.setColour(MixCoachTheme::warning());
        g.drawText("CURRENT PHASE: 2 - ORGANIZATION", badgeRect, juce::Justification::centred);

        // ─── Hint (elegante, más pequeño) ─────────────────────────────────────
        area.removeFromTop(2);
        auto hintArea = area;
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.65f));
        g.drawText("Type a message or choose a quick suggestion below.", hintArea, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawSystemMessage — Mensaje del sistema en formato compacto y dimmed
    // ═══════════════════════════════════════════════════════════════════════════
    void
    ChatMessagesComponent::drawSystemMessage(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg)
    {
        // ─── Texto word-wrapped, dimmed, sin burbuja, sin timestamp ────────
        auto textArea = bounds.reduced(4, 1);

        // Pequeña barra de acento a la izquierda (altura completa)
        g.setColour(MixCoachTheme::accent().withAlpha(0.20f));
        g.fillRect(textArea.getX() - 2.0f, textArea.getY() + 1.0f, 2.0f, textArea.getHeight() - 2.0f);

        // Icono + texto con word-wrap (ocupa toda el área, sin timestamp)
        juce::Font sysFont(juce::FontOptions(8.5f));
        juce::String displayText = juce::String("· ") + msg.text;
        drawMarkdownText(g, displayText, sysFont, MixCoachTheme::textMuted().withAlpha(0.65f), textArea);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTypingIndicator — Indicador animado de "IA está escribiendo..."
    // ═══════════════════════════════════════════════════════════════════════════
    /** Bounce ease-out: overshoot + settle (4 rebotes). */
    static float bounceOut(float t) noexcept
    {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;
        if (t < 1.0f / d1)
            return n1 * t * t;
        else if (t < 2.0f / d1)
            return n1 * (t -= 1.5f / d1) * t + 0.75f;
        else if (t < 2.5f / d1)
            return n1 * (t -= 2.25f / d1) * t + 0.9375f;
        else
            return n1 * (t -= 2.625f / d1) * t + 0.984375f;
    }

    void ChatMessagesComponent::drawTypingIndicator(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const float cr = 12.0f;

        // ─── Sombra ───────────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.expanded(1.0f, 1.5f), cr);

        // ─── Fondo dark purple ────────────────────────────────────────────────
        g.setColour(juce::Colour(0xD81A0A2E));
        g.fillRoundedRectangle(bounds, cr);

        // ─── Glass highlight ────────────────────────────────────────────────
        auto glassH = bounds.withHeight(bounds.getHeight() * 0.4f);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.07f),
                                       glassH.getX(),
                                       glassH.getY(),
                                       juce::Colour(0x00000000),
                                       glassH.getX(),
                                       glassH.getBottom(),
                                       false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassH, cr);

        // ─── Borde ────────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.drawRoundedRectangle(bounds, cr, 0.5f);

        // ─── Tres puntos animados con bounce + stagger ───────────────────────
        float cx      = bounds.getX() + 20.0f;
        float cy      = bounds.getCentreY();
        float dotR    = 3.5f;
        float spacing = 12.0f;
        float bounceH = 7.0f; // altura máxima del bounce (px)
        float period = 1200.0f; // ms del ciclo completo de bounce
        float staggerMs = 200.0f; // stagger entre dots (0.2s)

        int64_t now   = juce::Time::getMillisecondCounter();
        float elapsed = (float)(now - typingStartMs_);

        for (int i = 0; i < 3; ++i) {
            // Calcular t con stagger: cada dot empieza 200ms después
            float dotElapsed = elapsed - (float)i * staggerMs;
            if (dotElapsed < 0) dotElapsed = 0.0f;

            // Ciclo infinito de bounce: sinusoidal en fase, bounceOut en forma
            float cycleT = fmod(dotElapsed, period) / period; // 0→1 continuo

            // bounceOut en subida (0→1), hold en bajada (1→0 con ease)
            // Usar half-cycle: subida rápida con bounce, bajada suave
            float bounceT;
            if (cycleT < 0.5f) {
                // Subida: bounce out (0→1)
                bounceT = bounceOut(cycleT * 2.0f);
            } else {
                // Bajada: ease-out quad (1→0)
                float fallT = (cycleT - 0.5f) * 2.0f;
                bounceT = 1.0f - easeOutQuad(fallT);
            }

            // Convertir bounceT a offset Y: 0 → -bounceH → 0
            float offsetY = -bounceH * bounceT;

            // Alpha brillante cuando está arriba, sutil cuando abajo
            float alpha = 0.4f + 0.6f * bounceT;

            g.setColour(MixCoachTheme::accentGlow().withAlpha(alpha));
            g.fillEllipse(cx + (float)i * spacing - dotR, cy + offsetY - dotR, dotR * 2.0f, dotR * 2.0f);
        }

        // ─── Label "escribiendo..." ──────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
        g.drawText(
            "MixCoach is typing...",
            juce::Rectangle<float>(bounds.getX() + 54.0f, bounds.getY(), bounds.getWidth() - 58.0f, bounds.getHeight()),
            juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawCoachBubbleStreaming — Burbuja compacta del coach con cursor ▊
    //  Estilo WhatsApp: 1 sombra, fondo sólido, padding reducido
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessagesComponent::drawCoachBubbleStreaming(juce::Graphics& g,
                                                         juce::Rectangle<float> bounds,
                                                         const ChatBubble& msg)
    {
        const float cr = 8.0f;

        // ─── Sombra única ────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRoundedRectangle(bounds.expanded(1.5f, 2.0f), cr + 1.0f);

        // ─── Fondo dark purple (0.83 alpha para glass effect consistente) ───
        g.setColour(juce::Colour(0xD41A0A2E));
        g.fillRoundedRectangle(bounds, cr);

        // ─── Borde ───────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.drawRoundedRectangle(bounds, cr, 0.5f);

        // ─── Texto + cursor intermitente ─────────────────────────────────────
        float innerPad  = 6.0f;
        auto textArea   = bounds.reduced(innerPad, innerPad);
        auto textBounds = textArea.withBottom(textArea.getBottom() - 4.0f);

        juce::Font msgFont(juce::FontOptions(12.5f));

        // Cursor intermitente: parpadeo sinusoidal
        int64_t now       = juce::Time::getMillisecondCounter();
        float cursorAlpha = 0.3f + 0.7f * (0.5f + 0.5f * std::sin((float)(now - streamingStartMs_) * 0.0119f));

        // PASO 1: Dibujar el texto SIN el cursor (con markdown)
        drawMarkdownText(g, msg.text, msgFont, MixCoachTheme::textPrimary(), textBounds);

        // PASO 2: Calcular posición del cursor al final del texto
        // Usamos GlyphArrangement para encontrar dónde termina el texto
        juce::String lastChar = juce::CharPointer_UTF8("\xE2\x96\x82"); // ▌
        juce::GlyphArrangement ga;
        ga.addLineOfText(msgFont, msg.text + " ", 0.0f, 0.0f);
        auto textBoundsFloat = ga.getBoundingBox(0, ga.getNumGlyphs(), true);
        float cursorX        = textBounds.getX() + textBoundsFloat.getWidth() + 2.0f;
        float cursorY        = textBounds.getY() + textBoundsFloat.getY();

        // Clamp cursor al ancho del texto disponible
        cursorX = juce::jmin(cursorX, textBounds.getRight() - 10.0f);

        // Dibujar cursor con alpha animado
        g.setColour(MixCoachTheme::accentGlow().withAlpha(cursorAlpha));
        g.setFont(msgFont);
        g.drawText(lastChar,
                   juce::Rectangle<float>(cursorX, cursorY, 7.0f, msgFont.getHeight()),
                   juce::Justification::centredLeft);

        // ─── Timestamp ───────────────────────────────────────────────────────
        auto tsBounds = bounds.withTop(bounds.getBottom() - 16.0f).reduced(10.0f, 0.0f);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.60f));
        g.drawText(msg.timestamp, tsBounds, juce::Justification::bottomLeft);

        // ─── Label "escribiendo..." junto al timestamp ─────────────────━━
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.4f + 0.3f * cursorAlpha));
        g.drawText(
            "typing...", tsBounds.reduced(0, 0).withLeft(tsBounds.getX() + 60.0f), juce::Justification::bottomLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawCoachBubble — Burbuja compacta del coach (izquierda)
    //  Estilo WhatsApp: 1 sombra, relleno sólido, padding reducido
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessagesComponent::drawCoachBubble(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg)
    {
        const float cr = 8.0f;

        // ─── Sombra única ────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRoundedRectangle(bounds.expanded(1.5f, 2.0f), cr + 1.0f);

        // ─── Fondo dark purple (0.83 alpha — unificado con streaming) ───────
        g.setColour(juce::Colour(0xD41A0A2E));
        g.fillRoundedRectangle(bounds, cr);

        // ─── Borde sutil ─────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.drawRoundedRectangle(bounds, cr, 0.5f);

        float innerPad = 6.0f;
        auto textArea  = bounds.reduced(innerPad, innerPad);
        float drawY    = textArea.getY();

        // ─── Tag opcional (píldora compacta) ────────────────────────────────
        if (msg.tag.isNotEmpty()) {
            auto tagFont = juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened();
            float tagW   = getTextWidth(tagFont, msg.tag) + 10.0f;
            float tagH   = 14.0f;
            auto tagRect = juce::Rectangle<float>(textArea.getX(), drawY, tagW, tagH);

            g.setColour(MixCoachTheme::accent().withAlpha(0.20f));
            g.fillRoundedRectangle(tagRect, 3.0f);
            g.setFont(tagFont);
            g.setColour(MixCoachTheme::accentGlow());
            g.drawText(msg.tag, tagRect, juce::Justification::centred);

            drawY += tagH + 4.0f;
        }

        // ─── Texto del mensaje ───────────────────────────────────────────────
        auto textBounds = textArea.withTop(drawY).withBottom(textArea.getBottom() - 2.0f);
        juce::Font msgFont(juce::FontOptions(12.5f));
        drawMarkdownText(g, msg.text, msgFont, MixCoachTheme::textPrimary(), textBounds);

        // ─── Timestamp ───────────────────────────────────────────────────────
        auto tsBounds = bounds.withTop(bounds.getBottom() - 14.0f).reduced(6.0f, 0.0f);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.50f));
        g.drawText(msg.timestamp, tsBounds, juce::Justification::bottomLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawUserBubble — Burbuja compacta del usuario (derecha)
    //  Estilo WhatsApp: 1 sombra, fondo dark blue, padding reducido
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessagesComponent::drawUserBubble(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg)
    {
        const float cr = 8.0f;

        // ─── Sombra única ────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRoundedRectangle(bounds.expanded(1.5f, 2.0f), cr + 1.0f);

        // ─── Fondo dark blue (0.83 alpha — unificado con coach bubbles) ─────
        g.setColour(juce::Colour(0xD40A1E2E));
        g.fillRoundedRectangle(bounds, cr);

        // ─── Borde sutil cyan ────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accentCyan().withAlpha(0.15f));
        g.drawRoundedRectangle(bounds, cr, 0.5f);

        // ─── Texto del mensaje ───────────────────────────────────────────────
        float innerPad = 6.0f;
        auto textArea  = bounds.reduced(innerPad, innerPad);

        juce::Font msgFont(juce::FontOptions(12.0f));
        auto textBounds = textArea.withBottom(textArea.getBottom() - 14.0f);
        drawMarkdownText(g, msg.text, msgFont, MixCoachTheme::textPrimary(), textBounds);

        // ─── ✓✓ checkmark + timestamp ────────────────────────────────────────
        auto statusBounds = bounds.withTop(bounds.getBottom() - 14.0f).reduced(6.0f, 0.0f);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
        g.setColour(MixCoachTheme::info().withAlpha(0.50f));
        g.drawText(
            msg.timestamp + juce::String("  \xE2\x9C\x93\xE2\x9C\x93"), statusBounds, juce::Justification::bottomRight);  // ✓✓
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SendButton — Botón premium violeta con ícono de avión + hover glow
    // ═══════════════════════════════════════════════════════════════════════════

    SendButton::SendButton()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setSize(30, 30);
    }

    void SendButton::paint(juce::Graphics& g)
    {
        auto bounds    = getLocalBounds().toFloat();
        const float cr = 6.0f;
        auto btnRect   = bounds.reduced(1.0f, 1.0f);

        // ─── Hover glow exterior ───────────────────────────────────────────
        if (hovered_) {
            auto glowBounds = btnRect.expanded(3.0f, 3.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.20f));
            g.fillRoundedRectangle(glowBounds, cr + 2.0f);

            auto glowBounds2 = btnRect.expanded(6.0f, 6.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
            g.fillRoundedRectangle(glowBounds2, cr + 4.0f);
        }

        // ─── Fondo con gradiente ───────────────────────────────────────────
        juce::ColourGradient btnGrad(MixCoachTheme::accent().brighter(0.15f),
                                     (float)btnRect.getCentreX(),
                                     (float)btnRect.getY(),
                                     MixCoachTheme::accentDim(),
                                     (float)btnRect.getCentreX(),
                                     (float)btnRect.getBottom(),
                                     false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(btnRect, cr);

        // ─── Borde hover ───────────────────────────────────────────────────
        if (hovered_) {
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.5f));
            g.drawRoundedRectangle(btnRect, cr, 1.0f);
        }

        // ─── Ícono de avión de papel ──────────────────────────────────────
        g.setColour(juce::Colours::white);
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float s  = 7.5f;

        juce::Path planePath;
        planePath.startNewSubPath(cx - s * 0.6f, cy + s * 0.5f);
        planePath.lineTo(cx + s * 0.7f, cy);
        planePath.lineTo(cx - s * 0.6f, cy - s * 0.5f);
        planePath.closeSubPath();
        g.fillPath(planePath);

        juce::Path planeBody;
        planeBody.startNewSubPath(cx - s * 0.6f, cy + s * 0.5f);
        planeBody.lineTo(cx + s * 0.2f, cy);
        planeBody.lineTo(cx - s * 0.6f, cy - s * 0.5f);
        planeBody.closeSubPath();
        g.setColour(juce::Colours::white.withAlpha(0.40f));
        g.fillPath(planeBody);
    }

    void SendButton::mouseEnter(const juce::MouseEvent&)
    {
        hovered_ = true;
        repaint();
    }

    void SendButton::mouseExit(const juce::MouseEvent&)
    {
        hovered_ = false;
        repaint();
    }

    void SendButton::mouseUp(const juce::MouseEvent&)
    {
        if (onClick) onClick();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixCoachPanel Implementation
    // ═══════════════════════════════════════════════════════════════════════════

    MixCoachPanel::MixCoachPanel()
        : modeCardMix_(ModeSelectionCard::ModeType::Mix),
          modeCardMaster_(ModeSelectionCard::ModeType::Master)
    {
        // ─── Section: PISTAS (columna derecha ~62%) ─────────────────────────────
        tracksSectionLabel_.setText("TRACKLIST", juce::dontSendNotification);
        tracksSectionLabel_.setFont(MixCoachTheme::sectionHeaderFont());
        tracksSectionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        addChildComponent(tracksSectionLabel_);

        // ─── Master Meter Panel ─────────────────────────────────────────────
        addAndMakeVisible(masterMeterPanel_);

        // ─── Section: REFERENCES (debajo del chat, columna izquierda) ─────────
        referencesSectionLabel_.setText("REFERENCE", juce::dontSendNotification);
        referencesSectionLabel_.setFont(MixCoachTheme::sectionHeaderFont());
        referencesSectionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        addChildComponent(referencesSectionLabel_);

        // ─── Panel de Referencias ──────────────────────────────────────────────
        addAndMakeVisible(refPanel_);
        addAndMakeVisible(evidencePanel_);

        // ─── Messenger List (panel izquierdo) con scroll ───────────────────────
        messengerViewport_.setViewedComponent(&messengerList_, false);
        messengerViewport_.setScrollBarsShown(true, false);
        messengerViewport_.setScrollBarThickness(6);
        messengerViewport_.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,
                                                            MixCoachTheme::accent().withAlpha(0.3f));
        messengerViewport_.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId,
                                                            juce::Colours::transparentBlack);
        addAndMakeVisible(messengerViewport_);

        // ─── Divider bar ──────────────────────────────────────────────────────
        dividerBar_.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        addAndMakeVisible(dividerBar_);

        // ─── Chat Messages (viewport con chatMessages_) ────────────────────────
        chatViewport_.setViewedComponent(&chatMessages_, false);
        chatViewport_.setScrollBarsShown(true, false);
        chatViewport_.setScrollBarThickness(6);
        chatViewport_.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,
                                                       MixCoachTheme::accent().withAlpha(0.3f));
        chatViewport_.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(chatViewport_);

        // ─── Chat Input ────────────────────────────────────────────────────────
        chatInput_.setMultiLine(false);
        chatInput_.setFont(juce::Font(juce::FontOptions(12.0f)));
        chatInput_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgInput());
        chatInput_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
        chatInput_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border());
        chatInput_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
        chatInput_.setIndents(10, 8);
        chatInput_.addListener(this);
        addAndMakeVisible(chatInput_);

        // ─── Send button ──────────────────────────────────────────────────────
        sendButton_.onClick = [this]() {
            auto text = chatInput_.getText().trim();
            if (text.isNotEmpty()) {
                // ═══ Interceptar comandos slash (mismo que textEditorReturnKeyPressed) ═══
                if (text.startsWith("/") && coachEngine_ != nullptr) {
                    coachEngine_->executeCommand(text);
                    chatInput_.clear();
                } else if (onMessageSent) {
                    onMessageSent(text);
                    chatInput_.clear();
                }
            }
        };

        // ─── Mic button — Activa dictado nativo del OS ──────────────────────────
        micButton_.setButtonText(juce::CharPointer_UTF8("\xF0\x9F\x8E\xA4")); // 🎤
        micButton_.setTooltip("Dictado por voz (Win+H en Windows, doble Fn en macOS)");
        micButton_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::bgInput());
        micButton_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::textMuted());
        micButton_.onClick = [this]() {
            // Enfocar TextEditor para dictado inmediato (el usuario solo tiene que
            // presionar Win+H en Windows o Fn+Fn en macOS para activar dictado nativo)
            chatInput_.grabKeyboardFocus();
            addSystemMessage("\xF0\x9F\x8E\xA4 Enfocado en el chat. Presiona Win+H (Windows) o Fn+Fn (macOS) para dictado.");
        };
        addChildComponent(micButton_);

        // ═══ FASE 5: Option cards clicables → reenviar a MixCoachPanel.onPluginCardClicked ═══
        // NavigationShell cablea este callback a narrativeDirector_->onOptionSelected()
        // para disparar el verify loop directamente.
        chatMessages_.onPluginCardClicked = [this](const juce::String& pluginName, const juce::String&) {
            if (pluginName.isEmpty()) return;
            if (onPluginCardClicked) {
                onPluginCardClicked(pluginName);
            }
        };

        // ═══ Acciones Directas — TrackProblemCard: 3 botones interactivos ═══
        // Estos callbacks se disparan cuando el usuario hace clic en
        // "✅ Aplicado", "\xE2\x8F\xAD Omitir", o "🔍 Explicate" en el footer de la tarjeta.

        // ─── "✅ Aplicado" — El usuario confirma que aplico la correccion ──
        trackProblemCard_.onActionApplied = [this](int slotIndex, const juce::String& domain) {
            if (coachEngine_ == nullptr) return;

            writeChatLog("[AccionDirecta] Aplicado slot=" + juce::String(slotIndex) + " domain=\"" + domain + "\"");

            // Obtener nombre de la pista para personalizar el mensaje
            juce::String trackName = {};
            auto& registry = coachEngine_->getSharedData().getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.slotIndex == slotIndex && info.trackName[0] != '\0') {
                    trackName = juce::String(info.trackName);
                }
            });

            // Delegar al engine: registra MixHistory + responde como coach
            coachEngine_->recordManualApplication(slotIndex, domain, trackName);

            hideTrackProblemCard();
        };

        // ─── "Omitir" — El usuario no quiere trabajar en este problema ──
        trackProblemCard_.onActionSkipped = [this](int slotIndex) {
            if (coachEngine_ == nullptr) return;

            writeChatLog("[AccionDirecta] Omitido slot=" + juce::String(slotIndex));

            // Obtener nombre de la pista
            juce::String trackName = {};
            auto& registry = coachEngine_->getSharedData().getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.slotIndex == slotIndex && info.trackName[0] != '\0') {
                    trackName = juce::String(info.trackName);
                }
            });

            // Delegar al engine: registra MixHistory + responde como coach
            coachEngine_->skipProblem(slotIndex, trackName);

            hideTrackProblemCard();
        };

        // ─── "Explicame" — El usuario quiere mas detalle tecnico ──
        trackProblemCard_.onActionExplain = [this](int slotIndex, const juce::String& problemType) {
            if (coachEngine_ == nullptr) return;

            writeChatLog("[AccionDirecta] Explicame slot=" + juce::String(slotIndex)
                         + " problemType=\"" + problemType + "\"");

            // Obtener nombre de la pista
            juce::String trackName = {};
            auto& registry = coachEngine_->getSharedData().getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.slotIndex == slotIndex && info.trackName[0] != '\0') {
                    trackName = juce::String(info.trackName);
                }
            });

            // Delegar al engine: explicación técnica completa como coach
            coachEngine_->explainProblem(slotIndex, problemType, trackName);
        };

        addAndMakeVisible(sendButton_);

        // ─── Ollama status (Phi-3 local) ─────────────────────────────────
        ollamaStatusLabel_.setText(
            "[ROBOT] "
            "Phi-3: verifying...",
            juce::dontSendNotification);
        ollamaStatusLabel_.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
        ollamaStatusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
        addChildComponent(ollamaStatusLabel_);

        // ─── Messenger placeholder: se muestra cuando no hay Messengers conectados ──
        messengerPlaceholder_.setText("[PLUGIN] Inserta un Messenger en cada pista\nde tu DAW para comenzar.", juce::dontSendNotification);
        messengerPlaceholder_.setFont(juce::Font(juce::FontOptions(10.0f)));
        messengerPlaceholder_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted().withAlpha(0.7f));
        messengerPlaceholder_.setJustificationType(juce::Justification::centred);
        addChildComponent(messengerPlaceholder_);

        ollamaRetryBtn_.setButtonText("\xE2\x9F\xB3");
        ollamaRetryBtn_.setTooltip("Retry Ollama connection");
        ollamaRetryBtn_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::accent().withAlpha(0.15f));
        ollamaRetryBtn_.setColour(juce::TextButton::buttonOnColourId, MixCoachTheme::accent().withAlpha(0.30f));
        ollamaRetryBtn_.setColour(juce::TextButton::textColourOnId, MixCoachTheme::accentGlow());
        ollamaRetryBtn_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::textDim());
        ollamaRetryBtn_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        ollamaRetryBtn_.onClick = [this]() {
            if (onRetryOllama) onRetryOllama();
        };
        addChildComponent(ollamaRetryBtn_);

        // ─── Default suggestions ──────────────────────────────────────────────
        suggestions_ = {"Analyze my mix", "What should I fix?", "Give me EQ tips", "How to improve balance?"};

        // ─── Footer bar ────────────────────────────────────────────────────────
        footerModeLabel_.setText("MODE: MIX", juce::dontSendNotification);
        footerModeLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerModeLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        addChildComponent(footerModeLabel_);

        footerPhaseLabel_.setText("CURRENT PHASE: 2 \xE2\x80\x93 ORGANIZATION", juce::dontSendNotification);
        footerPhaseLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerPhaseLabel_.setColour(juce::Label::textColourId, MixCoachTheme::warning());
        addChildComponent(footerPhaseLabel_);

        footerGenreLabel_.setText("GENRE: POP", juce::dontSendNotification);
        footerGenreLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerGenreLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        addChildComponent(footerGenreLabel_);

        footerTargetLabel_.setText("TARGET: -14 LUFS", juce::dontSendNotification);
        footerTargetLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerTargetLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
        addChildComponent(footerTargetLabel_);

        footerSampleRateLabel_.setText("SAMPLE RATE: 48 kHz", juce::dontSendNotification);
        footerSampleRateLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerSampleRateLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
        addChildComponent(footerSampleRateLabel_);

        // ─── Experience Level pill (clickable) ────────────────────────────────
        footerExpLevelLabel_.setText("LEVEL: INTERMEDIATE", juce::dontSendNotification);
        footerExpLevelLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerExpLevelLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        footerExpLevelLabel_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addChildComponent(footerExpLevelLabel_);
        footerExpLevelLabel_.addMouseListener(this, false);

        // ─── Coach Persona pill (clickable) ───────────────────────────────────
        auto personaTraits = mixcoach::getPersonaTraits(mixcoach::CoachPersona::Default);
        footerPersonaLabel_.setText(juce::String(personaTraits.icon) + " " + juce::String(personaTraits.name),
                                    juce::dontSendNotification);
        footerPersonaLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerPersonaLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        footerPersonaLabel_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addChildComponent(footerPersonaLabel_);
        footerPersonaLabel_.addMouseListener(this, false);

        // ─── Platform Target pill (clickable) ─────────────────────────────────
        footerPlatformLabel_.setText("TARGET: -14 LUFS (Streaming General)", juce::dontSendNotification);
        footerPlatformLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerPlatformLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
        footerPlatformLabel_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addChildComponent(footerPlatformLabel_);
        footerPlatformLabel_.addMouseListener(this, false);

        // ─── LLM Status indicator (not clickable, shows status + provider) ───
        footerLlmStatusLabel_.setText("\xF0\x9F\x9F\xA2 LLM: Conectado", juce::dontSendNotification);
        footerLlmStatusLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        footerLlmStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF22C55E));
        footerLlmStatusLabel_.setTooltip("LLM: Ollama (Qwen 2.5 7B)");
        addChildComponent(footerLlmStatusLabel_);

        // ═══ Phase panels: add as children without making visible ═══
        addChildComponent(gainStagingPanel_);
        addChildComponent(eqPanel_);
        addChildComponent(compressionPanel_);
        addChildComponent(spacePanel_);
        addChildComponent(masterCheckPanel_);

        // ═══ Incremento 3d: Panel de confirmación de inserts por pista ═══
        addChildComponent(pluginConfirmationPanel_);

        // ═══ CoachingEvidenceHost: unifica panel derecho en coaching ═══
        addChildComponent(coachingEvidenceHost_);
        // Wire phase panels into host (host doesn't own them)
    coachingEvidenceHost_.setPanels(
        gainStagingPanel_, eqPanel_, compressionPanel_,
        spacePanel_, automationPanel_, masterCheckPanel_, evidencePanel_);

        // ─── Wire messenger list selection to external callback ──────────────
        messengerList_.onSlotSelected = [this](int slotIndex) {
            if (onTrackSelected) onTrackSelected(slotIndex);
            syncSelectionToMixMap(slotIndex);
        };

        // ─── Wire Mix Map selection to external callback + MixMapDetailPanel ──
        mixMapComponent_.onTrackSelected = [this](int slotIndex) {
            if (onTrackSelected) onTrackSelected(slotIndex);
            syncSelectionToMessengerList(slotIndex);

            // ═══ P6: Mostrar MixMapDetailPanel con datos de la pista ═══════
            if (slotIndex >= 0 && coachEngine_ != nullptr) {
                auto& registry = coachEngine_->getSharedData().getSlotRegistry();
                auto info = registry.getSlotInfo(slotIndex);

                if (info.active) {
                    auto result = coachEngine_->getSharedData().getTrackAudioResult(slotIndex);
                    TrackRole role = coachEngine_->getTrackRole(slotIndex);

                    juce::String trackName = juce::String(info.trackName).trim();
                    if (trackName.isEmpty()) trackName = "Pista " + juce::String(slotIndex + 1);

                    // Compute average stereo width from per-band values
                    float avgStereoWidth = 0.0f;
                    for (int b = 0; b < 6; ++b) avgStereoWidth += result.stereoWidthPerBand[b];
                    avgStereoWidth /= 6.0f;

                    float peakDb = result.getPeakCombined();
                    float rmsDb  = result.getRmsCombined();
                    float targetLevel = -18.0f;

                    mixMapDetailPanel_.setVisible(true);
                    mixMapDetailPanel_.toFront(true);
                    mixMapDetailPanel_.setTrackData(
                        slotIndex,
                        trackName,
                        juce::String(getRoleName(role)),
                        peakDb,
                        rmsDb,
                        result.correlation,
                        avgStereoWidth,
                        result.bandEnergies,  // 30-band spectral
                        targetLevel,
                        0.0f);  // roleConfidence default

                    // Wire refresh callback for live data (~10fps)
                    mixMapDetailPanel_.onRefreshData = [this, slotIndex]() {
                        if (coachEngine_ == nullptr) return;
                        auto r = coachEngine_->getSharedData().getTrackAudioResult(slotIndex);
                        TrackRole role = coachEngine_->getTrackRole(slotIndex);

                        float avgSW = 0.0f;
                        for (int b = 0; b < 6; ++b) avgSW += r.stereoWidthPerBand[b];
                        avgSW /= 6.0f;

                        float targetLvl = -18.0f;

                        mixMapDetailPanel_.setTrackData(
                            slotIndex,
                            juce::String(coachEngine_->getSharedData().getSlotRegistry().getSlotInfo(slotIndex).trackName).trim(),
                            juce::String(getRoleName(role)),
                            r.getPeakCombined(),
                            r.getRmsCombined(),
                            r.correlation,
                            avgSW,
                            r.bandEnergies,
                            targetLvl,
                            0.0f);
                    };

                    // Wire close callback to hide the panel
                    mixMapDetailPanel_.onClose = [this]() {
                        mixMapDetailPanel_.setVisible(false);
                    };

                    // Wire pin toggle to persist pinned state
                    mixMapDetailPanel_.onTogglePinned = [this](bool pinned) {
                        mixMapDetailPanel_.setPinned(pinned);
                    };
                }
            } else {
                mixMapDetailPanel_.setVisible(false);
            }
        };

        // ─── Wire Confirm Map button → delegar a CoachEngine ═══════════════
        mixMapComponent_.onConfirmMap = [this]() {
            mixMapComponent_.setMapConfirmed(true);

            if (coachEngine_ != nullptr) coachEngine_->onMapConfirmed();

            repaint();
        };

        // ─── Wire ReferencePanel callbacks → MixCoachPanel callbacks ───────
        refPanel_.onFileReferenceAdded = [this](const juce::String& path) {
            if (onReferenceFileAdded) onReferenceFileAdded(path);
        };
        refPanel_.onURLReferenceAdded = [this](const juce::String& name, const juce::String& url) {
            if (onReferenceURLAdded) onReferenceURLAdded(name, url);
        };
        refPanel_.onReferencesChanged = [this]() {
            if (refPanel_.getNumReferences() == 0 && onReferenceCleared) onReferenceCleared();
            resized();
        };
        refPanel_.onPlayReference = [this](int refIndex) {
            if (onPlayReference) onPlayReference(refIndex);
        };
        refPanel_.onSeekReference = [this](double seconds) {
            if (onSeekReference) onSeekReference(seconds);
        };
        refPanel_.onSectionSelected = [this](int sectionIndex) {
            if (onSectionSelected) onSectionSelected(sectionIndex);
        };
        refPanel_.onSectionSeekTo = [this](double seconds) {
            if (onSeekReference) onSeekReference(seconds);
        };
        refPanel_.onReferenceSelected = [this](int refIndex) {
            if (onReferenceSelected) onReferenceSelected(refIndex);
        };

        // ─── Sprint 4: Reference-Driven Mode toggle callback ──────────────────
        refPanel_.onReferenceDrivenModeToggled = [this](bool enabled) {
            if (coachEngine_ != nullptr) coachEngine_->setReferenceDrivenMode(enabled);
        };

        // ─── Sprint 4: Reference-Driven Mode context menu ────────────────────
        refPanel_.onRefModeMenuAction = [this](ReferencePanelComponent::RefModeMenuAction action) {
            if (coachEngine_ == nullptr) return;

            switch (action) {
                case ReferencePanelComponent::RefModeMenuAction::ConfigureGaps:
                    coachEngine_->clearReferenceGapTracking();
                    coachEngine_->sendReferenceDrivenAnalysis();
                    break;

                case ReferencePanelComponent::RefModeMenuAction::ViewHistory: {
                    auto& prog       = coachEngine_->getReferenceProgress();
                    int historyCount = coachEngine_->getReferenceProgressHistoryCount();
                    auto& history    = coachEngine_->getReferenceProgressHistory();

                    if (historyCount == 0) {
                        coachEngine_->respondWith(
                            "📊 Aún no hay historial de matching contra la referencia. "
                            "Activa el Reference-Driven Mode y espera unos segundos "
                            "a que se recopilen datos.",
                            MentorMessage::Type::Info);
                    }
                    else {
                        juce::String msg;
                        msg += "📊 **Historial de match contra referencia**\n\n";
                        msg +=
                            "Match actual: **" + juce::String(static_cast<int>(prog.currentMatch * 100.0f)) + "%**\n";

                        if (prog.delta > 0.03f)
                            msg += "Tendencia: ▲ Mejorando (+ " + juce::String(prog.delta * 100.0f, 1) + "%)\n";
                        else if (prog.delta < -0.03f)
                            msg += "Tendencia: ▼ Empeorando (" + juce::String(prog.delta * 100.0f, 1) + "%)\n";
                        else
                            msg += "Tendencia: ➡ Estable\n";

                        msg += "\nÚltimos " + juce::String(historyCount) + " valores:\n";
                        int startIdx = (prog.currentMatch == 0.0f) ? 0 : (historyCount > 5 ? historyCount - 5 : 0);
                        for (int i = startIdx; i < historyCount; ++i) {
                            float val = history[i];
                            juce::String bar;
                            int barLen = static_cast<int>(val * 20.0f);
                            for (int b = 0; b < barLen; ++b) bar += "█";
                            msg += "  " + juce::String(static_cast<int>(val * 100.0f)) + "% " + bar + "\n";
                        }

                        msg += "\nGaps: " + juce::String(prog.totalGaps) + " total, " + juce::String(prog.criticalGaps)
                               + " críticos, " + juce::String(prog.warningGaps) + " warnings";
                        if (prog.resolvedGaps > 0) msg += " | " + juce::String(prog.resolvedGaps) + " resueltos ✓";

                        coachEngine_->respondWith(msg, MentorMessage::Type::Info);
                    }
                    break;
                }

                case ReferencePanelComponent::RefModeMenuAction::ResetProgress:
                    coachEngine_->resetReferenceProgress();
                    coachEngine_->clearReferenceGapTracking();
                    coachEngine_->respondWith(
                        "🔄 **Progreso de referencia reiniciado.** Los datos de match "
                        "se volverán a recopilar en el próximo ciclo de análisis.",
                        MentorMessage::Type::Info);
                    break;
            }
        };

        // ═══ ModeSelectionCard wiring ═══════════════════════════════════
        modeCardMix_.onCardSelected = [this]() {
            if (!isVisible() || coachRoomState_ != CoachRoomState::Intention) return;
            // P2: Avatar celebra al seleccionar modo
            setupAvatar_.setExpressionWithDecay(AvatarExpression::Surprised, 4000);
            if (onSuggestionClicked) onSuggestionClicked("Mezclar");
        };
        modeCardMaster_.onCardSelected = [this]() {
            if (!isVisible() || coachRoomState_ != CoachRoomState::Intention) return;
            // P2: Avatar celebra al seleccionar modo
            setupAvatar_.setExpressionWithDecay(AvatarExpression::Surprised, 4000);
            if (onSuggestionClicked) onSuggestionClicked("Masterizar");
        };
        modeCardMix_.setVisible(false);
        modeCardMaster_.setVisible(false);
        addAndMakeVisible(modeCardMix_);
        addAndMakeVisible(modeCardMaster_);

        // Setup avatar (setup mode only)

        // --- Greeting label for mode selection ---
        greetingLabel_.setFont(juce::Font(juce::FontOptions(28.0f)).boldened());
        greetingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
        greetingLabel_.setJustificationType(juce::Justification::centred);
        greetingLabel_.setVisible(false);
        addChildComponent(greetingLabel_);


        // ═══ P7: CoachingGuideWidget — se muestra en modo coaching ═══════
        coachingGuide_.setOpaque(false);
        coachingGuide_.setInterceptsMouseClicks(false, true);
        coachingGuide_.setVisible(false);
        addAndMakeVisible(coachingGuide_);

        mixMapDetailPanel_.setVisible(false);
        addAndMakeVisible(mixMapDetailPanel_);

    }

    void MixCoachPanel::updateChatPlaceholder(CoachRoomState state)
    {
        juce::String placeholder = juce::translate("Pregúntame lo que quieras sobre tu mezcla...");

        switch (state) {
            case CoachRoomState::Welcome:      placeholder = juce::translate("Escribe tu nombre aquí..."); break;
            case CoachRoomState::SessionPrep:  placeholder = juce::translate("¿Alguna duda con los Messengers?"); break;
            case CoachRoomState::EQ:           placeholder = juce::translate("¿Quieres saber más sobre este conflicto?"); break;
            case CoachRoomState::Compression:  placeholder = juce::translate("¿Cómo sientes la dinámica del bus?"); break;
            default: break;
        }

        chatInput_.setTextToShowWhenEmpty(placeholder, MixCoachTheme::textMuted().withAlpha(0.6f));
    }


void MixCoachPanel::lookAndFeelChanged()
{
    // Si el host cambia el LookAndFeel (temas, DPI, etc.), refrescamos
    // todos los colores y fuentes cacheados.
    repaint();
    resized();
}

void MixCoachPanel::resized()
{
        auto area = getLocalBounds().reduced(6);

        // ─── Footer: solo durante coaching, NO en setup ─────────────────
        if (!isPreFullUI(coachRoomState_)) {
            footerModeLabel_.setVisible(true);
            footerPhaseLabel_.setVisible(true);
            footerGenreLabel_.setVisible(true);
            footerTargetLabel_.setVisible(true);
            footerSampleRateLabel_.setVisible(true);
            footerExpLevelLabel_.setVisible(true);
            footerPersonaLabel_.setVisible(true);
            footerPlatformLabel_.setVisible(true);
            footerLlmStatusLabel_.setVisible(true);

            // Altura dinámica: mínimo 18px, se expande con la fuente en high-DPI
            const float footerFontH = juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened().getHeight();
            const int footerH = juce::roundToInt(juce::jmax(18.0f, footerFontH + 4.0f));
            auto footerArea = area.removeFromBottom(footerH);
            {
                float scale = (footerArea.getWidth() <= 0) ? 0.0f
                             : juce::jmin(1.0f, footerArea.getWidth() / 690.0f);
                int gap = juce::roundToInt(4.0f * scale);

                footerModeLabel_.setBounds(footerArea.removeFromLeft(juce::roundToInt(70.0f * scale)));
                footerArea.removeFromLeft(gap);
                footerPhaseLabel_.setBounds(footerArea.removeFromLeft(juce::roundToInt(160.0f * scale)));
                footerArea.removeFromLeft(gap);
                footerGenreLabel_.setBounds(footerArea.removeFromLeft(juce::roundToInt(110.0f * scale)));
                footerArea.removeFromLeft(gap);
                footerTargetLabel_.setBounds(footerArea.removeFromLeft(juce::roundToInt(90.0f * scale)));
                footerArea.removeFromLeft(gap);
                footerSampleRateLabel_.setBounds(footerArea.removeFromLeft(juce::roundToInt(110.0f * scale)));
                footerArea.removeFromLeft(gap);
                footerExpLevelLabel_.setBounds(footerArea.removeFromLeft(juce::roundToInt(100.0f * scale)));
                footerArea.removeFromLeft(gap);
                // Persona label: auto-width basado en contenido real
                auto personaW = juce::jmin(110, footerArea.getWidth());
                footerPersonaLabel_.setBounds(footerArea.removeFromLeft(personaW));
                footerArea.removeFromLeft(gap);
                // Platform target label: auto-width hasta 140px
                auto platformW = juce::jmin(140, footerArea.getWidth());
                footerPlatformLabel_.setBounds(footerArea.removeFromLeft(platformW));
                footerArea.removeFromLeft(gap);
                // LLM status indicator: auto-width hasta 120px
                auto llmStatusW = juce::jmin(120, footerArea.getWidth());
                footerLlmStatusLabel_.setBounds(footerArea.removeFromLeft(llmStatusW));
            }
            area.removeFromBottom(4);
        } else {
            footerModeLabel_.setVisible(false);
            footerPhaseLabel_.setVisible(false);
            footerGenreLabel_.setVisible(false);
            footerTargetLabel_.setVisible(false);
            footerSampleRateLabel_.setVisible(false);
            footerExpLevelLabel_.setVisible(false);
            footerPersonaLabel_.setVisible(false);
            footerPlatformLabel_.setVisible(false);
            footerLlmStatusLabel_.setVisible(false);
        }

        // ═══ SETUP MODE (pre-FullUI): full-width chat, sin side panels ═════════
        if (isPreFullUI(coachRoomState_)) {
            // Mostrar solo el chat a full-width
            refPanel_.setVisible(false);
            referencesSectionLabel_.setVisible(false);
            masterMeterPanel_.setVisible(false);
            tracksSectionLabel_.setVisible(false);
            messengerList_.setVisible(false);
            ollamaStatusLabel_.setVisible(false);
            ollamaRetryBtn_.setVisible(false);
            messengerViewport_.setVisible(false);
            dividerBar_.setVisible(false);

            // ═══ showSuggestions: mostrar chips cuando hay sugerencias Y
            //    (el chat está vacío O showSuggestionsOverride está activo).
            //    Esto permite que los chips inline (como Mix/Masterizar) se vean
            //    aunque el Coach ya haya posteado el mensaje de bienvenida.
            bool showSuggestions = !suggestions_.empty()
                && (showSuggestionsOverride_ || chatMessages_.isEmpty());
            suggestionChipBounds_.clear();

            if (showSuggestions) {
                auto chipRow = area.removeFromBottom(44);
                chatInput_.setBounds(chipRow.removeFromBottom(34).withTrimmedRight(70));
                sendButton_.setBounds(chipRow.getRight() - 32, chipRow.getY() + 2, 30, 30);
                micButton_.setBounds(chipRow.getRight() - 64, chipRow.getY() + 2, 30, 30);
                micButton_.setVisible(true);
                area.removeFromBottom(4);

                auto chipArea = area.removeFromBottom(24).reduced(4, 0);
                int chipGap = 4;
                int cx = chipArea.getX();
                for (auto& sug : suggestions_) {
                    auto chipFont = juce::Font(juce::FontOptions(12.0f)).boldened();
                    float textW = (float)juce::GlyphArrangement::getStringWidthInt(chipFont, sug);
                    int chipW = (int)textW + 16; // 8px padding a cada lado
                    chipW = juce::jmax(chipW, 40); // mínimo 40px para legibilidad
                    if (cx + chipW > chipArea.getRight()) break;
                    suggestionChipBounds_.push_back({cx, chipArea.getY(), chipW, chipArea.getHeight()});
                    cx += chipW + chipGap;
                }
                area.removeFromBottom(2);
            }
            else {
                // ═══ Mode cards activas: ocultar input (usuario responde clickeando) ═══
                bool modeCardsActive = (coachRoomState_ == CoachRoomState::Intention && modeCardMix_.isVisible());
                if (modeCardsActive) {
                    chatInput_.setVisible(false);
                    sendButton_.setVisible(false);
                    // NO remover espacio del input — el viewport usa toda el área disponible
                } else {
                    chatInput_.setVisible(true);
                    sendButton_.setVisible(true);
                    auto inputArea = area.removeFromBottom(34);
                    chatInput_.setBounds(inputArea.withTrimmedRight(70));
                    sendButton_.setBounds(inputArea.getRight() - 32, inputArea.getY() + 2, 30, 30);
                    micButton_.setBounds(inputArea.getRight() - 64, inputArea.getY() + 2, 30, 30);
                    micButton_.setVisible(true);
                    area.removeFromBottom(8);
                }
            }

            chatViewport_.setBounds(area);
            {
                int ch = chatMessages_.getTotalHeight();
                chatMessages_.setSize(area.getWidth(), ch);
            }

            // ─── Posicionar componentes del setup inline ───────────────────────
            performSprint1Layout();
            return;
        }

        // ─── COACHING MODE (FullUI): two-column layout ────────────────────────
        // Referencia Tab1: izquierda 38% chat+refs | derecha 62% pistas
        constexpr float kLeftColumnRatio = 0.38f;
        int splitX                       = (int)(area.getWidth() * kLeftColumnRatio);
        auto leftArea                    = area.removeFromLeft(splitX);
        auto rightArea                   = area.reduced(4, 0);

        dividerBar_.setBounds(leftArea.getRight() + 1, leftArea.getY(), 4, leftArea.getHeight());
        dividerBar_.setVisible(true);

        // ═══ Columna izquierda: adaptativa según visibilidad ──────────────
        // Durante coaching: SOLO chat (sin Master Meter ni References)
        // Durante setup: 3 secciones (Master Meter | Chat | Refs)
        // ═══ Constante de gap usada tanto dentro como fuera del if ═══════
        constexpr int kSectionGap = 3;

        if (masterMeterPanel_.isVisible()) {
            int totalAvailH           = leftArea.getHeight();
            int sectionH              = (totalAvailH - kSectionGap * 2) / 3;

            // ─── SECTION 1: Master Meter ───────────────────────────────────
            masterMeterPanel_.setBounds(leftArea.removeFromTop(sectionH));
            leftArea.removeFromTop(kSectionGap);

            // ─── SECTION 3: References (from bottom) ───────────────────────
            int refLabelH  = 16;
            int apiKeyRowH = 22;
            int refGaps    = 1 + kSectionGap + apiKeyRowH;
            int refPanelH  = sectionH - refLabelH - refGaps;
            refPanelH      = juce::jmax(refPanel_.getNumReferences() > 0 ? 80 : 48, refPanelH);

            refPanel_.setBounds(leftArea.removeFromBottom(refPanelH));
            leftArea.removeFromBottom(1);
            referencesSectionLabel_.setBounds(leftArea.removeFromBottom(refLabelH));

            // ═══ Ollama status row ────
            {
                auto ollamaArea = leftArea.removeFromBottom(apiKeyRowH);
                ollamaStatusLabel_.setBounds(ollamaArea.reduced(4, 0).removeFromLeft(ollamaArea.getWidth() - 24));
                ollamaRetryBtn_.setBounds(ollamaArea.getRight() - 22, ollamaArea.getY() + 2, 18, 18);
            }
        } else {
            // Coaching mode: left column es SOLO el chat, full height
            // No masterMeterPanel, no refPanel, no ollamaStatus
            // leftArea permanece intacto para que chatViewport_ lo use completo
        }

        leftArea.removeFromBottom(kSectionGap);

        // ─── SECTION 2: Chat (remaining = middle section) ────────────────────
        // Input bar + suggestion chips at bottom of section
        // ═══ showSuggestions: usar showSuggestionsOverride_ como override
        bool showSuggestions = !suggestions_.empty()
            && (showSuggestionsOverride_ || chatMessages_.isEmpty());
        suggestionChipBounds_.clear();

        if (showSuggestions) {
            // Chips row + input compact
            auto chipRow = leftArea.removeFromBottom(44);
            chatInput_.setBounds(chipRow.removeFromBottom(34).withTrimmedRight(38));
            sendButton_.setBounds(chipRow.getRight() - 32, chipRow.getY() + 2, 30, 30);
            leftArea.removeFromBottom(4);

            auto chipArea = leftArea.removeFromBottom(24).reduced(4, 0);
            int chipGap   = 4;
            int cx        = chipArea.getX();
            for (auto& sug : suggestions_) {
                float textW =
                    juce::GlyphArrangement::getStringWidthInt(juce::Font(juce::FontOptions(12.0f)).boldened(), sug)
                    + 16.0f;
                int chipW = juce::jmin((int)textW + 4, chipArea.getWidth() / 3);
                chipW     = juce::jmax(chipW, 60);
                if (cx + chipW > chipArea.getRight()) break;
                suggestionChipBounds_.push_back({cx, chipArea.getY(), chipW, chipArea.getHeight()});
                cx += chipW + chipGap;
            }
            leftArea.removeFromBottom(2);
        }
        else {
            auto inputArea = leftArea.removeFromBottom(34);
            chatInput_.setBounds(inputArea.withTrimmedRight(38));
            sendButton_.setBounds(inputArea.getRight() - 32, inputArea.getY() + 2, 30, 30);
            leftArea.removeFromBottom(8);
        }

        // Chat viewport takes the rest of the section
        chatViewport_.setBounds(leftArea);
        {
            int ch = chatMessages_.getTotalHeight();
            chatMessages_.setSize(leftArea.getWidth(), ch);
            writeChatLog("[Resize] chatViewport h=" + juce::String(leftArea.getHeight())
                         + " chatMessages totalH=" + juce::String(ch));
        }

        // ─── Posicionar componentes auxiliares (QuickReplyBar, etc.) ───────────
        performSprint1Layout();

        // ═══ Columna derecha: toolbar + panels (adaptativa según visibilidad) ══
        // Durante coaching: tracksSectionLabel_ oculta → sin toolbar, solo panel de fase
        if (tracksSectionLabel_.isVisible()) {
            tracksSectionLabel_.setBounds(rightArea.removeFromTop(20));
            rightArea.removeFromTop(2);

            auto toolbarArea = rightArea.removeFromTop(18).reduced(0, 1);
            {
                int tx = toolbarArea.getX() + 56;
                int ty = toolbarArea.getY();
                int th = toolbarArea.getHeight();

                chipTipoBounds_ = {tx, ty, 32, th};
                tx += 34;
                chipColorBounds_ = {tx, ty, 38, th};
                tx += 40;
                chipBusBounds_ = {tx, ty, 28, th};
                tx += 30;

                chipMapBounds_ = {tx, ty, 28, th};
                tx += 30;

                chipConfirmRolesBounds_ = {tx, ty, 64, th};

                int rightX         = toolbarArea.getRight();
                expandAllBounds_   = {rightX - 52, ty, 50, th};
                collapseAllBounds_ = {expandAllBounds_.getX() - 62, ty, 58, th};
            }
            rightArea.removeFromTop(2);
        }

        // ═══ Phase panels (gain staging / EQ) in right column ────────────
        // Split-view coaching: phase panel (left ~65%) + evidence panel (right ~35%)
        // Solo cuando messengerViewport_ está oculto (coaching mode)
        // ═══ Phase panels — CoachingEvidenceHost unifica panel derecho ─────
        // Durante coaching (messengerViewport_ oculto), el host gestiona
        // header + split 65/35 + progress dots automáticamente.
        bool isCoaching = !messengerViewport_.isVisible();
        if (isCoaching && (gainStagingPanel_.isVisible() || eqPanel_.isVisible()
            || compressionPanel_.isVisible() || spacePanel_.isVisible()
            || masterCheckPanel_.isVisible())) {
            coachingEvidenceHost_.setBounds(rightArea);
            coachingEvidenceHost_.setVisible(true);
            // Determinar fase activa
            CoachRoomState phase = CoachRoomState::GainStaging;
            if (eqPanel_.isVisible())          phase = CoachRoomState::EQ;
            else if (compressionPanel_.isVisible()) phase = CoachRoomState::Compression;
            else if (spacePanel_.isVisible())       phase = CoachRoomState::Space;
            else if (masterCheckPanel_.isVisible()) phase = CoachRoomState::MasterCheck;
            coachingEvidenceHost_.setActivePhase(phase);
            // Ocultar paneles legacy para que no dibujen fuera del host
            gainStagingPanel_.setVisible(false);
            eqPanel_.setVisible(false);
            compressionPanel_.setVisible(false);
            spacePanel_.setVisible(false);
            masterCheckPanel_.setVisible(false);
            evidencePanel_.setVisible(false);
        } else {
            coachingEvidenceHost_.setVisible(false);
            // ─── Legacy setup layout (no coaching) ────────────────────────
            if (gainStagingPanel_.isVisible()) {
                gainStagingPanel_.setBounds(rightArea);
            } else if (eqPanel_.isVisible()) {
                int eqH = juce::jmin(330, rightArea.getHeight() / 2);
                eqPanel_.setBounds(rightArea.removeFromTop(eqH));
                rightArea.removeFromTop(4);
                messengerViewport_.setViewedComponent(&messengerList_, false);
                messengerList_.setSize(rightArea.getWidth(), messengerList_.getPreferredHeight());
                messengerViewport_.setBounds(rightArea);
            } else if (compressionPanel_.isVisible()) {
                int compH = juce::jmin(280, rightArea.getHeight() - 20);
                compressionPanel_.setBounds(rightArea.removeFromTop(compH));
                rightArea.removeFromTop(4);
                messengerViewport_.setBounds(rightArea);
            } else if (spacePanel_.isVisible()) {
                int spaceH = juce::jmin(260, rightArea.getHeight() - 20);
                spacePanel_.setBounds(rightArea.removeFromTop(spaceH));
                rightArea.removeFromTop(4);
                messengerViewport_.setBounds(rightArea);
            } else if (masterCheckPanel_.isVisible()) {
                masterCheckPanel_.setBounds(rightArea);
            }

            // ─── EvidencePanel: posicionar inline con el panel activo ──────────
            // NO mutar rightArea — usar variables temporales para no afectar
            // el layout del messenger/placeholder que viene despu\xC3\xA9s.
            if (evidencePanel_.isVisible() && (gainStagingPanel_.isVisible()
                || eqPanel_.isVisible() || compressionPanel_.isVisible()
                || spacePanel_.isVisible() || masterCheckPanel_.isVisible())) {
                int evidenceW = rightArea.getWidth() * 35 / 100;
                auto phaseArea  = rightArea.withWidth(rightArea.getWidth() - evidenceW - 2);
                auto evidenceArea = rightArea.withLeft(phaseArea.getRight() + 2).withWidth(evidenceW);
                evidencePanel_.setBounds(evidenceArea);
            }

            // ─── EvidencePanel fallback: si está visible pero sin panel de fase ─
            if (evidencePanel_.isVisible() && !gainStagingPanel_.isVisible()
                && !eqPanel_.isVisible() && !compressionPanel_.isVisible()
                && !spacePanel_.isVisible() && !masterCheckPanel_.isVisible()) {
                evidencePanel_.setBounds(rightArea);
            }

            // ─── Messenger/List viewport ───────────────────────────────────────
            if (evidencePanel_.isVisible() && !gainStagingPanel_.isVisible()
                && !eqPanel_.isVisible() && !compressionPanel_.isVisible()
                && !spacePanel_.isVisible() && !masterCheckPanel_.isVisible()) {
                messengerViewport_.setViewedComponent(&messengerList_, false);
                messengerList_.setSize(rightArea.getWidth(),
                                       juce::jmax(messengerList_.getPreferredHeight(), rightArea.getHeight()));
            } else {
                messengerViewport_.setBounds(rightArea);
                messengerViewport_.setVisible(true);
                if (showMixMap_) {
                    messengerViewport_.setViewedComponent(&mixMapComponent_, false);
                    mixMapComponent_.setSize(rightArea.getWidth(),
                                             juce::jmax(mixMapComponent_.getPreferredHeight(), rightArea.getHeight()));
                } else {
                    messengerViewport_.setViewedComponent(&messengerList_, false);
                    messengerList_.setSize(rightArea.getWidth(),
                                           juce::jmax(messengerList_.getPreferredHeight(), rightArea.getHeight()));
                }
            }

            // ═══ Messenger placeholder: cuando no hay Messengers activos ─────────
            // Se muestra solo si todos los componentes est\xC3\xA1n ocultos
            bool noMessengers = !messengerViewport_.isVisible()
                         && !mixMapComponent_.isVisible()
                         && !messengerList_.isVisible()
                         && !gainStagingPanel_.isVisible()
                         && !eqPanel_.isVisible()
                         && !compressionPanel_.isVisible()
                         && !spacePanel_.isVisible()
                         && !masterCheckPanel_.isVisible();
        if (noMessengers) {
            messengerPlaceholder_.setVisible(true);
            messengerPlaceholder_.setBounds(rightArea.reduced(8, 0));
        } else {
            messengerPlaceholder_.setVisible(false);
        }
    }
} // close extra scope from resized

    void MixCoachPanel::paint(juce::Graphics& g)
    {
        // ═══ TRY/CRITICAL: Evitar que excepciones en paint() derriben FL Studio ═══
        // El crash recurrente VCRUNTIME140.dll + Access Violation ocurre durante
        // operaciones de pintado. JUCE_TRY captura tanto C++ exceptions como
        // SEH (Access Violation) en Windows.
        // Tras capturar, forzamos un repaint para que el próximo frame intente
        // pintar de nuevo (el estado inconsistente dura un frame como máximo).
        JUCE_TRY
        {
            auto area = getLocalBounds();

            // Fondo principal
            g.fillAll(MixCoachTheme::bgDark());

            // ═══ Mode cards: fondo oscuro limpio, sin split layout ═══════════
            // El glass gradient overlay usa white.withAlpha(0.02f).withAlpha(0.5f)
            // que JUCE interpreta como 50% white — NO queremos eso.
            // Todo el split layout + chips + toolbar se salta.
            if (!showSplitLayout_) {
                // Gradiente sutil oscuro: bgDark() -> bgDarker()
                // Reemplaza el glass gradient que tiene componente white.
                juce::ColourGradient modeBgGrad(MixCoachTheme::bgDark(),
                                                juce::Point<float>(0.0f, 0.0f),
                                                MixCoachTheme::bgDarker(),
                                                juce::Point<float>(0.0f, (float)area.getHeight()),
                                                false);
                g.setGradientFill(modeBgGrad);
                g.fillRect(area);

                // ═══ Suggestion chips en modo full-screen (ej: "Saltar referencia") ═══
                // Se posicionan centrados en la parte inferior, visibles incluso
                // cuando el chat está oculto durante ReferenceStage.
                if (!suggestionChipBounds_.empty() && !suggestions_.empty()) {
                    int numVisible = juce::jmin((int)suggestionChipBounds_.size(), (int)suggestions_.size());
                    for (int i = 0; i < numVisible; ++i) {
                        auto& chipRect = suggestionChipBounds_[i];
                        bool hovered = (i == hoveredSuggestionChip_);
                        g.setColour(hovered ? MixCoachTheme::accent().withAlpha(0.20f)
                                            : MixCoachTheme::accent().withAlpha(0.10f));
                        g.fillRoundedRectangle(chipRect.toFloat(), 4.0f);
                        g.setColour(hovered ? MixCoachTheme::accent().withAlpha(0.40f)
                                            : MixCoachTheme::accent().withAlpha(0.20f));
                        g.drawRoundedRectangle(chipRect.toFloat(), 4.0f, 0.5f);
                        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                        g.setColour(hovered ? MixCoachTheme::accentGlow() : MixCoachTheme::textMuted());
                        g.drawText(suggestions_[i], chipRect, juce::Justification::centred);
                    }
                }

                return;
            }

            // ─── Glass gradient overlay sutil ─────────────────────────────────────
            juce::ColourGradient bgGrad(MixCoachTheme::glassHighlight().withAlpha(0.5f),
                                        juce::Point<float>(0.0f, 0.0f),
                                        MixCoachTheme::bgDarker(),
                                        juce::Point<float>(0.0f, (float)area.getHeight()),
                                        false);
            g.setGradientFill(bgGrad);
            g.fillRect(area);

            // ─── Panel backgrounds (izq chat | der pistas) ─── glassmorphism premium ──
            auto inner = area.reduced(6);
            int splitX = (int)(inner.getWidth() * 0.38f);

            // Helper lambda to draw a chip
            auto drawChip = [&](juce::Rectangle<int> chipRect, bool isActive, const juce::String& text) {
                if (isActive) {
                    g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
                    g.fillRoundedRectangle(chipRect.toFloat(), 3.0f);
                    g.setColour(MixCoachTheme::accent().withAlpha(0.4f));
                    g.drawRoundedRectangle(chipRect.toFloat(), 3.0f, 0.5f);
                    g.setColour(MixCoachTheme::accentGlow());
                }
                else {
                    g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
                }
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
                g.drawText(text, chipRect, juce::Justification::centred);
            };

            drawChip(chipTipoBounds_, activeGroupingChip_ == 0, "TYPE");
            drawChip(chipColorBounds_, activeGroupingChip_ == 1, "COLOR");
            drawChip(chipBusBounds_, activeGroupingChip_ == 2, "BUS");

            // MAP toggle chip (V4 Mix Map) — cyan when active
            {
                bool mapActive = showMixMap_;
                auto chipRect  = chipMapBounds_.toFloat();
                if (mapActive) {
                    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.15f));
                    g.fillRoundedRectangle(chipRect, 3.0f);
                    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.4f));
                    g.drawRoundedRectangle(chipRect, 3.0f, 0.5f);
                    g.setColour(MixCoachTheme::accentCyan());
                }
                else {
                    g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
                }
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
                g.drawText("MAP", chipMapBounds_, juce::Justification::centred);
            }

            // Sprint 1: CONFIRMAR roles chip — success colour when pending roles exist,
            // muted when nothing to confirm. Shows pending count as a hint.
            {
                int pendingCount = 0;
                if (coachEngine_ != nullptr) pendingCount = coachEngine_->getIdentityProgress().pendingInferred;
                bool hasPending = (pendingCount > 0);
                auto chipRect   = chipConfirmRolesBounds_.toFloat();
                if (hasPending) {
                    g.setColour(MixCoachTheme::success().withAlpha(0.15f));
                    g.fillRoundedRectangle(chipRect, 3.0f);
                    g.setColour(MixCoachTheme::success().withAlpha(0.4f));
                    g.drawRoundedRectangle(chipRect, 3.0f, 0.5f);
                    g.setColour(MixCoachTheme::success());
                }
                else {
                    g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
                }
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
                juce::String label = hasPending ? "OK " + juce::String(pendingCount) // abbreviated: "OK 3"
                                                : "\xE2\x9C\x93";                 // ✓ (nothing pending)
                g.drawText(label, chipConfirmRolesBounds_, juce::Justification::centred);
            }

            // Draw collapse/expand labels
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("COLLAPSE", collapseAllBounds_, juce::Justification::centred);
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("EXPAND", expandAllBounds_, juce::Justification::centred);

        } // try
        JUCE_CATCH_EXCEPTION
        {
            writeChatLog("[MixCoachPanel::paint] EXCEPCIÓN CAPTURADA (JUCE_CATCH_EXCEPTION)");
            juce::Logger::outputDebugString("[MixCoachPanel::paint] Excepción capturada");
            // Forzar repaint para que el próximo frame intente pintar de nuevo
            repaint();
        }
    }

    void MixCoachPanel::updateMessengers(SlotRegistry& registry, SharedData& sharedData)
    {
        int oldHeight = messengerList_.getPreferredHeight();
        messengerList_.updateMessengers(registry, sharedData);

        if (showMixMap_ && trackRoles_) {
            int oldMapH = mixMapComponent_.getPreferredHeight();
            mixMapComponent_.updateData(registry, sharedData, *trackRoles_);
            int newMapH = mixMapComponent_.getPreferredHeight();

            // ═══ Forzar resize del viewport si el mapa cambió de altura ═══
            // Bug fix: el MixMapComponent no aparecía hasta redimensionar el plugin
            // porque updateData() no notificaba al viewport del cambio de tamaño.
            if (oldMapH != newMapH) {
                // Usar viewport width en lugar de component width porque el
                // componente puede tener width=0 en la primera carga (antes de
                // que resized() lo dimensione). El viewport siempre tiene bounds
                // válidos cuando está visible.
                int vpW = juce::jmax(100, messengerViewport_.getWidth());
                mixMapComponent_.setSize(vpW, newMapH);
                messengerViewport_.resized();
            }
        }

        int newHeight = messengerList_.getPreferredHeight();
        if (oldHeight != newHeight) resized();
    }

    void MixCoachPanel::addMessage(const juce::String& text, const juce::String& tag)
    {
        chatMessages_.addMessage(text, tag);
        scrollChatToBottom();
    }

    void MixCoachPanel::addUserMessage(const juce::String& text)
    {
        chatMessages_.addUserMessage(text);
        scrollChatToBottom();
    }

    void MixCoachPanel::addSystemMessage(const juce::String& text)
    {
        chatMessages_.addSystemMessage(text);
        scrollChatToBottom();
    }

    void MixCoachPanel::clearMessages()
    {
        chatMessages_.clear();
        lastTrackProblemCardsUs_ = 0;
    }

    void MixCoachPanel::setShowSystemMessages(bool show)
    {
        showSystemMessages_ = show;
        chatMessages_.setShowSystemMessages(show);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateCoachAdvice — Actualiza consejos del coach + roles + issues para Mix Map
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::addReverbCard(const ReverbCardData& data)
    {
        chatMessages_.addReverbCard(data);
        scrollChatToBottom();
    }

    void MixCoachPanel::addPluginSuggestionCard(const PluginSuggestionGroup& group)
    {
        chatMessages_.addPluginSuggestionCard(group);
        scrollChatToBottom();
    }

    void MixCoachPanel::addMasterCheckCard(const MasterCheckCardData& data)
    {
        chatMessages_.addMasterCheckCard(data);
        scrollChatToBottom();
    }

    void MixCoachPanel::addCorrectionCard(const CorrectionCardData& data)
    {
        chatMessages_.addCorrectionCard(data);
        scrollChatToBottom();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateLlmStatusLabel — Actualiza el indicador de estado LLM
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::updatePlatformLabel(mixcoach::MasterDestination dest)
    {
        juce::String label;
        juce::String tooltip;
        juce::Colour colour = MixCoachTheme::textMuted();

        switch (dest) {
            case mixcoach::MasterDestination::Spotify:
                label   = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8E\xA7")) + " Spotify";
                tooltip = "Target: -14 LUFS (Spotify)";
                colour  = juce::Colour(0xFF1DB954);
                break;
            case mixcoach::MasterDestination::YouTube:
                label   = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x93\xBA")) + " YouTube";
                tooltip = "Target: -13 LUFS (YouTube)";
                colour  = juce::Colour(0xFFFF0000);
                break;
            case mixcoach::MasterDestination::AppleMusic:
                label   = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8D\x8E")) + " Apple Music";
                tooltip = "Target: -16 LUFS (Apple Music)";
                colour  = juce::Colour(0xFFFA57A1);
                break;
            case mixcoach::MasterDestination::SoundCloud:
                label   = juce::String(juce::CharPointer_UTF8("\xE2\x98\x81")) + " SoundCloud";
                tooltip = "Target: -8 LUFS (SoundCloud)";
                colour  = juce::Colour(0xFFFF7700);
                break;
            case mixcoach::MasterDestination::CD:
                label   = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x92\xBF")) + " CD";
                tooltip = "Target: -9 LUFS (CD)";
                colour  = juce::Colour(0xFFC0C0C0);
                break;
            case mixcoach::MasterDestination::StreamingGeneral:
            default:
                label   = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x8A")) + " Streaming";
                tooltip = "Target: -14 LUFS (Streaming General)";
                colour  = juce::Colour(0xFF8B5CF6);
                break;
        }

        footerPlatformLabel_.setText(label, juce::dontSendNotification);
        footerPlatformLabel_.setColour(juce::Label::textColourId, colour);
        footerPlatformLabel_.setTooltip(tooltip);
    }

    void MixCoachPanel::updateLlmStatusLabel(CoachEngine::LlmStatus status)
    {
        using namespace mixcoach;
        juce::String emoji   = CoachEngine::llmStatusEmoji(status);
        juce::String name    = CoachEngine::llmStatusName(status);
        juce::String tooltip;
        juce::Colour colour;

        switch (status) {
            case CoachEngine::LlmStatus::Connected:
                colour  = juce::Colour(0xFF22C55E); // verde
                tooltip = "LLM conectado y respondiendo normalmente";
                break;
            case CoachEngine::LlmStatus::Fallback:
                colour  = juce::Colour(0xFFF59E0B); // amarillo
                tooltip = "LLM no disponible — usando analisis local DSP";
                break;
            case CoachEngine::LlmStatus::Offline:
                colour  = juce::Colour(0xFFEF4444); // rojo
                tooltip = "Sin conexion — solo analisis DSP basico";
                break;
        }

        footerLlmStatusLabel_.setText(emoji + " LLM: " + name, juce::dontSendNotification);
        footerLlmStatusLabel_.setColour(juce::Label::textColourId, colour);
        footerLlmStatusLabel_.setTooltip(tooltip);
    }

    void MixCoachPanel::updateCoachAdvice(CoachEngine& coach)
    {
        coachEngine_ = &coach;
        messengerList_.updateCoachAdvice(coach);

        // ─── Sync persona label with CoachEngine ───────────────────────────
        {
            auto persona = coach.getCoachPersona();
            auto traits  = mixcoach::getPersonaTraits(persona);
            footerPersonaLabel_.setText(juce::String(traits.icon) + " " + juce::String(traits.name),
                                        juce::dontSendNotification);
        }

        // ─── Sync platform target label with CoachEngine ────────────────────
        {
            auto dest = coach.getPlatformTarget();
            float lufs = ::mixcoach::getDestinationLUFS(dest);
            const char* name = coach.getPlatformTargetName();
            footerPlatformLabel_.setText("TARGET: " + juce::String(lufs, 0) + " LUFS (" + juce::String(name) + ")",
                                        juce::dontSendNotification);
        }

        // ─── Sync LLM status label + wire state change callback ─────────────
        {
            auto status = coach.getLlmStatus();
            updateLlmStatusLabel(status);

            // Wire state change callback (only once)
            coach.setLlmStatusChangedCallback(
                [this](mixcoach::CoachEngine::LlmStatus oldStatus, mixcoach::CoachEngine::LlmStatus newStatus) {
                    updateLlmStatusLabel(newStatus);

                    // Toast on state change
                    juce::String msg;
                    if (newStatus == mixcoach::CoachEngine::LlmStatus::Connected)
                        msg = "[LLM] Asistente AI reconectado \xE2\x9C\x85";
                    else if (newStatus == mixcoach::CoachEngine::LlmStatus::Fallback)
                        msg = "[LLM] Usando analisis local \xF0\x9F\x9F\xA1 — LLM no disponible";
                    else if (newStatus == mixcoach::CoachEngine::LlmStatus::Offline)
                        msg = "[LLM] Sin conexion \xF0\x9F\x94\xB4 — solo analisis DSP";

                    if (msg.isNotEmpty() && coachEngine_ != nullptr)
                        coachEngine_->respondWith(msg, MentorMessage::Type::Info);
                });
        }
        trackRoles_ = &coach.getTrackRoles();

        // ─── Issue badges for Mix Map ──────────────────────────────────────
        // collectAllIssues() es O(n) por track, se llama desde el timer del
        // background thread (~8s) donde ya se ejecuta periodicAnalysis(),
        // por lo que no añade overhead extra en el UI thread.
        std::array<MixMapComponent::TrackIssueBadge, SlotRegistry::kMaxSlots> badges{};
        {
            auto issues = coach.collectAllIssues();

            // Indexar issues por slotIndex
            for (const auto& issue : issues) {
                int idx = issue.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) continue;

                auto& badge     = badges[idx];
                badge.hasIssues = true;
                badge.issueCount++;

                if (issue.isOptimal) {
                    badge.isOptimal = true;
                    badge.hasIssues = false; // Optimal = clean, no issues to show
                    continue;
                }

                // Severidad máxima por slot
                if (issue.severity > badge.maxSeverity) badge.maxSeverity = issue.severity;

                if (issue.isCritical) badge.isCritical = true;

                // Short issue type (primer issue encontrado)
                if (badge.shortType.isEmpty()) {
                    // Mapear domain e issueType a abreviaciones legibles
                    if (issue.domain == "gain") badge.shortType = "GAIN";
                    else if (issue.domain == "tonal")
                        badge.shortType = "EQ";
                    else if (issue.domain == "dynamics")
                        badge.shortType = "DYN";
                    else if (issue.domain == "spatial")
                        badge.shortType = "PHASE";
                    else if (issue.domain == "masking")
                        badge.shortType = "MASK";
                    else
                        badge.shortType = issue.issueType.substring(0, 4);
                }
            }

            // Nota: collectAllIssues() ya incluye pistas óptimas en el vector
            // con isOptimal=true, procesadas en el loop de arriba.
            // No es necesario llamar identifyOptimalTracks() por separado.
        }

        mixMapComponent_.setTrackIssues(badges);

        // ─── Wire plugin pill click → buildMessageForProblem() ──────────────
        if (trackProblemCard_.onPluginClicked == nullptr && coachEngine_ != nullptr) {
            trackProblemCard_.onPluginClicked = [this](int /*trackIndex*/,
                                                       int sugIndex,
                                                       const TrackProblemData& track) {
                if (coachEngine_ == nullptr) return;

                // ═══ Plugin tracking: guardar nombre del plugin aplicado ────────
                if (sugIndex >= 0 && sugIndex < (int)track.pluginSuggestions.size()) {
                    const auto& pluginName = track.pluginSuggestions[sugIndex].pluginName;
                    if (pluginName.isNotEmpty() && !appliedPlugins_.contains(pluginName)) {
                        appliedPlugins_.add(pluginName);
                        writeChatLog("[PluginTrack] Usuario aplic\xF3: \"" + pluginName + "\" en \""
                                     + track.trackName + "\"");
                        // ═══ Gap A: Sincronizar con CoachEngine ═══
                        if (coachEngine_ != nullptr)
                            coachEngine_->recordAppliedPlugin(pluginName);
                    }
                }

                // ═══ GAP #4: Compression suggestion → post crest before/after ═══
                if (track.domain == "dynamics") {
                    auto telem = coachEngine_->getLatestTelemetry(track.slotIndex);
                    float currentCrest = telem.crestFactor;

                    juce::String msg = "\xE2\x9C\x93 **" + track.trackName + "**: ";  // ✓ **TrackName**:
                    if (currentCrest > 0.0f && track.delta != 0.0f) {
                        bool isOvercompressed = track.issueType.contains("LOW_CREST")
                                                || track.issueType.contains("OVERCOMPRESS");
                        if (isOvercompressed) {
                            float targetCrest = currentCrest + std::abs(track.delta);
                            msg += "Crest **" + juce::String(currentCrest, 1) + " dB** [RIGHT] **"
                                   + juce::String(targetCrest, 1) + " dB** (reduce compresi\xC3\xB3n)";
                        } else {
                            float targetCrest = juce::jmax(0.0f, currentCrest - std::abs(track.delta));
                            msg += "Crest **" + juce::String(currentCrest, 1) + " dB** [RIGHT] **"
                                   + juce::String(targetCrest, 1) + " dB** (aplica compresi\xC3\xB3n)";
                        }
                    } else {
                        msg += "Aplica sugerencia de compresi\xC3\xB3n";
                    }
                    addSystemMessage(msg);
                    return;
                }

                // ─── Non-dynamics: existing behavior ─────────────────────────
                auto& provider = coachEngine_->getPluginSuggestionsProvider();

                // Reconstruir ProblemType desde domain/issueType almacenados
                auto problemType = PluginSuggestionsProvider::domainToProblemType(
                    track.domain, track.issueType);
                if (problemType == ProblemType::Unknown) return;

                // Construir mensaje detallado con buildMessageForProblem
                auto details = provider.buildMessageForProblem(
                    problemType, track.trackName, track.delta, track.frequencyHz);
                if (details.isNotEmpty()) {
                    addSystemMessage("[NOTES] **Detalles del plugin:**\n\n" + details);
                }
            };
        }

        // ─── Track Problem Cards inline — construir grupos con populatePluginSuggestions()
        updateTrackProblemCards(coach);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setSelectedTrackSlot — Selección externa (desde Tab 2 -> Tab 1 sync)
    // ═══════════════════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Filter toolbar chip clicks + collapse/expand
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();

        // ═══ Platform Target Selector (footer) ═══
        if (e.originalComponent == &footerPlatformLabel_) {
            if (coachEngine_ == nullptr) return;

            juce::PopupMenu menu;
            menu.addItem(1, "Spotify \xE2\x80\x94 -14 LUFS");
            menu.addItem(2, "YouTube \xE2\x80\x94 -14 LUFS");
            menu.addItem(3, "Apple Music \xE2\x80\x94 -16 LUFS");
            menu.addItem(4, "SoundCloud \xE2\x80\x94 -8 LUFS");
            menu.addItem(5, "CD \xE2\x80\x94 -9 LUFS");
            menu.addItem(6, "Streaming General \xE2\x80\x94 -14 LUFS");

            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&footerPlatformLabel_),
                               [this](int result) {
                                   if (coachEngine_ == nullptr) return;
                                   auto currentDest = coachEngine_->getMasterDestination();
                                   auto newDest = currentDest;

                                   switch (result) {
                                       case 1: newDest = MasterDestination::Spotify; break;
                                       case 2: newDest = MasterDestination::YouTube; break;
                                       case 3: newDest = MasterDestination::AppleMusic; break;
                                       case 4: newDest = MasterDestination::SoundCloud; break;
                                       case 5: newDest = MasterDestination::CD; break;
                                       case 6: newDest = MasterDestination::StreamingGeneral; break;
                                       default: return;
                                   }

                                   if (newDest != currentDest) {                        coachEngine_->setPlatformTarget(newDest);
                        updatePlatformLabel(newDest);
                                       // El coach confirma el cambio
                                       float lufs = coachEngine_->getDestinationLUFS();
                                       juce::String msg = "\xF0\x9F\x8E\xAF Target de loudness ajustado a "
                                           + juce::String(coachEngine_->getPlatformTargetName())
                                           + " \xE2\x80\x94 " + juce::String(lufs, 0) + " LUFS";
                                       addSystemMessage(msg);
                                   }
                               });
            return;
        }

        // ─── Grouping chips ────────────────────────────────────────────────
        if (chipTipoBounds_.contains(pos)) {
            activeGroupingChip_ = 0;
            showMixMap_         = false;
            messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Type);
            messengerViewport_.setViewedComponent(&messengerList_, false);
            resized();
            repaint();
            return;
        }
        if (chipColorBounds_.contains(pos)) {
            activeGroupingChip_ = 1;
            showMixMap_         = false;
            messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Colour);
            messengerViewport_.setViewedComponent(&messengerList_, false);
            resized();
            repaint();
            return;
        }
        if (chipBusBounds_.contains(pos)) {
            activeGroupingChip_ = 2;
            showMixMap_         = false;
            messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Bus);
            messengerViewport_.setViewedComponent(&messengerList_, false);
            resized();
            repaint();
            return;
        }

        // ─── Mix Map toggle (V4) ──────────────────────────────────────────
        if (chipMapBounds_.contains(pos)) {
            showMixMap_         = !showMixMap_;
            activeGroupingChip_ = showMixMap_ ? 3 : 2; // 3=MAP, 2=BUS (default)
            if (showMixMap_) {
                messengerViewport_.setViewedComponent(&mixMapComponent_, false);
            }
            else {
                messengerViewport_.setViewedComponent(&messengerList_, false);
            }
            resized();
            repaint();
            return;
        }

        // ─── Sprint 1: CONFIRMAR roles — confirms all pending and advances phase
        if (chipConfirmRolesBounds_.contains(pos) && coachEngine_ != nullptr) {
            int n = coachEngine_->confirmAllInferredRoles();
            if (n > 0) {
                messengerList_.syncRolesFromCoach();
                messengerList_.repaint();
                repaint();
            }
            return;
        }

        // ─── Collapse/Expand ───────────────────────────────────────────────
        if (collapseAllBounds_.contains(pos)) {
            messengerList_.collapseAll();
            resized();
            return;
        }
        if (expandAllBounds_.contains(pos)) {
            messengerList_.expandAll();
            resized();
            return;
        }

        // ─── System messages toggle ──────────────────────────────────────
        if (systemToggleBounds_.contains(pos) && systemToggleBounds_.getWidth() > 0
            && chatMessages_.getNumSystemMessages() > 0) {
            showSystemMessages_ = !showSystemMessages_;
            setShowSystemMessages(showSystemMessages_);
            repaint();
            return;
        }

        // ─── Suggestion chips (columna izquierda) ───────────────────────────
        for (size_t i = 0; i < suggestionChipBounds_.size(); ++i) {
            if (suggestionChipBounds_[i].contains(pos)) {
                if (onSuggestionClicked && i < suggestions_.size()) {
                    onSuggestionClicked(suggestions_[i]);
                    chatInput_.clear();
                    suggestionChipBounds_.clear();
                    repaint();
                }
                return;
            }
        }

        // ─── Ollama retry button already handled via onClick ─────────────
        // Nothing needed here since ollamaRetryBtn_.onClick handles the click

        // ─── Experience Level pill click → PopupMenu ───────────────────────
        if (footerExpLevelLabel_.getBounds().contains(pos)) {
            juce::PopupMenu menu;
            menu.addItem(1, "Novice", true, currentExpLevel_ == 0);
            menu.addItem(2, "Intermediate", true, currentExpLevel_ == 1);
            menu.addItem(3, "Advanced", true, currentExpLevel_ == 2);
            menu.addItem(4, "Expert", true, currentExpLevel_ == 3);

            menu.showMenuAsync(
                juce::PopupMenu::Options().withTargetComponent(&footerExpLevelLabel_), [this](int result) {
                    if (result >= 1 && result <= 4) {
                        int newLevel     = result - 1;
                        currentExpLevel_ = newLevel;

                        static const char* levelNames[] = {"NOVICE", "INTERMEDIATE", "ADVANCED", "EXPERT"};
                        footerExpLevelLabel_.setText("LEVEL: " + juce::String(levelNames[newLevel]),
                                                     juce::dontSendNotification);
                        repaint();

                        if (onExperienceLevelChanged) onExperienceLevelChanged(newLevel);
                    }
                });
            return;
        }

        // ═══ Platform Target click — select platform via PopupMenu ═══════════
        if (footerPlatformLabel_.getBounds().contains(pos)) {
            juce::PopupMenu menu;
            using namespace mixcoach;
            for (int i = 0; i < static_cast<int>(sizeof(destinationNames) / sizeof(destinationNames[0])); ++i) {
                auto dest = static_cast<MasterDestination>(i);
                float lufs = getDestinationLUFS(dest);
                menu.addItem(i + 1,
                             juce::String(destinationNames[i]) + " (" + juce::String(lufs, 0) + " LUFS)",
                             true,
                             coachEngine_ != nullptr && coachEngine_->getPlatformTarget() == dest);
            }

            menu.showMenuAsync(
                juce::PopupMenu::Options().withTargetComponent(&footerPlatformLabel_), [this](int result) {
                    if (result >= 1 && result <= 6 && coachEngine_ != nullptr) {
                        auto newDest = static_cast<MasterDestination>(result - 1);
                        coachEngine_->setPlatformTarget(newDest);
                        float lufs = getDestinationLUFS(newDest);
                        const char* name = destinationNames[result - 1];
                        footerPlatformLabel_.setText(
                            "TARGET: " + juce::String(lufs, 0) + " LUFS (" + juce::String(name) + ")",
                            juce::dontSendNotification);
                        repaint();

                        if (coachEngine_->hasEngineerName()) {
                            coachEngine_->respondWithPremium(
                                "[PLATAFORMA: " + juce::String(name) + "] "
                                "Target de loudness ajustado a " + juce::String(lufs, 0) + " LUFS ("
                                + juce::String(name) + ")",
                                MentorMessage::Type::Info);
                        }
                    }
                });
            return;
        }

        // ═══ Coach Persona click — cycle thru 3 personas via PopupMenu ═══════
        if (footerPersonaLabel_.getBounds().contains(pos)) {
            juce::PopupMenu menu;
            using namespace mixcoach;
            for (int i = 0; i < kNumPersonas; ++i) {
                auto p     = static_cast<CoachPersona>(i);
                auto traits = getPersonaTraits(p);
                menu.addItem(i + 1,
                             juce::String(traits.icon) + " " + juce::String(traits.name),
                             true,
                             coachEngine_ != nullptr && coachEngine_->getCoachPersona() == p);
            }

            menu.showMenuAsync(
                juce::PopupMenu::Options().withTargetComponent(&footerPersonaLabel_), [this](int result) {
                    if (result >= 1 && result <= kNumPersonas && coachEngine_ != nullptr) {
                        CoachPersona newPersona = static_cast<CoachPersona>(result - 1);
                        coachEngine_->setCoachPersona(newPersona);
                        auto traits = getPersonaTraits(newPersona);
                        footerPersonaLabel_.setText(juce::String(traits.icon) + " " + juce::String(traits.name),
                                                     juce::dontSendNotification);
                        repaint();

                        // Notify the coach to acknowledge the change
                        if (coachEngine_->hasEngineerName()) {
                            coachEngine_->respondWithPremium(
                                "[PERSONA: " + juce::String(traits.name) + "] "
                                + juce::String(traits.description),
                                MentorMessage::Type::Info);
                        }
                    }
                });
            return;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseMove — Suggestion chip hover tracking
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::mouseMove(const juce::MouseEvent& e)
    {
        auto pos               = e.getPosition();
        int oldHover           = hoveredSuggestionChip_;
        int oldSystemHover     = hoveredSystemToggle_;
        hoveredSuggestionChip_ = -1;
        hoveredSystemToggle_   = -1;

        // ─── Check system toggle hover ────────────────────────────────────
        if (systemToggleBounds_.contains(pos) && systemToggleBounds_.getWidth() > 0
            && chatMessages_.getNumSystemMessages() > 0) {
            hoveredSystemToggle_ = 1;
        }

        // ─── Check suggestion chips ────────────────────────────────────────
        for (size_t i = 0; i < suggestionChipBounds_.size(); ++i) {
            if (suggestionChipBounds_[i].contains(pos)) {
                hoveredSuggestionChip_ = (int)i;
                break;
            }
        }

        if (hoveredSuggestionChip_ != oldHover || hoveredSystemToggle_ != oldSystemHover) repaint();
    }

    void MixCoachPanel::setSelectedTrackSlot(int slotIndex)
    {
        messengerList_.setSelectedSlot(slotIndex);
        mixMapComponent_.setSelectedSlot(slotIndex);
    }

    void MixCoachPanel::syncSelectionToMixMap(int slotIndex)
    {
        mixMapComponent_.setSelectedSlot(slotIndex);
    }

    void MixCoachPanel::syncSelectionToMessengerList(int slotIndex)
    {
        messengerList_.setSelectedSlot(slotIndex);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateFooterInfo — Actualiza la footer bar con datos en tiempo real
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::updateFooterInfo(const juce::String& phaseName,
                                         const juce::String& genre,
                                         const juce::String& target,
                                         const juce::String& sampleRate,
                                         int expLevel)
    {
        static const char* levelNames[] = {"NOVICE", "INTERMEDIATE", "ADVANCED", "EXPERT"};
        int clampedLevel                = juce::jlimit(0, 3, expLevel);

        footerPhaseLabel_.setText("CURRENT PHASE: " + phaseName, juce::dontSendNotification);
        footerGenreLabel_.setText("GENRE: " + genre, juce::dontSendNotification);
        footerTargetLabel_.setText("TARGET: " + target, juce::dontSendNotification);
        footerSampleRateLabel_.setText("SAMPLE RATE: " + sampleRate, juce::dontSendNotification);
        footerExpLevelLabel_.setText("LEVEL: " + juce::String(levelNames[clampedLevel]), juce::dontSendNotification);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setExperienceLevel — Actualiza la UI del nivel de experiencia
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::setOllamaStatus(bool connected, const juce::String& providerLabel)
    {
        if (connected) {
            ollamaStatusLabel_.setText(providerLabel + " \xE2\x80\xA2 conectado",
                                       juce::dontSendNotification);
            ollamaStatusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::info());
            ollamaRetryBtn_.setVisible(false);
        }
        else {
            ollamaStatusLabel_.setText(providerLabel + " \xE2\x80\xA2 desconectado",
                                       juce::dontSendNotification);
            ollamaStatusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::warning().withAlpha(0.7f));
            ollamaRetryBtn_.setVisible(true);
        }
        repaint();
    }

    void MixCoachPanel::setCoachMode(bool isMasterMode)
    {
        if (isMasterMode) {
            footerModeLabel_.setText("MODE: MASTER", juce::dontSendNotification);
            footerModeLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentCyan());
        }
        else {
            footerModeLabel_.setText("MODE: MIX", juce::dontSendNotification);
            footerModeLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        }
        repaint();
    }

    void MixCoachPanel::setExperienceLevel(int levelIndex)
    {
        currentExpLevel_                = juce::jlimit(0, 3, levelIndex);
        static const char* levelNames[] = {"NOVICE", "INTERMEDIATE", "ADVANCED", "EXPERT"};
        footerExpLevelLabel_.setText("LEVEL: " + juce::String(levelNames[currentExpLevel_]),
                                     juce::dontSendNotification);
        repaint();
    }

    void MixCoachPanel::textEditorReturnKeyPressed(juce::TextEditor& editor)
    {
        if (&editor == &chatInput_) {
            auto text = chatInput_.getText().trim();

            // ═══ Atajos 4.1: Ctrl+Enter=/ok, Ctrl+Shift+Enter=/skip ═══════════
            auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
            if (mods.isCtrlDown()) {
                if (mods.isShiftDown()) {
                    // Ctrl+Shift+Enter → /skip
                    if (coachEngine_ != nullptr)
                        coachEngine_->executeCommand("/skip");
                } else {
                    // Ctrl+Enter → /ok (solo Ctrl, sin Shift)
                    if (coachEngine_ != nullptr)
                        coachEngine_->executeCommand("/ok");
                }
                chatInput_.clear();
                return;
            }

            if (text.isNotEmpty()) {
                // ═══ Interceptar comandos slash: No enviar al chat normal,
                //     ejecutar directo en el motor y mostrar sistema ↪ respuesta
                if (text.startsWith("/") && coachEngine_ != nullptr) {
                    coachEngine_->executeCommand(text);
                    chatInput_.clear();
                } else if (onMessageSent) {
                    onMessageSent(text);
                    chatInput_.clear();
                }
            }
        }
        // Ollama local no requiere input de API key
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Atajos 4.1: Keyboard shortcuts — Ctrl+/ → /why
    // ═══════════════════════════════════════════════════════════════════════════
    bool MixCoachPanel::keyPressed(const juce::KeyPress& key)
    {
        if (coachEngine_ == nullptr) return false;

        // Ctrl+/ → /why (explicar corrección actual)
        if (key == juce::KeyPress::createFromDescription("ctrl + /")) {
            coachEngine_->executeCommand("/why");
            return true;
        }

        return false;
    }



    // ═══════════════════════════════════════════════════════════════════════════
    //  refreshGainStagingPanel — Poblar GainStagingPanel desde CoachEngine
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::refreshGainStagingPanel(CoachEngine& coach)
    {
        std::vector<TrackGainRow> rows;
        auto trackRoles = coach.getTrackRoles();
        auto& registry = coach.getSharedData().getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            TrackGainRow row;
            row.slotIndex = info.slotIndex;
            row.trackName = juce::String(info.trackName);

            // Determinar rol y target
            if (info.slotIndex >= 0 && info.slotIndex < (int)trackRoles.size()) {
                auto role = trackRoles[info.slotIndex];
                row.roleName = juce::String(getRoleName(role));
                row.targetPeakDb = (role == TrackRole::Kick || role == TrackRole::BassSub || role == TrackRole::Bass808) ? -12.0f
                                 : (role == TrackRole::VozPrincipal || role == TrackRole::Snare) ? -15.0f
                                 : -18.0f;
            } else {
                row.roleName = "Unknown";
                row.targetPeakDb = -18.0f;
            }

            // Leer peak actual desde telemetría
            auto tr = coach.getSharedData().getTrackAudioResult(info.slotIndex);
            row.currentPeakDb = juce::jmax(tr.peakLeft, tr.peakRight);
            row.clipping = (row.currentPeakDb > -0.5f);
            row.suggestedDeltaDb = row.targetPeakDb - row.currentPeakDb;
            row.isApplied = false;

            rows.push_back(row);
        });

        gainStagingPanel_.setTrackData(rows);
    }



// ─── MixCoachPanel forwarding to ChatMessagesComponent ───────────
void MixCoachPanel::addTrackGroupCard(const TrackProblemGroup& group)
{
    chatMessages_.addTrackGroupCard(group);
}

    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void ChatMessagesComponent::visibilityChanged()
    {
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(60);
        }
        // Nunca detener el timer — el constructor ya lo arrancó y los
        // mensajes nuevos (addMessage) reinician el timer automáticamente.
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Avanza animaciones de entrada de burbujas
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessagesComponent::timerCallback()
    {
        bool anyAnimating = false;
        int64_t now = juce::Time::getMillisecondCounter();

        // ─── Animar entrada de mensajes ─────────────────────────────────
        for (auto& msg : messages_) {
            if (!msg.animateIn) continue;

            int64_t elapsed = now - msg.animStartMs;
            if (elapsed >= ChatBubble::kAnimDurationMs) {
                // Animación completada
                msg.animateIn = false;
                continue;
            }

            anyAnimating = true;
        }

        // ─── Mantener timer activo mientras el typing indicator está visible ──
        if (isTyping_) anyAnimating = true;

        if (anyAnimating)
            repaint();
        else
            stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  EvidencePanel — Mini visor contextual para split-view coaching
    // ═══════════════════════════════════════════════════════════════════════════

    EvidencePanel::EvidencePanel()
    {
        setOpaque(false);
    }

    void EvidencePanel::updateFromAnalyzer(const AudioAnalyzer& analyzer)
    {
        const auto& master = analyzer.getMasterAnalysis();
        // Guard: no actualizar si el analyzer aun no tiene datos reales
        if (master.getPeak() == 0.0f && master.getRMS() == 0.0f)
            return;
        vuLeft_ = master.getRMS();
        vuRight_ = master.getRMS(); // Stereo average as fallback
        peakLeft_ = master.getPeak();
        peakRight_ = master.getPeak();
        const auto* bands = master.getSpectrum();
        if (bands != nullptr) {
            // Nota: truncamos a 60 bands para la vista mini en EvidencePanel.
            // El analyzer completo usa kNumSpectrumBins para la vista detallada en Tools.
            numBands_ = juce::jmin(60, kNumSpectrumBins);
            float maxVal = 0.0f;
            for (int i = 0; i < numBands_; ++i) {
                bandEnergies_[i] = bands[i];
                if (bands[i] > maxVal) maxVal = bands[i];
            }
            if (maxVal > 0.0f)
                for (int i = 0; i < numBands_; ++i)
                    bandEnergies_[i] = juce::jlimit(0.0f, 1.0f, bandEnergies_[i] / maxVal);
        }
        repaint();
    }

    void EvidencePanel::updateCrestData(float peak, float rms, float crestFactor)
    {
        crestPeak_ = peak;
        crestRms_ = rms;
        crestFactor_ = crestFactor;
        repaint();
    }

    void EvidencePanel::updateSpatialData(float correlation, float width)
    {
        correlation_ = correlation;
        stereoWidth_ = width;
        repaint();
    }

    void EvidencePanel::updateMatchScore(float score)
    {
        matchScore_ = score;
        repaint();
    }

    void EvidencePanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
            return;

        // ─── Glass background (unificado con MixCoachTheme) ───────────────
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.8f));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
        g.drawRoundedRectangle(bounds, 6.0f, 0.5f);

        auto area = bounds.reduced(4, 4);

        // ─── Header ────────────────────────────────────────────────────────
        auto header = area.removeFromTop(16);
        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
        g.setColour(MixCoachTheme::textDim().withAlpha(0.6f));

        juce::String phaseName;
        switch (coachState_) {
            case CoachRoomState::GainStaging:  phaseName = "VU / RMS"; break;
            case CoachRoomState::EQ:           phaseName = "SPECTRUM"; break;
            case CoachRoomState::Compression:  phaseName = "CREST FACTOR"; break;
            case CoachRoomState::Space:        phaseName = "STEREO IMAGE"; break;
            case CoachRoomState::MasterCheck:  phaseName = "MATCH SCORE"; break;
            default:                           phaseName = "EVIDENCE"; break;
        }
        g.drawText(phaseName, header, juce::Justification::centredLeft);

        area.removeFromTop(2);

        // ─── Dibujar según el estado ───────────────────────────────────────
        switch (coachState_) {
            case CoachRoomState::GainStaging:
                drawVuMeter(g, area);
                break;
            case CoachRoomState::EQ:
                drawMiniSpectrum(g, area);
                break;
            case CoachRoomState::Compression:
                drawCrestGauge(g, area);
                break;
            case CoachRoomState::Space:
                drawVectorscope(g, area);
                break;
            case CoachRoomState::MasterCheck:
                drawMatchScore(g, area);
                break;
            default:
                break;
        }
    }

    void EvidencePanel::drawVuMeter(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float mid = area.getCentreX();
        float barW = area.getWidth() * 0.25f;
        float barH = area.getHeight() - 20;
        float barY = area.getY() + 16;

        // ─── Left channel ──────────────────────────────────────────────────
        auto leftBar = juce::Rectangle<float>(mid - barW - 4, barY, barW, barH);
        g.setColour(MixCoachTheme::bgInput());
        g.fillRoundedRectangle(leftBar, 2.0f);

        float leftNorm = juce::jmap(juce::jlimit(-60.0f, 0.0f, vuLeft_), -60.0f, 0.0f);
        auto leftFill = leftBar.removeFromBottom(leftBar.getHeight() * leftNorm);
        juce::ColourGradient grad(
            MixCoachTheme::success(), leftFill.getX(), leftFill.getY(),
            MixCoachTheme::error(), leftFill.getX(), leftFill.getBottom(),
            false);
        grad.addColour(0.7f, MixCoachTheme::warning());
        g.setGradientFill(grad);
        g.fillRoundedRectangle(leftFill, 2.0f);

        // ─── Right channel ─────────────────────────────────────────────────
        auto rightBar = juce::Rectangle<float>(mid + 4, barY, barW, barH);
        g.setColour(MixCoachTheme::bgInput());
        g.fillRoundedRectangle(rightBar, 2.0f);

        float rightNorm = juce::jmap(juce::jlimit(-60.0f, 0.0f, vuRight_), -60.0f, 0.0f);
        auto rightFill = rightBar.removeFromBottom(rightBar.getHeight() * rightNorm);
        juce::ColourGradient rightGrad(
            MixCoachTheme::success(), rightFill.getX(), rightFill.getY(),
            MixCoachTheme::error(), rightFill.getX(), rightFill.getBottom(),
            false);
        rightGrad.addColour(0.7f, MixCoachTheme::warning());
        g.setGradientFill(rightGrad);
        g.fillRoundedRectangle(rightFill, 2.0f);

        // ─── Label ─────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(MixCoachTheme::textPrimary());
        g.drawText(juce::String(vuLeft_, 1) + " dB",
                   juce::Rectangle<float>(mid - barW - 4, barY + barH + 2, barW, 16),
                   juce::Justification::centred);
        g.drawText(juce::String(vuRight_, 1) + " dB",
                   juce::Rectangle<float>(mid + 4, barY + barH + 2, barW, 16),
                   juce::Justification::centred);
    }

    void EvidencePanel::drawMiniSpectrum(juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (numBands_ < 2) {
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
            g.drawText("Waiting for audio...", area, juce::Justification::centred);
            return;
        }

        float barW = area.getWidth() / (float)numBands_;
        float barH = area.getHeight() - 4;

        for (int i = 0; i < numBands_; ++i) {
            float h = barH * bandEnergies_[i];
            auto bar = juce::Rectangle<float>(area.getX() + i * barW,
                                               area.getBottom() - 4 - h,
                                               barW - 1, h);
            // Color gradient: low=cyan, mid=purple, high=orange
            float t = (float)i / (float)numBands_;
            juce::Colour col = t < 0.33f ? MixCoachTheme::accentCyan().withAlpha(0.7f)
                            : t < 0.66f ? MixCoachTheme::accent().withAlpha(0.7f)
                            : MixCoachTheme::warning().withAlpha(0.7f);
            g.setColour(col);
            g.fillRect(bar);
        }

        // ─── Highlight frequency marker ────────────────────────────────────
        if (highlightedFreq_ > 0.0f && numBands_ > 0) {
            float maxFreq = 20000.0f;
            float minFreq = 20.0f;
            float t = (std::log2(highlightedFreq_ / minFreq) / std::log2(maxFreq / minFreq));
            int idx = (int)(t * numBands_);
            idx = juce::jlimit(0, numBands_ - 1, idx);
            float mx = area.getX() + idx * barW + barW * 0.5f;
            g.setColour(MixCoachTheme::error().withAlpha(0.6f));
            g.drawVerticalLine((int)mx, area.getY(), area.getBottom());
            if (highlightLabel_.isNotEmpty()) {
                g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
                g.drawText(highlightLabel_,
                           juce::Rectangle<float>(mx - 20, area.getY(), 40, 12),
                           juce::Justification::centred);
            }
        }
    }

    void EvidencePanel::drawCrestGauge(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // ─── Crest factor dial ──────────────────────────────────────────────
        float cx = area.getCentreX();
        float cy = area.getCentreY() - 8;
        float rad = juce::jmin(area.getWidth(), area.getHeight() * 0.7f) * 0.4f;

        // Arc background
        g.setColour(MixCoachTheme::bgInput());
        g.fillEllipse(cx - rad, cy - rad, rad * 2, rad * 2);

        // Arc fill (180 degrees)
        float crestNorm = juce::jlimit(0.0f, 1.0f, crestFactor_ / 20.0f);
        float startAngle = juce::MathConstants<float>::pi * 0.75f;
        float endAngle = startAngle + (float)juce::MathConstants<float>::pi * 1.5f * crestNorm;

        juce::Path arc;
        arc.addCentredArc(cx, cy, rad - 3, rad - 3, 0.0f,
                          startAngle, endAngle, true);
        juce::Colour arcCol = crestFactor_ < 8.0f ? MixCoachTheme::success()
                            : crestFactor_ < 14.0f ? MixCoachTheme::warning()
                            : MixCoachTheme::error();
        g.setColour(arcCol);
        g.strokePath(arc, juce::PathStrokeType(3.0f));

        // Needle
        float needleAngle = startAngle + (float)juce::MathConstants<float>::pi * 1.5f * crestNorm;
        float nx = cx + (rad - 6) * std::cos(needleAngle);
        float ny = cy + (rad - 6) * std::sin(needleAngle);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawLine(cx, cy, nx, ny, 1.5f);

        // Center dot
        g.fillEllipse(cx - 2.5f, cy - 2.5f, 5, 5);

        // Value labels
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(MixCoachTheme::textPrimary());
        g.drawText(juce::String(crestFactor_, 1) + " dB",
                   juce::Rectangle<float>(cx - 25, cy + rad + 2, 50, 14),
                   juce::Justification::centred);

        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
        g.drawText("CREST",
                   juce::Rectangle<float>(cx - 15, cy - 6, 30, 12),
                   juce::Justification::centred);
    }

    void EvidencePanel::drawVectorscope(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float cx = area.getCentreX();
        float cy = area.getCentreY() - 4;
        float rad = juce::jmin(area.getWidth(), area.getHeight()) * 0.35f;

        // ─── Scope circle ──────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgInput());
        g.fillEllipse(cx - rad, cy - rad, rad * 2, rad * 2);
        g.setColour(MixCoachTheme::border().withAlpha(0.3f));
        g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 0.5f);

        // Crosshair
        g.drawLine(cx - rad, cy, cx + rad, cy, 0.3f);
        g.drawLine(cx, cy - rad, cx, cy + rad, 0.3f);

        // ─── Correlation indicator ─────────────────────────────────────────
        float corrNorm = (correlation_ + 1.0f) * 0.5f; // -1→0, +1→1
        float lx = cx - rad * 0.7f;
        float ly = cy - (corrNorm - 0.5f) * rad * 1.2f;

        juce::Colour corrCol = correlation_ < -0.3f ? MixCoachTheme::error()
                             : correlation_ < 0.3f ? MixCoachTheme::warning()
                             : MixCoachTheme::success();

        // Dot + glow
        g.setColour(corrCol.withAlpha(0.2f));
        g.fillEllipse(lx - 8, ly - 8, 16, 16);
        g.setColour(corrCol);
        g.fillEllipse(lx - 3, ly - 3, 6, 6);

        // ─── Width bar (horizontal line) ───────────────────────────────────
        float widthNorm = juce::jlimit(0.0f, 1.0f, stereoWidth_);
        float wl = cx - rad * widthNorm;
        float wr = cx + rad * widthNorm;
        g.setColour(MixCoachTheme::accentCyan().withAlpha(0.5f));
        g.drawHorizontalLine((int)cy + 4, wl, wr);

        // ─── Labels ────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
        juce::String corrStr = (correlation_ >= 0.0f ? "+" : "") + juce::String(correlation_, 2);
        g.drawText("Corr: " + corrStr,
                   juce::Rectangle<float>(cx - 25, cy + rad + 2, 50, 12),
                   juce::Justification::centred);
    }

    void EvidencePanel::drawMatchScore(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float cx = area.getCentreX();
        float cy = area.getCentreY() - 6;
        float rad = juce::jmin(area.getWidth(), area.getHeight()) * 0.3f;

        // ─── Score ring ────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgInput());
        g.fillEllipse(cx - rad, cy - rad, rad * 2, rad * 2);

        // Arc fill
        float score = juce::jlimit(0.0f, 1.0f, matchScore_);
        float startAngle = -juce::MathConstants<float>::pi * 0.5f;
        float endAngle = startAngle + (float)juce::MathConstants<float>::pi * 2.0f * score;

        juce::Path arc;
        arc.addCentredArc(cx, cy, rad - 4, rad - 4, 0.0f,
                          startAngle, endAngle, true);
        juce::Colour scoreCol = score > 0.8f ? MixCoachTheme::success()
                              : score > 0.5f ? MixCoachTheme::warning()
                              : MixCoachTheme::error();
        g.setColour(scoreCol);
        g.strokePath(arc, juce::PathStrokeType(4.0f));

        // ─── Score percentage ──────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::String((int)(score * 100.0f)) + "%",
                   juce::Rectangle<float>(cx - 25, cy - 10, 50, 20),
                   juce::Justification::centred);

        // ─── Label ─────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
        g.drawText("MATCH",
                   juce::Rectangle<float>(cx - 20, cy + rad + 2, 40, 10),
                   juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  advanceSmoothScroll — Avanza la animación de scroll suave
    //  Definición inline movida aquí desde CoachChatComponent.h para evitar
    //  error de función duplicada entre la sección pública y privada.
    // ═══════════════════════════════════════════════════════════════════════════
    bool MixCoachPanel::advanceSmoothScroll()
    {
        if (!smoothScroll_.active) return false;

        smoothScroll_.elapsedFrames++;
        float t = (float)smoothScroll_.elapsedFrames / (float)SmoothScrollState::kDurationFrames;
        if (t >= 1.0f) {
            t = 1.0f;
            smoothScroll_.active = false;
        }

        // Ease-out quad
        float eased = t * (2.0f - t);
        double pos = smoothScroll_.currentPos + (smoothScroll_.targetPos - smoothScroll_.currentPos) * (double)eased;

        auto& scrollbar = chatViewport_.getVerticalScrollBar();
        scrollbar.setCurrentRange(pos, scrollbar.getCurrentRangeSize(), juce::dontSendNotification);

        if (!smoothScroll_.active) {
            scrollbar.setCurrentRange(smoothScroll_.targetPos, scrollbar.getCurrentRangeSize(), juce::sendNotificationSync);
        }
        repaint();
        return smoothScroll_.active;
    }

} // namespace mixcoach

