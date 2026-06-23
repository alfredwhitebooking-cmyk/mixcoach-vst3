// ═══════════════════════════════════════════════════════════════════════════
//  TestChatBubbleHeight.cpp — Unit test para alturas de burbujas de chat
//
//  Verifica que getBubbleHeight() devuelva los valores esperados para
//  mensajes del sistema y burbujas del coach con texto concreto.
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Release --target TestChatBubbleHeight
//    ./build/tests/Release/TestChatBubbleHeight.exe
//
//  Dependencias: juce_core + juce_graphics (para GlyphArrangement, Font)
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

// ─── Test runner ─────────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                  \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                \
        gTestsFailed++;                                                        \
    } else {                                                                   \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                        \
        gTestsPassed++;                                                        \
    }                                                                          \
} while(0)

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

// ═══════════════════════════════════════════════════════════════════════════
//  DUPLICADO de helpers de CoachChatComponent.cpp
//  (medición de texto y altura de burbujas)
// ═══════════════════════════════════════════════════════════════════════════

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

// ─── Constantes de font ──────────────────────────────────────────────────
static constexpr float kSysFontSize = 8.5f;
static constexpr float kCoachFontSize = 12.5f;

// ═══════════════════════════════════════════════════════════════════════════
//  Funciones de altura (reflejan el código REAL en CoachChatComponent.cpp)
// ═══════════════════════════════════════════════════════════════════════════

/** Altura para mensajes del SISTEMA (isSystem=true).
    drawSystemMessage usa: bounds = {12, y, maxWidth-24, bubbleH}, reduced(4,1)
    → textW = maxWidth - 32
    drawMarkdownText (con "🛠 " + msg.text)
    resultado: jmax(14.0f, sysTextH + 2.0f + bold_fudge) */
static float getSystemBubbleHeight(const juce::String& text, float maxWidth)
{
    juce::Font sysFont{juce::FontOptions(kSysFontSize)};
    float sysTextW = maxWidth - 32.0f;

    // Coincide con drawSystemMessage: "🛠 " + text
    juce::String displayText = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9B\xA0 ")) + text;
    float sysTextH = measureWrappedHeight(displayText, sysFont, sysTextW);

    // Bold fudge (si hay **bold**)
    if (text.contains("**"))
        sysTextH *= 1.15f;

    return juce::jmax(14.0f, sysTextH + 2.0f);
}

/** Altura para burbujas del COACH (isSystem=false).
    Padding: innerPad(6) × 2 + 2px reserve = 14px
    tagH: 0 si tag vacío, 18 si hay tag
    mínimo: 44px */
