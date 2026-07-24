#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  GenreProfile — Perfil de género con características específicas
//  para que el coach ofrezca orientación personalizada al seleccionar
//  un género musical.
//
//  Uso: GenreProfile::getMessage("afrobeat")
//    → "El Afrobeat necesita:\n✓ Groove\n✓ Punch\n✓ Movimiento estéreo..."
// ═══════════════════════════════════════════════════════════════════════════
struct GenreProfile
{
    juce::String key;       // "afrobeat", "pop", "rock"...
    juce::String label;     // "Afrobeat", "Pop", "Rock"...
    juce::String icon;      // Emoji representativo
    std::vector<juce::String> characteristics;  // Características clave
    juce::String advice;    // Consejo inicial del coach para este género

    /** Construye el mensaje del coach con las características del género.
        @return Mensaje formateado listo para addSystemMessage() */
    [[nodiscard]] juce::String buildMessage() const
    {
        juce::String msg;
        msg << "\\xF0\\x9F\\x8E\\xAF **" << icon << " " << label << "**\\n\\n";
        msg << "Excelente elecci\\xC3\\xB3n. El **" << label << "** necesita:\\n\\n";
        for (const auto& c : characteristics) {
            msg << "\\xE2\\x9C\\x85 " << c << "\\n";
        }
        if (advice.isNotEmpty()) {
            msg << "\\n\\xF0\\x9F\\x92\\xA1 " << advice;
        }
        return msg;
    }

    /** Retorna el perfil para un key de género dado.
        @param genreKey  Clave del género (insensible a mayúsculas)
        @return Puntero al GenreProfile, o nullptr si no se encuentra */
    static const GenreProfile* getByKey(const juce::String& genreKey) noexcept
    {
        juce::String lower = genreKey.trim().toLowerCase();
        for (const auto& p : getAll())
            if (p.key == lower) return &p;
        return nullptr;
    }

