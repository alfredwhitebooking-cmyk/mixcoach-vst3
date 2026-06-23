## 808 / SUB-BASS — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Deep sub** | 20–50 Hz | Pure feel, subwoofer territory | Boost for club impact, HPF at 25–30 Hz |
| **Fundamental** | 40–100 Hz | Note root, weight | Core body—varies by note (E1=41Hz, G1=49Hz, A1=55Hz, C2=65Hz) |
| **Upper harmonics** | 100–500 Hz | Distortion harmonics, presence | Shaped by saturation, keep clean or cut |
| **Mid attack** | 500–3 kHz | Click/punch for 808 attack layer | Blend in a transient layer for definition |
| **Top** | 3–10 kHz | Sizzle from distortion | Control with low-pass or de-ess |

## 808 vs 808-style Bass

| Type | Character | Typical Tuning | Best For |
|------|-----------|---------------|----------|
| **TR-808 original** | Boomy, round, sine-like | E1–C2 | Hip-hop, trap, R&B |
| **Modern trap 808** | Distorted, sustained | C1–C2 | Trap, drill, modern hip-hop |
| **Zaytoven 808** | Cleaner, more melodic | Variable | Melodic trap, hip-hop |
| **Phonk 808** | Heavily distorted, lo-fi | Low tuning (E1–A1) | Phonk, dark trap |
| **Future bass sub** | Clean sine, sidechained | D2–C3 | EDM, future bass, pop |

## Tuning

**Critical:** 808s MUST be in key with the song.

| Note | Frequency | Common Key |
|------|-----------|------------|
| E1 | 41.2 Hz | E minor |
| F1 | 43.7 Hz | F minor |
| F#1/Gb1 | 46.2 Hz | F#/Gb |
| G1 | 49.0 Hz | G minor |
| Ab1/G#1 | 51.9 Hz | G#/Ab |
| A1 | 55.0 Hz | A minor |
| Bb1 | 58.3 Hz | Bb |
| B1 | 61.7 Hz | B |
| C2 | 65.4 Hz | C minor |
| Db2/C#2 | 69.3 Hz | Db/C# |

**Tip:** Use a tuner plugin on the 808. If the 808 note clashes with the kick fundamental, retune or pitch-shift the kick.

## Distortion & Saturation

| Type | Effect | Amount |
|------|--------|--------|
| **Soft clip** | Adds harmonic presence, keeps sub | Moderate |
| **Tube saturation** | Warm 2nd order harmonics | Light–moderate |
| **Tape saturation** | Smooth, compressed | Light |
| **Bit-crusher** | Lo-fi, aggressive | Use sparingly |
| **Overdrive** | Harsh, metallic | Use in parallel |

**Best practice (parallel processing):**
1. Send 808 to a parallel bus
2. Distort the bus aggressively
3. High-pass the distorted signal at 100–200 Hz
4. Blend back: 10–40% wet

This preserves the clean sub while adding harmonics that help the 808 cut through on small speakers.

## Compression

| Genre | Ratio | Attack | Release | Gain Reduction |
|-------|-------|--------|---------|----------------|
| Trap | 4:1–8:1 | 1–5 ms | 20–40 ms | 3–6 dB |
| Hip-Hop | 3:1–6:1 | 3–8 ms | 30–60 ms | 3–5 dB |
| EDM | 2:1–4:1 | 0–2 ms | 10–30 ms | 2–4 dB |
| Lo-fi | 6:1–10:1 | 5–10 ms | 40–80 ms | 4–8 dB |

## Envelope Shaping

| Style | Attack | Decay | Sustain | Release |
|-------|--------|-------|---------|---------|
| Short/Boom | 0–5 ms | 100–300 ms | Low | 50–100 ms |
| Long/Sustained | 0–5 ms | 500–2000 ms | High | 200–500 ms |
| Percussive | 2–10 ms | 50–100 ms | Medium | 30–60 ms |
| Sidechained | Pumped by kick | — | Varies | Release matches tempo |

Use an envelope follower or ADSR to shape the 808's decay to fit the groove.

## 808 & Kick Relationship

**The most critical relationship in trap/hip-hop mixing.**

### Option A: Sidechain (Most Common)
1. Sidechain compress the 808 from the kick
2. Attack: 1–5 ms, Release: match tempo (1/8 note = ~250ms @ 120 BPM)
3. Threshold: −10 to −20 dB depending on how much "pump" you want
4. Ratio: 4:1–10:1

### Option B: Complementary EQ
1. Kick fundamental: 50–80 Hz
2. 808 fundamental: 30–50 Hz
3. Cut each instrument by 2–4 dB at the other's fundamental

### Option C: Sidechain Dynamic EQ
- Dynamic cut at kick's fundamental (40–60 Hz) by 3–6 dB
- Release matches tempo
- More transparent than full sidechain compression

## EQ by Genre

- **Trap:** HPF @ 30 Hz, boost 40–50 Hz (+2–4 dB), cut 200–400 Hz (−3–5 dB), LPF distortion at 8–12 kHz
- **Hip-Hop:** HPF @ 25 Hz, boost 50–60 Hz (+2–4 dB), cut 300 Hz (−3 dB)
- **EDM/Future Bass:** HPF @ 30 Hz, sidechain pattern, clean sine with saturation blend
- **Phonk:** Heavily distorted, HPF clean sub at 40 Hz, distort parallel at 200 Hz+ and blend
- **Lo-fi:** HPF @ 40 Hz, tape saturation, slight LPF at 6–8 kHz for warmth

## Level Setting

| Genre | Peak Level (relative to mix) | LUFS Contribution | Headroom |
|-------|------------------------------|-------------------|----------|
| Trap | −6 to −10 dB | −12 to −8 LUFS | Keep 6 dB |
| Hip-Hop | −8 to −12 dB | −10 to −6 LUFS | Keep 6 dB |
| EDM | −6 to −10 dB | −8 to −5 LUFS | Sidechain creates room |
| Lo-fi | −10 to −14 dB | −14 to −10 LUFS | Keep 8–10 dB |

## Monitoring

- **Cannot mix 808s on laptop speakers alone — you need subwoofer or good headphones**
- Check translation: listen on subwoofer, closed-back headphones, and phone speaker
- Use a spectrum analyzer to ensure fundamental frequency is present but not overwhelming
- The 808 should be felt, not heard

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| 808 doesn't hit hard | Not enough attack transient | Layer a kick hit on the transient, or boost 1–2 kHz |
| Muddy/mushy | Too much low-mid (100–300 Hz) | Cut 150–300 Hz by 3–5 dB |
| Can't hear on phone | No harmonics | Add parallel distortion/saturation |
| Clashing with kick | Same fundamental frequency | Sidechain or tune differently |
| Not felt in the club | Missing 40–60 Hz range | Boost 40–60 Hz +2–4 dB |
| Too much rumble | Energy below 30 Hz taking headroom | HPF at 25–30 Hz aggressively |
| Out of tune | Not tuned to song key | Pitch-shift or retune to match key |

## Banjo — Instrument Guide

## Frequency Map

| Range | Frequencies | Description |
|-------|-------------|-------------|
| Bass | 80–200 Hz | Low-end body, resonance head |
| Low-mid | 200–500 Hz | Warmth, body |
| Mid | 500–2 kHz | Core banjo tone |
| High-mid | 2–5 kHz | Attack, pick noise, twang |
| High | 5–8 kHz | String sparkle, bridge resonance |
| Air | 8–12 kHz | Shimmer, harmonic overtones |

## EQ by Genre

### Bluegrass
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 100–150 Hz | Bell | 1.5 | +1.5 dB | Body |
| 300–500 Hz | Bell | 1.5 | -2 dB | Reduce boxiness |
| 1–2 kHz | Bell | 1.0 | +2 dB | Classic banjo presence |
| 4–6 kHz | Bell | 1.0 | +2.5 dB | Twang, cut through |
| 10 kHz+ | HBF | — | +1 dB | Sparkle |

### Folk / Old-Time
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 120–180 Hz | Bell | 1.5 | +1 dB | Warmth |
| 400–600 Hz | Bell | 1.5 | -2 dB | Reduce nasal quality |
| 2–3 kHz | Bell | 1.0 | +1.5 dB | Articulation |
| 8–10 kHz | HBF | — | +1 dB | Air |

### Pop / Contemporary
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 80–100 Hz | HSF | — | -2 dB | Tighten |
| 200–400 Hz | Bell | 1.8 | -3 dB | Clear mud |
| 2.5–5 kHz | Bell | 1.0 | +2 dB | Presence |
| 10 kHz+ | HBF | — | +1.5 dB | Shimmer |

## Compression Settings

| Context | Ratio | Attack | Release | Gain Reduction | Notes |
|---------|-------|--------|---------|----------------|-------|
| Bluegrass (Scruggs) | 3:1 | 10 ms | 80 ms | 3–5 dB | Keep picking dynamics |
| Bluegrass (ensemble) | 4:1 | 8 ms | 60 ms | 4–7 dB | Tame transients |
| Folk/Old-Time | 2.5:1 | 15 ms | 100 ms | 3–5 dB | Natural feel |
| Pop/Contemporary | 4:1 | 5 ms | 50 ms | 5–8 dB | Controlled, consistent |
| Frailing/Clawhammer | 3:1 | 8 ms | 70 ms | 4–6 dB | Even out bum-ditty pattern |

## Reverb

| Type | Pre-delay | Decay | Mix | Use Case |
|------|-----------|-------|-----|----------|
| Room | 10–15 ms | 0.6–1.0 s | 10–20% | Bluegrass (tight) |
| Hall | 20–30 ms | 1.5–2.0 s | 15–25% | Folk/solo |
| Plate | 15–20 ms | 1.0–1.5 s | 15–20% | Pop/contemporary |
| Spring | 5–10 ms | 0.8–1.2 s | 10–15% | Old-time/vintage |

## Common Issues & Solutions

### Harsh / too twangy
- **Cause**: Excessive 4–6 kHz presence
- **Fix**: Cut 4–6 kHz with narrow Q, roll-off highs above 12 kHz

### Thin / lacks body
- **Cause**: Missing low frequencies
- **Fix**: Boost 100–200 Hz, ensure HPF not too high (>80 Hz)

### Muddy / boomy
- **Cause**: Resonance head frequencies clashing
- **Fix**: Cut 200–400 Hz, tighten HPF to 100 Hz

### Pick noise / clicky
- **Cause**: Metal picks hitting strings
- **Fix**: De-esser at 5–8 kHz, gentle cut at 3 kHz

### Rolls not articulate
- **Cause**: Notes blending together in fast rolls
- **Fix**: Compressor with faster attack (5–8 ms), cut 300–500 Hz

### String buzz
- **Cause**: Low action or worn frets
- **Fix**: Cut 2–3 kHz, limit peaks at 3 kHz

## Special Techniques

### Scruggs style (Bluegrass)
- Three-finger picking with metal fingerpicks
- Moderate compression (3:1, 10 ms attack)
- Mid-forward EQ for cut-through in bluegrass band

### Frailing / Clawhammer
- Down-picking with nail on head
- Slightly more compression (3–4:1)
- Warmer EQ, less high-end twang

### Double tracking
- Record two takes for width
- Pan L/R, slight timing offset
- Or use harmonizer with slight detune

### Banjo rolls
- For fast rolls, use optical compressor for smooth release
- Ensure attack is fast enough to catch initial pick transient

## Recommended Processing Chain

1. **HPF** 80 Hz (remove low rumble)
2. **EQ** cut 300–500 Hz (reduce boxiness)
3. **Compressor** (3:1, 10 ms attack, 80 ms release)
4. **EQ** boost 1–2 kHz (presence) + 5 kHz (twang)
5. **Reverb** (short room, 15% mix — keep it tight)
6. **Limiter** −2 dB (catch pick transients)

## BASS — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub-bass** | 20–60 Hz | Feel, rumble | Roll off below 30 Hz, boost for sub presence |
| **Fundamental** | 60–200 Hz | Root notes, weight | Core frequency range—boost/cut depending on genre |
| **Upper bass** | 200–500 Hz | Warmth, mud | Cut 2–4 dB if muddy, boost for vintage warmth |
| **Low-mid** | 500–1000 Hz | Honk, boxiness | Cut if honky or competing with guitars |
| **Mid presence** | 1–3 kHz | Fret noise, attack | Boost for finger/pick attack definition |
| **High** | 3–8 kHz | Slap, string noise | Boost for modern clarity, cut if fizzy |