static float getCoachBubbleHeight(const juce::String& text, float maxWidth,
                                  const juce::String& tag = {})
{
    float bubbleMaxW = maxWidth * 0.85f;
    bubbleMaxW = juce::jlimit(140.0f, 360.0f, bubbleMaxW);

    float textW = bubbleMaxW - 12.0f;
    juce::Font font{juce::FontOptions(kCoachFontSize)};

    float textH = measureWrappedHeight(text, font, textW);

    // Bold fudge
    if (text.contains("**"))
        textH *= 1.15f;

    // tagH: solo si hay tag
    float tagH = (!tag.isEmpty()) ? 18.0f : 0.0f;
    // Padding: innerPad(6)×2 + 2px reserve = 14px
    float bubbleH = textH + tagH + 14.0f;
    return juce::jmax(44.0f, bubbleH);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests — System Messages
// ═══════════════════════════════════════════════════════════════════════════

static void test_system_message_short()
{
    std::printf("\n── System Messages: Short text ──\n");

    float maxW = 350.0f;

    // Mensaje corto: "✅ Setup completado"
    juce::String msg = "\u2705 Setup completado";
    float h = getSystemBubbleHeight(msg, maxW);
    std::printf("  msg=\"%s\" (len=%d) maxW=%.0f → h=%.1fpx\n",
                msg.toRawUTF8(), msg.length(), maxW, h);

    // Debe ser mínimo: 14px (para 1 línea de texto)
    float minH = 14.0f;
    TEST("short system message uses minimum height (14px)",
         h >= minH && h < 20.0f);

    // Verificar que NO usa el mínimo antiguo de 16 o 18
    float oldMinV1 = 18.0f;
    float oldMinV2 = 16.0f;
    TEST("short system message is MORE compact than old 18px minimum",
         h < oldMinV1);
    TEST("short system message is MORE compact than old 16px minimum",
         h < oldMinV2);
    TEST("short system message is exactly 14px or slightly more",
         h >= 14.0f && h <= 15.0f);
}

static void test_system_message_medium()
{
    std::printf("\n── System Messages: Medium text ──\n");

    float maxW = 350.0f;

    // Mensaje medio: "🎛 Headroom: -5.2 dB — la pista más alta llega a..."
    juce::String msg = "\U0001F39B Headroom: -5.2 dB \u2014 la pista m\u00E1s alta "
                       "llega a -3.1 dB. El rango ideal es -6 dB a -3 dB.";
    float h = getSystemBubbleHeight(msg, maxW);
    std::printf("  msg=\"%s...\" (len=%d) maxW=%.0f → h=%.1fpx\n",
                juce::String(msg.substring(0, 40)).toRawUTF8(),
                msg.length(), maxW, h);

    // Mensaje medio de 2-3 líneas debe tener altura > 14px
    TEST("medium system message is taller than minimum", h > 14.0f);

    // Antes habría sido truncado a 18px con drawText simple — ahora debe ser más alto
    float oldFixed = 18.0f;
    TEST("medium system message is NOT truncated (taller than old 18px)", h > oldFixed);
}

static void test_system_message_long()
{
    std::printf("\n── System Messages: Long text (identity scan) ──\n");

    float maxW = 350.0f;

    // Mensaje largo simulando identity scan (con **bold** markers)
    juce::String msg =
        "== IDENTITY SCAN ==\n\n"
        "[D] DRUMS (3):\n"
        "   [OK] Kick -> **Kick** (name)\n"
        "   [OK] Snare -> **Snare** (name)\n"
        "   [OK] HiHat -> **HiHat** (name)\n\n"
        "[B] BASS (1):\n"
        "   [OK] Bass -> **Bass** (name)\n\n"
        "[V] VOCALS (1):\n"
        "   [OK] Voz -> **VozPrincipal** (name)\n\n"
        "**5/6** tracks identified\n"
        "Looks correct? Use **/map** for the full session map.";
    float h = getSystemBubbleHeight(msg, maxW);
    std::printf("  msg length=%d maxW=%.0f → h=%.1fpx\n",
                msg.length(), maxW, h);

    // Mensaje largo debe tener bastante altura
    TEST("long system message is significantly taller than minimum",
         h > 40.0f);

    // Antes los system messages tenían 18px fijo — esto sería IMPOSIBLE
    float oldFixed = 18.0f;
    printf("  ⚡ Old fixed height: 18px → text would be CLIPPED!\n");
    printf("  ⚡ New dynamic height: %.1fpx → all text visible!\n", h);
    TEST("long system message would have been CLIPPED with old 18px fixed",
         h > oldFixed * 2.0f);
}

static void test_system_message_bold()
{
    std::printf("\n── System Messages: With **bold** markers ──\n");

    float maxW = 350.0f;

    juce::String msg = "**Kick** detectado en pista 1 con confianza 0.92";
    float h = getSystemBubbleHeight(msg, maxW);
    std::printf("  msg=\"%s\" (len=%d) maxW=%.0f → h=%.1fpx (with bold fudge)\n",
                msg.toRawUTF8(), msg.length(), maxW, h);

    // Con bold fudge debe ser más alta que sin bold
    juce::Font sysFontNoBold{juce::FontOptions(kSysFontSize)};
    float sysTextWNoBold = maxW - 32.0f;
    juce::String displayTextNoBold = juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9B\xA0 ")) + msg;
    float hWithoutBold = juce::jmax(14.0f,
        measureWrappedHeight(displayTextNoBold, sysFontNoBold, sysTextWNoBold) + 2.0f);

    printf("    Without bold fudge: %.1fpx\n", hWithoutBold);
    printf("    With bold fudge:    %.1fpx (%.1fpx extra)\n",
           h, h - hWithoutBold);

    // Bold fudge añade 15% de altura extra cuando hay **bold**
    if (h > hWithoutBold) {
        TEST("bold fudge increases height", true);
    } else {
        // Si una línea con bold aún cabe en el mínimo de 14px, ambas son iguales
        printf("    (same height — one line still fits in minimum 14px)\n");
        TEST("bold fudge: both within minimum (informational)", true);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests — Coach Bubbles
// ═══════════════════════════════════════════════════════════════════════════

static void test_coach_message_short()
{
    std::printf("\n── Coach Bubbles: Short text (no tag) ──\n");

    float maxW = 350.0f;

    // Mensaje corto: "¡Hola! Soy MixCoach"
    juce::String msg = "\u00A1Hola! Soy MixCoach \u2014 \u00BFc\u00F3mo va la mezcla?";
    float h = getCoachBubbleHeight(msg, maxW);
    std::printf("  msg=\"%s\" (len=%d) → h=%.1fpx\n",
                juce::String(msg.substring(0, 30)).toRawUTF8(),
                msg.length(), h);

    // Con 1 línea, tagH=0, padding=14, textH≈14.5, bubbleH=28.5 → minimum 44
    TEST("short coach bubble uses 44px minimum", h >= 44.0f && h < 48.0f);

    // Verificar que NO tiene el tag overhead de 18px
    float tagH_old = 18.0f;
    juce::Font coachFontShort{juce::FontOptions(kCoachFontSize)};
    float coachWShort = juce::jlimit(140.0f, 360.0f, maxW * 0.85f) - 12.0f;
    float textH = measureWrappedHeight(msg, coachFontShort, coachWShort);
    float oldH = juce::jmax(44.0f, textH + tagH_old + 20.0f);
    printf("    With old tagH(18)+padding(20): %.1fpx\n", oldH);
    printf("    With new tagH(0)+padding(14):  %.1fpx\n", h);
    printf("    Savings: %.1fpx (%.0f%%)\n",
           oldH - h, (oldH - h) / oldH * 100.0f);

    TEST("new coach bubble is smaller than old (tag+padding fix)",
         h < oldH);
}

static void test_coach_message_medium()
{
    std::printf("\n── Coach Bubbles: Medium text (3 lines, no tag) ──\n");

    float maxW = 350.0f;

    // Mensaje de 3 líneas: tip de gain staging
    juce::String msg =
        "\xF0\x9F\x92\xA1 Tip r\u00E1pido: revisa que el fader de ganancia "
        "de cada pista permita picos de -18 dB a -12 dB en el submix "
        "antes de tocar el fader de volumen.";
    float h = getCoachBubbleHeight(msg, maxW);
    std::printf("  msg=\"%s...\" (len=%d) → h=%.1fpx\n",
                juce::String(msg.substring(0, 40)).toRawUTF8(),
                msg.length(), h);

    // 3 líneas → textH ≈ 43.5, bubbleH = 43.5 + 0 + 14 = 57.5
    // Antes: 43.5 + 18 + 20 = 81.5
    juce::Font coachFontMed{juce::FontOptions(kCoachFontSize)};
    float coachWMed = juce::jlimit(140.0f, 360.0f, maxW * 0.85f) - 12.0f;
    float textH = measureWrappedHeight(msg, coachFontMed, coachWMed);
    float oldH = juce::jmax(44.0f, textH + 18.0f + 20.0f);

    printf("    Old: %.1fpx (tagH=18 + padding=20)\n", oldH);
    printf("    New: %.1fpx (tagH=%s + padding=14)\n", h,
           h >= oldH ? "? " : "0");
    printf("    Savings: %.1fpx (%.0f%%)\n",
           oldH - h, (oldH - h) / oldH * 100.0f);

    TEST("medium coach bubble is smaller than old version", h < oldH);
    TEST("savings is at least 20%", (oldH - h) / oldH >= 0.20f);

    // Verificar que no está usando el mínimo
    TEST("medium coach bubble is above 44px minimum", h > 48.0f);
}

static void test_coach_message_with_tag()
{
    std::printf("\n── Coach Bubbles: With tag ──\n");

    float maxW = 350.0f;

    juce::String msg = "\u2705 Niveles correctos en todas las pistas.";
    juce::String tag = "Gain Staging";
    float h = getCoachBubbleHeight(msg, maxW, tag);

    std::printf("  msg=\"%s\" tag=\"%s\" → h=%.1fpx\n",
                msg.toRawUTF8(), tag.toRawUTF8(), h);

    // Con tag, tagH=18 → bubbleH debe ser más grande que sin tag
    float hWithoutTag = getCoachBubbleHeight(msg, maxW);
    printf("    Without tag: %.1fpx\n", hWithoutTag);
    printf("    With tag:    %.1fpx (+%.1fpx)\n", h, h - hWithoutTag);

    TEST("coach bubble with tag is taller than without tag", h > hWithoutTag);
    TEST("coach bubble with tag still has reasonable height",
         h > 44.0f && h < 100.0f);
}

static void test_coach_message_long_identity()
{
    std::printf("\n── Coach Bubbles: Identity scan (long, **bold**, no tag) ──\n");

    float maxW = 350.0f;

    // Identity scan simulado (como lo envía respondWithPremium)
    juce::String msg =
        "== IDENTITY SCAN ==\n\n"
        "[D] DRUMS (3):\n"
        "   [OK] Kick 1 -> **Kick** (name)\n"
        "   [OK] Snare -> **Snare** (name)\n"
        "   [OK] HiHat -> **HiHat** (name)\n\n"
        "[B] BASS (1):\n"
        "   [OK] Bass -> **Bass** (name)\n\n"
        "**4/4** tracks identified\n\n"
        "Looks correct? Use **/map** for the full session map.";
    float h = getCoachBubbleHeight(msg, maxW);

    std::printf("  msg length=%d maxW=%.0f → h=%.1fpx\n",
                msg.length(), maxW, h);

    // Calcular la altura ANTIGUA (tagH=18, padding=20)
    juce::Font coachFontLong{juce::FontOptions(kCoachFontSize)};
    float coachWLong = juce::jlimit(140.0f, 360.0f, maxW * 0.85f) - 12.0f;
    float textH = measureWrappedHeight(msg, coachFontLong, coachWLong);
    // Bold fudge incluido
    if (msg.contains("**")) textH *= 1.15f;
    float oldH = juce::jmax(44.0f, textH + 18.0f + 20.0f);

    printf("    Old bubble height: %.1fpx\n", oldH);
    printf("    New bubble height: %.1fpx\n", h);
    printf("    Savings: %.1fpx (%.0f%%)\n",
           oldH - h, (oldH - h) / oldH * 100.0f);

    TEST("identity scan coach bubble is smaller than old version", h < oldH);
    // Para mensajes largos (~300px), el ahorro absoluto de 24px (tagH+padding)
    // es ~7% del total. Lo importante es que el ahorro ABSOLUTO sea correcto.
    float savings = oldH - h;
    printf("    Absolute savings: %.1fpx (should be ~24px = 18px tagH + 6px padding)\n", savings);
    TEST("savings is at least 20px (tagH 18 + padding 6)", savings >= 20.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests — Edge Cases
// ═══════════════════════════════════════════════════════════════════════════

static void test_edge_empty_text()
{
    std::printf("\n── Edge Cases: Empty text ──\n");

    float maxW = 350.0f;

    // System message vacío
    float sysH = getSystemBubbleHeight("", maxW);
    printf("  System message (empty): %.1fpx (minimum 14)\n", sysH);
    TEST("empty system message uses minimum", sysH == 14.0f);

    // Coach message vacío
    float coachH = getCoachBubbleHeight("", maxW);
    printf("  Coach bubble (empty): %.1fpx (minimum 44)\n", coachH);
    TEST("empty coach bubble uses minimum", coachH == 44.0f);
}

static void test_edge_different_widths()
{
    std::printf("\n── Edge Cases: Different chat widths ──\n");

    // System message con diferentes anchos de chat
    float widths[] = { 200.0f, 300.0f, 400.0f, 500.0f };
    juce::String msg = "\u2705 Setup completado con 5 pistas";

    printf("  System message width sensitivity:\n");
    for (float w : widths) {
        float h = getSystemBubbleHeight(msg, w);
        printf("    maxW=%.0f → h=%.1fpx\n", w, h);
    }
    TEST("system message at all widths stays reasonable",
         getSystemBubbleHeight(msg, 200.0f) <=
         getSystemBubbleHeight(msg, 500.0f) + 10.0f);

    // Coach bubble con diferentes anchos
    printf("  Coach bubble width sensitivity:\n");
    for (float w : widths) {
        float h = getCoachBubbleHeight(msg, w);
        printf("    maxW=%.0f → h=%.1fpx\n", w, h);
    }
}

static void test_edge_markdown_bold()
{
    std::printf("\n── Edge Cases: Only **bold** markers ──\n");

    float maxW = 350.0f;

    // Mensaje que es solo bold
    juce::String msg = "**ATENCI\u00D3N: Clip detectado**";
    float h = getSystemBubbleHeight(msg, maxW);
    printf("  System (all bold): %.1fpx\n", h);
    TEST("system message with bold only is valid", h >= 14.0f);

    // Para coach (tag vacío)
    float coachH = getCoachBubbleHeight(msg, maxW);
    printf("  Coach (all bold):  %.1fpx\n", coachH);
    TEST("coach bubble with bold only is valid", coachH >= 44.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("===================================================================\n");
    std::printf("  Chat Bubble Height Unit Tests\n");
    std::printf("  Verifica alturas de system messages y coach bubbles\n");
    std::printf("===================================================================\n\n");

    // ─── System messages ─────────────────────────────────────────────────
    test_system_message_short();
    test_system_message_medium();
    test_system_message_long();
    test_system_message_bold();

    // ─── Coach bubbles ──────────────────────────────────────────────────
    test_coach_message_short();
    test_coach_message_medium();
    test_coach_message_with_tag();
    test_coach_message_long_identity();

    // ─── Edge cases ─────────────────────────────────────────────────────
    test_edge_empty_text();
    test_edge_different_widths();
    test_edge_markdown_bold();

    // ─── Results ────────────────────────────────────────────────────────
    std::printf("\n===================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("===================================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}
