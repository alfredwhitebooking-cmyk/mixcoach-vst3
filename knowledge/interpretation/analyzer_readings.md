# Nivel 4 — Interpretación de Analizadores

> **Propósito:** Enseñar al CoachEngine qué *significan* las lecturas de los analizadores.
> No son datos — son evidencias con consecuencias.

---

## 📊 Correlation Meter

| Lectura | Interpretación | Consecuencia | Acción | Severidad |
|---------|---------------|--------------|--------|:---------:|
| Correlation = +1.0 | Señal idéntica en L/R — mono perfecto | Sin imagen estéreo, mezcla angosta | Revisa si hay elementos que deberían tener ancho estéreo (pads, reverbs) | ✅ Info |
| Correlation = +0.5 a +0.9 | Rango saludable de estéreo | Buena compatibilidad mono con imagen estéreo natural | — | ✅ Praise |
| Correlation = +0.3 a +0.5 | Estéreo conservador | Mezcla puede sonar angosta en sistemas estéreo | Abre panoramas, agrega delays o reverbs en los lados | ℹ️ Info |
| Correlation = 0.0 a +0.3 | Baja correlación | Posible pérdida de definición en mono | Revisa si hay widesters estéreo o phase flipping | ⚠️ Warning |
| Correlation < 0.0 | **FASE INVERTIDA** | Cancelación severa en mono — partes de la mezcla desaparecen | Revisa polaridad de micrófonos, wideners estéreo, M/S processing | 🔴 Critical |
| Correlation = -1.0 | Fase completamente invertida | Cancelación total en mono | Flip polarity en la pista problemática | 🔴 Critical |

**Contexto por género:**
- **EDM/Trap**: correlación más baja (0.3-0.6) es aceptable por diseño
- **Jazz/Clásica**: correlación 0.5-0.8 es normal por micrófonos de sala
- **Pop/Comercial**: mantener > 0.5 para traducción a mono
- **Podcast/Radio**: > 0.8 — prioridad máxima de mono compatibilidad

---

## 🎯 Crest Factor (Peak - RMS en dB)

| Lectura | Interpretación | Consecuencia | Acción | Severidad |
|---------|---------------|--------------|--------|:---------:|
| Crest < 4 dB | **Sobrecompresión severa** | La mezcla suena aplastada, sin vida ni transientes | Reduce ratios de compresión, aumenta attack times | 🔴 Critical |
| Crest 4-6 dB | Muy comprimido | Fatiga auditiva rápida, falta de punch | Revisa cadenas de compresión, busca reducir GR | ⚠️ Warning |
| Crest 6-10 dB | Rango dinámico saludable (la mayoría de géneros) | Buen balance entre punch y control | — | ✅ Praise |
| Crest 10-16 dB | Muy dinámico | Dificultad para escuchar a bajo volumen, partes suaves se pierden | Agrega compresión suave en tracks más dinámicos | ℹ️ Info |
| Crest > 16 dB | Extremadamente dinámico | Mezcla no traduce bien a streaming o club | Comprime las tracks más dinámicas, usa limitador suave en master | ⚠️ Warning |

**Contexto por género:**
| Género | Crest Esperado | Si sube X% | Si baja X% |
|--------|:--------------:|:-----------:|:----------:|
| Pop | 5-8 dB | +30% → mezcla más dinámica, menos pulida | -30% → sobrecomprimida |
| EDM | 4-7 dB | +40% → pierde impacto en club | -30% → aplastada |
| Hip-Hop/Trap | 5-8 dB | +30% → menos punch | -30% → sin vida |
| Rock | 8-12 dB | +20% → más dinámico (bueno) | -30% → pierde energía |
| Jazz | 10-16 dB | — | -40% → pierde naturalidad |
| Clásica | 14-20 dB | — | -50% → pierde rango dinámico |

---

## 🌡️ Spectral Centroid

| Lectura | Interpretación | Consecuencia | Acción | Severidad |
|---------|---------------|--------------|--------|:---------:|
| Centroid +20% vs referencia | Mezcla más brillante que la referencia | Posible fatiga auditiva en frecuencias medias-altas (3-5 kHz) | Suaviza presencia en voces, revisa hi-hats y platillos | ⚠️ Warning |
| Centroid -20% vs referencia | Mezcla más oscura que la referencia | Falta de claridad y definición, mezcla opaca | Agrega shelving en 8-12 kHz, revisa HPF agresivos | ⚠️ Warning |
| Centroid +10% vs referencia | Ligeramente más brillante | Aceptable — puede sonar más moderno | — | ℹ️ Info |
| Centroid -10% vs referencia | Ligeramente más oscuro | Aceptable — puede sonar más vintage | — | ℹ️ Info |
| Centroid dentro de ±10% | Balance tonal similar a la referencia | Buena traducción espectral | — | ✅ Praise |