## Bass Types

| Type | Character | Best For | Mix Approach |
|------|-----------|----------|-------------|
| **Fingerstyle** | Warm, round, smooth | Rock, R&B, pop | Compress 3–6 dB, boost 100–200 Hz |
| **Picked** | Aggressive, defined | Rock, punk, metal | Less compression, boost 2–3 kHz |
| **Slap** | Bright, percussive | Funk, pop, fusion | Light compression, boost 5–8 kHz |
| **808** | Boomy, sub-heavy | Hip-hop, trap, EDM | See separate 808 guide |
| **Synth bass** | Variable, clean | EDM, pop, synthwave | Sculpt with filter + saturation |
| **Acoustic upright** | Woody, natural | Jazz, folk, bluegrass | Very gentle compression, minimal EQ |

## Compression

| Genre | Ratio | Attack | Release | Gain Reduction |
|-------|-------|--------|---------|----------------|
| Rock | 3:1–6:1 | 20–50 ms | 50–100 ms | 3–6 dB |
| Pop | 3:1–5:1 | 10–30 ms | 40–80 ms | 3–5 dB |
| Funk | 4:1–8:1 | 30–80 ms | 50–100 ms | 4–7 dB |
| Metal | 6:1–10:1 | 10–30 ms | 60–120 ms | 5–10 dB |
| Jazz | 2:1–3:1 | 30–60 ms | 60–150 ms | 1–3 dB |

**Key tip:** Slow attack (20–50 ms) lets the transient through for punch. Faster attack controls the note bloom.

## EQ by Genre

- **Rock:** Boost 100–150 Hz (+3–5 dB), cut 300–400 Hz (−3 dB), boost 2 kHz (+2–3 dB)
- **Pop:** Boost 80–120 Hz (+3–4 dB), cut 400 Hz (−2–3 dB), gentle high shelf +2 dB @ 5 kHz
- **Metal:** Cut 250–400 Hz (−3–4 dB), boost 1–2 kHz (+3–5 dB) for attack, boost 60 Hz (+3 dB)
- **Funk:** Boost 100–150 Hz (+2–3 dB), boost 2–3 kHz (+3–5 dB), high shelf +3 dB @ 8 kHz
- **Jazz:** Gentle EQ, slight low boost 80 Hz (+1–2 dB), gentle presence 2 kHz (+1–2 dB)

## Bass & Kick Relationship

- **Sidechain compress** bass from kick at kick's fundamental (40–80 Hz)
- **EQ carve:** Cut bass 2–4 dB at kick's fundamental frequency
- **Complementary EQ:** Boost kick at 60 Hz, cut bass at 60 Hz (or vice versa)
- **RMS match:** Bass and kick RMS should be roughly equal in most genres
- **Level setting:** Bass should feel "locked" with the kick—adjust until they feel like one rhythm section

## Saturation & Distortion

- **Saturation** adds harmonics that help bass cut through on small speakers
- **Parallel distortion** (blend 10–30%) adds presence without losing low end
- **Tube/tape saturation:** Adds 2nd order harmonics (warm, musical)
- **Clipping** (soft): can add sustain and punch

## Bass Guitar Tone Controls

| Control | Effect | Typical Setting |
|---------|--------|----------------|
| Volume | Overall level | Unity or slight boost |
| Tone/treble | Brightness | 60–80% (rock), 50–70% (jazz) |
| Bass/low | Low-end boost | 50–80% |
| Mid | Presence/cut | 50–70% (boost for cut) |
| Pickup blend | Neck vs bridge | Neck=warm, bridge=bright, blend=balanced |

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Muddy/low-end rumble | Too much 40–80 Hz or not HPF'd | High-pass at 30–50 Hz |
| Boomy/unclear | Too much 200–400 Hz | Cut 200–400 Hz by 3–5 dB |
| Can't hear on small speakers | Not enough harmonics | Add saturation/distortion, boost 1–2 kHz |
| Fizzy in top end | Too much 5–8 kHz | Cut or low-pass above 6–8 kHz |
| Floppy/dynamic | Not compressed enough | Increase ratio or lower threshold |
| Competing with kick | Same frequency range | Sidechain compress or EQ carve |

## BRASS — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Low** | 60–200 Hz | Body, warmth, pedal tones | HPF at 60–80 Hz, keep for fullness |
| **Low-mid** | 200–500 Hz | Mud, boxiness | Cut 3–5 dB if muddy |
| **Mid** | 500–1 kHz | Horn-like, nasal | Cut if too brassy/honky |
| **Upper mid** | 1–4 kHz | Presence, bite | Boost for cut-through |
| **Brilliance** | 4–8 kHz | Brightness, edge | Boost for modern, cut for vintage |
| **Air** | 8–16 kHz | Shimmer, openness | Gentle high shelf |

## Brass Types

| Type | Range | Character | Best For |
|------|-------|-----------|----------|
| **Trumpet** | 150 Hz–8 kHz | Bright, piercing, agile | Melodic lines, stabs |
| **Trombone** | 80 Hz–5 kHz | Rich, powerful, warm | Harmonies, bass lines |
| **French horn** | 90 Hz–6 kHz | Round, mellow, distant | Background, orchestral |
| **Tuba** | 40 Hz–3 kHz | Deep, round, foundational | Bass of brass section |
| **Saxophone** | 80 Hz–7 kHz | Versatile, warm to bright | Solos, pads, melodies |

## Compression

| Brass Type | Ratio | Attack | Release | GR |
|-----------|-------|--------|---------|-----|
| Trumpet | 3:1–5:1 | 10–20 ms | 40–80 ms | 3–6 dB |
| Trombone | 3:1–4:1 | 15–30 ms | 50–100 ms | 2–5 dB |
| Saxophone | 3:1–5:1 | 10–30 ms | 40–80 ms | 3–6 dB |
| Brass section | 3:1–4:1 | 10–20 ms | 40–80 ms | 2–4 dB |

## EQ by Genre

- **Orchestral:** Natural, gentle presence 2–4 kHz, HPF at 60 Hz
- **Jazz:** Warm, cut 400 Hz (−2–3 dB), presence 2–3 kHz (+2–4 dB)
- **Funk:** HPF at 100 Hz, cut 400 Hz (−3 dB), presence 3–5 kHz (+3–5 dB)
- **Ska/Punk:** HPF at 100 Hz, presence 3–5 kHz (+4–6 dB), air shelf

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Harsh/piercing | Cut 3–5 kHz 2–4 dB, add warmth 200–300 Hz |
| Muddy | Cut 300–500 Hz 3–5 dB |
| Thin/weak | Boost 200–300 Hz, lower HPF |
| Brass sticks out too much | Reduce 1–3 kHz presence |
| Synthetic samples | Add room reverb, analog saturation, slight pitch variation |

## CHOIR / VOCAL ENSEMBLE — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub** | 30–80 Hz | Rumble, bass voices | HPF at 60–80 Hz |
| **Low body** | 80–250 Hz | Fullness, warmth (bass/baritone) | Keep, but watch for mud |
| **Low-mid** | 250–600 Hz | Mud, boxiness | Cut 3–5 dB — choir's mud zone |
| **Mid** | 600 Hz–1 kHz | Nasality, honk | Cut 2–4 dB if nasal |
| **Presence** | 2–5 kHz | Clarity, intelligibility | Boost for diction and cut-through |
| **Air** | 5–16 kHz | Sparkle, sibilance | Gentle shelf, watch for sibilance |

## Voice Types & Arrangement

| Voice | Range | Role | Panning |
|-------|-------|------|---------|
| **Soprano** | 260 Hz–1 kHz | Highest, melody | L30–50% |
| **Alto** | 175 Hz–700 Hz | Lower harmony | R30–50% |
| **Tenor** | 130 Hz–440 Hz | Male high part | L10–20% |
| **Baritone** | 110 Hz–350 Hz | Mid male | C or slightly off |
| **Bass** | 80 Hz–300 Hz | Lowest foundation | R10–20% |

## Section Width

| Section Size | Panning Strategy | Width |
|-------------|------------------|-------|
| Small (4–8) | Moderate spread, each voice gets a position | 50–70% |
| Medium (8–16) | Section groups L/C/R | 70–100% |
| Large (16+) | Divide SATB into L/R groups | 80–100% |

## Compression

| Context | Ratio | Attack | Release | GR |
|---------|-------|--------|---------|-----|
| Classical choir | 2:1–3:1 | 20–40 ms | 100–200 ms | 1–3 dB |
| Pop/gospel choir | 3:1–5:1 | 10–20 ms | 50–100 ms | 3–6 dB |
| Soloist within choir | 4:1–6:1 | 5–15 ms | 30–60 ms | 4–8 dB |

## Reverb

| Context | Reverb Type | Decay | Pre-delay | Mix |
|---------|-------------|-------|-----------|-----|
| Classical | Hall | 2.0–4.0 s | 20–40 ms | 30–50% |
| Gospel | Hall/Plate | 1.5–3.0 s | 15–30 ms | 25–40% |
| Pop | Plate | 1.0–2.0 s | 15–30 ms | 20–30% |
| Cinematic | Cathedral | 3.0–6.0 s | 30–60 ms | 40–60% |

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Muddy choir (most common) | Cut 300–500 Hz 3–6 dB on choir bus |
| Indistinguishable words | Boost 2–5 kHz presence 3–5 dB |
| Too wide/weak center | Keep some voices centered, don't spread all |
| Harsh sibilance | De-ess choir bus at 6–8 kHz |
| Boomy/chesty | Cut 150–250 Hz 2–4 dB |
| Choir covers soloist | Reduce choir level 2–3 dB, EQ carve 2–5 kHz for soloist |

## DRUM MACHINE / ELECTRONIC DRUMS — Mixing Guide

## Classic Drum Machines & Characters

| Machine | Character | Best For | Mix Character |
|---------|-----------|----------|---------------|
| **TR-808** | Boomy, round, soft | Hip-hop, trap, R&B, house | Warm, punchy, subby kick, sizzly hats |
| **TR-909** | Punchy, aggressive, tight | House, techno, acid | Hard kick, crisp snare, metallic hats |
| **TR-707** | Digital, crisp, punchy | 80s, pop, house | Clean, precise, bright |
| **LinnDrum** | Warm, realistic | 80s pop, rock, R&B | Warm, sampled, roomy |
| **DMX** | Aggressive, punchy | 80s hip-hop, electro | Hard, in-your-face |
| **808 clap** | Snappy, wide, roomy | All genres | Comp 6:1, plate reverb |
| **Machine (general)** | Clean, precise, artificial | Modern pop, EDM | Process heavily to add character |

## Processing by Element

### Kick (Drum Machine)
- **808 kick:** Shorten or lengthen decay, pitch envelope, soft clip for presence
- **909 kick:** Emphasize 50–60 Hz and 3–4 kHz click, compress 4:1
- **Dirty kick:** Add saturation, bit crush, or distortion
- **Low end:** Keep mono, HPF at 30 Hz

### Snare / Clap
- **TR-808 clap:** Layer with snare for body, room reverb, compress 6:1
- **TR-909 snare:** Boost 200 Hz body, 4–5 kHz crack, short reverb
- **Rim shot:** HPF 500 Hz, boost 2–4 kHz, very dry

### Hi-Hats / Cymbals
- **Open/closed hats:** Velocity variation is critical for groove
- **Shuffle/swing:** 8th or 16th note swing for human feel
- **Processing:** HPF 300–500 Hz, gentle compression or envelope shaping
- **Ride/crash:** HPF 500 Hz, boost 8–10 kHz, short decay

## Groove & Humanization

| Technique | Effect | How |
|-----------|--------|-----|
| **Velocity variation** | Natural feel | Vary velocity 30–90%, not all 127 |
| **Timing offset** | Relaxed or pushy feel | Shift hats/percussion 5–20 ms off grid |
| **Swing/groove** | Shuffled feel | Apply 50–66% swing to 16th notes |
| **Flam** | Double hit | Layer two hits 5–15 ms apart (snare, clap) |
| **Sample layering** | Unique sound | Combine 2–3 sounds per drum hit |

## Bus Processing