    /** Retorna todos los perfiles de género disponibles. */
    static const std::vector<GenreProfile>& getAll() noexcept
    {
        static const std::vector<GenreProfile> profiles = {
            {
                "reggaeton",
                "Reggaet\\xC3\\xB3n",
                "\\xF0\\x9F\\x94\\x8A",
                {
                    "\\xF0\\x9F\\xA5\\x81 **Kick bombeado** — El 808 debe sentirse en el pecho",
                    "\\xF0\\x9F\\x94\\xA5 **Hi-Hats r\\xC3\\xA1pidos** — Rollos de tres, abiertos y sincopados",
                    "\\xF0\\x9F\\x94\\x8A **Movimiento est\\xC3\\xA9reo amplio** — Percusiones paneadas, voces al centro",
                    "\\xF0\\x9F\\x93\\xA3 **Voz presente y clara** — El vocalista es el centro de atenci\\xC3\\xB3n",
                    "\\xF0\\x9F\\x94\\x89 **Transiciones suaves** — Fills, risers cada 8 compases"
                },
                "Presta especial atenci\\xC3\\xB3n al **balance entre el 808 y el Kick**. "
                "En Reggaet\\xC3\\xB3n, el 808 suele ocupar todo el subgrave, "
                "as\\xC3\\xAD que aseg\\xC3\\xBArate de que el Kick tenga presencia en los medios (60-100 Hz)."
            },
            {
                "pop",
                "Pop",
                "\\xF0\\x9F\\x92\\x9B",
                {
                    "\\xF0\\x9F\\x8E\\xA4 **Voz principal ultra-clara** — Es el elemento m\\xC3\\xA1s importante",
                    "\\xF0\\x9F\\x93\\x8A **Compresi\\xC3\\xB3n suave y constante** — Loudness uniforme",
                    "\\xF0\\x9F\\x8E\\xB9 **Ganchos mel\\xC3\\xB3dicos** — Sintetizadores y pads pegajosos",
                    "\\xF0\\x9F\\xA5\\x81 **Bater\\xC3\\xADa ajustada** — Kick y Snare con presencia sin dominar",
                    "\\xF0\\x9F\\x94\\x8A **Imagen est\\xC3\\xA9reo equilibrada** — Ancha pero enfocada"
                },
                "El Pop moderno requiere **referencias de loudness altas** (-8 a -10 LUFS integrados). "
                "Usa una referencia comercial del mismo estilo para guiar tu master."
            },
            {
                "rock",
                "Rock",
                "\\xF0\\x9F\\x8E\\xB8",
                {
                    "\\xF0\\x9F\\xA4\\x98 **Bater\\xC3\\xADa potente y natural** — Sonido de sala, no triggeada",
                    "\\xF0\\x9F\\x8E\\xA8 **Guitarras con car\\xC3\\xA1cter** — Distorsi\\xC3\\xB3n controlada, medios presentes",
                    "\\xF0\\x9F\\x93\\xA3 **Voz energ\\xC3\\xA9tica** — Con presencia y actitud, no demasiado procesada",
                    "\\xF0\\x9F\\x94\\x8A **Amplio rango din\\xC3\\xA1mico** — No comprimir en exceso, mantener la vida",
                    "\\xF0\\x9F\\x8E\\x99\\xEF\\xB8\\x8F **Bajo definido** — Que se sienta sin opacar a las guitarras"
                },
                "El rock se beneficia de **menos compresi\\xC3\\xB3n de la que crees**. "
                "Deja que la bater\\xC3\\xADa respire naturalmente. Apunta a -12 LUFS integrados."
            },
            {
                "edm",
                "EDM",
                "\\xF0\\x9F\\x92\\xA0",
                {
                    "\\xF0\\x9F\\x94\\x8A **Imagen est\\xC3\\xA9reo masiva** — Pads y leads anch\\xC3\\xADsimos",
                    "\\xF0\\x9F\\xA5\\x81 **Kick contundente** — Ataca fuerte en 100 Hz con subgrave controlado",
                    "\\xF0\\x9F\\x94\\x8B **Subgrave profundo** — 40-60 Hz presente pero sin distorsionar",
                    "\\xF0\\x9F\\x93\\x88 **Build-ups y drops** — Automatizaci\\xC3\\xB3n de filtros y risers",
                    "\\xF0\\x9F\\x94\\x8D **Transiciones limpias** — FX, fills, silencios estrat\\xC3\\xA9gicos"
                },
                "El EDM exige **consistencia de loudness**. Usa compresi\\xC3\\xB3n en el bus master "
                "con ratio suave (2:1) y ataque r\\xC3\\xA1pido para mantener la energ\\xC3\\xADa constante."
            },
            {
                "hiphop",
                "Hip-Hop",
                "\\xF0\\x9F\\x93\\xA3",
                {
                    "\\xF0\\x9F\\x8E\\xA4 **Voz por encima de todo** — Clara, presente, con personalidad",
                    "\\xF0\\x9F\\xA5\\x81 **808 profundo y controlado** — El subgrave es el alma del g\\xC3\\xA9nero",
                    "\\xF0\\x9F\\xA5\\x81 **Kick y Snare con pegada** — Transientes r\\xC3\\xA1pidos y agresivos",
                    "\\xF0\\x9F\\x94\\xA5 **Hi-Hats r\\xC3\\xA1pidos y r\\xC3\\\\xADtmicos** — Rolls y swing caracter\\xC3\\xADstico",
                    "\\xF0\\x9F\\x93\\x8A **Compresi\\xC3\\xB3n vocal creativa** — Efecto de proximidad y presencia"
                },
                "En Hip-Hop, el **808 y la voz compiten por atenci\\xC3\\xB3n**. "
                "Usa sidechain compression para que el 808 baje cuando la voz entra. "
                "El rango din\\xC3\\xA1mico puede ser m\\xC3\\xA1s agresivo que en otros g\\xC3\\xA9neros."
            },
            {
                "afrobeat",
                "Afrobeat",
                "\\xF0\\x9F\\xA5\\xA1",
                {
                    "\\xF0\\x9F\\x8E\\xB6 **Groove infeccioso** — La percusi\\xC3\\xB3n es el motor r\\xC3\\xADtmico",
                    "\\xF0\\x9F\\x8F\\x8B\\xEF\\xB8\\x8F **Punch en los transientes** — Cada golpe debe sentirse",
                    "\\xF0\\x9F\\x94\\x8A **Movimiento est\\xC3\\xA9reo amplio** — Congas, shakers y guitarras paneadas",
                    "\\xF0\\x9F\\x93\\xA3 **Voces muy presentes** — Lead y coros con mucha claridad",
                    "\\xF0\\x9F\\x94\\x89 **Graves controlados** — Sub presente pero no abrumador"
                },
                "El Afrobeat necesita **espacio**. No satures el master. "
                "Mant\\xC3\\xA9n el bus de bater\\xC3\\xADa con suficiente headroom "
                "para que la percusi\\xC3\\xB3n respire. Apunta a -10/-12 LUFS."
            },
            {
                "jazz",
                "Jazz",
                "\\xF0\\x9F\\x8E\\xB7",
                {
                    "\\xF0\\x9F\\x8E\\xB7 **Sonido natural y org\\xC3\\xA1nico** — M\\xC3\\xADnimo procesamiento",
                    "\\xF0\\x9F\\x93\\x8A **Gran rango din\\xC3\\xA1mico** — De pianissimo a fortissimo",
                    "\\xF0\\x9F\\xA5\\x81 **Bater\\xC3\\xADa ac\\xC3\\xBAstica y aireada** — Platillos sutiles, bombo natural",
                    "\\xF0\\x9F\\x8E\\x99\\xEF\\xB8\\x8F **Contrabajo definido** — Que se sienta pero sin boom",
                    "\\xF0\\x9F\\x94\\x8A **Imagen est\\xC3\\xA9reo realista** — Como escuchar el ensayo en vivo"
                },
                "El Jazz es el g\\xC3\\xA9nero que **menos procesamiento necesita**. "
                "Usa compresi\\xC3\\xB3n muy leve (ratio 1.5:1 o menos) y evita limitar "
                "el master. La din\\xC3\\xA1mica es parte de la m\\xC3\\\\xBasica."
            },
            {
                "country",
                "Country",
                "\\xF0\\x9F\\x8E\\xB8",
                {
                    "\\xF0\\x9F\\xA4\\xA0 **Voz narrativa y clara** — La historia es lo primero",
                    "\\xF0\\x9F\\x8E\\xA8 **Guitarras ac\\xC3\\xBAsticas c\\xC3\\xA1lidas** — Medios presentes, agradables",
                    "\\xF0\\x9F\\xA5\\x81 **Bater\\xC3\\xADa con groove natural** — Menos procesada, m\\xC3\\xA1s humana",
                    "\\xF0\\x9F\\x92\\x9B **Pedal steel o fiddle presentes** — Caracter\\xC3\\xADsticos del g\\xC3\\xA9nero",
                    "\\xF0\\x9F\\x94\\x8A **Mezcla equilibrada y c\\xC3\\xA1lida** — Sin excesos en ninguna frecuencia"
                },
                "El Country moderno usa **compresi\\xC3\\xB3n vocal m\\xC3\\xA1s fuerte** que el cl\\xC3\\xA1sico. "
                "Mant\\xC3\\xA9n las guitarras ac\\xC3\\xBAsticas con presencia en 2-5 kHz para que brillen."
            },
            {
                "metal",
                "Metal",
                "\\xF0\\x9F\\xA4\\x98",
                {
                    "\\xF0\\x9F\\xA4\\x98 **Guitarras distorsionadas masivas** — Capas de gain, palm mutes potentes",
                    "\\xF0\\x9F\\xA5\\x81 **Bater\\xC3\\xADa ultrafr\\xC3\\xA1gil** — Doble bombo definido, snare explosivo",
                    "\\xF0\\x9F\\x93\\xA3 **Voz agresiva** — Screams y guturales con presencia sin dolor",
                    "\\xF0\\x9F\\x94\\x89 **Bajo que sigue a la guitarra** — Definido en medios, no solo sub",
                    "\\xF0\\x9F\\x93\\x8A **Compresi\\xC3\\xB3n fuerte pero controlada** — Pegada sin perder claridad"
                },
                "El Metal es el g\\xC3\\xA9nero m\\xC3\\xA1s exigente con el **rango din\\xC3\\xA1mico**. "
                "Usa compresi\\xC3\\xB3n en el bus de bater\\xC3\\xADa con ataque r\\xC3\\xA1pido (10-20ms) "
                "para mantener la pegada. Apunta a -8/-10 LUFS."
            },
            {
                "latino",
                "Latino",
                "\\xF0\\x9F\\x92\\x83",
                {
                    "\\xF0\\x9F\\x92\\x83 **Ritmo contagioso** — Percusi\\xC3\\xB3n latina al frente",
                    "\\xF0\\x9F\\x8E\\xA4 **Voz c\\xC3\\xA1lida y presente** — El carisma es clave",
                    "\\xF0\\x9F\\x94\\x8A **Amplio est\\xC3\\xA9reo** — Congas, bong\\xC3\\xB3s, maracas paneadas",
                    "\\xF0\\x9F\\x8E\\xB6 **Groove constante** — El bajo y la percusi\\xC3\\xB3n deben bailar juntos",
                    "\\xF0\\x9F\\x93\\x8A **Compresi\\xC3\\xB3n suave** — Que no mate la vitalidad del ritmo"
                },
                "La m\\xC3\\xBasica Latina vive en el **medio y el grave medio**. "
                "Aseg\\xC3\\xBArate de que el bajo tenga cuerpo (100-200 Hz) sin opacar "
                "a las percusiones. Mant\\xC3\\xA9n los agudos brillantes pero no agresivos."
            },
            {
                "house",
                "House",
                "\\xF0\\x9F\\x92\\xA0",
                {
                    "\\xF0\\x9F\\xA5\\x81 **Four-on-the-floor constante** — Kick en cada negra, s\\xC3\\xB3lido y profundo",
                    "\\xF0\\x9F\\x94\\x8A **Hi-Hat abierto y groove** — Off-beat, shuffle, movimiento",
                    "\\xF0\\x9F\\x8E\\xB9 **L\\xC3\\xADnea de bajo hipn\\xC3\\xB3tica** — Simple pero efectiva, con subgrave",
                    "\\xF0\\x9F\\x8E\\xB6 **Pads y acordes atmosf\\xC3\\xA9ricos** — Rellenan el espacio est\\xC3\\xA9reo",
                    "\\xF0\\x9F\\x94\\x89 **Transiciones suaves** — Filtros, claps y breakdowns cada 16-32 compases"
                },
                "El House necesita **consistencia de groove**. Usa sidechain del Kick al bajo "
                "para crear el bombeo caracter\\xC3\\xADstico. Ataque r\\xC3\\xA1pido (5-10ms), release al tempo."
            },
            {
                "rnb",
                "R&B",
                "\\xF0\\x9F\\x8E\\xBC",
                {
                    "\\xF0\\x9F\\x8E\\xA4 **Voz sedosa y presente** — El alma del g\\xC3\\xA9nero",
                    "\\xF0\\x9F\\xA5\\x81 **Bater\\xC3\\xADa relajada pero precisa** — Groove suave, caja con cuerpo",
                    "\\xF0\\x9F\\x8E\\xB9 **Teclados y pads c\\xC3\\xA1lidos** — Acordes jazzy, atm\\xC3\\xB3sfera rica",
                    "\\xF0\\x9F\\x94\\x8A **Bajo mel\\xC3\\xB3dico y presente** — Que camina entre el sub y el medio",
                    "\\xF0\\x9F\\x93\\x8A **Compresi\\xC3\\xB3n vocal cuidada** — Presencia \\xC3\\xADntima, como susurrando"
                },
                "El R&B se beneficia de **reverb y delay en la voz** para crear intimidad. "
                "Usa un reverb de sala corto (0.8-1.2s) con pre-delay de 30-40ms. "
                "No satures el master — d\\xC3\\xA9jale espacio para respirar."
            },
            {
                "trap",
                "Trap",
                "\\xF0\\x9F\\x94\\xA5",
                {
                    "\\xF0\\x9F\\x94\\xA5 **808 subgrave masivo** — Fundamental en 30-50 Hz, con distorsi\\xC3\\xB3n arm\\xC3\\xB3nica",
                    "\\xF0\\x9F\\xA5\\x81 **Hi-Hats r\\xC3\\xA1pidos y rolls** — 1/32, triplets, variaciones constantes",
                    "\\xF0\\x9F\\x93\\xA3 **Voz con actitud** — Procesada, a veces con autotune \\xC3\\xA1spero",
                    "\\xF0\\x9F\\x8E\\xB6 **Snare/Clap contundente** — Agudo y presente, con cola de reverb",
                    "\\xF0\\x9F\\x94\\x89 **Construcci\\xC3\\xB3n por capas** — Elementos que entran y salen"
                },
                "El Trap es el g\\xC3\\xA9nero del **subgrave**. El 808 debe sentirse en el cuerpo. "
                "Usa saturaci\\xC3\\xB3n arm\\xC3\\xB3nica suave en el 808 para que se escuche "
                "en altavoces peque\\xC3\\xB1os. Apunta a -8 LUFS integrados."
            },
            {
                "techno",
                "Techno",
                "\\xF0\\x9F\\xA4\\x96",
                {
                    "\\xF0\\x9F\\xA5\\x81 **Kick hipn\\xC3\\xB3tico y constante** — El motor del g\\xC3\\xA9nero",
                    "\\xF0\\x9F\\x94\\x8A **Texturas y atm\\xC3\\xB3sferas** — Pads oscuros, drones, ruido blanco",
                    "\\xF0\\x9F\\x8E\\xB6 **Percusi\\xC3\\xB3n m\\xC3\\xADnima y precisa** — Hi-hats, claps, shakers",
                    "\\xF0\\x9F\\x94\\x89 **Evoluci\\xC3\\xB3n lenta** — Cambios sutiles que construyen tensi\\xC3\\xB3n",
                    "\\xF0\\x9F\\x8E\\x9B\\xEF\\xB8\\x8F **L\\xC3\\xADnea de bajo profunda** — Subgrave que no compite con el Kick"
                },
                "El Techno vive del **espacio y la repetici\\xC3\\xB3n**. "
                "Usa delays y reverbs largos (2-3s) para crear profundidad. "
                "El Kick debe ocupar 40-60 Hz con presencia en 100-120 Hz para aud\\xC3\\xADfonos."
            },
            {
                "lofi",
                "Lo-Fi",
                "\\xF0\\x9F\\x93\\xBC",
                {
                    "\\xF0\\x9F\\x93\\xBC **Textura c\\xC3\\xA1lida y vintage** — Ruido de vinilo, saturaci\\xC3\\xB3n suave",
                    "\\xF0\\x9F\\xA5\\x81 **Bater\\xC3\\xADa relajada y loopy** — Groove simple, caja con cuerpo",
                    "\\xF0\\x9F\\x8E\\xB9 **Teclados y samples nost\\xC3\\xA1lgicos** — Jazz samples, pads c\\xC3\\xA1lidos",
                    "\\xF0\\x9F\\x93\\x8A **Compresi\\xC3\\xB3n suave** — Que abrace la mezcla sin aplastarla",
                    "\\xF0\\x9F\\x92\\x9B **Tonos c\\xC3\\xA1lidos y suaves** — Graves presentes sin agresividad"
                },
                "El Lo-Fi es el g\\xC3\\xA9nero donde **los defectos sonvirtudes**. "
                "A\\xC3\\xB1ade ruido de vinilo, saturaci\\xC3\\xB3n, y recorta los agudos "
                "(low-pass en 14-16 kHz). No tengas miedo de usar un bitcrusher suave."
            },
            {
                "classical",
                "Cl\\xC3\\xA1sica",
                "\\xF0\\x9F\\x8E\\xBC",
                {
                    "\\xF0\\x9F\\x8E\\xBC **Rango din\\xC3\\xA1mico enorme** — De ppp a fff sin compresi\\xC3\\xB3n",
                    "\\xF0\\x9F\\x94\\x8A **Imagen est\\xC3\\xA9reo natural** — Como escuchar la orquesta en vivo",
                    "\\xF0\\x9F\\x8E\\xB7 **Timbre ac\\xC3\\xBAstico puro** — M\\xC3\\xADnimo procesamiento, m\\xC3\\xA1ximo respeto",
                    "\\xF0\\x9F\\x94\\x89 **Silencios y espacios** — El silencio es parte de la m\\xC3\\xBasica",
                    "\\xF0\\x9F\\x93\\x8A **Sin compresi\\xC3\\xB3n de master** — La din\\xC3\\xA1mica original es sagrada"
                },
                "La m\\xC3\\xBasica Cl\\xC3\\xA1sica se graba y mezcla **sin limitadores en el master**. "
                "Usa micr\\xC3\\xB3fonos espaciados para capturar la profundidad natural de la sala. "
                "El LUFS objetivo es secundario — la din\\xC3\\xA1mica real es lo importante."
            }
        };
        return profiles;
    }
};

} // namespace mixcoach
