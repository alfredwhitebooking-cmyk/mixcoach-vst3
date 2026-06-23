#include "CoachChatComponent.h"
#include "../engine/CoachEngine.h"

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
        auto logFile = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("MixCoach_Logs")
            .getChildFile("MixCoach_Crash.log");
        logFile.getParentDirectory().createDirectory();
        juce::FileOutputStream fos(logFile, true);
        if (fos.openedOk()) {
            fos << "[" << juce::Time::getCurrentTime().toString(true, true)
                << "] [CHAT] " << msg << "\n";
            fos.flush();
        }
    } catch (...) {
        // Silencio total — no podemos loggear un fallo de logging
    }
}

static float getTextWidth(const juce::Font& font, const juce::String& text)
{
    if (text.isEmpty())
        return 0.0f;
    juce::GlyphArrangement ga;
    ga.addLineOfText(font, text, 0.0f, 0.0f);
    return ga.getBoundingBox(0, ga.getNumGlyphs(), true).getWidth();
}

static float measureWrappedHeight(const juce::String& text,
                                  const juce::Font& font,
                                  float maxWidth)
{
    if (text.isEmpty())
        return font.getHeight();

    auto paragraphs = juce::StringArray::fromLines(text);
    float totalH = 0.0f;
    float lineH = font.getHeight();
    float spaceW = getTextWidth(font, " ");

    for (int p = 0; p < paragraphs.size(); ++p) {
        auto words = juce::StringArray::fromTokens(paragraphs[p], " ", "");
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

        if (p < paragraphs.size() - 1)
            totalH += lineH * 0.5f;
    }

    return totalH;
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawMarkdownText — Text wrapping con soporte markdown básico (**bold**)
//  Tokeniza el texto por marcadores **, alternando entre normal y bold.
// ═══════════════════════════════════════════════════════════════════════════
struct MarkdownSegment {
    juce::String text;
    bool isBold;
};

/** Divide el texto en segmentos: normal y bold según **bloque** */
static std::vector<MarkdownSegment> parseMarkdown(const juce::String& text)
{
    std::vector<MarkdownSegment> segments;
    if (text.isEmpty())
        return segments;

    int pos = 0;
    while (pos < text.length())
    {
        int openIdx = text.indexOf(pos, "**");
        if (openIdx < 0)
        {
            // Resto del texto sin markers
            segments.push_back({ text.substring(pos), false });
            break;
        }
        // Texto antes del marker
        if (openIdx > pos)
            segments.push_back({ text.substring(pos, openIdx), false });

        // Buscar cierre **
        int closeIdx = text.indexOf(openIdx + 2, "**");
        if (closeIdx < 0)
        {
            // No hay cierre — tratar resto como normal
            segments.push_back({ text.substring(pos), false });
            break;
        }
        // Texto bold
        segments.push_back({ text.substring(openIdx + 2, closeIdx), true });
        pos = closeIdx + 2;
    }

    return segments;
}

/** Dibuja texto con word-wrap, usando boldened font para segmentos **bold** */
static void drawMarkdownText(juce::Graphics& g,
                              const juce::String& text,
                              const juce::Font& baseFont,
                              const juce::Colour& colour,
                              juce::Rectangle<float> area)
{
    if (text.isEmpty())
        return;

    auto segments = parseMarkdown(text);
    if (segments.empty())
        return;

    juce::Font boldFont = baseFont.boldened();

    float maxW = area.getWidth();
    float x = area.getX();
    float y = area.getY();
    float lineH = baseFont.getHeight();
    float spaceW = getTextWidth(baseFont, " ");

    // Word-wrap: construimos línea por línea con segmentos
    // Para simplificar, mezclamos todo en una línea de marcadores
    // y hacemos wrap a nivel de palabra
    struct Word {
        juce::String text;
        bool isBold;
    };
    std::vector<Word> words;
    for (auto& seg : segments)
    {
        if (seg.text.isEmpty()) continue;
        // Dividir segmento en palabras
        auto tokens = juce::StringArray::fromTokens(seg.text, " ", "");
        for (int i = 0; i < tokens.size(); ++i)
        {
            if (tokens[i].isNotEmpty())
                words.push_back({ tokens[i], seg.isBold });
        }
    }

    // Renderizar word-wrap con medición bold-aware
    int wordIdx = 0;
    while (wordIdx < (int)words.size())
    {
        float lineW = 0.0f;
        int startIdx = wordIdx;

        // Medir cuántas palabras caben en esta línea
        while (wordIdx < (int)words.size())
        {
            auto& w = words[wordIdx];
            juce::Font& f = w.isBold ? boldFont : const_cast<juce::Font&>(baseFont);
            float wordW = getTextWidth(f, w.text);
            float sep = (lineW > 0.0f && startIdx < wordIdx) ? spaceW : 0.0f;

            if (lineW + sep + wordW > maxW && startIdx < wordIdx)
                break;

            lineW += sep + wordW;
            wordIdx++;
        }

        if (startIdx == wordIdx)
        {
            // Una palabra más ancha que maxW — dibujar de todas formas
            wordIdx = startIdx + 1;
        }

        // Dibujar la línea con los segmentos correspondientes
        float drawX = x;
        for (int i = startIdx; i < wordIdx; ++i)
        {
            auto& w = words[i];
            g.setFont(w.isBold ? boldFont : baseFont);
            g.setColour(w.isBold ? colour.brighter(0.3f) : colour);

            float wordW = w.isBold 
                ? getTextWidth(boldFont, w.text) 
                : getTextWidth(baseFont, w.text);
            juce::Rectangle<float> wordRect(drawX, y, wordW + 1.0f, lineH);
            g.drawText(w.text, wordRect, juce::Justification::centredLeft);

            drawX += wordW;
            if (i < wordIdx - 1)
                drawX += spaceW;
        }

        y += lineH + 2.0f;
    }
}

// ─── AM/PM timestamp formatter ─────────────────────────────────────────────
static juce::String formatTimestamp()
{
    auto now = juce::Time::getCurrentTime();
    int hours = now.getHours();
    int mins  = now.getMinutes();
    juce::String ampm = (hours < 12) ? "AM" : "PM";
    if (hours == 0) hours = 12;
    else if (hours > 12) hours -= 12;
    return juce::String(hours) + ":"
         + (mins < 10 ? "0" : "") + juce::String(mins)
         + " " + ampm;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
//  ChatMessagesComponent — Renderiza burbujas de chat
// ═══════════════════════════════════════════════════════════════════════════

ChatMessagesComponent::ChatMessagesComponent()
{
    setOpaque(false);
}

void ChatMessagesComponent::addMessage(const juce::String& text, const juce::String& tag)
{
    ChatBubble bubble;
    bubble.text      = text;
    bubble.tag       = tag;
    bubble.isUser    = false;
    bubble.isSystem  = false;
    bubble.timestamp = formatTimestamp();
    messages_.push_back(bubble);
    {
        juce::String preview = text.substring(0, 80);
        preview = preview.replace("\n", " ");
        writeChatLog("[ChatBubble] addMessage (coach): \"" + preview + "\" (len=" + juce::String(text.length()) + ")");
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
    messages_.push_back(bubble);
    {
        juce::String preview = text.substring(0, 80);
        preview = preview.replace("\n", " ");
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
    messages_.push_back(bubble);
    {
        juce::String preview = text.substring(0, 80);
        preview = preview.replace("\n", " ");
        writeChatLog("[ChatBubble] addSystemMessage: \"" + preview + "\" (len=" + juce::String(text.length()) + ")");
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
        if (m.isSystem)
            ++count;
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

    streamingMessageIndex_ = (int)messages_.size();
    messages_.push_back(bubble);
    streamingActive_ = true;
    streamingStartMs_ = juce::Time::getMillisecondCounter();

    resized();
    repaint();
}

void ChatMessagesComponent::appendToStreamingMessage(const juce::String& text)
{
    if (!streamingActive_ || streamingMessageIndex_ < 0
        || streamingMessageIndex_ >= (int)messages_.size())
        return;

    messages_[streamingMessageIndex_].text += text;
    resized();
    repaint();
}

void ChatMessagesComponent::finalizeStreamingMessage()
{
    if (!streamingActive_)
        return;

    streamingActive_ = false;
    streamingMessageIndex_ = -1;
    resized();
    repaint();
}

void ChatMessagesComponent::setTypingIndicator(bool isTyping)
{
    if (isTyping != isTyping_) {
        isTyping_ = isTyping;
        if (isTyping)
            typingStartMs_ = juce::Time::getMillisecondCounter();
        resized();
        repaint();
    }
}

int ChatMessagesComponent::getTotalHeight() const
{
    float total = 6.0f;  // top padding (compacto)

    if (messages_.empty()) {
        // Welcome card height
        total += 160.0f;
        total += 6.0f;
    } else {
        float maxW = (float)getWidth();
        for (auto& msg : messages_) {
            // Saltar mensajes del sistema si están ocultos
            if (msg.isSystem && !showSystemMessages_)
                continue;
            float bh = getBubbleHeight(msg, maxW);
            total += bh;
            total += 6.0f;  // spacing compacto
        }
    }

    // Typing indicator
    if (isTyping_)
        total += 36.0f;

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
float ChatMessagesComponent::getBubbleHeight(const ChatBubble& bubble,
                                              float maxWidth) const
{
    // Throttle logging: solo loggear cada 100 llamadas para no inundar el log
    static int heightLogCounter = 0;
    bool shouldLogHeight = (++heightLogCounter % 100 == 1);
    float bubbleMaxW = maxWidth * (bubble.isUser ? 0.75f : 0.85f);
    bubbleMaxW = juce::jlimit(120.0f, 350.0f, bubbleMaxW);

    float textW = bubbleMaxW - 12.0f;  // 6px padding cada lado
    juce::Font font(juce::FontOptions(12.5f));

    float textH = measureWrappedHeight(bubble.text, font, textW);

    // ─── Mensajes del sistema: altura JUSTA, midiendo con el MISMO texto que se renderiza
    //     drawSystemMessage renderiza "🛠 " + msg.text con drawMarkdownText.
    //     Usamos el mismo prefijo para que la medición coincida EXACTAMENTE con el render.
    //     El padding mínimo es 2px para que el texto no quede pegado al borde.
    if (bubble.isSystem) {
        float resultH;
        juce::Font sysFont(juce::FontOptions(8.5f));
        float sysTextW = maxWidth - 32.0f;  // coincide con drawSystemMessage
        juce::String displayText = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9B\xA0 ")) + bubble.text;
        float sysTextH = measureWrappedHeight(displayText, sysFont, sysTextW);
        // Bold fudge: si el texto contiene **bold**, palabras bold son ~15% más anchas
        if (bubble.text.contains("**"))
            sysTextH *= 1.15f;
        resultH = juce::jmax(14.0f, sysTextH + 2.0f);  // 2px padding mínimo
        if (shouldLogHeight) {
            juce::String preview = bubble.text.substring(0, 60).replace("\n", " ");
            writeChatLog("[Height] SYSTEM maxW=" + juce::String(maxWidth, 0) + " textW=" + juce::String(sysTextW, 1)
                + " textH=" + juce::String(sysTextH, 1) + " result=" + juce::String(resultH, 1)
                + " txt=\"" + preview + "\"");
        }
        return resultH;
    }

    // ─── Bold fudge: si el texto contiene **bold**, las palabras bold
    //     son ~15% más anchas, lo que puede añadir líneas extra.
    bool hasBold = bubble.text.contains("**");
    if (hasBold)
        textH *= 1.15f;

    // ─── Tag height: solo si hay tag (la mayoría de mensajes NO tienen tag)
    //     Esto ahorra 18px por burbuja en mensajes sin tag.
    float tagH = (!bubble.isUser && !bubble.tag.isEmpty()) ? 18.0f : 0.0f;
    // ─── Padding: innerPad(6px) × 2 + 2px reserva timestamp
    float bubbleH = textH + tagH + 14.0f;
    float resultH = juce::jmax(44.0f, bubbleH);

    if (shouldLogHeight) {
        juce::String preview = bubble.text.substring(0, 60).replace("\n", " ");
        juce::String type = bubble.isUser ? "USER" : "COACH";
        writeChatLog("[Height] " + type + " maxW=" + juce::String(maxWidth, 0) + " textW=" + juce::String(textW, 1)
            + " textH=" + juce::String(textH, 1) + " tagH=" + juce::String(tagH, 1)
            + " result=" + juce::String(resultH, 1)
            + " bold=" + (hasBold ? "Y" : "N") + " tag=\"" + bubble.tag + "\""
            + " txt=\"" + preview + "\"");
    }

    return resultH;
}

// ═══════════════════════════════════════════════════════════════════════════
//  paint — Dibuja welcome card o burbujas + typing indicator
// ═══════════════════════════════════════════════════════════════════════════
void ChatMessagesComponent::paint(juce::Graphics& g)
{
    float y = 6.0f;
    float maxW = (float)getWidth();

    if (messages_.empty()) {
        // ─── Welcome card ────────────────────────────────────────────────
        drawWelcomeCard(g, juce::Rectangle<float>(6.0f, y, maxW - 12.0f, 160.0f));
        y += 166.0f;
    } else {
        int visibleCount = 0;
        for (int i = 0; i < (int)messages_.size(); ++i) {
            auto& msg = messages_[i];

            // Saltar mensajes del sistema si están ocultos
            if (msg.isSystem && !showSystemMessages_)
                continue;

            float bubbleH = getBubbleHeight(msg, maxW);
            float bubbleMaxW = juce::jlimit(120.0f, 350.0f, maxW * (msg.isUser ? 0.75f : 0.85f));

            if (msg.isSystem) {
                // ─── Mensajes del sistema: compactos, sin burbuja ──────
                drawSystemMessage(g, { 12.0f, y, maxW - 24.0f, bubbleH }, msg);
            } else if (msg.isUser) {
                float x = maxW - bubbleMaxW - 6.0f;
                drawUserBubble(g, { x, y, bubbleMaxW, bubbleH }, msg);
            } else {
                // ─── Para mensaje en streaming, dibujar con cursor ──────
                if (streamingActive_ && i == streamingMessageIndex_) {
                    drawCoachBubbleStreaming(g, { 6.0f, y, bubbleMaxW, bubbleH }, msg);
                } else {
                    drawCoachBubble(g, { 6.0f, y, bubbleMaxW, bubbleH }, msg);
                }
            }
            juce::String type = msg.isSystem ? "SYS" : (msg.isUser ? "USR" : "COA");
            if (visibleCount % 5 == 0 && visibleCount < 10) {
                juce::String preview = msg.text.substring(0, 40).replace("\n", " ");
                writeChatLog("[Paint] idx=" + juce::String(i) + " type=" + type
                    + " y=" + juce::String(y, 1) + " h=" + juce::String(bubbleH, 1)
                    + " txt=\"" + preview + "\"");
            }
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
void ChatMessagesComponent::drawWelcomeCard(juce::Graphics& g,
                                             juce::Rectangle<float> bounds)
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
    juce::ColourGradient bgGrad(
        juce::Colour(0xDD2A1048), bounds.getX(), bounds.getY(),
        juce::Colour(0xDD081020), bounds.getX(), bounds.getBottom(), false);
    bgGrad.addColour(0.5f, juce::Colour(0xDD181A38));
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(bounds, cr);

    // ─── Glass highlight (2 capas de profundidad) ───────────────────────
    auto glassTop = bounds.withHeight(bounds.getHeight() * 0.3f);
    juce::ColourGradient glassGrad(
        juce::Colours::white.withAlpha(0.09f), glassTop.getX(), glassTop.getY(),
        juce::Colour(0x00000000),              glassTop.getX(), glassTop.getBottom(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(glassTop, cr);

    // Second glass layer (shorter, brighter)
    auto glassPeak = glassTop.withHeight(glassTop.getHeight() * 0.4f);
    juce::ColourGradient glassPeakGrad(
        juce::Colours::white.withAlpha(0.12f), glassPeak.getX(), glassPeak.getY(),
        juce::Colour(0x00000000),              glassPeak.getX(), glassPeak.getBottom(), false);
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
    float iconCx = iconArea.getCentreX();
    float iconCy = iconArea.getY() + 20.0f;

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
    // Emoji
    g.setFont(juce::Font(juce::FontOptions(17.0f)));
    g.setColour(MixCoachTheme::accentGlow());
    g.drawText(juce::CharPointer_UTF8("\xF0\x9F\xA4\x96"),
               juce::Rectangle<float>(iconCx - 12.0f, iconCy - 12.0f, 24.0f, 24.0f),
               juce::Justification::centred);

    area.removeFromTop(4);

    // ─── Título con glow sutil ───────────────────────────────────────────
    auto titleArea = area.removeFromTop(22);
    // Subtle text glow
    g.setFont(juce::Font(juce::FontOptions(14.5f)).boldened());
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.setFont(juce::Font(juce::FontOptions(14.5f)).boldened());
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.drawText("Welcome to MixCoach!",
               titleArea.translated(0, 1), juce::Justification::centred);
    g.setColour(MixCoachTheme::textBright());
    g.drawText("Welcome to MixCoach!",
               titleArea, juce::Justification::centred);

    // ─── Decorative line under title ─────────────────────────────────────
    float lineCx = titleArea.getCentreX();
    float lineY = (float)titleArea.getBottom() + 2.0f;
    float lineW = 50.0f;
    juce::ColourGradient lineGrad(
        MixCoachTheme::accent().withAlpha(0.25f), lineCx, lineY,
        MixCoachTheme::accent().withAlpha(0.0f),  lineCx + lineW, lineY, false);
    lineGrad.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.12f));
    g.setGradientFill(lineGrad);
    g.drawHorizontalLine((int)lineY, lineCx - lineW, lineCx + lineW);

    area.removeFromTop(4);

    // ─── Subtítulo ────────────────────────────────────────────────────────
    auto subArea = area.removeFromTop(16);
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    g.setColour(MixCoachTheme::textDim());
    g.drawText("Your AI assistant for professional-sounding mixes.",
               subArea, juce::Justification::centred);

    area.removeFromTop(4);

    // ─── Badge de fase premium ────────────────────────────────────────────
    auto badgeArea = area.removeFromTop(22);
    auto badgeRect = badgeArea.withSizeKeepingCentre(150, 20).toFloat();
    // Glow exterior
    g.setColour(MixCoachTheme::warning().withAlpha(0.06f));
    g.fillRoundedRectangle(badgeRect.expanded(4.0f, 3.0f), 11.0f);
    // Fondo con gradiente
    juce::ColourGradient badgeGrad(
        MixCoachTheme::warning().withAlpha(0.10f), badgeRect.getX(), badgeRect.getY(),
        MixCoachTheme::warning().withAlpha(0.05f), badgeRect.getX(), badgeRect.getBottom(), false);
    badgeGrad.addColour(0.5f, MixCoachTheme::warning().withAlpha(0.08f));
    g.setGradientFill(badgeGrad);
    g.fillRoundedRectangle(badgeRect, 10.0f);
    // Borde
    g.setColour(MixCoachTheme::warning().withAlpha(0.40f));
    g.drawRoundedRectangle(badgeRect, 10.0f, 0.5f);
    // Texto
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
    g.setColour(MixCoachTheme::warning());
    g.drawText("CURRENT PHASE: 2 - ORGANIZATION",
               badgeRect, juce::Justification::centred);

    // ─── Hint (elegante, más pequeño) ─────────────────────────────────────
    area.removeFromTop(2);
    auto hintArea = area;
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.65f));
    g.drawText("Type a message or choose a quick suggestion below.",
               hintArea, juce::Justification::centred);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawSystemMessage — Mensaje del sistema en formato compacto y dimmed
// ═══════════════════════════════════════════════════════════════════════════
void ChatMessagesComponent::drawSystemMessage(juce::Graphics& g,
                                                juce::Rectangle<float> bounds,
                                                const ChatBubble& msg)
{
    // ─── Texto word-wrapped, dimmed, sin burbuja, sin timestamp ────────
    auto textArea = bounds.reduced(4, 1);

    // Pequeña barra de acento a la izquierda (altura completa)
    g.setColour(MixCoachTheme::accent().withAlpha(0.20f));
    g.fillRect(textArea.getX() - 2.0f, textArea.getY() + 1.0f, 2.0f, textArea.getHeight() - 2.0f);

    // Icono + texto con word-wrap (ocupa toda el área, sin timestamp)
    juce::Font sysFont(juce::FontOptions(8.5f));
    juce::String displayText = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9B\xA0 ")) + msg.text;
    drawMarkdownText(g, displayText, sysFont,
                     MixCoachTheme::textMuted().withAlpha(0.65f), textArea);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawTypingIndicator — Indicador animado de "IA está escribiendo..."
// ═══════════════════════════════════════════════════════════════════════════
void ChatMessagesComponent::drawTypingIndicator(juce::Graphics& g,
                                                  juce::Rectangle<float> bounds)
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
    juce::ColourGradient glassGrad(
        juce::Colours::white.withAlpha(0.07f), glassH.getX(), glassH.getY(),
        juce::Colour(0x00000000),              glassH.getX(), glassH.getBottom(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(glassH, cr);

    // ─── Borde ────────────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
    g.drawRoundedRectangle(bounds, cr, 0.5f);

    // ─── Tres puntos animados ────────────────────────────────────────────
    float cx = bounds.getX() + 20.0f;
    float cy = bounds.getCentreY();
    float dotR = 3.5f;
    float spacing = 12.0f;

    int64_t now = juce::Time::getMillisecondCounter();
    float elapsed = (float)(now - typingStartMs_);

    for (int i = 0; i < 3; ++i) {
        // Animar alpha sinusoidal: cada punto con offset de fase
        float phase = (float)i * 2.094f;  // 120° apart
        float alpha = 0.3f + 0.7f * (0.5f + 0.5f * std::sin(elapsed * 0.005f + phase));

        g.setColour(MixCoachTheme::accentGlow().withAlpha(alpha));
        g.fillEllipse(cx + (float)i * spacing - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    // ─── Label "escribiendo..." ──────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(8.0f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
    g.drawText("MixCoach is typing...",
               juce::Rectangle<float>(bounds.getX() + 54.0f, bounds.getY(),
                                       bounds.getWidth() - 58.0f, bounds.getHeight()),
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
    float innerPad = 6.0f;
    auto textArea = bounds.reduced(innerPad, innerPad);
    auto textBounds = textArea.withBottom(textArea.getBottom() - 4.0f);

    juce::Font msgFont(juce::FontOptions(12.5f));

    // Cursor intermitente: parpadeo sinusoidal
    int64_t now = juce::Time::getMillisecondCounter();
    float cursorAlpha = 0.3f + 0.7f * (0.5f + 0.5f * std::sin((float)(now - streamingStartMs_) * 0.0119f));

    // PASO 1: Dibujar el texto SIN el cursor (con markdown)
    drawMarkdownText(g, msg.text, msgFont, MixCoachTheme::textPrimary(), textBounds);

    // PASO 2: Calcular posición del cursor al final del texto
    // Usamos GlyphArrangement para encontrar dónde termina el texto
    juce::String lastChar = juce::CharPointer_UTF8("\xE2\x96\x8A"); // ▊
    juce::GlyphArrangement ga;
    ga.addLineOfText(msgFont, msg.text + " ", 0.0f, 0.0f);
    auto textBoundsFloat = ga.getBoundingBox(0, ga.getNumGlyphs(), true);
    float cursorX = textBounds.getX() + textBoundsFloat.getWidth() + 2.0f;
    float cursorY = textBounds.getY() + textBoundsFloat.getY();

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
    g.drawText("typing...",
               tsBounds.reduced(0, 0).withLeft(tsBounds.getX() + 60.0f),
               juce::Justification::bottomLeft);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawCoachBubble — Burbuja compacta del coach (izquierda)
//  Estilo WhatsApp: 1 sombra, relleno sólido, padding reducido
// ═══════════════════════════════════════════════════════════════════════════
void ChatMessagesComponent::drawCoachBubble(juce::Graphics& g,
                                             juce::Rectangle<float> bounds,
                                             const ChatBubble& msg)
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
    auto textArea = bounds.reduced(innerPad, innerPad);
    float drawY = textArea.getY();

    // ─── Tag opcional (píldora compacta) ────────────────────────────────
    if (msg.tag.isNotEmpty()) {
        auto tagFont = juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened();
        float tagW = getTextWidth(tagFont, msg.tag) + 10.0f;
        float tagH = 14.0f;
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
void ChatMessagesComponent::drawUserBubble(juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            const ChatBubble& msg)
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
    auto textArea = bounds.reduced(innerPad, innerPad);

    juce::Font msgFont(juce::FontOptions(12.0f));
    auto textBounds = textArea.withBottom(textArea.getBottom() - 14.0f);
    drawMarkdownText(g, msg.text, msgFont, MixCoachTheme::textPrimary(), textBounds);

    // ─── ✓✓ checkmark + timestamp ────────────────────────────────────────
    auto statusBounds = bounds.withTop(bounds.getBottom() - 14.0f).reduced(6.0f, 0.0f);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
    g.setColour(MixCoachTheme::info().withAlpha(0.50f));
    g.drawText(msg.timestamp + juce::String("  \xE2\x9C\x93\xE2\x9C\x93"),
               statusBounds, juce::Justification::bottomRight);
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
    auto bounds = getLocalBounds().toFloat();
    const float cr = 6.0f;
    auto btnRect = bounds.reduced(1.0f, 1.0f);

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
    juce::ColourGradient btnGrad(
        MixCoachTheme::accent().brighter(0.15f),
        (float)btnRect.getCentreX(), (float)btnRect.getY(),
        MixCoachTheme::accentDim(),
        (float)btnRect.getCentreX(), (float)btnRect.getBottom(),
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
    float s = 7.5f;

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
    if (onClick)
        onClick();
}

// ═══════════════════════════════════════════════════════════════════════════
//  MixCoachPanel Implementation
// ═══════════════════════════════════════════════════════════════════════════

MixCoachPanel::MixCoachPanel()
{
    // ─── Section: PISTAS (columna derecha ~62%) ─────────────────────────────
    tracksSectionLabel_.setText("TRACKLIST",
                                juce::dontSendNotification);
    tracksSectionLabel_.setFont(MixCoachTheme::sectionHeaderFont());
    tracksSectionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    addAndMakeVisible(tracksSectionLabel_);

    // ─── Master Meter Panel ─────────────────────────────────────────────
    addAndMakeVisible(masterMeterPanel_);

    // ─── Section: REFERENCES (debajo del chat, columna izquierda) ─────────
    referencesSectionLabel_.setText("REFERENCE",
                                      juce::dontSendNotification);
    referencesSectionLabel_.setFont(MixCoachTheme::sectionHeaderFont());
    referencesSectionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    addAndMakeVisible(referencesSectionLabel_);

    // ─── Panel de Referencias ──────────────────────────────────────────────
    addAndMakeVisible(refPanel_);

    // ─── Messenger List (panel izquierdo) con scroll ───────────────────────
    messengerViewport_.setViewedComponent(&messengerList_, false);
    messengerViewport_.setScrollBarsShown(true, false);
    messengerViewport_.setScrollBarThickness(6);
    messengerViewport_.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId, MixCoachTheme::accent().withAlpha(0.3f));
    messengerViewport_.getVerticalScrollBar().setColour(
        juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(messengerViewport_);

    // ─── Divider bar ──────────────────────────────────────────────────────
    dividerBar_.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    addAndMakeVisible(dividerBar_);

    // ─── Chat Messages (viewport con chatMessages_) ────────────────────────
    chatViewport_.setViewedComponent(&chatMessages_, false);
    chatViewport_.setScrollBarsShown(true, false);
    chatViewport_.setScrollBarThickness(6);
    chatViewport_.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId, MixCoachTheme::accent().withAlpha(0.3f));
    chatViewport_.getVerticalScrollBar().setColour(
        juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(chatViewport_);

    // ─── Chat Input ────────────────────────────────────────────────────────
    chatInput_.setMultiLine(false);
    chatInput_.setFont(juce::Font(juce::FontOptions(12.0f)));
    chatInput_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgInput());
    chatInput_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    chatInput_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border());
    chatInput_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
    chatInput_.setTextToShowWhenEmpty("Type your message...", MixCoachTheme::textMuted());
    chatInput_.setIndents(10, 8);
    chatInput_.addListener(this);
    addAndMakeVisible(chatInput_);

    // ─── Send button ──────────────────────────────────────────────────────
    sendButton_.onClick = [this]() {
        auto text = chatInput_.getText().trim();
        if (text.isNotEmpty() && onMessageSent) {
            onMessageSent(text);
            chatInput_.clear();
        }
    };
    addAndMakeVisible(sendButton_);

    // ─── Ollama status (Phi-3 local) ─────────────────────────────────
    ollamaStatusLabel_.setText("\xF0\x9F\xA4\x96 " "Phi-3: verifying...", juce::dontSendNotification);
    ollamaStatusLabel_.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
    ollamaStatusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(ollamaStatusLabel_);

    ollamaRetryBtn_.setButtonText("\xE2\x9F\xB3");
    ollamaRetryBtn_.setTooltip("Retry Ollama connection");
    ollamaRetryBtn_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::accent().withAlpha(0.15f));
    ollamaRetryBtn_.setColour(juce::TextButton::buttonOnColourId, MixCoachTheme::accent().withAlpha(0.30f));
    ollamaRetryBtn_.setColour(juce::TextButton::textColourOnId, MixCoachTheme::accentGlow());
    ollamaRetryBtn_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::textDim());
    ollamaRetryBtn_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    ollamaRetryBtn_.onClick = [this]() {
        if (onRetryOllama)
            onRetryOllama();
    };
    addAndMakeVisible(ollamaRetryBtn_);

    // ─── Default suggestions ──────────────────────────────────────────────
    suggestions_ = {
        "Analyze my mix",
        "What should I fix?",
        "Give me EQ tips",
        "How to improve balance?"
    };

    // ─── Footer bar ────────────────────────────────────────────────────────
    footerModeLabel_.setText("MODE: MIX", juce::dontSendNotification);
    footerModeLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
    footerModeLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    addAndMakeVisible(footerModeLabel_);

    footerPhaseLabel_.setText("CURRENT PHASE: 2 \xE2\x80\x93 ORGANIZATION", juce::dontSendNotification);
    footerPhaseLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
    footerPhaseLabel_.setColour(juce::Label::textColourId, MixCoachTheme::warning());
    addAndMakeVisible(footerPhaseLabel_);

    footerGenreLabel_.setText("GENRE: POP", juce::dontSendNotification);
    footerGenreLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
    footerGenreLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    addAndMakeVisible(footerGenreLabel_);

    footerTargetLabel_.setText("TARGET: -14 LUFS", juce::dontSendNotification);
    footerTargetLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
    footerTargetLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(footerTargetLabel_);

    footerSampleRateLabel_.setText("SAMPLE RATE: 48 kHz", juce::dontSendNotification);
    footerSampleRateLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
    footerSampleRateLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(footerSampleRateLabel_);

    // ─── Experience Level pill (clickable) ────────────────────────────────
    footerExpLevelLabel_.setText("LEVEL: INTERMEDIATE", juce::dontSendNotification);
    footerExpLevelLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
    footerExpLevelLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    footerExpLevelLabel_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible(footerExpLevelLabel_);
    footerExpLevelLabel_.addMouseListener(this, false);

    // ─── Wire messenger list selection to external callback ──────────────
    messengerList_.onSlotSelected = [this](int slotIndex) {
        if (onTrackSelected)
            onTrackSelected(slotIndex);
        syncSelectionToMixMap(slotIndex);
    };

    // ─── Wire Mix Map selection to external callback ─────────────────────
    mixMapComponent_.onTrackSelected = [this](int slotIndex) {
        if (onTrackSelected)
            onTrackSelected(slotIndex);
        syncSelectionToMessengerList(slotIndex);
    };

    // ─── Wire Confirm Map button → delegar a CoachEngine ═══════════════
    mixMapComponent_.onConfirmMap = [this]() {
        mixMapComponent_.setMapConfirmed(true);

        if (coachEngine_ != nullptr)
            coachEngine_->onMapConfirmed();

        repaint();
    };

    // ─── Wire ReferencePanel callbacks → MixCoachPanel callbacks ───────
    refPanel_.onFileReferenceAdded = [this](const juce::String& path) {
        if (onReferenceFileAdded)
            onReferenceFileAdded(path);
    };
    refPanel_.onURLReferenceAdded = [this](const juce::String& name, const juce::String& url) {
        if (onReferenceURLAdded)
            onReferenceURLAdded(name, url);
    };
    refPanel_.onReferencesChanged = [this]() {
        if (refPanel_.getNumReferences() == 0 && onReferenceCleared)
            onReferenceCleared();
        resized();
    };
    refPanel_.onPlayReference = [this](int refIndex) {
        if (onPlayReference)
            onPlayReference(refIndex);
    };
    refPanel_.onSeekReference = [this](double seconds) {
        if (onSeekReference)
            onSeekReference(seconds);
    };
    refPanel_.onSectionSelected = [this](int sectionIndex) {
        if (onSectionSelected)
            onSectionSelected(sectionIndex);
    };
    refPanel_.onSectionSeekTo = [this](double seconds) {
        if (onSeekReference)
            onSeekReference(seconds);
    };
    refPanel_.onReferenceSelected = [this](int refIndex) {
        if (onReferenceSelected)
            onReferenceSelected(refIndex);
    };

    // ─── Sprint 4: Reference-Driven Mode toggle callback ──────────────────
    refPanel_.onReferenceDrivenModeToggled = [this](bool enabled) {
        if (coachEngine_ != nullptr)
            coachEngine_->setReferenceDrivenMode(enabled);
    };

    // ─── Sprint 4: Reference-Driven Mode context menu ────────────────────
    refPanel_.onRefModeMenuAction = [this](ReferencePanelComponent::RefModeMenuAction action) {
        if (coachEngine_ == nullptr)
            return;

        switch (action) {
            case ReferencePanelComponent::RefModeMenuAction::ConfigureGaps:
                coachEngine_->clearReferenceGapTracking();
                coachEngine_->sendReferenceDrivenAnalysis();
                break;

            case ReferencePanelComponent::RefModeMenuAction::ViewHistory:
            {
                auto& prog = coachEngine_->getReferenceProgress();
                int historyCount = coachEngine_->getReferenceProgressHistoryCount();
                auto& history = coachEngine_->getReferenceProgressHistory();

                if (historyCount == 0) {
                    coachEngine_->respondWith(
                        "📊 Aún no hay historial de matching contra la referencia. "
                        "Activa el Reference-Driven Mode y espera unos segundos "
                        "a que se recopilen datos.",
                        MentorMessage::Type::Info);
                } else {
                    juce::String msg;
                    msg += "📊 **Historial de match contra referencia**\n\n";
                    msg += "Match actual: **" + juce::String(static_cast<int>(prog.currentMatch * 100.0f)) + "%**\n";

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

                    msg += "\nGaps: " + juce::String(prog.totalGaps) + " total, "
                           + juce::String(prog.criticalGaps) + " críticos, "
                           + juce::String(prog.warningGaps) + " warnings";
                    if (prog.resolvedGaps > 0)
                        msg += " | " + juce::String(prog.resolvedGaps) + " resueltos ✓";

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
}

void MixCoachPanel::resized()
{
    auto area = getLocalBounds().reduced(6);

    // ─── Footer: MODE + PHASE + GENRE + TARGET + SAMPLE RATE + EXP LEVEL ───
    auto footerArea = area.removeFromBottom(18);
    {
        footerModeLabel_.setBounds(footerArea.removeFromLeft(70));
        footerArea.removeFromLeft(4);
        footerPhaseLabel_.setBounds(footerArea.removeFromLeft(160));
        footerArea.removeFromLeft(4);
        footerGenreLabel_.setBounds(footerArea.removeFromLeft(110));
        footerArea.removeFromLeft(4);
        footerTargetLabel_.setBounds(footerArea.removeFromLeft(90));
        footerArea.removeFromLeft(4);
        footerSampleRateLabel_.setBounds(footerArea.removeFromLeft(110));
        footerArea.removeFromLeft(4);
        footerExpLevelLabel_.setBounds(footerArea.removeFromLeft(130));
    }
    area.removeFromBottom(4);

    // Referencia Tab1: izquierda 38% chat+refs | derecha 62% pistas
    constexpr float kLeftColumnRatio = 0.38f;
    int splitX = (int)(area.getWidth() * kLeftColumnRatio);
    auto leftArea = area.removeFromLeft(splitX);
    auto rightArea = area.reduced(4, 0);

    dividerBar_.setBounds(leftArea.getRight() + 1, leftArea.getY(), 4, leftArea.getHeight());

    // ═══ Columna izquierda: 3 secciones iguales (Master Meter | Chat | Refs) ═══
    constexpr int kSectionGap = 3;
    int totalAvailH = leftArea.getHeight();
    int sectionH = (totalAvailH - kSectionGap * 2) / 3;

    // ─── SECTION 1: Master Meter ───────────────────────────────────────────
    masterMeterPanel_.setBounds(leftArea.removeFromTop(sectionH));
    leftArea.removeFromTop(kSectionGap);

    // ─── SECTION 3: References (from bottom) ──────────────────────────────
    int refLabelH = 16;
    int apiKeyRowH = 22;      // API key row above references
    int refGaps   = 1 + kSectionGap + apiKeyRowH;
    int refPanelH = sectionH - refLabelH - refGaps;
    refPanelH = juce::jmax(refPanel_.getNumReferences() > 0 ? 80 : 48, refPanelH);

    refPanel_.setBounds(leftArea.removeFromBottom(refPanelH));
    leftArea.removeFromBottom(1);
    referencesSectionLabel_.setBounds(leftArea.removeFromBottom(refLabelH));

    // ═══ Ollama status row: [🤖 Phi-3: conectado/desconectado] [↳] ────
    {
        auto ollamaArea = leftArea.removeFromBottom(apiKeyRowH);
        ollamaStatusLabel_.setBounds(ollamaArea.reduced(4, 0).removeFromLeft(ollamaArea.getWidth() - 24));
        ollamaRetryBtn_.setBounds(ollamaArea.getRight() - 22, ollamaArea.getY() + 2, 18, 18);
    }

    leftArea.removeFromBottom(kSectionGap);

    // ─── SECTION 2: Chat (remaining = middle section) ────────────────────
    // Input bar + suggestion chips at bottom of section
    bool showSuggestions = !suggestions_.empty() && chatMessages_.isEmpty();
    suggestionChipBounds_.clear();

    if (showSuggestions) {
        // Chips row + input compact
        auto chipRow = leftArea.removeFromBottom(44);
        chatInput_.setBounds(chipRow.removeFromBottom(34).withTrimmedRight(38));
        sendButton_.setBounds(chipRow.getRight() - 32, chipRow.getY() + 2, 30, 30);
        leftArea.removeFromBottom(4);

        auto chipArea = leftArea.removeFromBottom(24).reduced(4, 0);
        int chipGap = 4;
        int cx = chipArea.getX();
        for (auto& sug : suggestions_) {
            float textW = juce::GlyphArrangement::getStringWidthInt(
                juce::Font(juce::FontOptions(8.0f)).boldened(), sug) + 16.0f;
            int chipW = juce::jmin((int)textW + 4, chipArea.getWidth() / 3);
            chipW = juce::jmax(chipW, 60);
            if (cx + chipW > chipArea.getRight()) break;
            suggestionChipBounds_.push_back({ cx, chipArea.getY(), chipW, chipArea.getHeight() });
            cx += chipW + chipGap;
        }
        leftArea.removeFromBottom(2);
    } else {
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

    // ═══ Columna derecha: SESIÓN 3 pistas + toolbar ════════════════════════
    tracksSectionLabel_.setBounds(rightArea.removeFromTop(20));
    rightArea.removeFromTop(2);

    auto toolbarArea = rightArea.removeFromTop(18).reduced(0, 1);
    {
        int tx = toolbarArea.getX() + 56;
        int ty = toolbarArea.getY();
        int th = toolbarArea.getHeight();

        chipTipoBounds_ = { tx, ty, 32, th };
        tx += 34;
        chipColorBounds_ = { tx, ty, 38, th };
        tx += 40;
        chipBusBounds_ = { tx, ty, 28, th };
        tx += 30;

        // Mixed Map toggle chip (V4)
        chipMapBounds_ = { tx, ty, 28, th };
        tx += 30;

        // Sprint 1: CONFIRMAR roles chip (placed right after MAP)
        chipConfirmRolesBounds_ = { tx, ty, 64, th };

        int rightX = toolbarArea.getRight();
        expandAllBounds_ = { rightX - 52, ty, 50, th };
        collapseAllBounds_ = { expandAllBounds_.getX() - 62, ty, 58, th };
    }
    rightArea.removeFromTop(2);

    messengerViewport_.setBounds(rightArea);
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

    // ─── Glass gradient overlay sutil ─────────────────────────────────────
    juce::ColourGradient bgGrad(
        MixCoachTheme::glassHighlight().withAlpha(0.5f),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::bgDarker(),
        juce::Point<float>(0.0f, (float)area.getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(area);

    // ─── Panel backgrounds (izq chat | der pistas) ─── glassmorphism premium ──
    auto inner = area.reduced(6);
    int splitX = (int)(inner.getWidth() * 0.38f);

    auto leftBounds = inner.removeFromLeft(splitX).toFloat();
    MixCoachTheme::fillGlassPanel(g, leftBounds, 6.0f);

    auto rightBounds = inner.reduced(4, 0).toFloat();
    MixCoachTheme::fillGlassPanel(g, rightBounds, 6.0f);

    int dividerX = (int)leftBounds.getRight() + 1;
    g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
    g.drawVerticalLine(dividerX, leftBounds.getY() + 4, leftBounds.getBottom() - 4);

    // ─── Premium header underlines ─── gradient glow below each section ─────
    auto drawHeaderUnderline = [&](const juce::Label& label) {
        auto b = label.getBounds();
        float uy = (float)b.getBottom() - 1.0f;
        float uw = (float)b.getWidth();
        // Línea divisoria oscura
        g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
        g.drawHorizontalLine((int)uy + 1, (float)b.getX(), (float)b.getRight());
        // Glow violeta en el centro (100px)
        float cx = (float)b.getX() + uw * 0.5f;
        float glowW = juce::jmin(100.0f, uw * 0.6f);
        auto glowRect = juce::Rectangle<float>(cx - glowW * 0.5f, uy - 1.0f, glowW, 3.0f);
        juce::ColourGradient glow(
            MixCoachTheme::accent().withAlpha(0.25f), cx, uy,
            MixCoachTheme::accent().withAlpha(0.0f),  cx + glowW * 0.5f, uy, false);
        glow.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.10f));
        g.setGradientFill(glow);
        g.fillRect(glowRect);
    };
    drawHeaderUnderline(tracksSectionLabel_);
    drawHeaderUnderline(referencesSectionLabel_);

    // ═══ SUGGESTION CHIPS — columna izquierda (chat vacío) ═══════════════
    for (size_t i = 0; i < suggestionChipBounds_.size(); ++i) {
        auto chipRectF = suggestionChipBounds_[i].toFloat();
        bool isHovered = ((int)i == hoveredSuggestionChip_);
        const float chipCr = 5.0f;

        // ─── Hover glow exterior ───────────────────────────────────────
        if (isHovered) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
            g.fillRoundedRectangle(chipRectF.expanded(3.0f, 3.0f), chipCr + 2.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.05f));
            g.fillRoundedRectangle(chipRectF.expanded(6.0f, 6.0f), chipCr + 4.0f);
        }

        // ─── Shadow sutil ─────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(isHovered ? 0.20f : 0.10f));
        g.fillRoundedRectangle(chipRectF.translated(0.0f, isHovered ? 1.5f : 1.0f), chipCr);

        // ─── Fondo glass con gradiente ────────────────────────────────
        juce::ColourGradient chipGrad(
            MixCoachTheme::accent().withAlpha(isHovered ? 0.18f : 0.08f),
            chipRectF.getX(), chipRectF.getY(),
            MixCoachTheme::accent().withAlpha(isHovered ? 0.08f : 0.03f),
            chipRectF.getX(), chipRectF.getBottom(), false);
        if (isHovered)
            chipGrad.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.14f));
        g.setGradientFill(chipGrad);
        g.fillRoundedRectangle(chipRectF, chipCr);

        // ─── Glass highlight ──────────────────────────────────────────
        if (isHovered) {
            auto chipGlass = chipRectF.withHeight(chipRectF.getHeight() * 0.4f);
            juce::ColourGradient chipGlassGrad(
                juce::Colours::white.withAlpha(0.08f), chipGlass.getX(), chipGlass.getY(),
                juce::Colour(0x00000000),              chipGlass.getX(), chipGlass.getBottom(), false);
            g.setGradientFill(chipGlassGrad);
            g.fillRoundedRectangle(chipGlass, chipCr);
        }

        // ─── Borde ────────────────────────────────────────────────────
        float borderAlpha = isHovered ? 0.45f : 0.20f;
        g.setColour(MixCoachTheme::accent().withAlpha(borderAlpha));
        g.drawRoundedRectangle(chipRectF, chipCr, isHovered ? 0.7f : 0.5f);

        // ─── Texto ────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.setColour(isHovered ? MixCoachTheme::accentGlow() : MixCoachTheme::accentGlow().withAlpha(0.75f));
        g.drawText(suggestions_[i], chipRectF, juce::Justification::centred);
    }

    // ═══ SYSTEM MESSAGES TOGGLE — bar between viewport and input ═════════
    {
        systemToggleBounds_ = {
            chatViewport_.getX(),
            chatViewport_.getBottom(),
            chatViewport_.getWidth(),
            chatInput_.getY() - chatViewport_.getBottom()
        };

        int numSystem = chatMessages_.getNumSystemMessages();
        if (numSystem > 0 && systemToggleBounds_.getHeight() > 4)
        {
            auto tb = systemToggleBounds_.toFloat();
            bool isHovered = (hoveredSystemToggle_ >= 0);
            const float cr = 4.0f;

            // Fondo dimmed
            g.setColour(MixCoachTheme::bgInput().withAlpha(0.3f));
            g.fillRoundedRectangle(tb.reduced(2, 1), cr);

            if (isHovered) {
                g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
                g.fillRoundedRectangle(tb.reduced(2, 1), cr);
            }

            // Texto
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));

            if (showSystemMessages_) {
                // Visible → "🛠 Ocultar sistema"
                g.setColour(MixCoachTheme::accentGlow().withAlpha(0.7f));
                g.drawText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9B\xA0 Hide system events")),
                           tb.toNearestInt(), juce::Justification::centred);
            } else {
                // Oculto → "🛠 X eventos ocultos • Mostrar"
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
                juce::String label = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9B\xA0 "))
                    + juce::String(numSystem)
                    + (numSystem == 1 ? " hidden event \xE2\x80\xA2 Show"
                                      : " hidden events \xE2\x80\xA2 Show");
                g.drawText(label, tb.toNearestInt(), juce::Justification::centred);
            }
        }
        else
        {
            systemToggleBounds_.setBounds(0, 0, 0, 0);
        }
    }

    // ─── Input focus glow (drawn in paint when chatInput_ is focused) ────
    if (chatInput_.hasKeyboardFocus(true)) {
        auto inputBounds = chatInput_.getBounds().toFloat();
        g.setColour(MixCoachTheme::accent().withAlpha(0.04f));
        g.fillRoundedRectangle(inputBounds.expanded(4.0f, 4.0f), 6.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
        g.fillRoundedRectangle(inputBounds.expanded(2.0f, 2.0f), 6.0f);
    }

    // ═══ FILTER TOOLBAR — columna derecha (pistas) ═════════════════════════
    auto innerArea = area.reduced(6);
    innerArea.removeFromLeft(splitX + 4);
    int toolbarY = innerArea.getY() + 22;

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
    g.drawText("GROUP BY:",
               innerArea.getX() + 2, toolbarY + 1, 56, 16,
               juce::Justification::centredLeft);

    // Helper lambda to draw a chip
    auto drawChip = [&](juce::Rectangle<int> chipRect, bool isActive, const juce::String& text) {
        if (isActive) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
            g.fillRoundedRectangle(chipRect.toFloat(), 3.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.4f));
            g.drawRoundedRectangle(chipRect.toFloat(), 3.0f, 0.5f);
            g.setColour(MixCoachTheme::accentGlow());
        } else {
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
        auto chipRect = chipMapBounds_.toFloat();
        if (mapActive) {
            g.setColour(MixCoachTheme::accentCyan().withAlpha(0.15f));
            g.fillRoundedRectangle(chipRect, 3.0f);
            g.setColour(MixCoachTheme::accentCyan().withAlpha(0.4f));
            g.drawRoundedRectangle(chipRect, 3.0f, 0.5f);
            g.setColour(MixCoachTheme::accentCyan());
        } else {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        }
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
        g.drawText("MAP", chipMapBounds_, juce::Justification::centred);
    }

    // Sprint 1: CONFIRMAR roles chip — success colour when pending roles exist,
    // muted when nothing to confirm. Shows pending count as a hint.
    {
        int pendingCount = 0;
        if (coachEngine_ != nullptr)
            pendingCount = coachEngine_->getIdentityProgress().pendingInferred;
        bool hasPending = (pendingCount > 0);
        auto chipRect = chipConfirmRolesBounds_.toFloat();
        if (hasPending) {
            g.setColour(MixCoachTheme::success().withAlpha(0.15f));
            g.fillRoundedRectangle(chipRect, 3.0f);
            g.setColour(MixCoachTheme::success().withAlpha(0.4f));
            g.drawRoundedRectangle(chipRect, 3.0f, 0.5f);
            g.setColour(MixCoachTheme::success());
        } else {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        }
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
        juce::String label = hasPending
            ? "OK " + juce::String(pendingCount)   // abbreviated: "OK 3"
            : "OK \xE2\x9C\x93";                   // "OK ✓" (nothing pending)
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
    // Solo actualizar el MixMap si está visible (evita CPU innecesario)
    if (showMixMap_ && trackRoles_) {
        mixMapComponent_.updateData(registry, sharedData, *trackRoles_);
    }
    int newHeight = messengerList_.getPreferredHeight();
    if (oldHeight != newHeight)
        resized();
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
}

void MixCoachPanel::setShowSystemMessages(bool show)
{
    showSystemMessages_ = show;
    chatMessages_.setShowSystemMessages(show);
}

// ═══════════════════════════════════════════════════════════════════════════
//  updateCoachAdvice — Actualiza consejos del coach + roles + issues para Mix Map
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachPanel::updateCoachAdvice(CoachEngine& coach)
{
    coachEngine_ = &coach;
    messengerList_.updateCoachAdvice(coach);
    trackRoles_ = &coach.getTrackRoles();

    // ─── Issue badges for Mix Map ──────────────────────────────────────
    // collectAllIssues() es O(n) por track, se llama desde el timer del
    // background thread (~8s) donde ya se ejecuta periodicAnalysis(),
    // por lo que no añade overhead extra en el UI thread.
    std::array<MixMapComponent::TrackIssueBadge, SlotRegistry::kMaxSlots> badges{};
    {
        auto issues = coach.collectAllIssues();

        // Indexar issues por slotIndex
        for (const auto& issue : issues)
        {
            int idx = issue.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots)
                continue;

            auto& badge = badges[idx];
            badge.hasIssues = true;
            badge.issueCount++;

            if (issue.isOptimal)
            {
                badge.isOptimal = true;
                badge.hasIssues = false;  // Optimal = clean, no issues to show
                continue;
            }

            // Severidad máxima por slot
            if (issue.severity > badge.maxSeverity)
                badge.maxSeverity = issue.severity;

            if (issue.isCritical)
                badge.isCritical = true;

            // Short issue type (primer issue encontrado)
            if (badge.shortType.isEmpty())
            {
                // Mapear domain e issueType a abreviaciones legibles
                if (issue.domain == "gain")
                    badge.shortType = "GAIN";
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

    // ─── Grouping chips ────────────────────────────────────────────────
    if (chipTipoBounds_.contains(pos)) {
        activeGroupingChip_ = 0;
        showMixMap_ = false;
        messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Type);
        messengerViewport_.setViewedComponent(&messengerList_, false);
        resized();
        repaint();
        return;
    }
    if (chipColorBounds_.contains(pos)) {
        activeGroupingChip_ = 1;
        showMixMap_ = false;
        messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Colour);
        messengerViewport_.setViewedComponent(&messengerList_, false);
        resized();
        repaint();
        return;
    }
    if (chipBusBounds_.contains(pos)) {
        activeGroupingChip_ = 2;
        showMixMap_ = false;
        messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Bus);
        messengerViewport_.setViewedComponent(&messengerList_, false);
        resized();
        repaint();
        return;
    }

    // ─── Mix Map toggle (V4) ──────────────────────────────────────────
    if (chipMapBounds_.contains(pos)) {
        showMixMap_ = !showMixMap_;
        activeGroupingChip_ = showMixMap_ ? 3 : 2; // 3=MAP, 2=BUS (default)
        if (showMixMap_) {
            messengerViewport_.setViewedComponent(&mixMapComponent_, false);
        } else {
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
    if (systemToggleBounds_.contains(pos)
        && systemToggleBounds_.getWidth() > 0
        && chatMessages_.getNumSystemMessages() > 0)
    {
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
    if (footerExpLevelLabel_.getBounds().contains(pos))
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Novice", true, currentExpLevel_ == 0);
        menu.addItem(2, "Intermediate", true, currentExpLevel_ == 1);
        menu.addItem(3, "Advanced", true, currentExpLevel_ == 2);
        menu.addItem(4, "Expert", true, currentExpLevel_ == 3);

        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetComponent(&footerExpLevelLabel_),
            [this](int result)
            {
                if (result >= 1 && result <= 4)
                {
                    int newLevel = result - 1;
                    currentExpLevel_ = newLevel;

                    static const char* levelNames[] = {
                        "NOVICE", "INTERMEDIATE", "ADVANCED", "EXPERT"
                    };
                    footerExpLevelLabel_.setText(
                        "LEVEL: " + juce::String(levelNames[newLevel]),
                        juce::dontSendNotification);
                    repaint();

                    if (onExperienceLevelChanged)
                        onExperienceLevelChanged(newLevel);
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
    auto pos = e.getPosition();
    int oldHover = hoveredSuggestionChip_;
    int oldSystemHover = hoveredSystemToggle_;
    hoveredSuggestionChip_ = -1;
    hoveredSystemToggle_ = -1;

    // ─── Check system toggle hover ────────────────────────────────────
    if (systemToggleBounds_.contains(pos)
        && systemToggleBounds_.getWidth() > 0
        && chatMessages_.getNumSystemMessages() > 0)
    {
        hoveredSystemToggle_ = 1;
    }

    // ─── Check suggestion chips ────────────────────────────────────────
    for (size_t i = 0; i < suggestionChipBounds_.size(); ++i) {
        if (suggestionChipBounds_[i].contains(pos)) {
            hoveredSuggestionChip_ = (int)i;
            break;
        }
    }

    if (hoveredSuggestionChip_ != oldHover || hoveredSystemToggle_ != oldSystemHover)
        repaint();
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
    static const char* levelNames[] = {
        "NOVICE", "INTERMEDIATE", "ADVANCED", "EXPERT"
    };
    int clampedLevel = juce::jlimit(0, 3, expLevel);

    footerPhaseLabel_.setText("CURRENT PHASE: " + phaseName, juce::dontSendNotification);
    footerGenreLabel_.setText("GENRE: " + genre, juce::dontSendNotification);
    footerTargetLabel_.setText("TARGET: " + target, juce::dontSendNotification);
    footerSampleRateLabel_.setText("SAMPLE RATE: " + sampleRate, juce::dontSendNotification);
    footerExpLevelLabel_.setText("LEVEL: " + juce::String(levelNames[clampedLevel]), juce::dontSendNotification);
}

// ═══════════════════════════════════════════════════════════════════════════
//  setExperienceLevel — Actualiza la UI del nivel de experiencia
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachPanel::setOllamaStatus(bool connected, const juce::String& modelName)
{
    if (connected)
    {
        ollamaStatusLabel_.setText(
            "\xF0\x9F\xA4\x96 " + modelName + ": \xE2\x9C\x93 connected",
            juce::dontSendNotification);
        ollamaStatusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::info());
        ollamaRetryBtn_.setVisible(false);
    }
    else
    {
        ollamaStatusLabel_.setText(
            "\xF0\x9F\xA4\x96 " + modelName + ": \xE2\x9C\x97 disconnected",
            juce::dontSendNotification);
        ollamaStatusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::warning().withAlpha(0.7f));
        ollamaRetryBtn_.setVisible(true);
    }
    repaint();
}

void MixCoachPanel::setCoachMode(bool isMasterMode)
{

if (isMasterMode)
    {
        footerModeLabel_.setText("MODE: MASTER", juce::dontSendNotification);
        footerModeLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentCyan());
    }
    else
    {
        footerModeLabel_.setText("MODE: MIX", juce::dontSendNotification);
        footerModeLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    }
    repaint();
}

void MixCoachPanel::setExperienceLevel(int levelIndex)
{
    currentExpLevel_ = juce::jlimit(0, 3, levelIndex);
    static const char* levelNames[] = {
        "NOVICE", "INTERMEDIATE", "ADVANCED", "EXPERT"
    };
    footerExpLevelLabel_.setText(
        "LEVEL: " + juce::String(levelNames[currentExpLevel_]),
        juce::dontSendNotification);
    repaint();
}

void MixCoachPanel::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &chatInput_) {
        auto text = chatInput_.getText().trim();
        if (text.isNotEmpty() && onMessageSent) {
            onMessageSent(text);
            chatInput_.clear();
        }
    }
    // Ollama local no requiere input de API key
}

} // namespace mixcoach