| Bus | Processing | Effect |
|-----|-----------|--------|
| **Drum bus** | Comp 2:1–4:1, 1–3 dB GR | Glue drums together |
| **Drum bus parallel** | Heavy comp 10:1+, blend 20–40% | Add weight/punch |
| **Hat/perch bus** | HPF 300 Hz, light comp | Keep percussion clean |
| **Drum reverb** | Room or plate send | Space and depth |

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Robotic/static feel | Humanize velocity and timing |
| Thin drums | Layer with samples, add saturation |
| Muddy low end | HPF percussion at 200–400 Hz, keep kick/bass low end clean |
| Harsh hats | Cut 8–10 kHz 2–3 dB, or low-pass at 12–14 kHz |
| Drums don't punch | Add transient shaper, fast attack on compressor |
| Stereo hat too wide | Keep hats in narrower spread (40–60%) |

## FLUTE — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Low** | 200–400 Hz | Body, warmth | Keep for fullness, cut if muddy |
| **Low-mid** | 400–800 Hz | Mud, boxiness | Cut 3–5 dB if too warm |
| **Mid** | 1–3 kHz | Presence, breath | Boost for definition and cut-through |
| **Upper** | 3–6 kHz | Brightness, edge | Boost gently, cut if shrill |
| **Air** | 6–16 kHz | Breath, shimmer | Gentle shelf for airy tone |

## Flute Types

| Type | Range | Character | Best For |
|------|-------|-----------|----------|
| **C flute (standard)** | 261 Hz–2 kHz | Bright, agile | Classical, pop, orchestral |
| **Alto flute** | 185 Hz–1.5 kHz | Warmer, darker | Jazz, soft passages |
| **Bass flute** | 145 Hz–1 kHz | Deep, rich | Chamber, ambient |
| **Pan flute** | 200 Hz–3 kHz | Ethereal, folk | New age, folk, world |
| **Recorder** | 250 Hz–2 kHz | Clear, simple | Baroque, educational |

## Mixing Approach

| Context | Processing | Reverb | Level |
|---------|-----------|--------|-------|
| **Classical solo** | Minimal comp (2:1, 1–2 dB GR), natural EQ | Hall 1.5–2.5 s | Prominent |
| **Pop/rock** | Comp 3:1–4:1, presence 3–5 kHz | Plate 1.0–1.5 s | Blend |
| **Jazz** | Light comp (2:1–3:1), warm EQ | Room 0.8–1.2 s | Medium |
| **Ambient** | Floating, heavy reverb, wide | Hall/shimmer 2–4 s | Low-mid |

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Shrill/piercing | Cut 4–6 kHz 2–4 dB |
| Thin/weak | Boost 200–400 Hz, add body |
| Breath noise too loud | Gate or gentle HPF, automate quiet sections |
| Lost in mix | Boost 1–3 kHz presence 2–4 dB |

## Glockenspiel — Instrument Guide

## Frequency Map

| Range | Frequencies | Description |
|-------|-------------|-------------|
| Bass | 500–800 Hz | Low bars (C4–E4), fundamental range |
| Low-mid | 800–1.5 kHz | Mid-low bars, fundamental |
| Mid | 1.5–3 kHz | Core tone, body, attack |
| High-mid | 3–6 kHz | Bell-like attack, mallet impact |
| High | 6–12 kHz | Ring, sustain shimmer |
| Air | 12–20 kHz | Extreme brilliance, metallic overtones |

## EQ by Genre

### Orchestral / Classical
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 500–800 Hz | Bell | 1.5 | -1.5 dB | Reduce boxiness in lower bars |
| 1.5–2.5 kHz | Bell | 1.0 | +1.5 dB | Presence, cut through |
| 3–5 kHz | Bell | 1.5 | -2 dB | Reduce harsh mallet attack |
| 8–12 kHz | HBF | — | +2 dB | Sparkle, shimmer |
| 15 kHz+ | HBF | — | +1 dB | Air for upper harmonics |

### Pop / Contemporary
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 600–900 Hz | Bell | 1.5 | -2 dB | Reduce mud |
| 2–4 kHz | Bell | 1.0 | +2.5 dB | Aggressive presence |
| 5–8 kHz | Bell | 1.5 | -2 dB | Tame harshness |
| 10–15 kHz | HBF | — | +3 dB | Shimmer |

### Marching Band / Percussion Ensemble
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 800–1.2 kHz | Bell | 1.0 | +1 dB | Cutting power |
| 3–5 kHz | Bell | 1.5 | -2 dB | Reduce harshness outdoors |
| 10–15 kHz | HBF | — | +2 dB | Projection |

## Compression Settings

| Context | Ratio | Attack | Release | Gain Reduction | Notes |
|---------|-------|--------|---------|----------------|-------|
| Orchestral | 2:1 | 20 ms | 150 ms | 2–3 dB | Gentle, natural ring |
| Pop/Contemporary | 3:1 | 10 ms | 100 ms | 3–5 dB | Controlled |
| Marching band | 4:1 | 5 ms | 80 ms | 4–6 dB | Even projection |

## Reverb

| Type | Pre-delay | Decay | Mix | Use Case |
|------|-----------|-------|-----|----------|
| Hall | 20–30 ms | 2.0–3.0 s | 25–40% | Orchestral |
| Plate | 10–15 ms | 1.0–1.5 s | 15–25% | Pop/contemporary |
| Room | 5–10 ms | 0.6–1.0 s | 10–15% | Marching band (tight) |
| Cathedral | 30–50 ms | 3.0–4.0 s | 30–50% | Solo ethereal |

## Common Issues & Solutions

### Harsh / piercing
- **Cause**: Natural metallic resonance in 3–6 kHz range
- **Fix**: Cut 3–5 kHz with narrow Q, use softer mallets (yarn vs plastic)

### Too thin
- **Cause**: Missing body, sounds like just attack
- **Fix**: Boost 800 Hz–1.5 kHz, add reverb tail for sustain

### Ringing too long
- **Cause**: Natural sustain of metal bars
- **Fix**: Gate with 1–2 second release, or manual volume automation

### Metallic clang
- **Cause**: Mallet hitting bar too hard
- **Fix**: Cut 5–8 kHz, use felt or yarn mallets, reduce dynamics

### Muddy in low bars
- **Cause**: Lower bars (C4–E4) have fundamental frequencies in 500–800 Hz
- **Fix**: Narrow cut at 500–700 Hz for lower register notes

### Lost in dense mix
- **Cause**: Glockenspiel is in same frequency range as hi-hats and cymbals
- **Fix**: Boost 2–4 kHz for presence, pan to center or slightly left

## Special Techniques

### Mallet selection
| Mallet Type | Tone | Use Case |
|-------------|------|----------|
| Brass/Plastic | Bright, piercing | Marching band, aggressive pop |
| Nylon | Balanced | Orchestral, pop |
| Felt/Yarn | Warm, soft | Classical, solo |
| Rubber | Dark, mellow | Practice, intimate |

### Glockenspiel vs Celesta
- **Glockenspiel**: Brighter, more metallic, shorter sustain
- **Celesta**: Warmer, softer, organ-like sustain
- Process similarly but glockenspiel needs more taming of 3–6 kHz

### Doubling with other instruments
- Glockenspiel + flute: Magical, ethereal for melodies
- Glockenspiel + piano: Adds sparkle to upper register lines
- Glockenspiel + bells/chimes: Creates shimmer texture

### Roll notation
- Rolls with two mallets create tremolo effect
- Fast compressor (2 ms attack) to even out rolls
- Add subtle chorus for shimmer

## Recommended Processing Chain

1. **HPF** 400–500 Hz (tight, remove sub-ringing)
2. **EQ** cut 3–5 kHz (tame harshness) if needed
3. **Compressor** (2.5:1, 10–15 ms attack, 100 ms release)
4. **EQ** boost 8–15 kHz (sparkle, shimmer)
5. **Reverb** (hall or plate, 25–35% mix)
6. **Gate** (optional) 1–2 second hold to control ringing
7. **Limiter** −2 dB (catch mallet attack peaks)

## GUITARS — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub/low** | 40–100 Hz | Rumble, stage boom | High-pass at 80–120 Hz (varies by tuning) |
| **Low body** | 100–300 Hz | Fullness, warmth | Keep for rhythm, cut for lead clarity |
| **Low-mid** | 300–600 Hz | Mud, boxiness | Cut 3–6 dB to clean up mix space |
| **Mid** | 600–1000 Hz | Honk, nasal | Cut if competing with vocals |
| **Upper mid** | 1–4 kHz | Presence, bite | Boost for cut-through, cut if harsh |
| **Presence** | 4–8 kHz | Brightness, string noise | Boost for definition, cut for vintage |
| **Air** | 8–16 kHz | Sparkle, openness | Light shelf for modern sound |

## Electric vs Acoustic Guitars

| Aspect | Electric | Acoustic |
|--------|----------|----------|
| **HPF** | 80–120 Hz | 60–80 Hz (body resonance lower) |
| **Character** | Shaped heavily by amp + pedals | Natural wood resonance |
| **Compression** | 2:1–6:1 | 2:1–4:1 (lighter) |
| **Main freq range** | 200 Hz–5 kHz | 100 Hz–6 kHz |
| **Common EQ cuts** | 300–500 Hz (mud) | 200–300 Hz (boom) |
| **Reverb** | Room/Plate | Room/Ambient |

## Guitar Types & Roles

| Role | Characteristic | Mix Placement |
|------|---------------|---------------|
| **Rhythm (distorted)** | Chunky, palm-muted, wall of sound | Hard L/R (doubled) |
| **Rhythm (clean)** | Jangling, chordal | L/R or slightly spread |
| **Lead** | Singing, sustained, present | Center or slightly off |
| **Arpeggios** | Picked, melodic | Spread wide or centered |
| **Fingerpicking** | Delicate, dynamic | Centered with stereo verb |
| **Barre chords** | Full, strumming | Doubled L/R for width |

## Double Tracking (Essential Technique)

**Double tracking** — recording the same part twice and panning L/R — is the foundation of professional guitar mixing.

| Technique | Recording | Panning | Result |
|-----------|-----------|---------|--------|
| **True double** | Two separate takes | Hard L/R (100% each) | Wide, natural |
| **Quad tracked** | Four takes | L90, L30, R30, R90 | Massive wall of sound |
| **Copy + pitch shift** | One take, copied | Hard L/R, pitch 3–8 cents | Fake but passable |
| **Amp sim dual** | One DI, two amp models | Hard L/R | Good for demos |
| **Triple tracked** | Three takes | L100, C, R100 | Rock standard |

**Level rule:** When doubled, reduce each side by ~3–6 dB from single-track level.

## Compression

| Guitar Style | Ratio | Attack | Release | Gain Reduction |
|-------------|-------|--------|---------|----------------|
| Clean rhythm | 3:1–5:1 | 10–30 ms | 30–80 ms | 2–5 dB |
| Distorted rhythm | 2:1–4:1 | 20–60 ms | 40–100 ms | 1–3 dB (already compressed) |
| Lead | 4:1–8:1 | 5–20 ms | 30–80 ms | 4–8 dB |
| Acoustic | 2:1–4:1 | 10–20 ms | 40–100 ms | 2–4 dB |
| Funk | 4:1–8:1 | 1–5 ms | 20–50 ms | 5–10 dB |

**For distorted guitars:** they're often already heavily compressed by distortion itself. Use light compression just for glue.

## EQ by Genre

### Rock
- **Rhythm:** HPF @ 100 Hz, cut 300–500 Hz (−4 dB), boost 1–3 kHz (+3–4 dB), slight presence +2 dB @ 5 kHz
- **Lead:** HPF @ 100 Hz, boost 2–4 kHz (+3–6 dB), cut 300 Hz (−3 dB)

### Metal
- **Rhythm:** HPF @ 100–120 Hz, cut 300–500 Hz (−5–8 dB), boost 1.5–3 kHz (+3–5 dB), LPF @ 8–10 kHz (tighten)
- **Lead:** Boost 2–4 kHz (+4–6 dB), cut 400 Hz (−3–4 dB)

### Pop
- **Clean:** HPF @ 100 Hz, gentle high shelf +2–3 dB @ 8 kHz
- **Acoustic:** HPF @ 80 Hz, boost 1–3 kHz (+2–4 dB) for definition

### Blues
- **Lead:** HPF @ 80 Hz, boost 80–120 Hz (+2–3 dB) for warmth, boost 2–3 kHz (+3–5 dB)
- **Rhythm:** Minimal EQ, let the amp do the work

