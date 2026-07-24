#pragma once
#include <juce_core/juce_core.h>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachPersona — Personalidad del coach
    //  Define cómo habla, qué énfasis pone y qué tono usa.
    //  El usuario puede cambiar entre 3 personalidades en settings.
    // ═══════════════════════════════════════════════════════════════════════════
    enum class CoachPersona : uint8_t
    {
        // ─── Motivador: 👍 emojis, ánimo, "vas bien", "confío en ti" ──────────
        Motivador = 0,

        // ─── Técnico: números, dB, Hz, Q, "reduce 2.3dB en 2500Hz con Q=1.8" ──
        Tecnico = 1,

        // ─── Directo: sin rodeos, 1-2 frases, "baja el kick 2dB" ──────────────
        Directo = 2,

        // ─── Default ──────────────────────────────────────────────────────────
        Default = Motivador
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachPersonaTraits — Características de cada personalidad
    // ═══════════════════════════════════════════════════════════════════════════
    struct CoachPersonaTraits
    {
        const char* name;       // Nombre legible ("Motivador", "Técnico", "Directo")
        const char* icon;       // Emoji icon ("👍", "🔢", "🎯")
        const char* description; // Descripción corta ("Emojis, ánimo, celebración")

        // ─── System prompt additions ──────────────────────────────────────────
        const char* toneInstruction;   // Cómo hablar
        const char* emojiInstruction;  // Uso de emojis
        const char* dataInstruction;   // Cuántos números/datos incluir
        const char* lengthInstruction; // Longitud de respuestas
    };

    /** Retorna los traits para una persona dada. */
    inline CoachPersonaTraits getPersonaTraits(CoachPersona persona) noexcept
    {
        switch (persona) {
            case CoachPersona::Motivador:
                return {
                    "Motivador",
                    "\xF0\x9F\x91\x8D", // 👍
                    "Emojis, animo, celebracion",
                    // toneInstruction
                    "Hablas con calidez y entusiasmo. Eres un mentor que cree en el usuario. "
                    "Usa un tono alentador y positivo en TODO momento.",
                    // emojiInstruction
                    "USA EMOJIS CON LIBERTAD. Cada respuesta debe tener al menos 1-2 emojis "
                    "para reforzar el tono. 👍 para logros, 🔥 para progreso, 🎯 para precision, "
                    "💪 para animar, ✨ para celebraciones.",
                    // dataInstruction
                    "MENCIONA NUMEROS CON MODERACION. Cuando des datos, hazlo de forma suave: "
                    "'El kick bajo unos 3dB y quedo perfecto' en vez de 'El kick bajo 3.2dB exactos'. "
                    "Los numeros redondos son mejores. Enfocate en como SUENA, no en los numeros exactos.",
                    // lengthInstruction
                    "RESPUESTAS NORMALES (2-6 parrafos). No hay limite de longitud. "
                    "Usa el espacio para explicar, animar y celebrar."
                };

            case CoachPersona::Tecnico:
                return {
                    "Tecnico",
                    "\xF0\x9F\x94\xA2", // 🔢
                    "Numeros, dB, Hz, Q, precision",
                    // toneInstruction
                    "Hablas como un ingeniero de mezcla con anos de experiencia. "
                    "Eres preciso, tecnico y detallista. El usuario respeta tu conocimiento.",
                    // emojiInstruction
                    "NO USES EMOJIS. Cero emojis. Solo texto. "
                    "Si acaso, usa simbolos tecnicos como ~, →, ±, dB, Hz.",
                    // dataInstruction
                    "INCLUYE NUMEROS EXACTOS SIEMPRE. Frecuencias exactas, dB exactos, Q, ratio, attack, release. "
                    "Ej: 'Aplica un HPF a 83Hz con pendiente 12dB/oct en el pad. "
                    "Despues un bell cut de 2.3dB en 347Hz con Q=1.8.' "
                    "Cada recomendacion debe tener parametros verificables.",
                    // lengthInstruction
                    "RESPUESTAS DETALLADAS (3-8 parrafos). Explica el POR QUE detras de cada numero. "
                    "El usuario quiere entender la ciencia detras del ajuste."
                };

            case CoachPersona::Directo:
            default:
                return {
                    "Directo",
                    "\xF0\x9F\x8E\xAF", // 🎯
                    "Sin rodeos, 1-2 frases, al grano",
                    // toneInstruction
                    "Ve DIRECTAMENTE al grano. Sin introducciones, sin rodeos, sin explicaciones. "
                    "El usuario sabe lo que hace y solo necesita la instruccion.",
                    // emojiInstruction
                    "NO USES EMOJIS. Solo texto limpio y directo.",
                    // dataInstruction
                    "NUMEROS ESENCIALES SOLO. Da el dato exacto pero sin explicacion. "
                    "Ej: 'Baja el kick 2dB.' 'HPF 80Hz en el pad.' 'Compresion 4:1, threshold -18dB.' "
                    "Maximo 2 numeros por recomendacion.",
                    // lengthInstruction
                    "RESPUESTAS ULTRA-CORTAS (1-3 frases). Maximo 4 lineas. "
                    "Si necesitas mas de 4 lineas, divide en 2 mensajes. "
                    "No des contexto ni explicacion a menos que el usuario pregunte."
                };
        }
    }

    /** Retorna el nombre legible de la persona. */
    inline const char* personaName(CoachPersona persona) noexcept
    {
        return getPersonaTraits(persona).name;
    }

    /** Retorna el icono de la persona. */
    inline const char* personaIcon(CoachPersona persona) noexcept
    {
        return getPersonaTraits(persona).icon;
    }

    /** Retorna el número total de personalidades. */
    inline constexpr int kNumPersonas = 3;

    /** Itera a la siguiente persona (cíclicamente). */
    inline CoachPersona nextPersona(CoachPersona current) noexcept
    {
        int next = (static_cast<int>(current) + 1) % kNumPersonas;
        return static_cast<CoachPersona>(next);
    }

} // namespace mixcoach