**Rangos típicos de centroid por género:**
- EDM/Trap: 2.5-4 kHz (brillante)
- Pop: 2-3.5 kHz
- Rock: 1.5-3 kHz
- Jazz: 1-2.5 kHz
- Clásica: 0.8-2 kHz
- Lo-fi: 1-2 kHz (oscuro intencional)

---

## 🔊 LUFS / Loudness

| Lectura | Interpretación | Consecuencia | Acción | Severidad |
|---------|---------------|--------------|--------|:---------:|
| Integrated LUFS > target +2 dB | Mezcla más fuerte que el target | Posible distorsión en streaming, mezcla sin headroom | Baja el master fader 2-3 dB, revisa limitadores | ⚠️ Warning |
| Integrated LUFS < target -4 dB | Mezcla más silenciosa que el target | Sonará más bajito que otras canciones en playlists | Sube niveles, agrega compresión/limitación suave | ℹ️ Info |
| Integrated LUFS cerca del target (±2 dB) | Loudness alineado con el género | Buena traducción a plataformas | — | ✅ Praise |
| True Peak > -1 dBTP | **Intersample peaks peligrosos** | Distorsión en conversión D/A y códecs con pérdida | Reduce output del limitador 1-2 dB, usa True Peak limiting | 🔴 Critical |
| Loudness Range (LRA) < 3 LU | Rango de loudness muy estrecho | Mezcla monótona, fatiga auditiva | Revisa compresión excesiva, agrega variación dinámica | ⚠️ Warning |
| Loudness Range (LRA) > 12 LU | Rango de loudness muy amplio | Partes suaves se pierden en ambientes ruidosos | Comprime las secciones más dinámicas | ℹ️ Info |

**Targets LUFS por género (para el CoachEngine):**
| Género | LUFS I | LUFS ST | TP | LRA |
|--------|:------:|:-------:|:--:|:---:|
| Pop | -9 a -7 | -12 a -8 | -1 dBTP | 5-8 LU |
| EDM | -7 a -4 | -10 a -6 | -0.5 dBTP | 3-6 LU |
| Hip-Hop/Trap | -8 a -6 | -11 a -7 | -0.5 dBTP | 4-6 LU |
| Rock | -10 a -8 | -13 a -9 | -1 dBTP | 6-10 LU |
| Jazz | -14 a -16 | -18 a -14 | -2 dBTP | 10-16 LU |
| Clásica | -16 a -20 | -20 a -16 | -2 dBTP | 14-20 LU |
| Reggaeton | -7 a -5 | -10 a -6 | -0.5 dBTP | 4-6 LU |
| Metal | -8 a -5 | -11 a -7 | -0.5 dBTP | 3-6 LU |

---

## 🔄 Vectorscope / Goniometer

| Observación | Interpretación | Consecuencia | Acción | Severidad |
|-------------|---------------|--------------|--------|:---------:|
| Traza predominantemente horizontal | Exceso de información en el canal Side | Imagen estéreo exagerada, problemas en mono | Revisa elementos paneados extremos, reduce ancho de pads/reverbs | ⚠️ Warning |
| Traza predominantemente vertical (> 45°) | Mayoría de energía en Mid | Mezcla angosta, falta de apertura | Agrega ancho estéreo en elementos secundarios | ℹ️ Info |
| Traza circular/ dispersa | Ruido estéreo o ambiencia | Puede sonar difuso, falta de foco | Revisa si hay demasiada reverb o efectos estéreo | ℹ️ Info |
| Traza lineal en 45° (L=R) | Señal 100% mono | Sin imagen estéreo — intencional para bajos | Verifica que sea solo el bajo/kick lo que está en mono | ✅ Info |
| Traza se contrae al monitorear en mono | Cancelación de fase | Pérdida de información | Usa correlación para identificar frecuencias problemáticas | ⚠️ Warning |

---

## 📈 Espectro RTA por Bandas