### Funk
- **Clean:** HPF @ 100 Hz, boost 1–3 kHz (+4–6 dB), high shelf +3 dB @ 8 kHz

### Country
- **Telecaster:** HPF @ 100 Hz, boost 2–4 kHz (+3–5 dB), light compression

### Jazz
- **Warm clean:** HPF @ 80 Hz, slight cut 2–3 kHz (−1–2 dB), very gentle

## Amp & Cabinet EQ Reference

| Control | Effect | Typical Setting |
|---------|--------|----------------|
| **Bass** | Low-end chunk | 3–6 (out of 10) — less for clarity |
| **Mid** | Presence, cut through mix | 5–8 — high mids = better mix fit |
| **Treble** | Brightness, edge | 5–7 |
| **Presence** | Extreme high-end | 4–7 |
| **Gain** | Distortion amount | As needed for genre |

**Cocktail chart truth:** Mids are the guitar's best friend. Scooping mids might sound good soloed, but the guitar disappears in the mix.

## Reverb & Ambience

| Genre | Reverb Type | Decay | Mix |
|-------|-------------|-------|-----|
| Rock | Room/Plate | 0.8–1.5 s | 15–25% |
| Metal | Short room | 0.5–1.0 s | 10–20% |
| Pop (clean) | Plate/Hall | 1.0–2.0 s | 20–30% |
| Blues | Spring/Room | 0.5–1.0 s | 15–25% |
| Country | Slap delay + small room | 80–120 ms delay | 10–20% |
| Ambient | Hall/Shimmer | 2.0–4.0 s | 30–50% |

## Mono Compatibility

- **Check in mono** — doubled guitars panned hard L/R will phase-cancel if not truly doubled
- **Use a correlation meter** — if correlation drops below 0, your stereo spread may cause issues in mono
- **Haas effect (delay one side 10–30 ms):** sounds wide in stereo but can collapse in mono — use carefully

## Lead vs Rhythm Balance

- **In a busy mix:** rhythm guitars should sit around −10 to −18 dB from peak
- **Lead guitar:** should sit 2–6 dB above the rhythm guitars
- **Volume automation:** ride the lead guitar volume up for solos and down for verses
- **EQ carve:** lead stands out partly by being in a slightly different frequency range (more mids, less low end)

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Muddy | Too much 300–500 Hz | Cut 300–500 Hz 3–6 dB |
| Thin/weak | Not enough low-mid or HPF too high | Lower HPF to 80 Hz, add 150–200 Hz |
| Harsh | Too much 2–5 kHz or too much treble | Cut 2–5 kHz by 2–4 dB, reduce amp treble |
| Too wide | Double tracks not tight enough | Tighten performance, use InPhase alignment |
| Not cutting through | Not enough mids or too much reverb | Boost 1–3 kHz, dry up the reverb |
| Phase issues in mono | Not true-doubled or Haas effect | Re-record double, align transients |
| Boomy, resonant | Low-mid buildup from acoustic | Cut 200–300 Hz, use dynamic EQ at resonance |

## Harmonica — Instrument Guide

## Frequency Map

| Range | Frequencies | Description |
|-------|-------------|-------------|
| Bass | 100–250 Hz | Low-end body, low octave (holes 1–4) |
| Low-mid | 250–600 Hz | Body, warmth |
| Mid | 600–2 kHz | Core tone, reed resonance |
| High-mid | 2–4 kHz | Attack, breath, articulation |
| High | 4–8 kHz | Reed buzz, breath noise |
| Air | 8–12 kHz | Sibilance, harmonics |

## EQ by Genre

### Blues
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 120–200 Hz | Bell | 1.5 | +1.5 dB | Warm body |
| 400–600 Hz | Bell | 1.5 | -2 dB | Reduce nasal-ness |
| 800–1.2 kHz | Bell | 1.0 | +2 dB | Classic blues bark |
| 2–4 kHz | Bell | 1.0 | +2.5 dB | Presence, cut through |
| 8–10 kHz | HBF | — | +1 dB | Air |

### Folk / Acoustic
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 150–250 Hz | Bell | 1.5 | +1 dB | Warmth |
| 500–700 Hz | Bell | 1.5 | -2 dB | Reduce boxiness |
| 1.5–3 kHz | Bell | 1.0 | +1.5 dB | Articulation |
| 8 kHz+ | HBF | — | +1 dB | Air |

### Rock / Pop
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 100–150 Hz | HSF | — | -2 dB | Tighten |
| 250–500 Hz | Bell | 1.8 | -3 dB | Clear mud |
| 1–2 kHz | Bell | 1.0 | +2 dB | Aggressive presence |
| 3–6 kHz | Bell | 1.0 | +2.5 dB | Cut through band |
| 10 kHz+ | HBF | — | +1 dB | Sparkle |

## Compression Settings

| Context | Ratio | Attack | Release | Gain Reduction | Notes |
|---------|-------|--------|---------|----------------|-------|
| Blues (solo/cupped) | 3:1 | 10 ms | 80 ms | 4–6 dB | Even out dynamics |
| Blues (amplified) | 4:1 | 5 ms | 50 ms | 5–8 dB | Controlled, aggressive |
| Folk (acoustic) | 2.5:1 | 15 ms | 100 ms | 3–5 dB | Natural, dynamic |
| Rock/Pop | 5:1 | 3 ms | 40 ms | 6–10 dB | Aggressive control |
| With harmonica mic | 4:1 | 5 ms | 60 ms | 5–8 dB | Tame proximity effect |

### Multi-band Options
| Band | Frequency | Ratio | Why |
|------|-----------|-------|-----|
| Low | <300 Hz | 4:1 | Control breath blasts |
| Mid | 300–3 kHz | 3:1 | Even out note dynamics |
| High | >3 kHz | 2.5:1 | Tame breath sibilance |

## Reverb

| Type | Pre-delay | Decay | Mix | Use Case |
|------|-----------|-------|-----|----------|
| Room | 10–15 ms | 0.6–1.0 s | 15–20% | Blues (intimate) |
| Hall | 20–30 ms | 1.5–2.0 s | 20–30% | Folk/solo |
| Plate | 15–20 ms | 1.0–1.5 s | 15–25% | Rock/pop |
| Spring | 5–10 ms | 0.8–1.2 s | 10–15% | Vintage blues |

## Common Issues & Solutions

### Harsh breath noise
- **Cause**: Breath hitting mic capsule
- **Fix**: De-esser at 5–8 kHz, use windscreen, cut 4–6 kHz

### Too nasal
- **Cause**: 400–700 Hz resonance
- **Fix**: Narrow cut at problem frequency (sweep 400–700 Hz), or boost 1–2 kHz

### Thin / weak
- **Cause**: Missing low-mid body
- **Fix**: Boost 150–250 Hz, reduce HPF to 80 Hz max

### Breath blasts / p pops
- **Cause**: Plosive breath pressure
- **Fix**: HPF 80–100 Hz, de-esser, or windscreen

### Feedback (amplified)
- **Cause**: Harmonica mic + amp creating feedback loop
- **Fix**: Notch filter at feedback frequency, keep HPF active

### Over-blows crackling
- **Cause**: Bent notes / over-blows create distortion
- **Fix**: Distortion pedal or amp sim with controlled gain

## Special Techniques

### Cupping (Blues)
- Hands cupped around harmonica for tone shaping
- Creates bass boost (proximity effect) — compensate with HPF
- Wah-like effect by opening/closing hands

### Harp mic (Bullet mic)
- Classic blues sound with Shure Green Bullet or JT-30
- Heavy compression (5:1), EQ boost at 1–2 kHz
- Use amp sim (Fender Twin or tweed-style)

### Tremolo / vibrato
- Created by throat modulation
- Light reverb enhances natural vibrato
- Avoid heavy compression that kills breath dynamics

### Cross-harp / 2nd position
- Playing in a different key from the harp
- Creates bluesy bent notes
- EQ: boost 800–1.2 kHz for "bark"

## Recommended Processing Chain

1. **HPF** 80–100 Hz (remove rumble, breath blasts)
2. **De-esser** 5–8 kHz (tame breath noise)
3. **EQ** cut 400–600 Hz (reduce nasal), boost 1–2 kHz (presence)
4. **Compressor** (3–4:1, 5–10 ms attack, 60–80 ms release)
5. **EQ** boost 2–4 kHz (cut through)
6. **Reverb** (room for blues, hall for folk)
7. **Limiter** −2 dB (catch breath peaks)

## Harp — Instrument Guide

## Frequency Map

| Range | Frequencies | Description |
|-------|-------------|-------------|
| Sub-bass | 30–60 Hz | Pedal notes, fundamental of lowest strings |
| Bass | 60–150 Hz | Lower register, fundamental of mid-low strings |
| Low-mid | 150–400 Hz | Mid-range body, harmonics |
| Mid | 400–2 kHz | Presence, articulation, pluck attack |
| High-mid | 2–5 kHz | Finger noise, definition |
| Air | 5–10 kHz | Sparkle, shimmer, harmonic overtones |

## EQ by Genre

### Classical / Orchestral
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 60–80 Hz | HSF | — | -3 dB | Reduce pedal resonance |
| 200–400 Hz | Bell | 1.5 | -2 dB | Reduce mud in ensemble |
| 2–4 kHz | Bell | 1.0 | +1.5 dB | Add presence if buried |
| 8–10 kHz | HBF | — | +1 dB | Air for solo passages |

### Pop / Contemporary
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 80–100 Hz | HSF | — | -2 dB | Tighten low end |
| 300–500 Hz | Bell | 1.8 | -3 dB | Clear mud |
| 3–5 kHz | Bell | 1.0 | +2 dB | Articulation |
| 10 kHz+ | HBF | — | +1.5 dB | Shimmer |

### Celtic / Folk
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 100–150 Hz | Bell | 1.5 | +1 dB | Warmth |
| 400–600 Hz | Bell | 1.5 | -2 dB | Reduce boxiness |
| 2–4 kHz | Bell | 1.0 | +1.5 dB | Clarity |
| 8 kHz+ | HBF | — | +1 dB | Sparkle |

## Compression Settings

| Context | Ratio | Attack | Release | Gain Reduction | Notes |
|---------|-------|--------|---------|----------------|-------|
| Classical (solo) | 2:1 | 20 ms | 150 ms | 2–4 dB | Gentle, preserve dynamics |
| Classical (ensemble) | 3:1 | 15 ms | 100 ms | 3–5 dB | Tame dynamic range |
| Pop/Contemporary | 3–4:1 | 10 ms | 80 ms | 4–6 dB | More controlled |
| Folk | 2.5:1 | 15 ms | 120 ms | 3–5 dB | Keep natural feel |

### Multi-band Options
| Band | Frequency | Ratio | Why |
|------|-----------|-------|-----|
| Low | <200 Hz | 3:1 | Control pedal resonance |
| Mid | 200–2 kHz | 2.5:1 | Even out dynamics |
| High | >2 kHz | 2:1 | Gentle presence control |

## Reverb

| Type | Pre-delay | Decay | Mix | Use Case |
|------|-----------|-------|-----|----------|
| Hall | 30–50 ms | 2.5–3.5 s | 25–35% | Classical/orchestral |
| Room | 15–25 ms | 1.0–1.5 s | 15–25% | Pop/contemporary |
| Plate | 20–30 ms | 1.5–2.0 s | 20–30% | Celtic/folk |
| Chamber | 25–40 ms | 1.8–2.5 s | 20–30% | Adding depth |

## Common Issues & Solutions

### Muddy low end
- **Cause**: Pedal resonance + room modes
- **Fix**: HSF at 60 Hz, cut 200–400 Hz with narrow Q

### Harsh pluck attack
- **Cause**: Finger/pick transient too prominent
- **Fix**: Cut 3–5 kHz with bell, use slower attack on compressor (20–30 ms)

### Lacks shimmer
- **Cause**: Missing high-frequency content in dense mix
- **Fix**: HBF shelf boost at 8–10 kHz, or add subtle harmonic exciter

### Pedal noise
- **Cause**: Mechanical noise from pedals during register changes
- **Fix**: Gate set to 40–50 ms attack, or manual volume automation

### Too resonant in room
- **Cause**: Natural resonance overlaps with mix frequencies
- **Fix**: Narrow cuts at problematic frequencies (sweep 150–500 Hz)