| Observación | Interpretación | Consecuencia | Acción | Severidad |
|-------------|---------------|--------------|--------|:---------:|
| 250 Hz = Low Mid | Banda de calidez o barro, según cantidad | Si está reforzada → body; si está atenuada → claridad | Corte quirúrgico de 2-4 dB si hay acumulación | ℹ️ Info |
| 3 kHz = Presencia | Banda crítica para inteligibilidad vocal | Si sobra → fatiga auditiva; si falta → voz sepultada | Ajuste fino de 1-2 dB, depende del contexto | ℹ️ Info |
| 8 kHz = Air | Brillo y apertura | Si sobra → sibilancia; si falta → mezcla opaca | Shelving suave, control con de-esser si hay sibilancia | ℹ️ Info |
| Sub (20-60 Hz) +8 dB vs referencia | Exceso de sub-graves | Headroom consumido, mezcla turbia en sistemas sin sub | HPF en 25-30 Hz en elementos que no sean kick/bass | ⚠️ Warning |
| Presence (3-6 kHz) -6 dB vs referencia | Falta de presencia | Voces e instrumentos principales suenan lejanos, falta de foco | Boost shelving en 4-6 kHz de 1-3 dB en voces/pistas principales | ⚠️ Warning |
| Low-Mid (250-500 Hz) acumulado | Barro en la mezcla | Mezcla turbia, falta de claridad | Corte de 2-4 dB en 300-500 Hz en guitarras, teclados, voces | ⚠️ Warning |

---

## 🥁 Instrumentos por Rango Esperado

| Instrumento | Rango esperado | Indicador de problema |
|-------------|:--------------:|-----------------------|
| Kick moderno (reggaetón) | 45-80 Hz (sub), 2-5 kHz (click) | Si falta 50-60 Hz → sin peso; si falta 2-4 kHz → sin ataque |
| 808 trap | 30-60 Hz (fundamental), 100-300 Hz (armónicos distorsión) | Sin armónicos → inaudible en móviles; sin fundamental → no se siente |
| Voz pop | 200-400 Hz (body), 3-5 kHz (presencia), 8-12 kHz (air) | 300-500 Hz acumulado → barro; 5-8 kHz sin control → sibilancia |
| Snare | 150-250 Hz (body), 4-6 kHz (crack) | 400-600 Hz → sonido de caja; falta 4-6 kHz → sin snap |
| Hi-hats | 7-10 kHz (brillo) | 200-400 Hz → clutter innecesario |
| Bass guitar | 60-120 Hz (fundamental), 700-1 kHz (growl) | 200-400 Hz → mud; 1-2 kHz → harsh |
| Piano | 80-200 Hz (body), 2-4 kHz (presence) | 300-500 Hz → barro en registros medios |
| Synth pad | 200-400 Hz (cuerpo), 1-3 kHz (presencia) | Ancho estéreo excesivo → problemas de fase en mono |

---

## 🎚️ Interpretación por Fase de Mezcla

Cada fase del CoachEngine debe interpretar los analizadores de forma diferente:

### FASE 3 — Coaching Active (Gain Staging)
```
Pregunta: ¿Los niveles son seguros?
Mirar:   Peak/RMS meters, Crest Factor, True Peak
Acción:  Si crest < 4 dB → "Estás sobrecomprimiendo, la mezcla pierde transientes"
         Si True Peak > -1 dBTP → "Intersample peaks peligrosos, baja el output 1-2 dB"
         Si peak global > -3 dB → "Poco headroom, deja 6 dB para mastering"
```

### FASE 4 — References
```
Pregunta: ¿Qué tan cerca estoy de la referencia?
Mirar:   Spectral Centroid, LUFS, Crest Factor, band energy
Acción:  Si centroid diff > 20% → explicar qué significa sonoramente
         Si LUFS diff > 3 dB → mostrar en contexto de plataformas
```

### FASE 5 — Refinement
```
Pregunta: ¿El estéreo y la profundidad funcionan?
Mirar:   Correlation, Vectorscope, Stereo Width, Reverb
Acción:  Si correlación < 0 → "Pérdida en mono, revisa wideners"
         Si stereo width muy alto en bajos → "Problemas de fase en sub-graves"
```

---

## 📝 Formato para el LLM Context

Cuando se envía contexto al LLM, incluir las interpretaciones activas:

```
--- ANALYZER INTERPRETATIONS ---
📊 Correlation: -0.23
  → FASE INVERTIDA: posible cancelación al reproducirse en mono
  → Acción: Revisa polaridad de micrófonos o wideners estéreo

🎯 Crest Factor: 3.2 dB (target Pop: 5-8 dB)
  → SOBRECOMPRIMIDO: la mezcla pierde transientes y suena aplastada
  → Acción: Reduce ratios de compresión, aumenta attack times

🌡️ Spectral Centroid: +22% vs referencia (Pop)
  → MEZCLA MÁS BRILLANTE: posible fatiga auditiva en 3-5 kHz
  → Acción: Suaviza presencia en voces, revisa hi-hats

🔊 Integrated LUFS: -6.2 LUFS (target Pop: -9 a -7)
  → MEZCLA MÁS FUERTE que el target: puede distorsionar en streaming
  → Acción: Baja master fader 2-3 dB, revisa limitador
```