### Glissing artifacts
- **Cause**: Harp glissandos create transient peaks
- **Fix**: Limiter with fast attack (1 ms), catch peaks at −3 dB

## Special Techniques

### Finger damping
- Record closer to soundboard for less string resonance
- Use felt or cloth damping for percussive effects

### Close vs ambient miking
- **Close (6–12")**: More articulation, less room — better for pop
- **Ambient (3–6')**: Natural blend, more body — better for classical

### Ethereal harp
- Heavy reverb (hall, 3+ seconds)
- Reverse reverb tails
- Layered with octave doubler or pitch shifter (+12 dB subtle)

### Percussive harp
- Muted strings + close miking
- Compressor with fast attack (5 ms), high ratio (6:1)
- Add transient shaper for attack

## Recommended Processing Chain

1. **HPF** 40 Hz (remove subsonic rumble)
2. **EQ** cuts in mud range (200–500 Hz)
3. **Compressor** (gentle, 2–3:1 ratio)
4. **EQ** boosts for presence (2–4 kHz) and air (8–10 kHz)
5. **Reverb** (hall or room depending on context)
6. **Limiter** −3 dB (catch peaks from glissandos)

## KICK DRUM — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub weight** | 40–80 Hz | Thump, chest punch | Boost for power, roll off below 30 Hz |
| **Body** | 80–250 Hz | Fullness, warmth | Boost for roundness, cut if muddy |
| **Low-mid** | 250–500 Hz | Boxiness, cardboard | Cut 3–6 dB if it sounds boxy |
| **Attack/click** | 1.5–5 kHz | Beater attack, presence | Boost for definition, cut if harsh |
| **Air** | 5–10 kHz | Stick/slap noise | Light shelf boost for modern genres |

## Compression

| Genre | Ratio | Attack | Release | Gain Reduction |
|-------|-------|--------|---------|----------------|
| Rock | 4:1–8:1 | 1–5 ms | 50–100 ms | 3–6 dB |
| Pop | 3:1–6:1 | 3–10 ms | 30–80 ms | 2–4 dB |
| EDM | 2:1–4:1 | 0–2 ms | 10–30 ms | 1–3 dB (more punch) |
| Metal | 6:1–10:1 | 1–3 ms | 50–120 ms | 4–8 dB |
| Jazz | 2:1–3:1 | 10–30 ms | 80–150 ms | 1–3 dB |

## EQ by Genre

- **Rock/Metal:** Boost 60–80 Hz (+3–6 dB), cut 300–500 Hz (−3 dB), boost 2–4 kHz (+2–4 dB)
- **Pop:** Boost 50–60 Hz (+3–4 dB), boost 3–5 kHz (+2–3 dB), gentle high shelf +2 dB @ 8 kHz
- **EDM:** Boost 40–60 Hz (+4–8 dB), cut 200–400 Hz (−3 dB), boost 4–6 kHz (+2–4 dB)
- **Hip-Hop:** Boost 50–60 Hz (+3–6 dB), cut 300 Hz (−3 dB), boost 2–3 kHz (+2–3 dB)
- **Reggaeton:** Boost 45–55 Hz (+3–5 dB), cut 400 Hz (−2–3 dB), boost 2–4 kHz (+2–3 dB)
- **Jazz/Classical:** Gentle or no EQ—let the natural sound through

## Transient Shaping

- **Short attack (< 1 ms) + medium sustain:** Increases punch (good for EDM, hip-hop)
- **Slow attack (10–30 ms) + long sustain:** More body, less beater (good for jazz, ballads)
- Use transient shaper before compression to control envelope independently

## Layering

Common approach for modern genres:
1. **Sub layer** (sine wave, 40–60 Hz) — provides weight
2. **Mid layer** (acoustic/electronic kick) — provides body
3. **Top layer** (short attack sound) — provides click

Gate/filter each layer so they don't overlap unnecessarily. Align transients within 0–3 ms.

## Sidechain to Bass

- **SC compress** the bass from the kick at 60–120 Hz
- **Ratio:** 3:1–6:1
- **Attack:** 1–5 ms, **Release:** 30–60 ms
- **Gain reduction:** 2–6 dB on bass
- Alternative: use sidechain EQ (dynamic EQ on bass at kick's fundamental)

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| No punch | Too much low-mid (200–500 Hz) | Cut 200–400 Hz, boost 60–80 Hz |
| Flabby | Too much sub-sustain | Gate or volume envelope to tighten |
| Click but no weight | Too much 3–5 kHz, not enough 60 Hz | Add 60 Hz boost, reduce 3–5 kHz |
| Muddy in mix | Clashing with bass fundamental | Sidechain or EQ carve at bass freq |
| Thin | Not enough body or too much HPF | Lower HPF to 30 Hz, add 100–200 Hz |
| Too loud in mix | Level too high for genre | Check RMS vs. bass and snare balance |

## Kick-to-Mix Balance

- **Kick RMS** should be roughly equal to bass RMS in most genres
- In EDM/Hip-Hop: kick may be 1–3 dB louder than bass
- In Rock/Metal: kick and snare should have similar RMS levels
- Check in context: if you can't feel the kick, add 40–60 Hz or increase level 1–2 dB

## Mandolin — Instrument Guide

## Frequency Map

| Range | Frequencies | Description |
|-------|-------------|-------------|
| Bass | 80–200 Hz | Low-end body, resonance |
| Low-mid | 200–500 Hz | Body, warmth |
| Mid | 500–2 kHz | Core tone, presence |
| High-mid | 2–5 kHz | Attack, pick noise, definition |
| High | 5–10 kHz | String shimmer, fret noise |
| Air | 10–15 kHz | Sparkle, harmonic overtones |

## EQ by Genre

### Bluegrass / Folk
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 100–150 Hz | Bell | 1.5 | +1 dB | Warm body |
| 300–500 Hz | Bell | 1.5 | -2 dB | Reduce boxiness |
| 1–2 kHz | Bell | 1.0 | +1.5 dB | Presence |
| 4–6 kHz | Bell | 1.0 | +2 dB | Cut through mix |
| 10 kHz+ | HBF | — | +1.5 dB | Sparkle |

### Classical / Italian
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 120–180 Hz | Bell | 1.5 | +1 dB | Warmth |
| 400–600 Hz | Bell | 1.5 | -2.5 dB | Reduce nasal quality |
| 2–3 kHz | Bell | 1.0 | +1 dB | Articulation |
| 8–12 kHz | HBF | — | +1 dB | Air |

### Rock / Pop
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 80–100 Hz | HSF | — | -3 dB | Tighten low end |
| 200–400 Hz | Bell | 1.8 | -3 dB | Clear mud |
| 2.5–5 kHz | Bell | 1.0 | +2.5 dB | Aggressive presence |
| 10 kHz+ | HBF | — | +2 dB | Shimmer |

## Compression Settings

| Context | Ratio | Attack | Release | Gain Reduction | Notes |
|---------|-------|--------|---------|----------------|-------|
| Bluegrass solo | 2.5:1 | 15 ms | 100 ms | 3–5 dB | Keep dynamics |
| Bluegrass ensemble | 3:1 | 10 ms | 80 ms | 4–6 dB | Tame transients |
| Classical | 2:1 | 20 ms | 120 ms | 2–4 dB | Gentle, natural |
| Rock/Pop | 4:1 | 5 ms | 60 ms | 5–8 dB | Aggressive, controlled |
| Fast tremolo | 5:1 | 2 ms | 40 ms | 6–10 dB | Even out tremolo dynamics |

## Reverb

| Type | Pre-delay | Decay | Mix | Use Case |
|------|-----------|-------|-----|----------|
| Room | 10–20 ms | 0.8–1.2 s | 15–25% | Bluegrass (natural) |
| Hall | 25–40 ms | 1.8–2.5 s | 20–30% | Classical/solo |
| Plate | 15–25 ms | 1.2–1.8 s | 15–25% | Rock/pop |
| Chamber | 20–30 ms | 1.5–2.0 s | 20–30% | Adding depth |

## Common Issues & Solutions

### Harsh / piercing
- **Cause**: Excessive pick attack, especially with heavy picking
- **Fix**: Cut 3–6 kHz with narrow Q, use softer pick or fingerstyle

### Nasal tone
- **Cause**: Resonant frequency around 400–800 Hz
- **Fix**: Narrow cut at problem frequency (sweep 400–800 Hz)

### Thin / lacks body
- **Cause**: Missing low-mid content
- **Fix**: Boost 100–200 Hz, cut some highs to balance

### Tremolo not even
- **Cause**: Uneven picking dynamics
- **Fix**: Faster compressor (2 ms attack), higher ratio (5:1), lower threshold

### Buzzy string noise
- **Cause**: Fret buzz or high action
- **Fix**: Cut 2–3 kHz, de-esser at 5–7 kHz, check instrument setup

### Stident double-stops
- **Cause**: Two strings hit simultaneously create harsh transient
- **Fix**: Limiter 2:1 with 1 ms attack, or multi-band limiting at 3–6 kHz

## Special Techniques

### Double tracking
- Record two takes, pan L/R
- Slight timing differences create natural width
- Or use stereo doubler with 12–18 ms delay

### Mandolin chop (Bluegrass)
- Short, percussive chord on 2 & 4
- Fast compressor (2 ms attack), high ratio (6:1)
- Gate with fast release to tighten

### Arpeggios
- Slight compression (3:1, 10 ms attack) to even out
- Gentle reverb (room, 15% mix)

### Tremolo picking
- Heavy compression for consistency (5:1, 2 ms attack)
- Avoid excessive reverb — keep clarity

## Recommended Processing Chain

1. **HPF** 60–80 Hz (remove rumble)
2. **EQ** cut 300–500 Hz (reduce boxiness)
3. **Compressor** (3:1, 10–15 ms attack, 80 ms release)
4. **EQ** boost 2–5 kHz (presence) + 10 kHz (air)
5. **Reverb** (room for bluegrass, hall for solo)
6. **Limiter** −2 dB (catch pick transients)

## ORGAN — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub** | 20–50 Hz | Pedal notes, rumble | HPF at 25–40 Hz unless pedal is used |
| **Bass** | 50–200 Hz | Fundamental pedal tones | Keep for weight, cut 2–4 dB if muddy |
| **Low-mid** | 200–600 Hz | Mud, cloudiness | Cut 4–6 dB — organ's biggest problem zone |
| **Mid** | 600 Hz–2 kHz | Body, character | Keep, organ lives here |
| **Upper mid** | 2–5 kHz | Presence, cut-through | Boost for clarity, cut for background |
| **Air** | 5–16 kHz | Brightness, shimmer | High shelf for modern sound |

## Organ Types

| Type | Character | Best For | Mix Approach |
|------|-----------|----------|-------------|
| **Hammond B3** | Warm, growly, rich harmonics | Gospel, blues, rock, jazz | Leslie sim, cut 300–500 Hz, presence 2–4 kHz |
| **Hammond M3** | Brighter, less bass | Rock, indie | Similar to B3, less need for HPF |
| **Farfisa** | Bright, reedy, thin | 60s/70s rock, psychedelic | Presence boost, less low end |
| **Vox Continental** | Buzzy, aggressive | 60s rock, garage | Mid-forward, less sub |
| **Pipe organ** | Massive, deep, grand | Classical, cinematic | Full frequency, minimal processing |
| **Chord organ** | Soft, reedy | Vintage pop, easy listening | HPF higher, gentle |

## Leslie Speaker Simulation

| Setting | Slow | Fast | Effect |
|---------|------|------|--------|
| **Speed** | 0.5–1 Hz | 5–8 Hz | Slow: warm, fast: shimmering |
| **Horn balance** | 50/50 | 50/50 | Balance between rotor and horn |
| **Mic distance** | Near = more present, Far = more ambient | Vary for feel |

## Compression

| Style | Ratio | Attack | Release | GR |
|-------|-------|--------|---------|-----|
| Gospel | 3:1–5:1 | 10–30 ms | 40–80 ms | 3–6 dB |
| Rock | 3:1–4:1 | 15–30 ms | 50–100 ms | 2–5 dB |
| Jazz | 2:1–3:1 | 20–40 ms | 60–120 ms | 2–4 dB |
| Background | 4:1–6:1 | 10–20 ms | 30–60 ms | 4–8 dB |

## EQ by Genre

- **Gospel:** HPF 50 Hz, cut 300–500 Hz (−4–6 dB), presence 2–4 kHz (+3–5 dB)
- **Blues:** HPF 60 Hz, cut 400 Hz (−3 dB), boost 2–3 kHz (+2–4 dB)
- **Rock:** HPF 80 Hz, cut 400–500 Hz (−4–6 dB), boost 2–4 kHz (+3–5 dB)
- **Jazz:** HPF 50 Hz, warm 200 Hz (+2 dB), cut 400 Hz (−3 dB), gentle presence

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Muddy (most common issue) | Cut 300–500 Hz aggressively (4–8 dB) |
| Too boomy/low-end heavy | HPF at 50–80 Hz, cut 100–200 Hz 3–4 dB |
| Harsh | Cut 3–5 kHz 2–3 dB |
| Leslie effect too extreme | Slow down rotation speed, reduce stereo spread |
| Loses presence in mix | Boost 2–4 kHz 3–5 dB |

## PAD / SYNTH PAD — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub** | 20–60 Hz | Foundation, rumble | HPF at 30–50 Hz unless sub is needed |
| **Body** | 60–250 Hz | Warmth, fullness | Cut if muddy, keep if supporting |
| **Low-mid** | 250–800 Hz | Mud, cloudiness | Cut 3–6 dB — pads easily cloud the mix |
| **Mid** | 800 Hz–3 kHz | Presence, character | Cut to leave space for vocals/leads |
| **Presence** | 3–8 kHz | Brightness, shimmer | Boost for airy pads |
| **Air** | 8–20 kHz | Sparkle, openness | High shelf +2–4 dB for modern pads |

## Pad Types

| Type | Character | Best For | Mix Approach |
|------|-----------|----------|-------------|
| **Analog warm** | Round, smooth, filter | House, deep house, lo-fi | HPF 200 Hz, wide stereo, light comp |
| **Digital/spectral** | Glassy, evolving, bright | Ambient, EDM, pop | Wide stereo, reverb, light sidechain |
| **Organ pad** | Sustained, warm | Gospel, soul, rock | Moderate width, cut 300–500 Hz |
| **String pad** | Orchestral, lush | Pop, cinematic | Wide, hall reverb, light comp 2:1 |
| **Vocal pad** | Ethereal, breathy | Ambient, pop | Heavy reverb, wide, layered |
| **Bass pad** | Subby, foundational | Ambient, cinematic | Mono sub, stereo high frequencies |

## Sidechain to Kick

Pad sidechain is essential in EDM, house, and pop:
- **Ratio:** 3:1–8:1
- **Attack:** 1–5 ms
- **Release:** 100–300 ms (match tempo)
- **GR:** 3–10 dB

Creates rhythmic "breathing" effect. For subtle pumping, use 3:1 ratio, 3–5 dB GR.

## Mixing Principles

| Goal | Action |
|------|--------|
| Create space | HPF at 200–400 Hz to leave room for kick/bass |
| Add atmosphere | Wide stereo, 25–50% reverb (hall/shimmer 2–4 s) |
| Avoid mud | Cut 300–500 Hz aggressively (4–8 dB) |
| Keep out of vocal way | Cut 1–4 kHz 2–4 dB dynamically or sidechain |
| Stay background | Keep level 6–12 dB below main elements |

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Muddy/washy | HPF higher (200–400 Hz), cut 300–500 Hz 4–6 dB |
| Hides vocal | Sidechain pad from vocal or EQ carve 2–4 kHz |
| Too wide/phasey | Check in mono, reduce stereo spread if issues |
| No movement | Add filter automation, LFO modulation, or sidechain from kick |
| Too synthetic | Add saturation/tape warmth slightly |

## PERCUSSION — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Low** | 40–200 Hz | Body, thump (congas, toms) | HPF at 60–150 Hz depending on instrument |
| **Low-mid** | 200–500 Hz | Mud, boxiness | Cut 3–5 dB for clarity |
| **Mid** | 500–2 kHz | Attack, slap (congas, bongos) | Boost for definition |
| **Upper** | 2–8 kHz | Snap, skin, stick sound | Boost for clarity and cut-through |
| **Air** | 8–16 kHz | Shimmer (shakers, cymbals, triangles) | Boost for brightness |

## Percussion Instruments

| Instrument | Range | HPF | Character | Treatment |
|-----------|-------|-----|-----------|-----------|
| **Congas** | 100 Hz–4 kHz | 80–100 Hz | Warm, thumpy, slap | Comp 4:1–6:1, 3–5 dB GR |
| **Bongos** | 200 Hz–5 kHz | 150–200 Hz | Bright, tight | Comp 3:1–4:1 |
| **Timbales** | 150 Hz–6 kHz | 100–150 Hz | Bright, cutting | Light comp, presence boost |
| **Djembe** | 80 Hz–4 kHz | 60–80 Hz | Deep slap, bass | Comp 4:1 |
| **Shaker** | 4–16 kHz | 500 Hz–1 kHz | Crisp sizzle | Gate, light compression |
| **Tambourine** | 3–16 kHz | 300–500 Hz | Jangling | Gate, boost 8–10 kHz |
| **Triangle** | 5–20 kHz | 2–4 kHz | Pure high shimmer | Very light comp |
| **Maracas** | 3–12 kHz | 500 Hz | Raspy shaker | Gate, boost 5–8 kHz |
| **Cabasa** | 3–14 kHz | 500 Hz | Sizzling, rhythmic | Gate, boost 6–10 kHz |
| **Cowbell** | 1–5 kHz | 500 Hz | Cutting, metallic | Comp 4:1, boost 2–4 kHz |
| **Claves** | 1–6 kHz | 500 Hz | Sharp, wooden | Dry, no reverb, boost 2–4 kHz |
| **Guiro** | 2–10 kHz | 300–500 Hz | Rasping | Gate, boost 4–8 kHz |
| **Agogo bells** | 1–6 kHz | 500 Hz | High-pitched, bell-like | Boost 2–4 kHz |
| **Djembe slap** | 200 Hz–5 kHz | 100 Hz | Bright, crack | Comp 4:1–6:1 |

## Percussion Mixing Principles

1. **HPF aggressively** — Most percussion doesn't need low frequencies. HPF at 100–500 Hz depending on instrument
2. **Pan for separation** — Spread percussion across the stereo field for clarity
3. **Gate or automate** — Keep percussion tight, no unwanted bleed
4. **Use reverb sparingly** — Short room or plate for most percussion, dry for latin/afro styles
5. **Level carefully** — Percussion should add energy without overpowering core instruments

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Too many percussion elements clash | Pan them apart, cut overlapping frequencies |
| Muddy low end from percussion | HPF everything at 100–300 Hz |
| Harsh or piercing | Cut 4–8 kHz on multiple percussion tracks |
| Lacks definition | Boost presence 3–5 kHz on each percussion group |
| Too dry/no energy | Add short room reverb (0.3–0.8 s) to percussion bus |

## PIANO — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub/low** | 30–100 Hz | Pedal resonance, rumble | HPF at 40–60 Hz to remove rumble |
| **Low body** | 100–300 Hz | Warmth, fundamental notes | Keep for fullness, cut if muddy |
| **Low-mid** | 300–800 Hz | Boxiness, mud | Cut 3–5 dB if muddy (common in dense mixes) |
| **Mid presence** | 1–4 kHz | Attack, definition | Boost for clarity and cut-through |
| **Presence** | 4–8 kHz | Brightness, hammer noise | Boost for modern sound, cut if harsh |
| **Air** | 8–16 kHz | Sparkle, openness | Gentle shelf boost for air |

## Piano Types

| Type | Character | Best For | Mix Approach |
|------|-----------|----------|-------------|
| **Grand piano** | Rich, full, dynamic | Classical, jazz, pop ballads | Full frequency, gentle comp, stereo width |
| **Upright** | Warmer, less sustain | Rock, indie, lo-fi | EQ to taste, moderate comp |
| **Electric (Rhodes)** | Bell-like, warm, smooth | R&B, soul, jazz, pop | Light comp, chorus effect, gentle EQ |
| **Electric (Wurlitzer)** | Biting, growly, dynamic | Rock, funk, soul | Comp 3:1–4:1, presence 2–4 kHz |
| **Synth piano** | Clean, digital | Pop, EDM, modern | Clean processing, often layered |
| **Honky-tonk** | Bright, detuned | Country, blues, ragtime | Boost 2–4 kHz, slight detune |

## Compression

| Style | Ratio | Attack | Release | Gain Reduction |
|-------|-------|--------|---------|----------------|
| Classical | 1.5:1–2.5:1 | 30–60 ms | 100–300 ms | 1–3 dB |
| Jazz | 2:1–3:1 | 20–40 ms | 80–200 ms | 2–4 dB |
| Pop/Rock | 3:1–5:1 | 10–30 ms | 50–100 ms | 3–6 dB |
| Funk/Rhodes | 4:1–6:1 | 5–15 ms | 30–60 ms | 4–7 dB |

## EQ by Genre

- **Classical:** Gentle HPF at 40 Hz, very gentle presence 2–3 kHz (+1–2 dB), natural
- **Jazz:** HPF at 50 Hz, cut 300–400 Hz (−2–3 dB), boost 2–3 kHz (+2–3 dB)
- **Pop:** HPF at 60 Hz, cut 400–500 Hz (−3–4 dB), boost 3–5 kHz (+2–4 dB), air shelf +2 dB
- **Rock:** HPF at 80 Hz, cut 400–500 Hz (−4 dB), boost 2–4 kHz (+3–5 dB)
- **Lo-fi:** HPF at 80 Hz, cut 2–4 kHz, tape saturation, LPF at 8–10 kHz

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Muddy in mix | Too much 200–500 Hz | Cut 300–500 Hz 3–5 dB |
| Thin/weak | Not enough 100–300 Hz or too much HPF | Lower HPF, add 100–200 Hz |
| Harsh/brittle | Too much 3–5 kHz | Cut 1–2 dB |
| Pedal noise/rumbling | Too much sub-low | HPF at 40–60 Hz |
| Too wide in stereo | Hard panned L/R on grand | Reduce width to 70–80% for mono compatibility |
| Lost in mix | Not enough presence | Boost 2–4 kHz 2–4 dB |

## SAXOPHONE — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Low** | 80–200 Hz | Body, warmth, chest | Keep for fullness |
| **Low-mid** | 200–600 Hz | Mud, boxiness | Cut 3–5 dB for clarity |
| **Mid** | 1–3 kHz | Presence, cut-through | Boost for lead, cut for background |
| **Upper** | 3–6 kHz | Brightness, edge | Boost for modern sound, cut if harsh |
| **Air** | 6–12 kHz | Shimmer, reed noise | Gentle shelf |

## Saxophone Types

| Type | Range | Character | Best For |
|------|-------|-----------|----------|
| **Soprano** | 200 Hz–7 kHz | Bright, piercing | Jazz leads, classical |
| **Alto** | 140 Hz–6 kHz | Versatile, bright | Jazz, pop, rock |
| **Tenor** | 110 Hz–5 kHz | Warm, rich, full | Jazz, blues, R&B, rock |
| **Baritone** | 70 Hz–3 kHz | Deep, powerful | Funk, jazz big band |

## Compression

| Style | Ratio | Attack | Release | GR |
|-------|-------|--------|---------|-----|
| Jazz | 2:1–3:1 | 15–30 ms | 50–100 ms | 2–4 dB |
| Pop/Rock | 3:1–5:1 | 10–20 ms | 40–80 ms | 3–6 dB |
| Funk | 4:1–6:1 | 5–15 ms | 30–60 ms | 4–7 dB |

## EQ by Genre

- **Jazz:** HPF 80 Hz, warm 200–300 Hz (+2 dB), presence 2–3 kHz (+2–3 dB)
- **Pop:** HPF 100 Hz, cut 400 Hz (−3 dB), boost 3–5 kHz (+3–4 dB)
- **Funk:** HPF 100 Hz, cut 400 Hz (−3 dB), boost 3–5 kHz (+4–6 dB)
- **Blues:** HPF 80 Hz, warm 200 Hz (+2–3 dB), mid 2–3 kHz (+2–3 dB)

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Harsh/biting | Cut 3–5 kHz 2–4 dB |
| Muddy | Cut 300–500 Hz 3–5 dB |
| Thin/weak | Boost 100–200 Hz, lower HPF |
| Lost in mix | Boost 2–3 kHz presence 2–4 dB |
| Too much breath noise | Gentle LPF at 10–12 kHz |

## SNARE DRUM — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Body/fatness** | 150–300 Hz | Weight, fullness | Boost for thicker snare, cut if muddy |
| **Boxiness** | 300–600 Hz | Cardboard, cheap | Cut 3–6 dB if it sounds boxy |
| **Low-mid** | 600–1000 Hz | Honk, nasal | Cut gently if honky |
| **Crack/snap** | 1–3 kHz | Attack, presence | Boost for definition and projection |
| **Sizzle** | 5–10 kHz | Snare wires, air | Boost for brightness and excitement |
| **Air** | 10–16 kHz | Sparkle | Gentle high shelf for modern pop |

## Snare Types & Tuning

| Snare Type | Typical Tuning | Character | Best For |
|------------|---------------|-----------|----------|
| **Deep (6.5")** | Medium-low | Fat, warm body | Rock, metal, hard rock |
| **Standard (5–5.5")** | Medium | Balanced | Pop, rock, R&B |
| **Piccolo (3–4")** | High | Bright, crack | Funk, pop, fusion |
| **Steel** | Medium-high | Bright, cutting | Rock, metal, live |
| **Wood** | Medium | Warm, round | Jazz, classic rock |

## Compression

| Genre | Ratio | Attack | Release | Gain Reduction |
|-------|-------|--------|---------|----------------|
| Rock | 4:1–8:1 | 1–5 ms | 50–150 ms | 4–8 dB |
| Pop | 3:1–6:1 | 3–10 ms | 30–100 ms | 2–5 dB |
| Metal | 6:1–10:1 | 1–3 ms | 80–200 ms | 5–10 dB |
| Hip-Hop | 4:1–8:1 | 5–15 ms | 40–80 ms | 3–6 dB |
| Jazz | 2:1–3:1 | 10–30 ms | 80–200 ms | 1–3 dB |

## EQ by Genre

- **Rock:** Boost 200–250 Hz (+2–4 dB), cut 400–500 Hz (−3 dB), boost 2–3 kHz (+3–5 dB)
- **Pop:** Boost 180–220 Hz (+2–3 dB), boost 3–4 kHz (+3–4 dB), air shelf +2 dB @ 10 kHz
- **Metal:** Cut 300–500 Hz (−4–6 dB), boost 2–4 kHz (+4–6 dB), boost 150 Hz (+2–3 dB)
- **Hip-Hop:** Boost 200–250 Hz (+3–5 dB), cut 400 Hz (−3 dB), boost 2–3 kHz (+2–3 dB)
- **Jazz:** Minimal EQ, gentle presence boost 2–3 kHz (+1–2 dB)

## Reverb for Snare

| Genre | Reverb Type | Pre-delay | Decay | Mix |
|-------|-------------|-----------|-------|-----|
| Rock | Room/Plate | 10–30 ms | 0.8–1.5 s | 20–30% |
| Pop | Plate/Hall | 15–40 ms | 1.0–2.0 s | 15–25% |
| Ballad | Hall | 20–50 ms | 1.5–3.0 s | 25–40% |
| Metal | Room | 5–15 ms | 0.5–1.0 s | 15–20% |
| Jazz | Room/Live | 5–20 ms | 0.6–1.2 s | 20–30% |

**Tip:** Gate the reverb return for a tight gated reverb sound (80s rock, pop).

## Layering

- **Low layer** (200–300 Hz focused): adds body/fatness
- **Mid layer** (main snare): provides crack and body
- **Top layer** (clap or rimshot): adds attack and sizzle

Line up transients within 0–2 ms for maximum impact.

## Snare-to-Kick Relationship

- Snare should sit slightly behind or equal to kick in perceived volume
- In rock/metal: snare and kick at similar RMS levels
- In pop/hip-hop: snare may be 1–2 dB quieter than kick
- Check snare ring: if it rings too long, gate or use transient shaper

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Thin/weak | Not enough 200–300 Hz | Add body boost, reduce HPF |
| Harsh | Too much 3–5 kHz | Cut 3–5 kHz by 2–4 dB |
| Flat/small | Not enough crack | Boost 2–3 kHz, shorten attack on compressor |
| Ringing overtone | Snare needs dampening | Gate more aggressively, EQ notch at ring freq |
| Boxy | Too much 400–600 Hz | Cut 400–600 Hz by 3–6 dB |
| Lost in mix | Too much reverb or not enough snap | Dry up, boost 2 kHz |

## SOUND FX / TRANSITIONS — Mixing Guide

## FX Types & Roles

| FX Type | Purpose | Placement | Level |
|---------|---------|-----------|-------|
| **Riser** | Build tension before drop/chorus | Wide/automated | Rising from −∞ to −6 dB |
| **Downlifter** | Landing/reset after drop | Wide/process | Descending from −6 to −∞ dB |
| **Impact** | Punch on beat 1 of section | Center, short | Loud (−6 to −3 dB peak) |
| **Reverse cymbal** | Swell into section | Wide | Moderate to loud |
| **White noise sweep** | Build-up, transitional | Wide | Moderate |
| **Cymbal crash** | Section accent | Slightly off-center | Moderate |
| **Rim shot/click** | Accent beats | Center or slight | Moderately loud |
| **Tonal FX (synth riser)** | Harmonic build-up | Center + stereo | Moderate |
| **Vocal chop/stutter** | Texture, rhythm | Center | Moderate |
| **Sub hit/brown note** | Low-end thump | Mono, center | Loud but blends with kick |
| **Glitch/stutter** | Rhythmic interruption | Center | Moderate |
| **Reverse reverb** | Swell into vocal/guitar | Follows source | Moderate |
| **Tape stop** | Slow-down effect | Center, follows mix | Blends with mix |
| **Ear candy** | Incidental decoration | Any, pan for interest | Low to moderate |

## Frequency EQ by FX Type

| FX Type | HPF | Emphasis | Reverb |
|---------|-----|----------|--------|
| **Riser** | 200–500 Hz (rising) | 3–8 kHz | Long reverb 2–4 s |
| **Impact** | 50–100 Hz | 50–100 Hz + 2–4 kHz | Short room |
| **Reverse cymbal** | 300 Hz | 5–10 kHz | Reverb on reverse |
| **White noise** | 200–500 Hz | 3–16 kHz | Gate |
| **Sub hit** | 20 Hz | 30–60 Hz | None (keep tight) |

## Transition Workflow

### 8-Bar Build (Typical EDM/Pop)

| Bar | What Happens | Automation |
|-----|-------------|------------|
| **−8 to −5** | Normal mix | Steady |
| **−4** | Add percussion loops, open hats | Slight high rise on master |
| **−3** | Introduce snare roll | Increase reverb send |
| **−2** | Cut bass, add riser | HPF rising, reverb increasing |
| **−1** | Snare 16th notes, riser peaks | Filter sweep, volume rise to −6 dB |
| **Drop (0)** | All elements back + impact | Low end back, impact hit on 1 |

### 4-Bar Build (Faster)

| Bar | Action |
|-----|--------|
| **−4** | Normal mix |
| **−2** | Add riser, increase HPF on master |
| **−1** | Cut low end, snare roll, riser peaks |
| **Drop** | Impact + full low end |

## Automation Tips

| Parameter | FX | Automation Shape |
|-----------|-----|-----------------|
| **Volume** | Risers, impacts | Exponential rise, sharp cut |
| **HPF** | Build tension | Rising filter during build |
| **Reverb** | Create space to drop | Increase before drop, cut at drop |
| **Pan** | Wide risers | Automated pan spread |
| **Delay feedback** | Buildup | Increase feedback before drop |

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Too many FX clutter the mix | Use fewer, more impactful FX |
| FX too loud | Reduce FX bus by 2–4 dB |
| Build-up has no impact | Cut low end just before drop for contrast |
| Risers too harsh | LPF riser at 8–10 kHz |
| Transitions feel forced | Let FX breathe naturally, follow arrangement energy |
| FX eat headroom | Sidechain FX from kick or use volume automation to duck them |

## STRINGS — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub** | 30–60 Hz | Rumble, double bass lowest notes | HPF at 40–50 Hz |
| **Low body** | 80–200 Hz | Warmth, fullness | Keep, but watch for mud |
| **Low-mid** | 200–600 Hz | Boxiness, mud | Cut 3–5 dB in busy mixes |
| **Mid** | 1–4 kHz | Presence, bow noise | Boost for definition, cut if scratchy |
| **Presence** | 5–8 kHz | Brightness, string detail | Gentle boost for clarity |
| **Air** | 8–16 kHz | Air, openness | Gentle shelf for realism |

## String Types

| Type | Range | Character | Best For |
|------|-------|-----------|----------|
| **Violin** | 200 Hz–8 kHz | Bright, singing, expressive | Lead lines, high harmonies |
| **Viola** | 130 Hz–5 kHz | Warm, darker than violin | Inner harmonies, padding |
| **Cello** | 65 Hz–4 kHz | Rich, warm, expressive | Bass lines, melodic parts |
| **Double bass** | 40 Hz–2 kHz | Deep, fundamental | Orchestral bass |

## Mixing Approach

| Use | Processing | Reverb |
|-----|-----------|--------|
| **Lead line** | Comp 2:1–3:1, presence boost | Hall 1.5–2.5 s, 20–30% |
| **Pad/atmosphere** | Light comp, wide stereo | Hall 2.0–4.0 s, 30–50% |
| **Section (full string ensemble)** | Gentle bus comp, HPF at 80 Hz | Hall 1.5–2.5 s, 20–30% |
| **Solo instrument** | Minimal processing, natural | Room or hall 1.0–2.0 s |

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Scratchy/harsh | Cut 2–4 kHz 2–4 dB, add tape saturation |
| Muddy | Cut 300–500 Hz 3–5 dB |
| Thin | Add 100–200 Hz, lower HPF |
| Too wide (loss of center) | Keep section centered, spread only divisi parts |
| Synthetic/unnatural sound | Add hall reverb, reduce compression, use real samples |

## SYNTH (LEAD / BASS / ARP) — Mixing Guide

## Synth Roles

| Role | Character | Frequency Focus | Mix Placement |
|------|-----------|----------------|---------------|
| **Lead** | Melodic, cutting | 500 Hz–5 kHz | Center or slightly off |
| **Bass** | Subby, deep | 30–150 Hz | Mono, center |
| **Arpeggio** | Rhythmic, repeating | 200 Hz–4 kHz | Spread L/R or centered |
| **Pluck** | Percussive, short decay | 500 Hz–4 kHz | Spread or centered |
| **FX/riser** | Transitional, sweeping | Varies | Wide, automated |
| **Wobble/growl** | Modulated, aggressive | 100–800 Hz | Center with stereo mod |

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub** | 20–80 Hz | Foundation (bass/lead sub) | Mono, center, HPF if not needed |
| **Body** | 80–300 Hz | Warmth, fullness | Cut if competing with bass |
| **Low-mid** | 300–800 Hz | Mud, cloudiness | Cut 3–6 dB to clear mix space |
| **Mid** | 800 Hz–3 kHz | Character, melody | Protect vocal space (2–3 kHz) |
| **Upper** | 3–8 kHz | Presence, aggression | Cut if harsh, boost for cut-through |
| **Air** | 8–20 kHz | Shimmer, sheen | High shelf for modern sound |

## Processing

| Synth Type | Compression | EQ | Width |
|-----------|-------------|-----|-------|
| **Lead** | Comp 3:1–5:1, 2–4 dB GR | HPF 150 Hz, presence 2–4 kHz | Center or slight stereo |
| **Bass** | Comp 4:1–8:1, 3–6 dB GR | HPF 25 Hz, cut 200–400 Hz | Mono (bass stays mono) |
| **Arp** | Comp 2:1–4:1, 1–3 dB GR | HPF 200–400 Hz | Wide L/R |
| **Pluck** | Comp 3:1–4:1, 2–4 dB GR | HPF 200 Hz, presence 3–5 kHz | Moderate width |
| **Wobble** | Multiband comp, 2–4 dB GR | Cut 300–500 Hz, shape with filter | Center + stereo mod |

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Bass synth muddy | Too much 100–300 Hz | Cut 200–400 Hz 3–5 dB, tighten with comp |
| Lead synth doesn't cut through | Too much low-mid or not enough presence | HPF higher, boost 2–4 kHz |
| Arp too cluttered | Too many notes or too wide | Reduce width, HPF at 300 Hz, cut 300–500 Hz |
| Synths eating vocal space | Competing at 2–4 kHz | Sidechain or EQ carve for vocal |
| Too harsh/piercing | Too much 3–5 kHz | Cut 1–3 dB, add high-shelf cut |

## Ukulele — Instrument Guide

## Frequency Map

| Range | Frequencies | Description |
|-------|-------------|-------------|
| Bass | 60–200 Hz | Low-end body (low G string fundamental ~65 Hz) |
| Low-mid | 200–500 Hz | Warmth, resonance |
| Mid | 500–2 kHz | Core tone, pluck, body |
| High-mid | 2–5 kHz | Attack, finger noise, definition |
| High | 5–10 kHz | String shimmer, fret noise |
| Air | 10–15 kHz | Sparkle, harmonics |

## EQ by Genre

### Hawaiian / Traditional
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 100–200 Hz | Bell | 1.5 | +1.5 dB | Warm body |
| 300–500 Hz | Bell | 1.5 | -2 dB | Reduce boxiness |
| 1–2 kHz | Bell | 1.0 | +1.5 dB | Presence |
| 4–6 kHz | Bell | 1.0 | +2 dB | Shimmer |
| 10 kHz+ | HBF | — | +1.5 dB | Air |

### Pop / Indie
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 80–120 Hz | HSF | — | -2 dB | Tighten low end |
| 200–400 Hz | Bell | 1.8 | -3 dB | Clear mud |
| 2–4 kHz | Bell | 1.0 | +2.5 dB | Presence |
| 8–12 kHz | HBF | — | +2 dB | Sparkle |

### Fingerstyle / Solo
| Frequency | Type | Q | Gain | Why |
|-----------|------|---|------|-----|
| 120–200 Hz | Bell | 1.5 | +1 dB | Warmth |
| 400–600 Hz | Bell | 1.5 | -2 dB | Reduce nasal |
| 1–2.5 kHz | Bell | 1.0 | +1.5 dB | Articulation |
| 8–10 kHz | HBF | — | +1 dB | Air |

## Compression Settings

| Context | Ratio | Attack | Release | Gain Reduction | Notes |
|---------|-------|--------|---------|----------------|-------|
| Strumming | 3:1 | 10 ms | 80 ms | 3–5 dB | Even out strum dynamics |
| Fingerstyle | 2.5:1 | 15 ms | 100 ms | 2–4 dB | Gentle, preserve dynamics |
| Pop/Indie | 4:1 | 5 ms | 60 ms | 4–7 dB | More controlled |
| Fast picking | 4:1 | 3 ms | 50 ms | 5–7 dB | Even out fast passages |
| Solo | 2:1 | 20 ms | 120 ms | 2–3 dB | Very gentle, natural |

## Reverb

| Type | Pre-delay | Decay | Mix | Use Case |
|------|-----------|-------|-----|----------|
| Room | 10–15 ms | 0.6–1.0 s | 15–20% | Pop (tight) |
| Hall | 20–30 ms | 1.5–2.0 s | 20–30% | Solo/fingerstyle |
| Chamber | 15–20 ms | 1.0–1.5 s | 15–25% | Hawaiian |
| Spring | 5–10 ms | 0.8–1.2 s | 10–15% | Vintage |

## Common Issues & Solutions

### Thin / lacks body
- **Cause**: Small body size, limited low-end
- **Fix**: Boost 100–200 Hz, avoid high HPF (keep 60 Hz or lower)

### Harsh strumming
- **Cause**: Nail hitting nylon strings
- **Fix**: Cut 3–6 kHz with bell, use felt pick or fingertips

### Muddy strumming
- **Cause**: Strummed chords overlapping in low-mid range
- **Fix**: Cut 250–500 Hz with medium Q, use bus HPF at 100 Hz

### Buzzy fret noise
- **Cause**: Light string tension, fret buzz
- **Fix**: Cut 2–3 kHz, de-esser at 5–7 kHz

### Squeaking (finger noise)
- **Cause**: Fingers sliding on wound low-G string
- **Fix**: De-esser at 4–6 kHz, or use unwound low G

### Feedback (amplified)
- **Cause**: Small body resonates easily
- **Fix**: Notch at feedback frequency, keep HPF active

## Special Techniques

### Low G vs Re-entrant G
- **Low G**: Fuller bass response, better for fingerstyle
- **Re-entrant G** (high G): Classic ukulele sound, better for strumming
- EQ differently: Low G needs HPF 60 Hz, Re-entrant G needs HPF 100 Hz

### Strumming patterns
- Palm-muted strums benefit from fast compressor (2 ms attack)
- Open strums benefit from moderate compression (10 ms attack)

### Tremolo picking
- Fast compressor (3 ms attack, 4:1 ratio)
- Light reverb for sustain

### Chord melody (fingerstyle)
- Minimal compression (2:1)
- Mid-forward EQ for clarity

## Recommended Processing Chain

1. **HPF** 60–80 Hz (low G) or 100 Hz (re-entrant G)
2. **EQ** cut 300–500 Hz (reduce boxiness)
3. **Compressor** (3:1, 10 ms attack, 80 ms release for strum; 2.5:1, 15 ms for fingerstyle)
4. **EQ** boost 2–4 kHz (presence) + 10 kHz (air)
5. **Reverb** (room for strum, hall for fingerstyle)
6. **Limiter** −2 dB (catch strum peaks)

## VOCALS — Mixing Guide

## Frequency Map

| Range | Hz | Character | Action |
|-------|-----|-----------|--------|
| **Sub/low rumble** | 20–80 Hz | Mic rumble, breath pops | High-pass filter aggressively |
| **Low body** | 80–200 Hz | Warmth, chest resonance | Boost for richness, cut if muddy |
| **Low-mid** | 200–500 Hz | Fullness, mud | Cut 3–5 dB if boxy or muddy |
| **Mid** | 500–1000 Hz | Nasality, telephone | Cut 2–4 dB if nasal (especially 800 Hz–1 kHz) |
| **Upper mid** | 1–4 kHz | Presence, intelligibility | Boost 3–5 kHz for clarity and cut |
| **Presence** | 4–8 kHz | Brightness, sibilance | Boost gently, watch for sibilance at 5–8 kHz |
| **Air** | 8–16 kHz | Sparkle, openness | Gentle high shelf boost for modern sound |

## Vocal Chain (Recommended Order)

1. **High-pass filter** — 80–120 Hz (lower for baritone, higher for soprano)
2. **De-esser** — Sibilance control at 5–8 kHz
3. **Subtractive EQ** — Remove muddy/nasal frequencies
4. **Compression** — Leveling and body
5. **Additive EQ** — Presence and air boost
6. **Saturation** — Warmth and harmonic content (optional)
7. **Reverb/Delay** — Space and depth
8. **Volume automation** — Final leveling

## Compression

| Vocal Style | Ratio | Attack | Release | Gain Reduction |
|-------------|-------|--------|---------|----------------|
| Pop lead | 3:1–6:1 | 10–30 ms | 30–80 ms | 3–6 dB |
| Rock lead | 4:1–8:1 | 5–20 ms | 40–100 ms | 4–8 dB |
| Ballad | 2:1–4:1 | 20–40 ms | 50–120 ms | 2–5 dB |
| Rap | 4:1–8:1 | 5–15 ms | 20–50 ms | 4–8 dB |
| Metal | 5:1–10:1 | 5–15 ms | 30–80 ms | 5–10 dB |
| Background | 3:1–6:1 | 5–15 ms | 20–60 ms | 4–8 dB |

**Multi-band compression** can be useful:
- Low band (below 200 Hz): control low-end fluctuation
- Mid band (200 Hz–5 kHz): main leveling
- High band (5 kHz+): tame sibilance or boost air

## EQ by Genre

- **Pop:** HPF @ 100 Hz, cut 300–500 Hz (−2–4 dB), boost 3–5 kHz (+2–4 dB), air shelf +2–4 dB @ 10 kHz
- **Rock:** HPF @ 80 Hz, boost 200 Hz (+2–3 dB) for body, cut 500 Hz (−3 dB), boost 3 kHz (+3–5 dB)
- **Rap/Hip-Hop:** HPF @ 100 Hz, cut 400 Hz (−3–5 dB), boost 2–3 kHz (+3–5 dB), air +2 dB @ 10 kHz
- **Metal:** HPF @ 100 Hz, cut 500 Hz (−4–6 dB), boost 2–4 kHz (+4–6 dB) for cut-through
- **R&B:** HPF @ 80 Hz, boost 150–200 Hz (+2–4 dB), air shelf +3–5 dB @ 10 kHz
- **Jazz:** Minimal EQ, HPF @ 60 Hz, gentle presence +2 dB @ 3 kHz

## De-essing

| Vocal Style | Frequency | Threshold | Ratio |
|-------------|-----------|-----------|-------|
| Bright pop | 6–8 kHz | Moderate | 4:1–8:1 |
| Rock | 5–7 kHz | Moderate | 3:1–6:1 |
| Sibilant voice | 5–8 kHz | Aggressive | 6:1–12:1 |
| Rap | 6–8 kHz | Moderate | 4:1–8:1 |

**Alternatives:** Use dynamic EQ at sibilance frequencies, or gain-automate sibilant syllables manually.

## Vocal Layering

| Layer | Technique | Level | Pan |
|-------|-----------|-------|-----|
| **Lead** | Main performance | 0 dB reference | Center |
| **Double** | Second take | −6 to −10 dB | Center or just off |
| **Harmony** | 3rds/5ths | −8 to −12 dB each | L/R spread |
| **Ad-libs** | Supporting lines | −10 to −15 dB | Varied |
| **Background** | Stacked 2–4 times | −12 to −18 dB | Spread wide |

## Reverb & Delay

| Genre | Reverb | Pre-delay | Decay | Mix |
|-------|--------|-----------|-------|-----|
| Pop | Plate/Hall | 15–30 ms | 1.0–2.0 s | 15–25% |
| Ballad | Hall | 20–50 ms | 1.5–3.0 s | 20–30% |
| Rap | Slap delay + short verb | 10–20 ms | 0.5–1.0 s | 10–20% |
| Rock | Room/Plate | 10–25 ms | 0.8–1.5 s | 15–25% |
| R&B | Plate | 15–30 ms | 1.0–1.8 s | 15–30% |

**Vocal delay tricks:**
- **Slap delay:** 80–150 ms, 1–2 repeats, 10–20% mix — adds width
- **Ping-pong delay:** 200–400 ms L/R alternation — fills space
- **Filtered delay:** Low-pass at 4–7 kHz — sits behind vocal
- **Sidechain reverb:** Compress reverb return from vocal — reverb swells between phrases

## Saturation for Vocals

| Type | Effect | Amount |
|------|--------|--------|
| Tube | Warmth, 2nd order harmonics | Light–moderate |
| Tape | Smooth top end, compression | Light |
| Console/Preamp | Subtle glue, presence | Very light |
| Distortion | Aggressive, lo-fi | Use in parallel only |

## Mic Selection Guide

| Mic Type | Character | Best For |
|----------|-----------|----------|
| Large diaphragm condenser | Detailed, bright, present | Pop, R&B, hip-hop, studio rock |
| Small diaphragm condenser | Natural, accurate | Acoustic, folk, classical |
| Dynamic (SM7B/SM58) | Warm, smooth, less sibilance | Rock, metal, rap, live |
| Ribbon | Warm, dark, smooth | Jazz, vintage, soft vocals |
| USB/mobile | Convenient, lo-fi | Demos, remote recording |

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Muffled/unclear | Too much low-mid, not enough presence | Cut 300–500 Hz, boost 3–5 kHz |
| Harsh/sibilant | Too much 5–8 kHz | De-ess, or cut 5–8 kHz 2–4 dB |
| Thin/weak | Not enough 100–200 Hz | Add body boost, lower HPF |
| Too dynamic | Not enough compression | Increase ratio or lower threshold |
| Sits behind mix | Not enough presence or level | Boost 2–4 kHz, increase level 1–2 dB |
| Nasal/annoying | 800 Hz–1 kHz buildup | Cut 800–1000 Hz 3–5 dB |
| Plosives | P/B pops from breath | HPF higher, use pop filter, automate clip gain |