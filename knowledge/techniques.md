## AUTOMATION — Mixing Technique

## Why Automation Matters

Automation is the difference between a static, lifeless mix and one that breathes, moves, and engages the listener. It lets you control every parameter over time: volume, pan, EQ, effects, and more.

**Key principle:** Automate for emotion, not just level. Every section of a song deserves its own mix.

## Volume Automation

### By Section

| Section | Volume Strategy | Typical Change |
|---------|----------------|----------------|
| **Intro** | Quieter, build anticipation | −3 to −6 dB from chorus |
| **Verse** | Intimate, controlled | −2 to −4 dB from chorus |
| **Pre-chorus** | Build tension | Rising 2–4 dB |
| **Chorus** | Full power, loudest section | 0 dB (reference) |
| **Bridge** | Stripped back, intimate | −4 to −8 dB from chorus |
| **Outro** | Fade or cut | Decreasing |

### Track-Level Automation

| Element | What to Automate | Why |
|---------|-----------------|-----|
| **Lead vocal** | Volume (clip gain) | Every phrase — even out dynamics. Most important automation in the mix. |
| **Bass** | Volume per section | Verses quieter (−1 dB), choruses bigger (+1 dB) |
| **Guitar solo** | Volume rise | +2–4 dB during solo, return after |
| **Background vox** | Volume per phrase | Bring up for important lines, duck behind lead |
| **FX/risers** | Volume curve | Swell in and out naturally |

## Filter Automation

| Technique | Effect | Best For |
|-----------|--------|----------|
| **HPF rise into drop** | Tension, anticipation | EDM builds, pop pre-chorus |
| **LPF decline out of chorus** | Energy reduction, fade-out | Bridges, breakdowns |
| **Filter sweep on riser** | Dramatic build | Transition FX |
| **Automated wah/bandpass** | Rhythmic movement | Synths, guitars |

## EQ Automation

| Application | Automation | Effect |
|-------------|-----------|--------|
| **Telephone vocal** | Band-pass at 300 Hz–3 kHz | Intro verse effect |
| **Build-up brightness** | Rising high shelf | Lift energy before drop |
| **Low-end cut before drop** | HPF rising then sudden drop | Maximum impact |
| **Vocal presence boost in chorus** | +2–3 dB at 3–5 kHz | Cut-through without level change |

## Pan Automation

| Technique | Effect | Best For |
|-----------|--------|----------|
| **Wide → center at drop** | Focus energy | Synths, pads |
| **Auto-pan** | Rhythmic L/R movement | Pads, percussion |
| **Lead vocal center always** | Keep focus | Never automate lead vocal pan |
| **FX sweeps** | Widen during build | Risers, impacts |

## Reverb/Delay Automation

| Technique | Effect |
|-----------|--------|
| **Increase reverb send on last word of phrase** | Tail rings out dramatically |
| **Cut reverb at drop** | Instant dryness = impact |
| **Increase delay feedback during buildup** | Chaos and tension |
| **Automate pre-delay** | Shorter for clarity, longer for depth |
| **Sidechain reverb from dry vocal** | Reverb swells between phrases |

## Compression Automation

| Technique | Effect |
|-----------|--------|
| **Reduce threshold in chorus** | Tighter, denser sound |
| **Increase ratio on bridge** | More controlled, intimate |
| **Makeup gain follows threshold** | Consistent perceived level |
| **Sidechain key automation** | On/off for rhythmic effect |

## Mastering-Style Automation

| Automation | Effect |
|-----------|--------|
| **Master bus HPF rising before drop** | Classic build technique |
| **Master bus low shelf boost in chorus** | More low-end impact |
| **Master bus limiter ceiling automation** | Controlled dynamics |
| **Very gentle master bus comp threshold** | Section-dependent glue |

## Workflow Tips

### DAW-Specific Shortcuts

| Task | Pro Tools | Logic | Ableton | FL Studio |
|------|-----------|-------|---------|-----------|
| **Write volume automation** | Touch/Latch mode | Touch/Latch | Arrange view → line | Right-click → edit events |
| **Draw automation** | Pencil tool | Pencil tool | Pen tool | Draw tool |
| **Trim automation** | Trim tool | Trim tool | Adjust handles | Shift+drag |

### General Workflow

1. **Clip gain first** — Balance track levels before writing fader automation
2. **Write in Touch mode** — Let go to return to previous value
3. **Trim with handles** — Drag the whole automation line, not individual points
4. **Use relative automation** — Some DAWs allow relative changes to existing automation
5. **Group related tracks** — Automate bus level for group moves
6. **Copy automation between sections** — Once chorus is automated, copy to other choruses

## Things to Automate (Checklist)

- [ ] Lead vocal volume per phrase
- [ ] Instrument levels per section
- [ ] Reverb sends (increase on last words, cut at drops)
- [ ] Delay throws (specific words/phrases)
- [ ] Filter sweeps on builds
- [ ] Low-end presence (cut before drop, restore at drop)
- [ ] Background vocal levels per phrase
- [ ] Guitar solo volume rise and return
- [ ] Master bus subtle section changes
- [ ] FX volume (risers, impacts, transitions)

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Automation too jerky/sudden | Smooth with longer attack/release or more points |
| Volume jumps at section changes | Crossfade automation points over 1–2 beats |
| Too much automation | Simplify — focus on 3–5 key parameters |
| Not enough automation | Start with lead vocal, then bass, then section levels |
| Phase issues from automated EQ | Use linear-phase EQ for automated EQ moves |

## BUS PROCESSING / SUBMIXING — Mixing Technique

## Concept

Bus (submix) processing groups related tracks together and processes them as a unit. This creates cohesion, saves CPU, and makes mix decisions faster.

**Bus ≠ Master.** Buses are subgroup processors; the master bus is the final sum.

## Standard Bus Structure

```
Track 1 (Kick) ─┐
Track 2 (Snare) ─┤
Track 3 (Hi-hat) ─┤
Track 4 (Toms) ───┤─── DRUM BUS ─┐
Track 5 (OH) ─────┘               │
                                  │
Track 6 (Bass) ─── BASS BUS ─────┤
                                  │
Track 7 (Gtr L) ─┐              │
Track 8 (Gtr R) ─┤─── GTR BUS ──┤─── MASTER BUS
                  │              │
Track 9 (Vox) ────┤─ VOX BUS ───┘
Track 10 (BG Vox) ┘
```

## Bus Processors by Type

### Drum Bus

| Processor | Setting | Purpose |
|-----------|---------|---------|
| **Compression** | 2:1–4:1, 10–30 ms attack, 50–150 ms release, 1–3 dB GR | Glue drums together |
| **EQ** | HPF at 20–30 Hz (remove sub rumble), cut 300–500 Hz (−2 dB), presence 3–4 kHz (+1–2 dB) | Tonal balance |
| **Saturation** | Very light tape/console | Warmth, cohesion |
| **Parallel comp** | Send to heavy comp (10:1, blend 20–30%) | Punch and weight |
| **Clipper (soft)** | 1–3 dB of clipping on peaks | Controlled transients, extra loudness |

### Bass Bus

| Processor | Setting | Purpose |
|-----------|---------|---------|
| **Compression** | 3:1–6:1, 10–30 ms attack, 40–80 ms release, 2–5 dB GR | Consistency |
| **EQ** | HPF at 25–40 Hz, cut 300–500 Hz (−2–3 dB) | Clean sub, less mud |
| **Saturation** | Tape or tube, subtle | Harmonics for small speakers |
| **Sidechain** | From kick bus | Kick/bass lock |

### Guitar Bus

| Processor | Setting | Purpose |
|-----------|---------|---------|
| **EQ** | HPF at 80–120 Hz, cut 300–500 Hz (−3–5 dB), slight presence 2–4 kHz | Clean up, fit in mix |
| **Compression** | 2:1–4:1, 10–30 ms attack, 40–80 ms release, 1–3 dB GR | Glue doubled guitars |
| **Saturation** | Console or tape | Warm vintage tone |
| **Width control** | M/S: adjust width | Keep centered or spread |

### Vocal Bus

| Processor | Setting | Purpose |
|-----------|---------|---------|
| **Compression** | 2:1–4:1 (bus comp, gentle), 10–20 ms attack, 40–60 ms release, 1–2 dB GR | Glue main + bg vocals |
| **EQ** | HPF at 80–100 Hz, cut 400–500 Hz (−1–2 dB), air shelf +2 dB | Polish |
| **De-esser** | On bus if needed | Catch all sibilance |
| **Reverb send** | Plate or hall 1.5–2.5 s | Space |
| **Delay send** | Ping-pong 1/4 or 1/8 | Width and interest |

### FX / Reverb Bus

| Processor | Setting | Purpose |
|-----------|---------|---------|
| **Reverb** | Hall or plate, 1.5–3.0 s | Main ambience |
| **EQ on return** | HPF 200–400 Hz, LPF 8–12 kHz | Clean reverb tail |
| **Compression** | 2:1–3:1, gentle | Smooth reverb tail |
| **Gate** | Optional gated reverb | Controlled decay |

## Bus Processing Principles

### 1. Process in Context
- Always solo-safe: listen to bus in full mix context, not soloed
- What sounds good soloed often sounds wrong in the mix

### 2. Less is More on Buses
| On Individual Tracks | On Bus |
|---------------------|--------|
| 3–6 dB GR | 1–3 dB GR |
| Aggressive EQ | Subtle EQ (1–2 dB) |
| Full processing | Gentle glue |

### 3. Bus Order Matters
Typical order: **EQ → Comp → Saturation → Limiter (if needed)**

### 4. Use Sends, Not Inserts
- **Insert:** Directly on the track/bus
- **Send:** Parallel processing (reverb, delay, parallel comp)

### 5. Bus Level Relationships

| Level Relationship (Peak) | Ratio |
|--------------------------|-------|
| Kick vs. Bass | Similar RMS |
| Kick vs. Snare | Kick 1–2 dB louder (rock) or equal (pop) |
| Vocals vs. instruments | Vocals 2–6 dB louder than instrumental bus |
| Drums bus vs. bass bus | Bass 1–2 dB louder (some genres) |
| Guitar bus vs. vocal bus | Vocal 3–6 dB louder |

## Bus Compression Settings Reference

| Bus | Ratio | Attack | Release | GR | Type |
|-----|-------|--------|---------|-----|------|
| **Drums** | 2:1–4:1 | 10–30 ms | 50–100 ms | 1–3 dB | VCA |
| **Bass** | 3:1–5:1 | 20–50 ms | 40–80 ms | 2–4 dB | VCA/Opto |
| **Guitars** | 2:1–3:1 | 20–40 ms | 50–100 ms | 1–2 dB | VCA |
| **Vocals** | 2:1–3:1 | 10–20 ms | 40–60 ms | 1–2 dB | Opto/VCA |
| **Master** | 1.5:1–2:1 | 10–30 ms | Auto/100 ms | 1–2 dB | Vari-Mu/VCA |

## Creative Bus Techniques

| Technique | How | Effect |
|-----------|-----|--------|
| **Drum bus distortion** | Soft clip or tape on drum bus | Cohesive, aggressive drums |
| **Sidechain guitar bus from vocal** | Compress guitar bus with vocal key | Vocal cuts through naturally |
| **Parallel drum bus** | Heavy comp on drum bus blended | Huge drums |
| **Reverb bus pre-delay** | Sidechain reverb bus from dry signal | Reverb swells between hits |
| **Bus panning** | Pan entire buses (not just tracks) | Wider, more organized mix |
| **Drum HPF rise** | Automate HPF on drum bus before drop | Classic build |

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Too much bus compression (lifeless) | Reduce bus comp GR to 1–2 dB |
| Bus comp pumping (drums) | Slow attack to 20–30 ms, release to 100 ms |
| Muddy bus | Check bus EQ: cut 300–500 Hz 2–3 dB |
| One track dominates the bus | Check individual levels before bus |
| Bus sounds worse than summing individual tracks | Remove bus processing, process individually instead |
| EQ on bus causes phase issues | Use minimum-phase EQ, avoid extreme cuts |

## COMPRESSION — Mixing Technique

## What Compression Does

Compression reduces the dynamic range by attenuating signals above a threshold. The result:
- **Louder perceived volume** (quiet parts come up, loud parts come down)
- **Tighter, more controlled sound**
- **Glue** (cohesion across multiple tracks)
- **Sustain** (notes ring longer)
- **Punch** (attack transient through right release settings)

## Parameter Guide

| Parameter | What It Does | Short Attack | Slow Attack |
|-----------|-------------|--------------|-------------|
| **Threshold** | Level at which compression starts | Lower = more compression | Higher = less compression |
| **Ratio** | Amount of gain reduction | 2:1 = gentle, 10:1 = hard limiting |
| **Attack** | How fast compression kicks in | < 5 ms = catches transient, less punch | > 20 ms = lets transient through, more punch |
| **Release** | How fast compression stops | < 30 ms = fast, can pump | > 100 ms = smooth, natural |
| **Knee** | How gradual compression engages | Hard knee = aggressive | Soft knee = smooth, musical |
| **Makeup Gain** | Restore output level | Add back what was reduced |

## Attack & Release Timing

### Attack Time Reference

| Attack | Effect | Best For |
|--------|--------|----------|
| 0–2 ms | Catches peak transient, maximum control | Bass, vocals (leveling), bus compression |
| 5–15 ms | Partial transient through, some punch retained | Drums, acoustic guitar, rock vocals |
| 20–40 ms | Most transient through, rhythm control | Drum bus, piano, clean guitar |
| 50–100 ms | Only sustained portions compressed | Room mics, overheads, pad synths |

### Release Time Reference

| Release | Effect | Best For |
|---------|--------|----------|
| 5–20 ms | Fast recovery, can distort/pump | Snare, percussion (tight) |
| 30–60 ms | Medium-fast, musical pump | Drum bus, 808 (sidechain) |
| 80–150 ms | Medium, natural leveling | Vocals, bass, guitars |
| 200–500 ms | Slow recovery, smooth glue | Mix bus, piano, pads |
| Auto/Program-dependent | Adapts to input | Vocals, bass, mix bus |

**Tempo-matched release (quarter note):** `Release (ms) = 60000 / BPM`

## Ratio by Application

| Ratio | Application | Effect |
|-------|-------------|--------|
| 1.5:1–2:1 | Gentle leveling | Subtle control, retains dynamics |
| 3:1–4:1 | General purpose | Standard mix compression |
| 5:1–8:1 | Strong control | Drums, aggressive bass, rock vocals |
| 8:1–20:1 | Very strong | Limiting, effect compression |
| 20:1+ | Hard limiting | Peak control, safety limiter |
| ∞:1 | Brickwall limiting | Ceiling, distortion prevention |

## Compressor Types

| Type | Character | Attack | Release | Best For |
|------|-----------|--------|---------|----------|
| **VCA** | Clean, precise, punchy | Fast | Medium | Drums, bus, aggressive material |
| **FET** | Aggressive, colored, fast | Very fast | Fast-Medium | Drums, rock vocals, parallel comp |
| **Opto** | Smooth, musical, slow | Slow | Slow | Vocals, bass, gentle leveling |
| **Vari-Mu** | Tube, warm, glue | Medium-Slow | Medium | Mix bus, masters, vocals |
| **Digital** | Transparent, precise | Variable | Variable | Clean leveling, surgical control |
| **Multiband** | Frequency-dependent | Variable | Variable | De-essing, bass control, mastering |

## Serial Compression

Using multiple compressors in series with light settings each, rather than one compressor doing all the work.

### Typical Serial Chain

**Chain 1: Leveling compressor** (gentle, all-purpose)
- Ratio: 2:1–3:1
- Attack: 10–30 ms
- Release: 50–100 ms
- GR: 1–3 dB

**Chain 2: Character compressor** (color, tone)
- FET or Opto
- Ratio: 4:1–8:1 (aggressive ratio but light GR)
- Attack: 5–20 ms
- GR: 2–4 dB

**Result:** More control with less pumping, more musical sound.

## Parallel Compression (New York Compression)

Blend compressed signal with dry signal for punch + density.

| Setting | Value |
|---------|-------|
| Ratio | 8:1–20:1 |
| Attack | 1–5 ms (fast) |
| Release | 10–50 ms (medium-fast) |
| Threshold | Low (−20 to −40 dB) |
| Blend | 10–40% compressed |
| Result | Punchy transients + dense sustain |

### Best applications
- **Drums:** Blend 20–40% heavy compression — punchy, huge
- **Vocals:** Blend 10–30% FET compression — presence without pumping
- **Bass:** Blend 20–30% — sustain without losing attack
- **Mix bus:** Blend 10–20% — glue without killing dynamics

## Sidechain Compression

| Scenario | Key Input | Attack | Release | Ratio | GR on Key |
|----------|-----------|--------|---------|-------|-----------|
| Kick → Bass | Kick | 1–5 ms | 40–80 ms | 4:1–10:1 | 3–8 dB |
| Kick → Pad/Synth | Kick | 1–3 ms | 100–300 ms | 3:1–8:1 | 3–10 dB |
| Vocal → Instruments | Vocal | 5–15 ms | 50–150 ms | 2:1–4:1 | 1–3 dB |
| Snare → Guitars | Snare | 1–5 ms | 50–100 ms | 3:1–6:1 | 2–4 dB |

## Gain Reduction Targets

| Material | Target GR | Notes |
|----------|-----------|-------|
| Vocals (lead) | 4–8 dB | Smooth leveling |
| Vocals (background) | 6–12 dB | More consistent |
| Bass (rock) | 3–6 dB | Keep punch |
| Bass (metal) | 6–10 dB | Tight control |
| Kick | 3–6 dB | Too much = no punch |
| Snare | 3–8 dB | Depends on ring |
| Drum bus | 2–6 dB | Gentle glue |
| Acoustic guitar | 2–5 dB | Keep dynamics |
| Electric guitar | 1–3 dB | Distortion already compresses |
| Mix bus | 1–3 dB | Very gentle |
| 808 | 3–6 dB | Control sustain |

## Multiband Compression

### Common Uses
| Application | Bands | Settings |
|-------------|-------|----------|
| **De-essing** | High band (5–8 kHz) | Ratio 3:1–6:1, fast attack/release |
| **Bass control** | Low band (20–200 Hz) | Ratio 2:1–4:1, gentle |
| **Tame harshness** | High-mid (2–5 kHz) | Ratio 2:1–3:1, fast attack, auto release |
| **Mastering** | 3–4 bands | Very gentle, 1.5:1–2:1 ratio, 0.5–2 dB GR |

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Pumping/breathing | Release too fast for tempo | Slow release, match to song tempo |
| No punch | Attack too fast (killing transients) | Slow attack to 20–40 ms |
| Distortion/grainy | Too much gain reduction | Lower ratio, higher threshold |
| Too quiet/weak | Not enough makeup gain | Add 1–6 dB makeup |
| Sound lifeless | Overcompressed | Reduce ratio, let 2–6 dB of dynamic range through |
| Inconsistent level | Not enough compression | Lower threshold, increase ratio |
| Harsh after compressor | FET or distortion is too aggressive | Try opto or VCA instead |

## Compressor Settings Quick Reference

| Material | Type | Ratio | Attack | Release | Threshold | GR |
|----------|------|-------|--------|---------|-----------|-----|
| Lead vocal | Opto/VCA | 3:1 | 10 ms | 60 ms | −15 dB | 4–6 dB |
| Rock vocal | FET | 4:1 | 5 ms | 40 ms | −18 dB | 5–8 dB |
| Kick | VCA | 4:1 | 3 ms | 50 ms | −10 dB | 3–5 dB |
| Snare | VCA/FET | 6:1 | 1 ms | 30 ms | −12 dB | 3–6 dB |
| Bass (finger) | Opto | 4:1 | 20 ms | 80 ms | −18 dB | 3–5 dB |
| Bass (picked) | VCA | 5:1 | 10 ms | 60 ms | −15 dB | 4–6 dB |
| Acoustic guitar | Opto | 3:1 | 15 ms | 50 ms | −15 dB | 2–4 dB |
| Electric guitar | VCA | 4:1 | 30 ms | 80 ms | −12 dB | 1–3 dB |
| Drum bus | VCA | 2:1–4:1 | 10 ms | Auto | −5 to −10 dB | 1–3 dB |
| Mix bus | Vari-Mu/VCA | 1.5:1–2:1 | 30 ms | Auto | −3 to −6 dB | 1–2 dB |

## Creative Effects — Technique Guide

## The Purpose of Creative FX

Creative effects transform ordinary sounds into extraordinary ones. They add movement, color, texture, and emotion. Unlike corrective processing (EQ, compression), creative FX serve the arrangement and emotion of the song.

## Effect Categories

| Category | Examples | Purpose |
|----------|----------|---------|
| **Modulation** | Chorus, flanger, phaser, vibrato | Movement, thickness, swirl |
| **Pitch** | Harmonizer, pitch shifter, octaver | Harmonies, special FX |
| **Time-based** | Delay, echo, reverb | Depth, space, rhythm |
| **Distortion** | Overdrive, fuzz, saturation, bit-crush | Grit, character, warmth |
| **Filter** | Auto-filter, wah, sweep | Movement, energy builds |
| **Glitch** | Stutter, beat repeat, looper | Rhythmic texture |
| **Spectral** | Ring mod, frequency shifter | Alien sounds, dissonance |
| **Granular** | Granular synth, cloud | Textures, pads, atmosphere |

## Modulation Effects

### Chorus
| Parameter | Typical Range | Tip |
|-----------|---------------|-----|
| Rate | 0.2–5 Hz | Slow (0.3 Hz) for thickness, fast (3 Hz) for warbly |
| Depth | 20–80% | Subtle (20–30%) for thickening, high (50%+) for effect |
| Mix | 10–50% | Usually in parallel (wet/dry blend) |

**Use cases:**
- **Piano:** Light chorus (rate 0.4 Hz, depth 25%) → vintage electric piano feel
- **Guitar:** Moderate chorus (rate 0.6 Hz, depth 40%) → 80s clean tone
- **Vocals:** Very light (rate 0.3 Hz, depth 15%) → subtle thickness
- **Bass:** Light (rate 0.5 Hz, depth 20%) → movement without mud
- **Synths:** Heavy (rate 2 Hz, depth 60%) → classic synth pad

### Flanger
| Parameter | Typical Range | Tip |
|-----------|---------------|-----|
| Rate | 0.1–4 Hz | Slow for jet-like sweeps, fast for metallic |
| Depth | 30–80% | High for extreme effect, low for subtle |
| Feedback | 0–95% | More feedback = more metallic/comb-like |
| Mix | 20–70% | Start subtle, increase for effect |

**Use cases:**
- **Snare:** Medium flanger (rate 0.5 Hz, depth 50%, feedback 30%) → 70s snare
- **Guitar:** Fast flanger (rate 2 Hz, depth 60%, feedback 40%) → 80s metal
- **Pad:** Slow flanger (rate 0.2 Hz, depth 70%, feedback 20%) → evolving texture

### Phaser
| Parameter | Typical Range | Tip |
|-----------|---------------|-----|
| Rate | 0.1–6 Hz | Slow for sweep, fast for tremolo-like |
| Depth | 30–80% | Controls intensity |
| Stages | 2–12 | More stages = more notches, smoother |
| Feedback | 0–90% | Higher = more resonant swoosh |

**Use cases:**
- **Rhodes/Wurlitzer:** Moderate phaser (rate 0.4 Hz, stages 4) → classic soul
- **Guitar:** Slow phaser (rate 0.3 Hz, stages 6) → psychedelic rock
- **Synths:** Fast phaser (rate 3 Hz, stages 8) → electronic movement

## Pitch Effects

### Harmonizer
| Type | Interval | Use |
|------|----------|-----|
| Up 3rd | +4 semitones | Bright harmony |
| Down 3rd | −3 semitones | Darker harmony |
| Up 5th | +7 semitones | Open, airy |
| Down 5th | −7 semitones | Powerful |
| Octave up | +12 semitones | Tiny/fairy/whistle |
| Octave down | −12 semitones | Thick, sub-like |

**Tips:**
- Blend with dry signal: 20–40% for subtle, 50%+ for obvious
- Add slight detune (±5 cents) to the harmony for natural feel
- Use mid-side: keep harmony in center, original in sides

### Detune
| Amount | Effect |
|--------|--------|
| ±1–3 cents | Subtle thickening, like double tracking |
| ±5–10 cents | Wide chorusing effect |
| ±15–30 cents | Obvious detune, artificial |

## Time-Based Effects (Creative)

### Ping-pong delay
| Parameter | Setting | Effect |
|-----------|---------|--------|
| Time L | 1/8 note | Left ear gets 8th note |
| Time R | 1/8 note + offset | Right ear gets offset version |
| Feedback | 20–50% | 2–5 repeats |
| Filter | HPF 200 Hz, LPF 8 kHz | Clean repeats |
| Mix | 15–30% | Blend with dry signal |

### Rhythmic delay
- **Dotted 8th (300 ms at 120 BPM):** Classic rockabilly, U2 slapback
- **Triplet delay (1/4 note triplet):** Wide, interesting rhythmic feel
- **Reverse delay:** Build tension → creates riser effect
- **Multi-tap (3–5 taps):** Complex rhythmic pattern

### Filtered delay
- **HPF delay:** Repeats get thinner → vintage tape echo feel
- **LPF delay:** Repeats get darker → muffled, dreamy
- **Band-pass delay:** Telephone effect on repeats
- **Aging delay:** Each repeat degrades (pitch down + filter)

## Distortion (Creative)

### Saturation by amount
| Amount | Effect | Use |
|--------|--------|-----|
| 1–5% | Warmth, harmonic enhancement | Master bus, vocals |
| 5–15% | Presence, edge | Drums, guitars |
| 15–30% | Drive, character | Bass, synths |
| 30–60% | Distortion | Guitars, dramatic FX |
| 60–100% | Fuzz | Extreme textures |

### Bit-crushing
| Bit Depth | Effect |
|-----------|--------|
| 16-bit | Inaudible (CD quality) |
| 8-bit | Retro video game, lo-fi texture |
| 4-bit | Crunchy, gritty |
| 2-bit | Extreme artifact, noise |

| Sample Rate Reduction | Effect |
|-----------------------|--------|
| 22 kHz | FM radio quality |
| 11 kHz | Telephone quality |
| 5 kHz | Lo-fi extreme |
| 1–2 kHz | Aliasing, robotic |

### Creative distortion uses
- **Parallel snare distortion:** Blend 10–30% of distorted snare for aggressive rock
- **808 fuzz:** Add fuzz to 808 for gnarly bass tone
- **Vocal growl:** Heavy distortion on vocal send, blend subtle (5–15%)
- **Drum bus saturation:** Tape saturation on drum bus for glue + edge

## Filter Effects

### Auto-filter / Wah
| Parameter | Setting | Effect |
|-----------|---------|--------|
| LFO Rate | 1/4 note | Filter sync'd to tempo |
| Resonance | 30–60% | Peak at cutoff |
| Envelope | 40–70% | Volume-triggered filter (wah) |
| HPF/LPF | Sweep 200–8k Hz | Movement |

**Creative filter techniques:**
- **Filter riser:** Open HPF from 20 Hz to 200 Hz over 8 bars → tension build
- **Filter drop:** Close LPF from 20 kHz to 200 Hz on beat → energy release
- **Envelope filter on bass:** Sidechain-triggered filter sync'd to kick → EDM wobble
- **Band-pass sweep:** Sweep narrow band-pass across frequency range → telephone sweep

## Glitch / Stutter Effects

### Beat repeat
| Parameter | Effect |
|-----------|--------|
| Repeat rate | 1/4 to 1/32 note |
| Gate | 25–100% (how much of the repeat plays) |
| Pitch | Normal, up/down octave, random |
| Chance | 0–100% (probability of triggering) |

### Stutter
| Speed | Effect |
|-------|--------|
| 1/4 note | Rhythmic chop |
| 1/8 note | Classic stutter, Daft Punk style |
| 1/16 note | Fast, glitchy |
| 1/32 note | Extreme, micro-edits |

## Sound Design FX (By Genre)

### EDM
- **Risers:** White noise + filter sweep + reverb tail + pitch rise
- **Drops:** Full cut, 1 beat silence, then impact + sub hit
- **Sweeps:** Filter sweep across 1–4 bars before drop
- **Impacts:** Layered kick + noise + sub + reverb
- **Vocal chops:** Chop vocal phrase into 1/8 or 1/16 slices

### Hip-Hop / Trap
- **808 slides:** Pitch bend 2–8 semitones over 1/2 bar
- **Hi-hat rolls:** 1/32 note rolls, varying velocity
- **Risers:** Reversed cymbal or filtered snare
- **Vocal tags:** Pitch-shifted "yeah," "what," "skrr"
- **Beat repeats:** Stutter on snare or vocal at transition

### Pop
- **Vocal doubles:** Same take panned L/R with 15–25 ms delay
- **Filter build:** LPF closing over 4 bars into chorus drop
- **Reverse reverb:** Reverse vocal before chorus entrance
- **Chorus width:** Doubled chorus panned hard L/R with slight detune

### Rock
- **Auto-wah:** Envelope filter on guitar solos
- **Tape delay:** Slapback on vocals (dotted 8th, 1 repeat)
- **Spring reverb:** Classic surf or vintage rock
- **Rotary speaker:** Leslie emulation on organ or guitar

## FX Automation Ideas

| Effect | Automation | Song Section |
|--------|-----------|--------------|
| Reverb send | 0% → 40% over last chorus | Bridge → Chorus |
| Filter cutoff | Close (200 Hz) → open (8 kHz) | Pre-chorus → Chorus |
| Delay feedback | 10% → 70% on final word | End of bridge |
| Pan | Center → hard L, then L→R sweep | Solo section |
| Pitch shift | 0 → −12 semitones on beat | Drop/transition |
| Stutter rate | 1/4 → 1/32 over 4 beats | Buildup → Drop |
| Distortion mix | 0% → 30% over section | Verse → Chorus |
| Bit-crush depth | 16-bit → 4-bit on transition | Breakdown |

## FX Chains for Specific Vibe

### Vintage / Retro
```
Audio → Lo-Fi saturation (8–16 bit) → Tape wobble (chorus 0.2 Hz) → HPF 300 Hz → LPF 5 kHz → Reverb (spring) → Output
```

### Space / Ambient
```
Audio → Reverse reverb → Huge hall reverb (3–5 s, 40%) → Ping-pong delay (1/4 note, 30%) → LPF 8 kHz → Output
```

### Aggressive / Industrial
```
Audio → Heavy distortion → Bit crusher (4-bit) → Flanger (fast, 80% feedback) → Limiter → Output
```

### Dream Pop / Shoegaze
```
Audio → Reverb (hall, 3 s, 60%) → Chorus (0.5 Hz, 60%) → Delay (1/4 triplet, 50%) → Saturation (light, 10%) → Output
```

## Rules for Creative FX

1. **Serve the song:** Does this effect make the arrangement more emotional?
2. **Don't mask the mix:** Check that FX aren't eating the dry signal's clarity
3. **Automate everything:** Static FX get boring. Move them through the arrangement
4. **Less is more with FX returns:** 3 reverbs, 2 delays, 1 modulation is plenty
5. **EQ your FX:** HPF reverb returns (200–500 Hz), LPF (8–15 kHz) for clarity
6. **Sidechain FX to dry:** Compress FX send with dry signal → FX duck when dry plays
7. **Use pre-fader sends:** FX volume stays consistent even when you automate dry track volume

## EQ (EQUALIZATION) — Mixing Technique

## Frequency Atlas

### Sub-Bass: 20–60 Hz
- **Character:** Felt more than heard, physical rumble
- **Instruments:** Kick sub, 808, synth bass, organ
- **Action:** High-pass most instruments here. Only kick and bass need this range.
- **Cautions:** Too much = mud, speaker strain. Too little = no low-end weight.

### Bass: 60–250 Hz
- **Character:** Fundamental notes, warmth, body
- **Instruments:** Bass, kick body, low tom, baritone vocals, cello
- **Action:** Boost for warmth and weight. Cut to clean up mud.
- **Cautions:** Buildup here = boxy, unclear mix. 100–200 Hz is the most common mud zone.

### Low-Mid: 250–1000 Hz
- **Character:** Fullness, warmth, but also mud, boxiness, honk
- **Instruments:** Guitars, vocals, snare, piano, brass, woodwinds
- **Action:** This is the trickiest range. Too much = muddy, too little = thin.
- **Cautions:** 300–500 Hz is the **mud zone** — cut 2–5 dB on most tracks except bass.

### Mid: 1–4 kHz
- **Character:** Presence, bite, intelligibility, aggression
- **Instruments:** Vocals (clarity), snare (crack), guitars (bite), kick (attack)
- **Action:** Boost for presence and cut-through. Too much = harsh, fatiguing.
- **Cautions:** 2–3 kHz is the vocal presence zone — protect it.

### Presence: 4–8 kHz
- **Character:** Brightness, definition, sibilance
- **Instruments:** Cymbals, hi-hat, vocals (air), acoustic guitar, strings
- **Action:** Boost for open, airy sound. Cut if sibilant or harsh.
- **Cautions:** 5–7 kHz is sibilance territory. Listen for "sss" and "tch" sounds.

### Air: 8–20 kHz
- **Character:** Sparkle, air, openness, "expensive" sound
- **Instruments:** Overheads, vocals, acoustic guitar, strings, synth pads
- **Action:** Gentle shelf boost (+2–4 dB) for modern, polished sound.
- **Cautions:** Too much = brittle, thin. Most speakers can't reproduce this accurately.

## Quick EQ Reference Table

| Frequency | What Lives Here | Common Problem | Typical Fix |
|-----------|----------------|----------------|-------------|
| 40–60 Hz | Kick sub, 808, bass | Too much rumble | HPF at 30 Hz |
| 80–120 Hz | Kick thump, bass root | Boomy, muddy | Cut 2–4 dB |
| 200–300 Hz | Guitar body, snare warmth | Muffled, boxy | Cut 3–5 dB |
| 400–600 Hz | Vocal low-mid, guitar | Muddy, cardboard | Cut 3–6 dB |
| 800 Hz–1 kHz | Nasal quality | Honky, telephone | Cut 2–4 dB |
| 1.5–2.5 kHz | Vocal presence, guitar bite | Harsh or buried | Cut or boost as needed |
| 3–5 kHz | Attack, definition | Fatiguing if too much | Cut gently for relief |
| 5–8 kHz | Sibilance, cymbal wash | Sss sounds harsh | De-ess or cut 2–4 dB |
| 8–12 kHz | Air, sparkle | Thin if too much | Gentle shelf +2–4 dB |
| 14–20 kHz | Extreme air | Inaudible on most systems | Use with caution |

## EQ Types & When to Use

| Type | Shape | Use Case |
|------|-------|----------|
| **High-pass (LPF)** | Cuts below frequency | Remove rumble, mud, stage noise |
| **Low-pass (HPF)** | Cuts above frequency | Tame harshness, lo-fi effect |
| **Bell** | Boost/cut at center freq | Surgical fixes, tonal shaping |
| **Shelf** | Boost/cut everything above/below | Broad tonal adjustment |
| **Notch** | Very narrow cut | Remove resonant frequencies, hum |
| **Band-pass** | Cuts above AND below | Telephone effect, isolate range |

## EQ Strategies

### 1. Subtractive First, Additive Second
1. **Cut problems first:** Find and remove bad frequencies
2. **Then boost:** Add positive tonal shaping
3. **Rule:** Cut 2–4 dB per filter, boost 1–3 dB per filter
4. **Reason:** Cutting removes problems, boosting adds noise + phase shift

### 2. Sweep Method (Finding Problem Frequencies)
1. Boost a narrow Q (1–3) by 6–10 dB
2. Sweep through the frequency range
3. When you hear something nasty, you found the problem
4. Cut that frequency by 3–6 dB with a wider Q (0.5–1.5)

### 3. The Vocal Space Protection Principle
- Vocals are the most important element in most genres
- Cut competing instruments at 2–3 kHz (vocal presence zone)
- Cut competing instruments at 100–200 Hz (vocal body zone)
- Boost vocals where you cut the instruments

### 4. Complementary EQ (The "Puzzle" Method)
- If kick is boosted at 60 Hz, cut bass at 60 Hz
- If snare is boosted at 200 Hz, cut guitars at 200 Hz
- If vocal is boosted at 3 kHz, cut pads/keys at 3 kHz
- Each instrument gets its own frequency "slot"

## Q Width Guide

| Q Value | Width | Use For |
|---------|-------|---------|
| 0.3–0.7 | Very wide | Broad tonal shaping (shelves) |
| 0.7–1.5 | Medium | General EQ moves |
| 1.5–3.0 | Narrow | Surgical corrections |
| 3.0–10+ | Very narrow | Notch filtering (resonances) |

## Common EQ Patterns by Genre

### Pop
- Kick: HPF 30 Hz, boost 60 Hz, boost 3 kHz
- Snare: HPF 100 Hz, boost 200 Hz, boost 4 kHz
- Bass: HPF 40 Hz, cut 300 Hz, boost 2 kHz
- Vocals: HPF 100 Hz, cut 400 Hz, boost 3 kHz + 10 kHz shelf

### Rock
- Kick: HPF 30 Hz, boost 60 Hz, cut 400 Hz, boost 2–4 kHz
- Guitars: HPF 100 Hz, cut 400 Hz, boost 2 kHz
- Bass: HPF 50 Hz, boost 100 Hz, cut 300 Hz, boost 1.5 kHz
- Vocals: HPF 80 Hz, cut 500 Hz, boost 2–3 kHz

### EDM
- Kick: HPF 25 Hz, boost 50 Hz, cut 300 Hz, boost 3–4 kHz
- 808/Sub: HPF 30 Hz, boost 40–50 Hz, cut 200 Hz
- Synth: HPF 200 Hz (pads), cut 400 Hz, boost wherever needed
- Vocals: HPF 100 Hz, cut 400 Hz, boost 3–5 kHz + 12 kHz shelf

### Hip-Hop / Trap
- 808: HPF 25 Hz, boost 40–50 Hz, cut 150–300 Hz
- Kick: boost 60 Hz, cut 400 Hz, boost 3 kHz
- Vocals: HPF 100 Hz, cut 400–500 Hz, boost 2–4 kHz + air shelf

## EQ Mistakes to Avoid

| Mistake | Why It's Bad | Better Approach |
|---------|-------------|----------------|
| Boosting before cutting | Adds noise, phase issues | Cut problems first |
| Too many boosts | Phase accumulation, unnatural | Cut competing tracks instead |
| Boost/cut too narrow | Resonant, unnatural sound | Match Q to the musical content |
| EQ soloed sounds good, disappears in mix | Wrong context | Always EQ in full mix context |
| Not enough low-end on kick/bass | Thin, powerless mix | Check HPF frequency—too high? |
| Too much 2–5 kHz everywhere | Harsh, fatiguing mix | Distribute frequencies carefully |
| Not checking phase | Comb filtering when summed | Check phase correlation after EQ |

## Frequency Analysis — Technique Guide

## Frequency Ranges Quick Reference

| Range | Frequency | Character | Issues |
|-------|-----------|-----------|--------|
| **Sub-bass** | 20–60 Hz | Feel, not hear. Physical rumble | Build-up causes mud, eats headroom |
| **Bass** | 60–250 Hz | Fundamental of low instruments | Masking with kick/bass, boominess |
| **Low-mid** | 250–500 Hz | Body, warmth, fullness | Mud zone! #1 problem area in amateur mixes |
| **Mid** | 500 Hz–2 kHz | Core tone, presence, definition | Nasal, honky, boxy when excessive |
| **High-mid** | 2–5 kHz | Attack, clarity, cut-through | Harshness, ear fatigue |
| **Presence** | 5–8 kHz | Sibilance, air, detail | Sss sounds, brittle-ness |
| **Air** | 8–20 kHz | Sparkle, shimmer, space | Can sound artificial if overdone |

## Instrument Frequency Zones

| Instrument | Fundamental | Body | Presence | Air | Problem Zones |
|-----------|-------------|------|----------|-----|---------------|
| Kick drum | 40–100 Hz | 60–200 Hz | 2–5 kHz | — | 200–400 Hz (boxy) |
| Snare | 150–250 Hz | 200–400 Hz | 3–5 kHz | 8–12 kHz | 400–600 Hz (boxy) |
| Hi-hat | — | 200–300 Hz | 3–5 kHz | 8–15 kHz | 300–500 Hz (ring) |
| Tom | 60–200 Hz | 200–400 Hz | 3–5 kHz | 8–12 kHz | 400–600 Hz (boxy) |
| Bass | 40–100 Hz | 100–250 Hz | 1–2 kHz | 5–8 kHz | 200–400 Hz (mud) |
| Electric guitar | 80–150 Hz | 200–600 Hz | 2–5 kHz | 8–12 kHz | 300–500 Hz (boxy) |
| Acoustic guitar | 80–150 Hz | 150–400 Hz | 2–5 kHz | 8–15 kHz | 200–400 Hz (mud) |
| Piano | 27–200 Hz | 200–600 Hz | 2–5 kHz | 8–15 kHz | 300–500 Hz (mud) |
| Organ | 30–100 Hz | 100–400 Hz | 2–4 kHz | 8–12 kHz | 200–500 Hz (mud) |
| Synth pad | 40–200 Hz | 200–600 Hz | 2–4 kHz | 8–12 kHz | 300–500 Hz (mud) |
| Lead vocal | 80–300 Hz | 300–800 Hz | 2–5 kHz | 8–15 kHz | 300–500 Hz (nasal), 200–300 Hz (boxy) |
| Background vox | 100–300 Hz | 300–800 Hz | 2–5 kHz | 8–15 kHz | Same as lead |
| Strings | 60–200 Hz | 200–600 Hz | 2–5 kHz | 8–18 kHz | 200–400 Hz (mud) |
| Brass | 60–200 Hz | 200–600 Hz | 2–4 kHz | 8–12 kHz | 300–500 Hz (nasal) |
| Sax | 80–200 Hz | 200–600 Hz | 2–4 kHz | 8–12 kHz | 300–500 Hz (nasal) |
| Flute | 200–400 Hz | 400–800 Hz | 2–4 kHz | 8–15 kHz | 400–600 Hz (boxy) |
| 808 / Sub-bass | 30–60 Hz | 60–100 Hz | 1–2 kHz | — | 100–200 Hz (mud) |

## The Spectrum Analyzer as a Tool

### How to read a spectrum analyzer
- **X-axis:** frequency (Hz), logarithmic scale
- **Y-axis:** amplitude (dB)
- **Color/Opacity:** persistence (brighter = more frequent)
- **Spectral tilt:** overall slope of the mix — should be gently downward

### What to look for
| Pattern | Meaning | Action |
|---------|---------|--------|
| Flat line from 20–20k | Too much noise or saturation | Check for hiss, reduce saturation |
| Big bump at 40–60 Hz | Sub-bass build-up | HPF unnecessary tracks, cut sub on master |
| Mountain at 200–500 Hz | Mud zone build-up | Cut 3–6 dB with wide Q on multiple tracks |
| Valley at 1–3 kHz | Lacks presence | Check if vocals need boost here |
| Steep drop after 10 kHz | Lacks air | Add gentle HBF shelf boost |
| Rising energy in high frequencies | Harsh mix | Cut high-mid on harsh elements |
| Narrow spike | Room resonance or feedback | Notch cut with very narrow Q |

### Spectrum by Genre
| Genre | Characteristic Shape |
|-------|---------------------|
| Pop | Gentle slope, even energy across spectrum, slight high boost |
| Rock | Emphasis on 100–400 Hz (guitars), 2–4 kHz presence |
| EDM | Strong sub 30–80 Hz, dip at 200–400 Hz, air boost 10k+ |
| Hip-Hop | Heavy sub 30–80 Hz (808), clear 2–4 kHz for vocals |
| Metal | Wall of sound 200–500 Hz, aggressive 2–5 kHz |
| Jazz | Gentle slope, less sub, natural mids |
| Classical | Very gentle slope, wide dynamic range, no limiting |

## Using a Spectrum Analyzer in Your Workflow

### 1. Identify masking
Solo two competing instruments together (e.g. kick + bass). Look for overlapping peaks in the same frequency range. The louder one masks the quieter. Cut one or the other.

### 2. Set HPF points
Watch the analyzer as you sweep HPF up. Stop when you see the fundamental starting to roll off. That's your HPF sweet spot.

### 3. Find resonances
With a narrow Q boost (6–10 dB), sweep through the spectrum. When a frequency sounds awful (ringing, harsh, boomy), cut it with a narrow Q at that frequency.

### 4. Compare to reference
Load a reference track. Match LUFS level (reference may be louder). Compare spectral shapes. Where does your mix have more/less energy? Adjust.

### 5. Check translation
Run pink noise through your mix. The spectrum should be relatively flat (within ±3 dB from 100 Hz to 10 kHz). Big deviations indicate translation problems.

## Common Frequency Problems & Solutions

| Problem | Frequency | Fix |
|---------|-----------|-----|
| Muddy low end | 200–500 Hz | Cut 3–5 dB with wide Q on bass, guitars, keys |
| Boxy kick | 300–500 Hz | Cut 2–4 dB with medium Q |
| Honky snare | 800–1.2 kHz | Cut 2–3 dB with narrow Q |
| Nasal vocals | 500–800 Hz | Cut 2–4 dB with narrow Q |
| Harsh cymbals | 5–8 kHz | Cut 2–3 dB with wide Q, or de-esser |
| Sibilant vocals | 5–8 kHz | De-esser with threshold −20 dB |
| Thin mix | 200–400 Hz | Boost 1–2 dB with wide Q on bass elements |
| Lack of presence | 2–4 kHz | Boost 1–2 dB on vocals, guitars |
| Boxy acoustic guitar | 400–800 Hz | Cut 3–4 dB with medium Q |
| Muddy piano | 200–500 Hz | Cut 3–5 dB with wide Q |
| Ringing snare | 800–1.2 kHz | Notch cut with very narrow Q |
| Feedback frequency | varies | Notch cut, find by sweeping |

## Spectral Balance Targets

| Band | Range | Target Level (relative to 1 kHz) |
|------|-------|----------------------------------|
| Sub | 20–60 Hz | 0 to −6 dB |
| Bass | 60–250 Hz | −2 to −4 dB |
| Low-mid | 250–500 Hz | −4 to −8 dB |
| Mid | 500–2 kHz | +0 dB (reference) |
| High-mid | 2–5 kHz | −2 to −4 dB |
| Presence | 5–8 kHz | −4 to −6 dB |
| Air | 8–20 kHz | −8 to −12 dB |

## Advanced Analysis Techniques

### Real-time vs averaged
- Real-time: shows current moment — good for seeing transients
- Averaged (1–10 seconds): shows overall balance — better for mixing decisions
- Use averaged (3 second window) for spectral decisions

### Pink noise matching
- Play pink noise at −18 dBFS RMS
- Your mix should have a similar spectral shape to pink noise (3 dB/octave slope)
- Big deviations indicate problems

### Spectrum of individual elements vs mix
- Solo each instrument, analyze its spectrum
- Then play the full mix, analyze the same instrument's spectrum (it changed because of masking)
- The difference = how much it's being masked by other instruments

## Frequency Analyzer Settings

| Parameter | Recommendation |
|-----------|---------------|
| FFT size | 4096 (good balance of resolution vs speed) |
| Window | Blackman-Harris or Hann |
| Averaging | 500 ms – 1 second for mixing, 50 ms for transient analysis |
| Range | −60 dB to 0 dB for full mix, −40 dB to 0 dB for individual tracks |
| Slope | +3 dB/octave (pink noise correction) for better visual balance |

## GAIN STAGING — Mixing Technique

## Core Philosophy

Gain staging is the practice of managing signal levels at every stage of the signal path to:
- Maintain optimal headroom before clipping
- Ensure analog-modeled plugins behave correctly (emulated voltage levels)
- Keep the mix stable and predictable
- Avoid cascading distortion from stage to stage

**Golden rule:** Set levels early. Every gain stage should have 6–20 dB of headroom before clipping.

## Level Targets by Stage

| Stage | Peak Target | RMS Target | Notes |
|-------|-------------|------------|-------|
| **Recorded track** | −6 to −3 dB | −18 to −12 dB | Clean recording, never clip |
| **After clip gain/trim** | −18 to −12 dB | −22 to −16 dB | Leave room for plugins |
| **After EQ** | −18 to −12 dB | −22 to −16 dB | EQ changes level—re-trim if needed |
| **After compression** | −12 to −6 dB | −18 to −12 dB | Compressor makeup gain |
| **Bus send** | −18 to −12 dB | −22 to −16 dB | Avoid overloading bus |
| **Bus return (compressed)** | −12 to −6 dB | −18 to −12 dB | Bus compressor makeup |
| **Master bus input** | −6 to −3 dB | −14 to −10 dB | Mixed sum of all buses |
| **Master output** | −1 to −0.3 dB | −14 to −8 dB (genre-dependent) | Final limiter handles |

## The −18 dBFS Reference Standard

Most analog-modeled plugins (SSL, Neve, API emulations) are calibrated to **−18 dBFS = 0 dBu** (or +4 dBu depending on the model).

| Plugin Brand | Reference Level | Behavior |
|-------------|-----------------|----------|
| UAD | −18 dBFS = 0 dBu | Optimal saturation |
| Waves | −18 dBFS = 0 dBu | Sweet spot for analog models |
| SSL Native | −18 dBFS = 0 dBu | EQ/comp behave like hardware |
| Softube | −18/−16 dBFS = 0 dBu | Check individual manual |
| FabFilter | Digital (no analog emulation) | No special staging needed |
| Valhalla | Digital | Reverbs—less critical |
| iZotope | Digital | Depends on module |

**Tip:** Place a VU meter plugin on each track and adjust trim so the VU reads around 0 VU (−18 dBFS average).

## Headroom by Genre

| Genre | Mix Bus Peak | Mix Bus RMS/LUFS | Before Mastering |
|-------|-------------|-------------------|------------------|
| Pop | −6 to −3 dB | −16 to −12 dB | Leave 6 dB headroom |
| Rock | −6 to −3 dB | −15 to −10 dB | Leave 6 dB |
| EDM | −6 to −3 dB | −12 to −8 dB | Leave 3–6 dB |
| Hip-Hop | −6 to −3 dB | −14 to −10 dB | Leave 6 dB |
| Jazz | −10 to −6 dB | −20 to −16 dB | Leave 10 dB |
| Classical | −12 to −6 dB | −24 to −18 dB | Leave 12 dB |
| Metal | −6 to −3 dB | −12 to −8 dB | Leave 3–6 dB |

## Digital vs Analog Clipping

| Domain | Clipping Behavior | Acceptable? |
|--------|-------------------|-------------|
| **Digital recording** | Hard clip at 0 dBFS | ❌ Never |
| **Digital plugin input** | Hard clip (or modeled soft) | ⚠️ Check per plugin |
| **Analog model input** | Soft saturation above 0 VU | ✅ Intended (desired color) |
| **Mix bus** | Digital clip | ❌ Before limiter? No |
| **Master limiter** | Final hard ceiling | ✅ At output set to −1 to −0.3 dBTP |

## Practical Workflow

### Step 1: Raw Track Levels
1. Set all faders to 0 dB (unity)
2. Adjust clip gain or input trim until each track peaks at −18 to −12 dB
3. Check the loudest section of the song per track

### Step 2: Bus Levels
1. Route tracks to buses (Drums, Bass, Guitars, Vocals, etc.)
2. Group bus faders should sum to peaks of −12 to −6 dB
3. Check bus meters with all member tracks playing

### Step 3: Mix Bus Level
1. With all buses playing the loudest section:
   - Peak: −6 to −3 dB
   - RMS/LUFS: appropriate for genre
2. If the mix bus exceeds −3 dB peak: reduce bus levels proportionally

### Step 4: Plugin Gain Staging
1. **After inserting EQ:** Check output level—EQ boosts can add 3–6 dB
2. **After compression:** Use makeup gain to restore to pre-compression level
3. **Before saturation:** Check input level—−18 dBFS is the sweet spot
4. **Before reverb:** Pre-fader send or post-fader? Post-fader = level follows fader

## Trim Plugin Usage

Use a trim/gain plugin at the beginning and end of each plugin chain:

```
[Track] → Trim (−18 dB) → EQ → Compressor → Trim (restore) → Fader
```

This lets you:
- A/B plugin chains at matched levels
- Insert analog models at correct input level
- Return to the same level after processing

## Metering Checklist

- **Peak meter:** Watch for digital clipping (red = bad)
- **RMS meter:** Average level (use for consistent staging)
- **VU meter:** 0 VU = −18 dBFS (analog calibration)
- **LUFS meter:** Perceived loudness (for genre targets)
- **Crest factor:** Peak − RMS (indicates dynamic range)

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Mix bus clips at −3 dB | Individual tracks too hot | Reduce all faders by 3–6 dB |
| Plugins sound harsh | Input level too hot for model | Trim input to −18 dBFS |
| Too quiet compared to reference | Not enough level throughout chain | Check each stage, add makeup gain |
| Compression sounds pumpy | Threshold too low or makeup too high | Re-adjust: lower ratio, fix level with trim |
| Headroom too low for mastering | Mix bus peaks at −1 dB | Lower bus levels, leave 3–6 dB |

## MASTERING — Mixing Technique

## What Mastering Is

Mastering is the final step in audio production — preparing a mix for distribution across all playback systems. It's NOT a fix for a bad mix. A good master enhances a good mix; it can't save a bad one.

**What mastering should do:**
- Balance frequency spectrum across the full track
- Control dynamics with transparent compression/limiting
- Set consistent loudness for the target platform/genre
- Ensure mono compatibility and phase coherence
- Add final polish (subtle saturation, width, depth)

**What mastering should NOT do:**
- Fix individual track problems (EQ on individual tracks, not the master)
- Drastically change the mix balance
- Add more than 2–4 dB of gain reduction on the limiter
- Cover up clipping or distortion from the mix

## Mastering Chain (Typical Order)

```
Mix from engineer → 
   1. Subtractive EQ           (cut resonances, rumble)
   2. Multiband compression    (gentle, 1–2 dB per band)
   3. Additive EQ              (tilt, presence, air)
   4. Stereo enhancement       (optional, very subtle)
   5. Saturation/Tape          (warmth, glue)
   6. Limiter                  (final level + ceiling)
   → Mastered file
     (optional: dither for 16-bit export)
```

## LUFS Targets by Genre & Platform

### Streaming Platform Targets (Normalized)

| Platform | Loudness Target | True Peak Ceiling | Notes |
|----------|----------------|-------------------|-------|
| **Spotify** | −14 LUFS-i | −1 dBTP | Normalizes to −14, quieter tracks left alone |
| **Apple Music** | −16 LUFS-i | −1 dBTP | Sound Check normalization |
| **YouTube** | −14 LUFS-i | −1 dBTP | Normalizes all content |
| **Tidal** | −14 LUFS-i | −1 dBTP | Loudness normalized |
| **Amazon Music** | −14 LUFS-i | −1 dBTP | Normalized |
| **Deezer** | −14 LUFS-i | −1 dBTP | Normalized |
| **SoundCloud** | None | −1 dBTP | No normalization, but quality degrades |
| **CD** | Industry standard | −0.1 to −0.3 dBTP | No normalization |

### Genre-Based LUFS Targets (for pre-master)

| Genre | Integrated LUFS | Short-Term LUFS | Peak | Crest Factor |
|-------|----------------|-----------------|------|-------------|
| **Pop** | −9 to −7 | −10 to −8 | −0.5 to −0.3 dBTP | 6–9 dB |
| **Rock** | −10 to −8 | −11 to −9 | −0.5 to −0.3 dBTP | 8–12 dB |
| **EDM** | −8 to −5 | −9 to −6 | −0.5 to −0.3 dBTP | 4–7 dB |
| **Hip-Hop** | −9 to −7 | −10 to −8 | −0.5 to −0.3 dBTP | 5–8 dB |
| **Metal** | −8 to −5 | −9 to −6 | −0.5 to −0.3 dBTP | 4–7 dB |
| **R&B** | −9 to −7 | −10 to −8 | −0.5 to −0.3 dBTP | 6–9 dB |
| **Jazz** | −14 to −12 | −16 to −14 | −1.0 to −0.5 dBTP | 12–16 dB |
| **Classical** | −18 to −14 | −22 to −16 | −1.0 to −0.5 dBTP | 14–20 dB |
| **Lo-fi** | −12 to −10 | −14 to −12 | −1.0 to −0.5 dBTP | 10–14 dB |
| **Reggaeton** | −8 to −6 | −9 to −7 | −0.5 to −0.3 dBTP | 5–8 dB |

### Platform Delivery Recommendation

| Platform | Deliver At | Ceiling |
|----------|-----------|---------|
| **Spotify** | −14 LUFS-i (or leave as-is if louder) | −1 dBTP |
| **Apple Music** | −16 LUFS-i | −1 dBTP |
| **All platforms (one master)** | −14 LUFS-i | −1 dBTP |

**Important:** If your mix is already louder than the target (e.g., −9 LUFS), leave it. Don't make it quieter just to match −14 LUFS — the platform will turn it down anyway. It's better to deliver your intended loudness.

## The Limiter

| Parameter | Effect | Typical Setting |
|-----------|--------|----------------|
| **Threshold** | Where limiting starts | −6 to −2 dB (for 2–6 dB GR) |
| **Ceiling/Output** | Maximum peak level | −1.0 to −0.3 dBTP |
| **Attack** | Release times differ | 0.1–3 ms (program-dependent) |
| **Release** | Automatic or set | Auto / 20–100 ms |
| **Style/Character** | Distortion shape | Modern = clean, Vintage = colored |

### Limiter Gain Reduction Guide

| GR | Sound | Best For |
|----|-------|----------|
| 0–2 dB | Transparent, clean | Jazz, classical, dynamic pop |
| 2–4 dB | Noticeable but musical | Pop, rock, R&B |
| 4–6 dB | Aggressive, dense | EDM, hip-hop, metal |
| 6–10 dB | Heavily squashed, fatiguing | Loudness wars style (avoid) |

**Rule:** Don't exceed 4–6 dB of gain reduction on a single limiter. If more is needed, use two limiters in series (2–3 dB each) or reduce mix level.

## Multiband Compression in Mastering

| Band | Range | Ratio | Attack | Release | GR |
|------|-------|-------|--------|---------|-----|
| **Low** | 20–200 Hz | 1.5:1–2.5:1 | 10–30 ms | 50–100 ms | 1–2 dB |
| **Low-mid** | 200–800 Hz | 1.2:1–2:1 | 10–30 ms | 30–60 ms | 0.5–1.5 dB |
| **High-mid** | 800 Hz–5 kHz | 1.2:1–2:1 | 5–15 ms | 20–40 ms | 0.5–1.5 dB |
| **High** | 5–20 kHz | 1.5:1–2.5:1 | 5–10 ms | 20–50 ms | 0.5–2 dB |

**Total multiband GR:** 2–6 dB maximum across all bands.

## EQ in Mastering

### Subtractive EQ (First)
- HPF at 15–30 Hz (remove subsonic rumble)
- Notch out resonant frequencies (narrow Q, 1–3 dB cut)
- Gentle cut at 200–400 Hz if muddy (−1 to −2 dB)
- Gentle cut at 2–5 kHz if harsh (−0.5 to −1.5 dB)

### Additive EQ (Second)
- Shelf boost at 30–60 Hz for low-end weight (+0.5 to −2 dB)
- Presence at 2–4 kHz (+0.5 to −1.5 dB)
- Air shelf at 10–16 kHz (+0.5 to −2 dB)

**Rule of mastering EQ:** Never boost more than 2 dB. If you need more, go back to the mix.

## Saturation in Mastering

| Type | Effect | Amount |
|------|--------|--------|
| **Tape** | Warmth, compression, high-end smoothing | Very subtle (VU at 0–+2) |
| **Tube** | 2nd order harmonics, warmth | Very gentle |
| **Console** | Subtle glue, 3D depth | Very light (bus at unity) |
| **Clipping (soft)** | Loudness before limiter | 1–3 dB of clip reduction |

**Best practice:** Apply saturation before the limiter. The limiter will catch any peaks the saturation creates.

## Stereo Imaging in Mastering

| Technique | Effect | Caution |
|-----------|--------|---------|
| **M/S EQ — high shelf on sides** | Wider top end | Check mono compatibility |
| **M/S EQ — HPF sides at 200 Hz** | Clearer mono low end | Can make bass feel disconnected |
| **Stereo spread (1–5%)** | Wider overall | Mono issues above 5% |
| **Don't touch** | Safe, professional | Safest option |

**Conservative approach:** Widen by at most 3% in the high frequencies only (above 2–4 kHz).

## Dithering

**What it is:** Low-level noise added when reducing bit depth (e.g., 32-bit float → 16-bit).

| Bit Depth | Dither? | Use Case |
|-----------|---------|----------|
| 24-bit | ❌ Not needed | Final delivery for streaming (Spotify, Apple Music) |
| 16-bit | ✅ **Required** to avoid quantization distortion | CD, some download stores |
| 32-bit float | ❌ Never | Mixing/mastering format only |

**Dither types:**
- **Noise shaping (64–128 Fs):** Pushes noise to less audible frequencies — best for 16-bit
- **Flat dither:** Even noise across all frequencies — safer, more transparent

**Rule:** Apply dither ONCE, at the very end of the chain, after all processing.

## Loudness Normalization & True Peak

| Measurement | Standard | Target |
|-------------|----------|--------|
| **Integrated LUFS** | EBU R128 / ITU-R BS.1770-4 | Genre/platform-dependent |
| **Short-term LUFS** | 3s sliding window | 1–3 LU louder than integrated |
| **Momentary LUFS** | 400ms window | Debugging, not a target |
| **Loudness Range (LRA)** | Statistical loudness variation | 4–10 LU (higher = more dynamic) |
| **True Peak (dBTP)** | Inter-sample peak detection | −1.0 to −0.3 dBTP |

## Checking the Master

### Translation Checklist

| Check | What to Listen For |
|-------|-------------------|
| **Headphones (closed)** | Detail, stereo imaging, sibilance |
| **Headphones (open)** | Soundstage, depth, reverb |
| **Studio monitors** | Balance, imaging, low end |
| **Laptop/phone speakers** | Vocals clear? Bass audible? |
| **Car/Bluetooth speaker** | Low end, energy, balance |
| **Club/loud speakers** | Does it pump? Distort? Lose low end? |
| **Mono** | Does anything disappear? Phase issues? |

### Master Quality Checklist

- [ ] No digital clipping (true peak −1 dBTP or below)
- [ ] LUFS-i appropriate for genre
- [ ] Loudness range (LRA) reasonable for genre
- [ ] Mono compatible (no phase cancellation)
- [ ] No resonant frequencies
- [ ] No pumping or breathing from limiter
- [ ] Bass translates to small speakers
- [ ] Vocals are clear and present
- [ ] Doesn't sound over-compressed or lifeless
- [ ] Matches reference track loudness (A/B within ±1 LU)

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Limiter pumping | Release too fast or gain reduction too high | Slow release, lower threshold, or chain two limiters |
| Distortion in limiter | Too much limiting (6 dB+) | Reduce mix bus level before limiter |
| Harsh/brittle | Too much top-end EQ boost | Reduce high shelf to 0.5–1 dB |
| Muddy/low-end unclear | Too much low-end buildup | Gentle HPF at 20–30 Hz, cut 200–400 Hz |
| Too quiet vs reference | Not enough limiting or mix is | Check mix level, increase limiter GR to 3–5 dB |
| Missing low end on small speakers | Sub-bass only, no harmonics | Add saturation to bass frequencies (tube/tape) |
| Narrow compared to reference | Not enough width | Subtle M/S enhancement in high frequencies |
| Over-compressed/lifeless | Limiter GR > 6 dB | Lower threshold, reduce mix level |
| Phase issues in mono | Stereo widener or reverb | Check correlation, keep > +0.3 |
| Not loud enough for genre | Needs more limiting or mix is quiet | Medium: reduce mix bus 3 dB, gain on limiter 3 dB |

## Mid-Side Processing — Technique Guide

## What is Mid-Side?

Mid-Side (M/S) is a stereo encoding/decoding technique that separates audio into:
- **Mid (M):** Everything panned center (L + R) — mono content
- **Side (S):** Everything panned wide (L − R) — stereo difference content

```
Traditional Stereo (L/R):
  Left ───┬─── L signal
           │
  Right ──┴─── R signal

Mid-Side (M/S):
  Mid   = L + R   → Everything that's the same in both channels (center)
  Side  = L − R   → Everything that's different (stereo width/difference)
```

## Encoding / Decoding

| Conversion | Formula | Notes |
|-----------|---------|-------|
| L/R → M/S | M = L + R, S = L − R | Encode for processing |
| M/S → L/R | L = M + S, R = M − S | Decode to normal stereo |

## When to Use M/S

| Situation | Why M/S? |
|-----------|----------|
| Widen or narrow a mix | Boost or cut the Side channel |
| Fix mono compatibility | Process Mid and Side differently, keep Mid clean |
| Brighten without making sides harsh | EQ Mid and Side separately |
| Bass reinforcement | EQ only the Mid (bass is typically center) |
| Widen a specific element | Convert to M/S, process only Side |
| Master bus EQ | EQ Mid for punch, Side for air |
| Mastering width control | Solo Side to hear artifacts, fix them |

## M/S EQ

### Mix bus M/S EQ
| Frequency | Mid | Side | Effect |
|-----------|-----|------|--------|
| 20–100 Hz | +1–2 dB | −6 dB or HPF | Tighter low end, more mono bass |
| 100–250 Hz | Keep flat | −3 to −6 dB | Cleaner low-mid, less muddy width |
| 250–500 Hz | Keep flat or −1 dB | −2 to −4 dB | Cleaner mud zone in sides |
| 2–5 kHz | Keep flat | +1–2 dB | Wider presence without harsh center |
| 8–15 kHz | Keep flat or −1 dB | +2–4 dB | Sparkle without eating vocal air |
| 15–20 kHz | Keep flat | HPF at 18 kHz | Clean extreme highs |

### Instrument-specific M/S EQ
| Instrument | Mid EQ | Side EQ |
|-----------|--------|---------|
| Kick | Full range, punch at 60 Hz | HPF 200 Hz, keep only attack click |
| Snare | Full range, body at 200 Hz | HPF 500 Hz, let snare ring in sides |
| Bass | Full range, cut 200–300 Hz | HPF 200 Hz, amp grit in sides |
| Vocals | Full range, presence 3–5 kHz | HPF 300–500 Hz, reverb in sides |
| Guitars | Cut 300–500 Hz, boost 2–4 kHz | Full range, enhance for width |

## M/S Compression

### Mid compression (parallel-capable)
| Setting | Value | Why |
|---------|-------|-----|
| Ratio | 2–4:1 | Gentle glue for center |
| Attack | 10–30 ms | Preserve transients |
| Release | 100–300 ms | Natural recovery |
| Mix | 30–60% | Parallel blend for keeping punch |

### Side compression
| Setting | Value | Why |
|---------|-------|-----|
| Ratio | 3–6:1 | Tame wild stereo elements |
| Attack | 5–15 ms | Catch fast transients in sides |
| Release | 50–150 ms | Let side energy breathe |
| GR | 3–6 dB | Controlled width |

## M/S Saturation

| Type | Mid | Side | Effect |
|------|-----|------|--------|
| Tape | Light (1–2%) | Moderate (3–5%) | Warmth+width |
| Tube | Very light (0.5–1%) | Light (1–2%) | Subtle harmonic enhancement |
| Transformer | Moderate (3–5%) | Off | Punch for center |
| Hard clip | Off | Light (1–2%) | Aggressive width |

## M/S Reverb

| Parameter | Mid | Side |
|-----------|-----|------|
| Reverb type | Room or plate | Hall or large room |
| Pre-delay | 20–30 ms | 10–20 ms |
| Decay | 1.0–1.5 s | 2.0–3.0 s |
| Mix | 10–20% | 30–50% |
| HPF | 300–500 Hz | 500–800 Hz |
| LPF | 8–10 kHz | 12–15 kHz |

**Result:** Clear center with the instrument, wide reverb tail in the sides. Creates depth without smearing the center image.

## Common M/S Processing Chains

### For a wider master
1. **Encode** L/R → M/S
2. **Side EQ:** Boost 10–15 kHz by 2–4 dB
3. **Side EQ:** Cut 200–300 Hz by 3–6 dB (clean muddy width)
4. **Mid EQ:** Boost 60–100 Hz by 1–2 dB (solid center bass)
5. **Side compressor:** 4:1, 5 ms attack, 50 ms release, 3 dB GR
6. **Decode** M/S → L/R
7. **Correlation meter:** Check it stays above 0

### For cleaner vocals in a dense mix
1. **Encode** L/R → M/S
2. **Mid EQ:** Boost 3–5 kHz by 1–2 dB (vocal clarity)
3. **Side EQ:** Cut 3–5 kHz by 2–3 dB (reduce competing elements)
4. **Mid compressor:** 3:1, 15 ms attack, 100 ms release
5. **Decode** M/S → L/R

### For tight bass with stereo top
1. **Encode** L/R → M/S
2. **Mid EQ:** Boost 60–100 Hz by 2 dB (bass fundamental)
3. **Side EQ:** HPF 200 Hz (remove bass from sides)
4. **Side EQ:** Boost 5–15 kHz by 3 dB (shimmer)
5. **Decode** M/S → L/R

## Width Control

| Desired Width | Mid Level | Side Level |
|---------------|-----------|------------|
| Mono | +6 dB (gain) | −∞ (mute) |
| Narrow | +3 dB | −6 to −3 dB |
| Normal | 0 dB | 0 dB |
| Wide | −1 to −2 dB | +2 to +4 dB |
| Extreme | −3 dB | +6 dB |

## Mono Compatibility with M/S

| Fact | Why |
|------|-----|
| M/S is naturally mono-compatible | Mono sums to Mid only — no cancellation |
| Side processing disappears in mono | That's correct — stereo content shouldn't appear in mono |
| Heavy Side EQ can cause phase issues | Use minimum phase EQ on sides, or check in mono |
| Side compression is mono-safe | Only affects side content |

**Rule:** Process the Mid channel as if it's the final mono mix. Everything that matters in mono comes from the Mid.

## Tools that Support M/S

| Tool Type | Examples |
|-----------|----------|
| Dedicated M/S encoder | Voxengo MSED, Brainworx bx_solo |
| EQ with M/S | FabFilter Pro-Q 3, TDR SlickEQ M, Waves F6 |
| Compressor with M/S | FabFilter Pro-C 2, Brainworx bx_townhouse |
| Limiter with M/S | Brainworx bx_limiter, DMG Audio Limitless |
| Saturation with M/S | Soundtoys Decapitator (via M/S matrix) |
| Full M/S channel strip | Brainworx bx_digital V3 |
| Mastering suite | iZotope Ozone (M/S module) |

## M/S Workflow Steps

1. **Insert M/S encoder** at the beginning of the chain
2. **Process Mid** (EQ, compression, saturation — anything that affects center)
3. **Process Side** (EQ, width, reverb — anything that affects width)  
4. **Insert M/S decoder** at the end of the chain
5. **Check in mono** — does center still sound good?
6. **Check correlation meter** — is it above 0?
7. **A/B against original** — is it actually better?

## Common Mistakes

| Mistake | Result | Fix |
|---------|--------|-----|
| Over-boosting side | Phasey, unnatural width | Keep side boost ≤ 4 dB |
| Not checking mono | Center disappears in mono | Check mono frequently |
| HPF side too high | Side sounds thin | Max HPF at 300 Hz for sides |
| Too much side compression | Width pumping | Use slower release on side comp |
| EQing both sides identically | Same as EQing stereo | Think differently for M vs S |
## Mid-Side — Quick Reference Cards

### EQ by Position
| Frequency | Mid | Side |
|-----------|-----|------|
| Sub (20–60 Hz) | Full range | −∞ (HPF) |
| Bass (60–250 Hz) | Full range | −6 dB |
| Low-mid (250–500 Hz) | Keep flat | −3 dB |
| Mid (500–2 kHz) | Keep flat | Keep flat |
| High-mid (2–5 kHz) | Keep flat | +1 dB |
| Presence (5–8 kHz) | Keep flat | +2 dB |
| Air (8–15 kHz) | Keep flat | +3 dB |

## Mixing Workflow — Technique Guide

## The Professional Workflow Pyramid

```
         ┌─────────────┐
         │  MASTERING   │  ← Final polish, LUFS target, limiting
        ┌┴─────────────┴┐
        │   REFINEMENT   │  ← Reverb, delay, spatial FX, automation
       ┌┴───────────────┴┐
       │   COMPRESSION    │  ← Dynamic control, glue, parallel
      ┌┴─────────────────┴┐
      │  EQUALIZATION (EQ)  │  ← Frequency carving, masking, tone
     ┌┴───────────────────┴┐
     │   GAIN STAGING       │  ← Levels, headroom, −18 dBFS reference
    ┌┴─────────────────────┴┐
    │    ORGANIZATION        │  ← Track naming, colors, bus routing, session prep
```

## Phase-by-Phase Workflow

### Phase 1: Organization (10 min)
- Name all tracks. Be specific: "Kick_IN", "Snare_Top", "Bass_DI", "Vox_Ld_Double"
- Color-code by bus family (drums=purple, bass=blue, guitars=orange, keys=teal, vocals=pink)
- Route to buses: Drums Bus, Bass Bus, Guitar Bus, Keys Bus, Vocal Bus, FX Bus
- Set bus colors to match family
- Mute all FX sends initially
- Set all faders to 0 dB, pans to center

### Phase 2: Gain Staging (15 min)
- **Goal:** Each track peaks around −18 dBFS to −12 dBFS
- Solo each track, set input trim/clip gain to hit −18 dBFS average
- Use a VU meter plugin or LUFS meter set to −18 dBFS = 0 VU
- Drums: kick −12 dBFS peak, snare −12 dBFS peak, overheads −15 dBFS
- Bass: −18 dBFS average, −12 dBFS peak on attack
- Guitars: −18 dBFS average
- Vocals: −15 dBFS average (lead), −18 dBFS (backgrounds)
- Master bus: all tracks+solo off → master should read −18 dBFS RMS
- **NO PLUGINS ON MASTER BUS YET**

### Phase 3: Balance → Static Mix (30 min)
- **Goal:** Rough balance with just faders, no compression
- Start with kick and snare as anchors
- Bring in bass, match to drums
- Layer in guitars/keys
- Vocals on top — should sit naturally above mix
- Pan: drums from drummer's perspective, bass center, guitars L/R, keys spread, vocals center
- Reference track at −14 LUFS to A/B your level balance
- **RULE:** If you can't hear it, don't add it — turn closer things up first

### Phase 4: EQ (30 min)
- **Goal:** Each instrument has its own frequency space
- Start with cuts, not boosts
- HPF everything that doesn't need low end:
  - Vocals: 80–120 Hz
  - Guitars: 80–120 Hz
  - Keys: 80–120 Hz
  - Snare: 100–200 Hz
  - Overheads: 100–200 Hz
  - Room mics: 100–200 Hz
  - Hi-hat: 300–400 Hz
- Find and remove mud (sweep technique)
- Carve competing elements:
  - Kick vs Bass: kick 60 Hz, bass 100 Hz (or vice versa)
  - Vocals vs Guitars: vocals 2–4 kHz, guitars 200–400 Hz
- Boost for character (sparingly)

### Phase 5: Compression (30 min)
- **Goal:** Control dynamics, glue, shape transients
- Start with tracks that need the most control: vocals, bass, drums
- Use slow attack (10–30 ms) to preserve transients
- Use fast attack (1–5 ms) to tame peaks
- Use slow release (100–300 ms) for natural recovery
- Use fast release (20–50 ms) for pumping effect
- **Bus compression:** 2:1 to 4:1, slow attack, auto release, 1–3 dB reduction

### Phase 6: Spatial FX (20 min)
- **Goal:** Depth — front to back dimension
- Reverb: send from bus, not individual tracks
- Three reverb returns: Room (short), Plate (medium), Hall (long)
- Pre-delay: 10–50 ms to separate dry from wet
- Delay: ping-pong for width, dotted 8th for rhythmic feel
- Automation: reverb/delay send increases during chorus

### Phase 7: Automation (20 min)
- **Goal:** Movement, energy, arrangement shape
- Volume automation for verse/chorus balance
- Filter automation for build-ups
- Reverb send automation for spatial dynamics
- Pan automation for interest
- FX mute automation for clean transitions

### Phase 8: Reference Check & Polish (15 min)
- A/B against reference at matched LUFS level
- Check translation: headphones, monitors, laptop, phone
- Check mono compatibility
- Check phase correlation
- Final LUFS target: −14 to −8 LUFS (depending on genre)

## Time Budget by Project Type

| Project Type | Total Time | Org | Gain Staging | Balance | EQ | Comp | FX | Auto | Polish |
|-------------|-----------|-----|-------------|---------|-----|------|-----|------|--------|
| Simple (8-12 tracks) | 2–3 hrs | 10 min | 15 min | 20 min | 25 min | 25 min | 15 min | 15 min | 10 min |
| Medium (16-24 tracks) | 4–6 hrs | 15 min | 20 min | 30 min | 40 min | 40 min | 25 min | 25 min | 15 min |
| Complex (30-60 tracks) | 8–12 hrs | 20 min | 30 min | 45 min | 60 min | 60 min | 40 min | 40 min | 25 min |
| Dense (60+ tracks) | 16–24 hrs | 30 min | 40 min | 60 min | 90 min | 90 min | 60 min | 60 min | 40 min |

## A/B Workflow

### Every 20 minutes of mixing:
1. **Reset faders** to unity (notch at 0 dB)
2. **Mute all processing**
3. Listen to raw tracks for 30 seconds
4. **A/B your mix** with processing on/off
5. Ask: "Is this actually better?"
6. If yes → continue. If no → remove the last 3 plugins you added
7. Check at **three volume levels**: conversation level (65 dB), moderate (80 dB), loud (90 dB)

## Decision Fatigue Prevention

| Problem | Solution |
|---------|----------|
| Endless EQ tweaking | Set a timer: 3 minutes per track max |
| Too many plugin options | Limit yourself to 3 EQ types, 3 comp types per session |
| Can't decide between two settings | Go with the first one that worked | 
| Losing perspective | Walk away for 10 minutes. Listen from outside the room |
| Ear fatigue | Mix at 75–80 dB SPL max. Take a 5-min break every hour |

## Checklist

- [ ] All tracks named and color-coded
- [ ] Buses routed and colored
- [ ] Gain staging done: peak target −18 dBFS
- [ ] Rough balance with faders only
- [ ] HPF applied where appropriate
- [ ] Mud removed with subtractive EQ
- [ ] EQ carving between kick/bass and vocal/guitar
- [ ] Compression on tracks that need control
- [ ] Bus compression with 1–3 dB reduction
- [ ] Reverb sends with pre-delay
- [ ] Volume automation for arrangement
- [ ] Reference A/B at matched level
- [ ] Mono check
- [ ] Phase correlation check
- [ ] Translation check (3+ listening systems)
- [ ] LUFS target met for genre

## PARALLEL PROCESSING — Mixing Technique

## Concept

Parallel processing blends a heavily processed version of a signal with the dry original. This gives you the benefits of extreme processing (compression, distortion, saturation) without losing the original dynamics and transients.

**Formula:** `Output = Dry × (1 − blend) + Wet × blend`

## Parallel Compression (New York Compression)

The most common parallel technique. Heavily compress a duplicate of the signal, then blend it under the dry.

| Setting | Typical Range |
|---------|---------------|
| **Ratio** | 8:1–20:1 (heavy) |
| **Attack** | 1–10 ms |
| **Release** | 10–50 ms |
| **Threshold** | Low (−20 to −40 dB) |
| **Blend** | 10–40% wet |
| **Result** | Punchy transients + dense sustain |

### By Instrument

| Instrument | Blend | Comp Ratio | Attack | Character |
|-----------|-------|-----------|--------|-----------|
| **Drums (bus)** | 20–40% | 10:1–20:1 | 1–5 ms | Huge, punchy, roomy |
| **Kick** | 10–25% | 8:1–15:1 | 1–3 ms | Sustained weight |
| **Snare** | 15–30% | 10:1–20:1 | 1–5 ms | Thick, roomy |
| **Vocals** | 10–30% | 8:1–12:1 | 5–15 ms | Present, dense |
| **Bass** | 20–40% | 8:1–15:1 | 10–30 ms | Sustained, even |
| **Guitars** | 20–40% | 10:1–20:1 | 5–15 ms | Wall of sound |
| **Mix bus** | 10–20% | 4:1–8:1 | 10–30 ms | Glue + energy |

## Parallel Distortion/Saturation

| Instrument | Effect | Blend | Processing |
|-----------|--------|-------|-----------|
| **Bass/808** | Harmonics for small speakers | 10–30% | Distort hard, HPF at 100–200 Hz, blend with clean sub |
| **Drums** | Grit, aggression | 10–25% | Tape saturation or soft clip on drum bus |
| **Vocals** | Presence, edge | 5–15% | Tube saturation, blend subtly |
| **Guitars** | Extra grit | 20–40% | Overdrive on duplicate, blend |
| **Mix bus** | Analog warmth | 5–15% | Tape or console sat, very subtle |

## Parallel Reverb

Send a signal to a reverb bus with extreme reverb settings, blend to taste.

| Setting | Typical |
|---------|---------|
| **Reverb type** | Hall or Plate |
| **Decay** | 2–4 s (longer than normal) |
| **Pre-delay** | 20–50 ms |
| **HPF on reverb** | 200–500 Hz |
| **LPF on reverb** | 8–12 kHz |
| **Blend** | 5–25% |

## Parallel EQ

| Technique | Effect |
|-----------|--------|
| **Parallel highs boost** | Air without phase shift on main signal |
| **Parallel lows boost** | Sub without mud on main signal |
| **Parallel mid scoop** | Extra clarity without thinning main signal |
| **Parallel mid boost** | Presence without harshness |

## Parallel Delay

Create a dedicated delay bus with:
- Filtered delays (HPF 300 Hz, LPF 7 kHz)
- Heavy compression on delays
- Saturation on delays

Blend 5–20% for atmospheric width.

## Parallel Bus Setup (DAW Template)

```
DRY CHANNEL                     PARALLEL BUS
  │                                 │
  ├─ Subtle EQ                     ├─ Heavy compression (10:1)
  ├─ Light comp (2–3 dB GR)        ├─ Extreme EQ (if desired)
  └─ Fader at 0 dB                 ├─ Saturation
                                    └─ Send fader at −10 to −20 dB
                                        │
                                        ▼
                                  MIX OUTPUT
                              (Dry + Parallel blended)
```

## Sidechain in Parallel

| Technique | Key Input | Blend | Effect |
|-----------|-----------|-------|--------|
| **Kick to parallel drum bus** | Kick | 20–40% | Pumping drum room |
| **Vocal to parallel instrument bus** | Vocal | 10–20% | Instruments duck for vocal |
| **Kick to parallel bass sat bus** | Kick | 20–30% | Bass distortion pumps rhythmically |

## Common Issues & Fixes

| Problem | Fix |
|---------|-----|
| Phase cancellation between dry and wet | Check polarity, use linear-phase EQ on wet bus |
| Mix gets muddy | HPF parallel bus at 100–300 Hz |
| Too much of the effect | Reduce blend to 5–15% — subtlety is key |
| Distortion too harsh | HPF distortion at 200–500 Hz, blend lower |
| Parallel comp pumping | Slower release on parallel comp (100–300 ms) |
| No dry signal left | Ensure blend is below 50% (dry should dominate) |

## Phase Alignment — Technique Guide

## What is Phase?

Phase describes the timing relationship between two or more audio signals. When two microphones capture the same sound source at different distances, the signals arrive at slightly different times, creating phase relationships.

```
In phase:    ╱‾‾‾╲     ╱‾‾‾╲     ╱‾‾‾╲
             ╱     ╲   ╱     ╲   ╱     ╲    → Sum: +6 dB
            ╱       ╲ ╱       ╲ ╱       ╲

Out of phase: ╱‾‾‾╲     ╲╱‾‾‾╱     ╲╱‾‾‾╱
              ╱     ╲   ╱     ╲   ╱     ╲  → Cancellation
             ╱       ╲ ╱       ╲ ╱       ╲
```

## Phase Relationships

| Relationship | Angle | Result |
|-------------|-------|--------|
| Perfectly in phase | 0° | Full sum (+6 dB) |
| Partially in phase | 0–90° | Partial sum (+1 to +5 dB) |
| Out of phase | 90–180° | Partial cancellation (−1 to −∞ dB) |
| Perfectly out of phase | 180° | Full cancellation (−∞ dB, silence) |

## Common Phase Alignment Scenarios

### 1. Multi-mic drum kit
| Mic Pair | Typical Distance | Phase Issue | Fix |
|----------|-----------------|-------------|-----|
| Kick (in + out) | 2–6 inches apart | Comb filtering, thin sound | Slide out track until kick punch aligns, or use phase alignment plugin |
| Snare (top + bottom) | 3–5 inches apart | Thin snare, missing low-end | Flip bottom snare polarity, then time-align or use polarity-only |
| Overheads vs close mics | 3–6 feet away | Drums sound phasey, lack punch | Align overheads to close mics with sample delay (1–3 ms = 1–3 feet) |
| Room mics vs close mics | 10–20 feet away | Washed out, no punch | Align room to close mics, or let room be late (creates depth) |

### 2. Bass recording
| Setup | Issue | Fix |
|-------|-------|-----|
| DI + amp mic | Comb filtering from latency | Align DI to amp mic (DI is faster), or use 100% one signal |
| Multi-amp rig | Phase smear between cabs | Time-align each amp track, sum to mono, check for phasiness |

### 3. Acoustic guitar
| Setup | Issue | Fix |
|-------|-------|-----|
| X/Y stereo pair | Minimal phase issues (coincident) | Already aligned, just check polarity |
| Spaced pair | Comb filtering on center | Time-align, or use Mid-Side technique |
| DI + mic | Comb filtering | Align tracks, or nudge one. Remove DI below 300 Hz |

### 4. Vocals
| Setup | Issue | Fix |
|-------|-------|-----|
| Multi-mic (front + side) | Hollow or phasey sound | Choose one mic, don't blend two unless carefully aligned |
| Lead + double track | Natural chorusing is fine | Don't nudge — slight timing differences are desirable |
| Reverb send | Pre-delay creates natural separation | No phase correction needed, pre-delay separates dry/wet |

## Tools for Phase Alignment

| Tool | Method | Use Case |
|------|--------|----------|
| Polarity flip (ø button) | Inverts waveform 180° | Quick snare top/bottom fix |
| Sample delay | Shifts track by samples (0.02 ms increments) | Drum multi-mic alignment |
| Nudge (ms) | Shifts track by milliseconds | Overhead vs close mic alignment |
| Phase correlation meter | Visualizes L/R phase relationship | Monitoring stereo phase |
| Auto-align plugin | Analyzes and shifts automatically | Complex multi-mic setups |
| Spectrum analyzer | Shows comb filtering pattern | Detecting phase issues visually |

## How to Align Multi-Mic Drums

### Step-by-step:
1. Play kick drum
2. Zoom in to sample level on kick-in and kick-out tracks
3. Identify the transient start (initial hit) on both tracks
4. Slide the kick-out track left/right until the transients align
5. Zoom in further and align within 1–2 samples
6. Sum to mono and check: does it sound punchy or hollow?
7. Repeat for snare (top + bottom, flip polarity first)
8. Align overheads to closest close mic (usually snare)

### Napkin math:
- Sound travels ~1 foot per millisecond
- If overheads are 3 feet above the snare → delay ≈ 3 ms
- Align overheads by pulling them 3 ms earlier (sample delay)
- Fine-tune by listening: punchy = aligned, hollow = misaligned

## Polarity vs Phase

| Aspect | Polarity | Phase |
|--------|----------|-------|
| What it is | 180° waveform inversion | Time delay (any degree) |
| How to fix | Flip button (ø) | Nudge, sample delay, alignment plugin |
| Frequency dependent? | No — all frequencies flipped | Yes — different frequencies cancel differently |
| Visual indicator | Waveform is mirrored vertically | Waveform is shifted horizontally |
| When to use | Snare top/bottom, kick in/out | Overheads, room mics, spaced pairs |

## Phase Correlation Meter Guide

### What the meter shows:
| Reading | Meaning | Action |
|---------|---------|--------|
| +1.0 | Perfectly mono (L=R) | OK, but check width |
| +0.5 to +1.0 | Typical for stereo mixes | Normal |
| 0.0 | No correlation (L different from R) | Check for issues |
| −0.5 to 0.0 | Some phase cancellation | Fix phase on problematic elements |
| −1.0 | Perfectly out of phase | CRITICAL — fix immediately, likely mono-incompatible |

### How to check:
1. Play the full mix
2. Look at the correlation meter
3. If it dips below 0, identify the element causing it:
   - Mute suspect tracks one by one
   - When meter returns to positive → found the problem
4. Fix by flipping polarity, time-aligning, or adjusting panning

## Common Phase Problems & Solutions

| Problem | Symptom | Fix |
|---------|---------|-----|
| Thin kick | Kick-in + kick-out phasey | Align tracks, then try polarity flip |
| Thin snare | Snare top + bottom cancelling | Flip bottom snare polarity, then time-align |
| Drums lack punch | Overheads + close mics misaligned | Align overheads to snare (closest point) |
| Bass sounds hollow | DI + amp mic comb filtering | Align or use only DI or only amp |
| Mix loses energy in mono | Multiple tracks out of phase | Find culprits with correlation meter |
| Specific frequency missing | Comb filtering notch | Identify distance causing the notch, adjust alignment |
| Vocal sounds phasey | Two mics on same source | Choose one mic. Hide, don't blend |
| Stereo width collapses | L/R phase issues | Fix with alignment or polarity flip |

## Mono Compatibility Checklist

- [ ] Sum mix to mono (mono button on master)
- [ ] Kick drum still punchy? (should be)
- [ ] Snare still present? (should cut through)
- [ ] Bass still solid? (critical — bass often cancels in mono)
- [ ] Vocals still clear and centered?
- [ ] Does the mix lose energy or sound hollow?
- [ ] Are any instruments disappearing?
- [ ] Check correlation meter: stays above 0?
- [ ] Check specific problem frequency: sweep 100–500 Hz

If mono sounds significantly worse, find the phase problem:
1. Mute tracks until mono sounds good
2. Unmute tracks one by one
3. When mono breaks → that track is the problem
4. Fix: polarity flip → sample delay → re-record if needed

## Advanced Techniques

### MS phase alignment
- Convert to M/S
- Process sides differently from center
- Center: keep tight phase coherence
- Sides: more lenient with phase, creates width

### All-pass filtering
- Shifts phase at specific frequencies
- Can align specific frequency ranges without delaying the whole signal
- Use with caution — can create weird artifacts

### Haas effect
- Delay one side by 10–30 ms to create width
- Can cause phase issues in mono
- Use MS technique instead for better mono compatibility

## Recording for Phase Coherence

### 3:1 Rule
```
Distance between mics ≥ 3 × distance from each mic to source
```
Example: If each mic is 1 foot from the source → mics must be at least 3 feet apart.

### Coincident vs spaced
| Technique | Phase Coherence | Stereo Width |
|-----------|----------------|--------------|
| X/Y (coincident) | Excellent | Moderate |
| ORTF | Very good | Good |
| Spaced pair | Poor (requires alignment) | Excellent |
| Mid-Side | Excellent | Excellent |

## REVERB & DELAY — Mixing Technique

## Reverb Types

| Type | Character | Decay Range | Best For |
|------|-----------|-------------|----------|
| **Room** | Natural, close, intimate | 0.3–1.0 s | Drums, guitars, vocals (intimate) |
| **Hall** | Grand, expansive, lush | 1.5–4.0 s | Vocals (ballads), strings, piano |
| **Plate** | Smooth, bright, dense | 1.0–3.0 s | Vocals (pop), snare, guitar solo |
| **Spring** | Bouncy, metallic, vintage | 1.0–3.0 s | Guitar amps, dub, surf rock |
| **Chamber** | Medium, natural echo | 0.8–2.0 s | Classic rock vocals, drums |
| **Ambient** | Subtle, spatial, texture | 1.0–6.0 s | Pads, sound design, atmosphere |
| **Convolution** | Real space impulse response | Variable | Realistic spaces, specific halls |
| **Shimmer** | Ethereal with pitch shift | 2.0–6.0 s | Ambient pads, dream pop |
| **Gate** | Abrupt cutoff of tail | 0.1–0.8 s | 80s snare, special effects |

## Reverb Parameters

| Parameter | Effect | Typical Range |
|-----------|--------|---------------|
| **Decay time** | How long the reverb rings | 0.3–4.0 s |
| **Pre-delay** | Gap before reverb starts | 0–100 ms |
| **Size** | Perceived room size | Small → Large |
| **Damping** | High-frequency absorption in the reverb tail | 0–100% (more = darker) |
| **Diffusion** | Density of reflections | Low = discrete echoes, High = smooth |
| **Mix/Wet** | Dry/wet balance | 5–40% typically |
| **Early reflections** | First distinct echoes | On/Off + level |

## Pre-Delay Guide

| Pre-delay | Effect | Best For |
|-----------|--------|----------|
| 0–10 ms | Reverb blends with source, intimate | Drums, lo-fi |
| 15–30 ms | Source stays clear, reverb behind | Vocals (pop/rock), snare |
| 30–60 ms | Clear separation, vocal stays forward | Ballads, lead vocals |
| 60–120 ms | Extreme separation, rhythmic effect | Special effects, sparse mixes |

**Formula:** `Pre-delay (ms) = 60000 / BPM × 1/16 note` (for a rhythmic pre-delay)

## Reverb by Instrument

| Instrument | Best Reverb Types | Decay | Pre-delay | Mix |
|------------|-------------------|-------|-----------|-----|
| **Lead vocal** | Plate, Hall | 1.2–2.5 s | 15–40 ms | 15–30% |
| **Background vox** | Hall, Plate | 1.5–3.0 s | 10–20 ms | 25–40% |
| **Snare** | Room, Plate | 0.5–1.5 s | 5–30 ms | 15–30% |
| **Kick** | Room, Plate | 0.3–1.0 s | 0–10 ms | 10–25% |
| **Toms** | Room, Hall | 0.5–1.5 s | 5–15 ms | 15–25% |
| **Acoustic guitar** | Room, Plate | 0.8–1.5 s | 10–20 ms | 15–25% |
| **Electric guitar** | Room, Plate | 0.5–1.5 s | 10–20 ms | 15–30% |
| **Piano** | Hall, Room | 1.0–3.0 s | 5–20 ms | 15–30% |
| **Strings** | Hall | 1.5–3.5 s | 10–30 ms | 20–40% |
| **Synth pads** | Hall, Shimmer | 2.0–4.0 s | 10–30 ms | 25–50% |

## Delay Types

| Type | Time | Character | Best For |
|------|------|-----------|----------|
| **Slapback** | 80–150 ms | Single repeat, classic | Rockabilly, country, rap vocals |
| **Ping-pong** | 200–500 ms | Alternates L/R, wide | Vocals, leads, dub |
| **Tape delay** | Variable | Warm, degraded repeats | Vintage, dub, lo-fi |
| **Digital delay** | Variable | Clean, precise repeats | Modern pop, EDM |
| **Filtered delay** | Variable | Dark repeats (LPF at ~5 kHz) | Warm, sits behind source |
| **Rhythmic** | Note subdivision | Syncopated, groove | EDM, pop (1/8, dotted 1/8, 1/16) |
| **Pitch delay** | Variable | Repeats pitch-shifted | Special effects, ambient |

## Tempo-Synced Delay Times

| Note Division | 80 BPM | 100 BPM | 120 BPM | 140 BPM |
|---------------|--------|---------|---------|---------|
| Dotted 1/2 | 1125 ms | 900 ms | 750 ms | 643 ms |
| 1/2 | 750 ms | 600 ms | 500 ms | 429 ms |
| Dotted 1/4 | 562 ms | 450 ms | 375 ms | 321 ms |
| 1/4 | 375 ms | 300 ms | 250 ms | 214 ms |
| Dotted 1/8 | 281 ms | 225 ms | 188 ms | 161 ms |
| 1/8 | 187 ms | 150 ms | 125 ms | 107 ms |
| Dotted 1/16 | 141 ms | 112 ms | 94 ms | 80 ms |
| 1/16 | 94 ms | 75 ms | 62 ms | 54 ms |

**Formula:** `Delay (ms) = 60000 / BPM × note_division` (1/4 note = 1, 1/8 = 0.5, dotted 1/8 = 0.75)

## Delay by Genre

| Genre | Delay Type | Timing | Feedback | Mix |
|-------|-----------|--------|----------|-----|
| **Pop** | Ping-pong, digital | 1/4 or 1/8 | 1–3 repeats | 10–20% |
| **Rock** | Slapback, tape | 80–150 ms | 1–2 repeats | 10–20% |
| **Hip-Hop** | Slapback, filtered | 100–200 ms | 1–3 repeats | 15–25% |
| **R&B** | Ping-pong, filtered | 1/4 dotted | 1–3 repeats | 15–25% |
| **EDM** | Rhythmic, ping-pong | 1/8, 1/16, dotted | 2–6 repeats | 15–30% |
| **Reggae/Dub** | Tape, filtered, spring | 1/4 or 1/8 | Many repeats | 20–40% |
| **Country** | Slapback | 100–150 ms | 1 repeat | 10–15% |

## Creating Depth with Reverb & Delay

### The Depth Framework (Front to Back)

| Plane | Dry/Wet | Reverb Type | Decay | Pre-delay | Brightness |
|-------|---------|-------------|-------|-----------|------------|
| **Front** (focal) | 90–100% dry | Minimal or none | — | — | Bright |
| **Mid-front** | 80–90% dry | Room | 0.5–1.0 s | 10–20 ms | Full |
| **Mid** | 70–85% dry | Plate | 1.0–1.8 s | 15–30 ms | Moderate |
| **Mid-back** | 60–75% dry | Hall | 1.5–2.5 s | 20–40 ms | Damped |
| **Back** (ambient) | 40–60% dry | Large Hall | 2.0–4.0 s | 30–60 ms | Dark |

### Combining Reverb + Delay

```
Send 1: Short Room (15% wet) — gives space without losing focus
Send 2: Ping-pong Delay (1/8 notes, 2 repeats) — rhythmic interest
Send 3: Plate Reverb (25% wet, 1.5s decay) — main ambience
Send 4: Hall Reverb (30% wet, 2.5s decay) — depth (used on chorus/important parts)
```

## Reverb Bus Processing

| Technique | Effect |
|-----------|--------|
| **Reverb HPF at 300 Hz** | Prevents muddy, boomy reverb tail |
| **Reverb LPF at 8–12 kHz** | Smoother, more natural tail |
| **Sidechain compress reverb from dry signal** | Reverb swells between phrases |
| **Pre-delay EQ cut at 2–3 kHz** | Vocal clarity preserved |
| **Reverb with modulation** | Lush, chorused tail |
| **Gated reverb** | 80s drum sound, controlled decay |

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Muddy/washy mix | Too much reverb on too many tracks | Reduce decay, cut low end from reverb |
| Vocals buried | Reverb too wet or long | Shorten decay, reduce wet mix, increase pre-delay |
| Harsh reverb tail | Too much high-frequency in reverb | LPF reverb send at 8–10 kHz |
| Washed out low end | Sub-bass in reverb | HPF reverb send at 200–400 Hz |
| No sense of space | Too dry or pre-delay too short | Add room reverb, increase pre-delay slightly |
| Reverb sounds fake | Wrong type for material | Match reverb type to genre/era |
| Tempo doesn't match | Delay not synced | Sync to tempo, 1/8 or dotted 1/8 |

## Sound Design — Technique Guide

## What is Sound Design?

Sound design is the art of creating, manipulating, and layering sounds to serve a musical or narrative purpose. In mixing, sound design techniques help you create unique tones, textures, and atmospheres that make a production stand out.

## Core Sound Design Categories

| Category | What You Create | Tools |
|----------|----------------|-------|
| **Layering** | Combined sounds from multiple sources | Samplers, audio tracks, grouping |
| **Resynthesis** | New sounds from analyzed audio | Granular, spectral processors |
| **Processing** | Transformation via effects | All FX types |
| **Sampling** | Using found sounds as instruments | Samplers, drum machines |
| **Synthesis** | Creating from scratch | Synths, modular, physical modeling |
| **Field recording** | Capturing real-world sounds | Microphones, recorders |

## Layering Techniques

### Drum layering
| Drums to Layer | Ratio | Result |
|---------------|-------|--------|
| Kick (sub) + Kick (click) | 70/30 to 80/20 | Full kick with attack + sub |
| Snare (body) + Snare (crack) | 50/50 | Thick snare with punch |
| 808 (sub) + Kick (attack) | 70/30 | 808 sub with kick transient |
| Clap + Snare | 60/40 | Wider, more aggressive snare |
| Kick + 808 (pitch to key) | 50/50 | Bass note + kick attack |

### Texture layering
| Layer 1 | Layer 2 | Layer 3 | Result |
|---------|---------|---------|--------|
| Synth pad | Reverse cymbal | Saw wave | Dreamy intro |
| Sub bass | Distorted mid | Noise | Aggressive bass |
| Piano | Pad | Reverb tail | Cinematic |
| Guitar | Synth strings | Noise floor | Ambient texture |

### Vocal layering
| Layer | Processing | Blend |
|-------|-----------|-------|
| Lead (dry) | None | 100% |
| Double (same take) | Slight detune (±3 cents) | 30–50% |
| Double (different take) | Slight delay (15–25 ms) | 30–50% |
| Harmonies | HPF 200 Hz, LPF 10 kHz | 20–40% |
| Whisper/falsetto | Saturation, reverb | 10–20% |

## Synthesis Techniques for Mixing

### Adding harmonics to weak sources
| Source | Technique | Tool |
|--------|-----------|------|
| Bass with weak fundamental | Add sine wave at root note | Sub generator |
| Thin synth | Add saw wave an octave above | Layered oscillator |
| Weak snare | Add noise burst at snare transient | Noise generator |
| Dull vocal | Add subtle sine at 3–5 kHz | Saturation harmonics |

### Creating sub-bass from existing sources
1. Duplicate the bass track
2. Insert a low-pass filter at 80–100 Hz
3. Add a sine wave sub generator (or pitch shift down 12 semitones)
4. Blend with original: 20–40% sub
5. HPF the sub at 30 Hz (remove subsonic rumble)
6. Result: rumble that translates to small speakers

### Transient design
| Technique | Description | Effect |
|-----------|-------------|--------|
| Transient layering | Layer a short "click" sound on attack | More definition |
| Noise burst | White noise burst at transient | Electronic punch |
| Pitch blip | Quick pitch sweep up at attack | Whistle/impact |
| Reverse attack | Reverse the first 50 ms | Suck-in effect |

## Resynthesis & Granular

### Granular techniques
| Technique | Settings | Effect |
|-----------|----------|--------|
| Cloud | Grain size 50–200 ms, spread 200 ms | Ethereal pad |
| Time stretch | Grain size 20–50 ms, no pitch change | Slow motion |
| Pitch shift | +12 semitones, grain 10 ms | Fairy vocals |
| Scrub | Fast playback speed variation | Glitch texture |
| Freeze | Sustain a single grain/loop | Droning texture |

### Spectral processing
| Technique | Effect | Use |
|-----------|--------|-----|
| Spectral freeze | Sound sustains infinitely | Drones, transitions |
| Spectral gate | Only pass frequencies above threshold | Metallic, thin |
| Spectral blur | Smear frequencies over time | Dreamy transitions |
| Frequency shift | Shift all frequencies by constant Hz | Alien, dissonant |

## Sampling & Found Sound

### Processing found sounds
| Source | Processing | Musical Use |
|--------|-----------|-------------|
| Glass breaking | Pitch down, reverb | Cinematic impact |
| Door creak | Time stretch 4x, filter | Ambient texture |
| Water drip | Pitch up 3 octaves, delay | Hi-hat replacement |
| Street noise | Band-pass 200–500 Hz, compress 10:1 | Lo-fi background |
| Coin drop | Sampled, pitched across keyboard | Percussion instrument |
| Paper tear | Saturation, pitch modulation | Snare layer |

### Creating risers and impacts

**Riser formula:**
```
Layer 1: Synth saw with filter sweep (closed → open)
Layer 2: White noise with same filter sweep
Layer 3: Reverse crash cymbal
Layer 4: Pitch riser (0 → +12 semitones)
Layer 5: Reverb (hall, 4 s) on all layers
→ Automation: Volume +10 dB over 4 bars
```

**Impact formula:**
```
Layer 1: Kick drum (full transient)
Layer 2: White noise burst (100–200 ms)
Layer 3: Low sine (30–60 Hz, 200–500 ms)
Layer 4: Cymbal crash
Layer 5: Sub layer (sine at root note of song, held 1–2 s)
→ Reverb (plate or hall, 2–3 s) on impact group
→ Limit: −3 dB true peak
```

## Creating Textures

### Ambient textures
| Texture | Method | Processing |
|---------|--------|-----------|
| Wind | White noise with slow LFO filter | LPF 2 kHz, reverb hall 5 s |
| Space | Reversed reverb tail frozen | Granular freeze + huge reverb |
| Machinery | Rhythmic noise gate on filtered noise | BPF 400–800 Hz, gate sync'd to tempo |
| Pulse | Low sine wave (40 Hz) with tremolo | Tremolo sync'd to 1/4 notes |
| Breath | Quiet vocal inhale processed | HPF 500 Hz, reverb 3 s |

### Bass textures
| Texture | Method | Processing |
|---------|--------|-----------|
| Growl | Saw wave + distortion + filter | LPF with envelope follower |
| Sub | Pure sine + subharmonic | Saturation for harmonics |
| Reese | Two detuned saws (+5/−5 cents) | HPF 100 Hz, chorus |
| Metal | Square wave + ring mod | Distortion, BPF 500 Hz |
| Pluck | Short attack envelope on saw | Decay 200 ms, no sustain |

### Pad textures
| Texture | Method | Processing |
|---------|--------|-----------|
| Dreamy | Saw wave + chorus + reverb | Slow attack (500 ms), slow release |
| Dark | Square wave + low-pass filter | Cut at 500 Hz, slight detune |
| Shimmer | Saw + pitch shifter (+12) + reverb | Bright reverb 5 s |
| Movement | LFO on filter + pan | Rate sync'd to 1/2 note |

## Sound Design by Genre

### EDM
| Element | Sound Design |
|---------|-------------|
| Build-up riser | White noise + synth pitch rise + filter sweep (4–8 bars) |
| Drop impact | Kick + noise burst + sub hit + reverb |
| Growl bass | Reese bass with envelope filter (wobble) |
| Hi-hat roll | 1/32 note hats with velocity increase |
| Snare build | Snare hit repeated every 1/4, velocity increasing |

### Hip-Hop / Trap
| Element | Sound Design |
|---------|-------------|
| 808 | Layered sine + saw, pitch slide, saturation |
| Snare roll | Trap snare, 1/16 notes, volume automation |
| Hi-hat triplet | Triplet pattern, flam on first hit |
| Vocal tag | Pitch down (−8 to −12), reverb, stereo spread |
| Ad-lib | Single word, pitch up (+5), delay, pan |

### Ambient / Cinematic
| Element | Sound Design |
|---------|-------------|
| Drone | Sustained pad with slow LFO (0.1 Hz) |
| Texture | Field recording processed with reverb |
| Riser | Reversed piano + cymbal swell |
| Impact | Low brass hit + sub rumble |
| Atmosphere | Multiple pads layered with different filter settings |

## Processing Chains for Sound Design

### From any source → pad texture
```
Source → Time stretch (200–400%) → HPF 200 Hz → Reverb (hall, 5 s) → Chorus (slow) → Filter sweep (automated) → Output
```

### From any source → percussive element
```
Source → Gate with short envelope (50 ms) → Pitch shift (+12 or +24) → Saturation → Limiter → Output
```

### From vocal → ethereal texture
```
Vocal → Pitch shift (+12) → Reverse → Reverb (cathedral, 4 s) → Granular (grain 100 ms, spread 300 ms) → LPF 5 kHz → Output
```

### From noise → transition effect
```
Noise → Band-pass (300 Hz to 8 kHz, automated sweep) → Reverb → Pitch rise (optional) → Sidechain compression to kick → Output
```

## Sound Design Workflow

1. **Source selection:** Choose or record the raw material
2. **Layering plan:** What layers will create the final sound?
3. **Processing chain:** What FX transform each layer?
4. **Blend:** Balance layers with gain and panning
5. **Texture:** Add reverb, delay, modulation for depth
6. **Movement:** Automate parameters for evolution
7. **Context check:** Does it work in the mix?

### Mindset principles
- **Less is more:** Start with 2 layers, add if needed
- **Subtract before adding:** Filter/cut before adding more layers
- **Process in context:** Sound design in solo ≠ sound design in mix
- **Reuse and mutate:** Save interesting chains as presets
- **Limitations are creative:** Use found sounds, cheap gear, bad mics
- **Record everything:** Your phone recordings, accidental sounds, room ambiance

## Creative Constraints as a Tool

| Constraint | Creative Outcome |
|-----------|-----------------|
| Only 3 layers | Deeper processing on each |
| No synth — only found sounds | Unique, organic textures |
| Only 2 FX plugins | Focus on impactful choices |
| 1 octave range only | Minimalist, focused |
| Source must be a single sample | Resourceful, creative transformation |
| Analog gear only | Characterful, imperfect |

## STEREO IMAGING — Mixing Technique

## Core Concepts

| Term | Definition |
|------|-----------|
| **Pan** | Placement of a mono signal in the stereo field (L–C–R) |
| **Stereo width** | Perceived spread of sound between L and R |
| **Mono compatibility** | How the mix sounds when collapsed to mono |
| **Phase correlation** | Phase relationship between L and R channels |
| **Mid/Side** | L+R (Mid) and L−R (Side) decomposition |
| **Haas effect** | Delaying one channel by 5–30 ms for perceived width |

## Pan Law

Different DAWs apply different pan laws — the perceived level change when panning a signal off-center.

| Pan Law | Center Level | Hard-panned Level | Common In |
|---------|-------------|-------------------|-----------|
| −3 dB | 0 dB | −3 dB | Logic, Pro Tools, Studio One |
| −4.5 dB | 0 dB | −4.5 dB | Cubase, Nuendo |
| −6 dB | 0 dB | −6 dB | Ableton Live, FL Studio (default) |
| 0 dB | 0 dB | 0 dB (louder when panned) | Some DJ software |

In **FL Studio (default −6 dB):** Center is 0 dB, hard-panned is −6 dB. This means:
- A center signal sounds 6 dB louder than a hard-panned one
- When mixing, a panned part may need compensation

## Standard Pan Placement

### Drum Kit (from drummer's perspective)

| Element | Pan |
|---------|-----|
| Kick | Center |
| Snare | Center (or slightly L 10–20%) |
| Hi-Hat | L 40–60% |
| Ride | R 40–60% |
| Toms | L30/R30 (as positioned in kit) |
| Overheads | L100/R100 (stereo pair) |
| Room mics | L100/R100 |

### Typical Bus Panning

| Element | Pan |
|---------|-----|
| Lead vocal | Center |
| Bass | Center |
| Kick | Center |
| Snare | Center |
| Rhythm guitars (doubled) | L70–100 / R70–100 |
| Piano | L60–80 / R60–80 |
| Strings | L80–100 / R80–100 |
| Pads | L80–100 / R80–100 |
| Background vocals | L40–60 / R40–60 |
| FX/Ad-libs | L50–100 / R50–100 |
| Percussion | L30–70 / R30–70 |

## Creating Width

### 1. Double Tracking (Most Effective)
Record the same part twice, pan one hard L and one hard R.
- Works for: Guitars, vocals, synths, percussion
- Best when takes are slightly different (timing, tone, vibrato)
- Avoids phase cancellation in mono

### 2. Haas Effect (Delay Panning)
Delay one channel by 5–30 ms for perceived width.
- **5–15 ms:** Subtle widening
- **15–30 ms:** Wide, noticeable effect
- **> 35 ms:** Becomes a discrete echo (slapback delay)
- **Caution:** Haas effect can cause phase cancellation in mono

### 3. Mid/Side Processing

| Technique | Mid (L+R) | Side (L−R) | Effect |
|-----------|-----------|------------|--------|
| Wide mix | Keep full | Boost 2–4 dB | Wider field |
| Narrow mix | Boost 1–3 dB | Cut 2–4 dB | Tighter, more mono compatible |
| EQ only sides | Unchanged | EQ (boost highs, cut lows) | Wide top end, solid mono low end |
| Compress only sides | Unchanged | Compress | Tightens wide elements |

### 4. Stereo Widener Plugins
- Use sparingly (10–30% mix)
- Can cause mono incompatibility
- Better on FX, pads, and synths than on bass or kick
- Listen in mono to verify!

### 5. Reverb with Different L/R Decays
Use two different reverb settings on L and R (or different room sizes) for natural width.

## Mono Compatibility

### Why Check Mono?
- Clubs, bars, and many venues play in mono
- Bluetooth speakers and phones often collapse to mono
- Streaming platforms may downmix
- FM radio broadcasts in mono

### The Mono Check Workflow
1. Switch your mix to mono (use a plugin or DAW feature)
2. Listen for:
   - **Level drops** — elements that disappear or get quieter
   - **Phase cancellation** — hollow, thin sounds
   - **Muddy low end** — low frequencies that clash
3. Fix issues before returning to stereo

### Ensuring Mono Compatibility

| Technique | Why It Works |
|-----------|-------------|
| Keep kick, snare, bass, lead vox centered | Core elements always present in mono |
| True doubles (not copy-paste) | No phase cancellation |
| Check bass is in phase (correlation > 0) | Low end survives mono collapse |
| Avoid Haas effect on bass and kick | Phase issues in mono |
| Use correlation meter (target > +0.5) | Ensures good mono compatibility |
| Check in mono before exporting | Final verification |

## Phase Correlation Meter

| Reading | Meaning |
|---------|---------|
| +1.0 | Perfectly in phase (mono) |
| +0.5 to +1.0 | Good stereo, no phase issues |
| 0.0 to +0.5 | Wide stereo, some phase difference |
| −0.5 to 0.0 | Phase problems — check mono |
| −1.0 | Completely out of phase (cancel in mono) |

**Target:** Keep correlation above +0.3 during the loudest sections.

## Width by Bus

| Bus | Panning Strategy | Width |
|-----|-----------------|-------|
| **Kick** | Mono, center | 100% center |
| **Snare** | Mono, center | 100% center |
| **Bass** | Mono, center | 100% center |
| **Drums** | Stereo overheads + panning | Moderate |
| **Guitars (rhythm)** | Double tracked, hard L/R | Very wide |
| **Guitars (lead)** | Center or slight offset | Narrow |
| **Vocals (lead)** | Mono, center | 100% center |
| **Vocals (BG)** | Spread L/R | Wide |
| **Keys/Piano** | Stereo L/R | Wide |
| **Pads** | Stereo, wide | Very wide |
| **FX** | Any, depends on effect | Variable |
| **Percussion** | Spread across field | Moderate |

## Frequency-Dependent Width

A common professional technique: the mix gets wider as frequency increases.

| Range | Width | Technique |
|-------|-------|-----------|
| **Sub (20–200 Hz)** | Mono (100% center) | Bass, kick, 808 — always mono |
| **Low (200–500 Hz)** | Narrow | Keep low-mid information centered |
| **Mid (500 Hz–2 kHz)** | Moderate | Vocals center, secondary elements spread |
| **High (2–8 kHz)** | Wide | Cymbals, FX, stereo effects |
| **Air (8–20 kHz)** | Very wide | Reverb tails, stereo wideners |

**Implement with M/S EQ:** HPF the Side channel at 200–400 Hz to keep low end mono.

## Common Issues & Fixes

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| Disappears in mono | Phase cancellation from Haas/widener | True double instead, check phase correlation |
| Bass phase issues | Stereo bass processing | Keep bass mono, HPF Side channel at 200 Hz |
| Mix too narrow | Everything is centered | Widen doubled guitars, spread BG vox |
| Mix too wide | Core elements panned away from center | Move kick/snare/bass/vocal back to center |
| Unbalanced stereo | All elements panned to one side | Balance L/R, use correlation meter |
| Phasy, hollow sound | Haas effect on critical element | Replace Haas with true double or EQ differences |
| Cymbals too wide | Overheads spread beyond 100% | Keep overheads at max 100% L/R |

## Transient Shaping — Technique Guide

## What is a Transient?

A transient is the initial burst of energy when a sound begins. It contains the attack, the "hit," the "pluck," the "punch." The transient is followed by the sustain (how long the sound lasts) and release (how it fades).

```
Amplitude
    │
   ██     ┌────────────────────────────┐
   ██     │   SUSTAIN / BODY            │
   ██     │       ╲                     │
   ██     │        ╲       RELEASE      │
 ▄▄██▄▄   │         ╲          ╲        │
 ├────┤   │          ╲           ╲       │
TRANSIENT └────────────┴───────────┴─────→ Time
```

## Transient vs Sustain

| Phase | Duration | Character | Control |
|-------|----------|-----------|---------|
| Transient | 0–20 ms | Attack, hit, punch, click | Transient shaper |
| Body | 20–200 ms | Tone, note, pitch | Compressor (attack time) |
| Sustain | 200 ms–5 s | Ring, decay, tail | Limiter, volume |
| Release | Variable | Fade, reverb tail | Gate, volume automation |

## What a Transient Shaper Does

| Parameter | Range | Effect |
|-----------|-------|--------|
| Attack | 0–100% | Makes transients louder (more punch) or quieter (softer attack) |
| Sustain | 0–100% | Makes the body/tail louder or quieter |
| Sensitivity | 0–100% | Threshold for detecting transients |
| Speed/Decay | 10–500 µs | How quickly the shaper returns to neutral after a transient |

### Attack control
```
     ┌──── Original ────┐     ┌─ Attack Boosted ─┐     ┌─ Attack Reduced ─┐
     ██                  │     █████              │     ██                │
     ██                  │     ██                 │     ██                │
  ▄▄██▄▄                │  ▄▄████▄▄              │  ▄▄██▄▄              │
  ├────┤                 │  ├────┤                 │  ├──┤               │
```

### Sustain control
```
     ┌──── Original ────┐     ┌─ Sustain Boosted ─┐     ┌─ Sustain Reduced ┐
     ██                  │     ██                  │     ██                │
     ██                  │     ████████████████    │     ██                │
  ▄▄██▄▄                │  ▄▄██▄▄                │  ▄▄██▄▄              │
  ├────┤                 │  ├────┤                 │  ├────┤               │
```

## When to Use Transient Shaping

| Instrument | Goal | Attack | Sustain | Why |
|-----------|------|--------|---------|-----|
| Kick drum | More punch | +3–6 dB | −2 to −4 dB | Tight, defined kick |
| Kick drum | Softer/round | −2 to −4 dB | 0 dB | Vintage/acoustic feel |
| Snare | More crack | +4–8 dB | −2 to −4 dB | Pop, rock |
| Snare | More body | 0 dB | +2–4 dB | Ballads, slower songs |
| Toms | More attack | +3–6 dB | 0 dB | Cut through mix |
| Bass (finger) | More definition | +3–6 dB | −2 dB | Each note pops out |
| Bass (pick) | Softer attack | −2 dB | +2 dB | Rounder tone |
| Acoustic guitar | More pick attack | +2–4 dB | −1 dB | Cut through dense mix |
| Electric guitar | Tame pick noise | −3 to −6 dB | 0 dB | Cleaner rhythm parts |
| Vocals | De-ess naturally | −3 dB at 5–8 kHz | 0 dB | Tame sibilance |
| Hi-hat | More stick | +3–6 dB | −4 dB | Defined rhythm |
| Overheads | More attack | +1–3 dB | −2 dB | Punchy cymbals |
| Room mics | More room | 0 dB | +6–12 dB | Big drum sound |
| Piano | More hammer | +2 dB | −1 dB | Percussive feel |
| Synth pluck | Exaggerated attack | +6–10 dB | −6 dB | EDM/electronic |

## Transient Shaper vs Compressor

| Aspect | Transient Shaper | Compressor |
|--------|-----------------|------------|
| Controls | Transient vs sustain | Overall dynamics |
| Attack | Instant (sub-ms) | 0.1–100 ms |
| Ratio | Continuous (0–100%) | Discrete (2:1 to 20:1) |
| Frequency dependent? | No (broadband) | No (unless multiband) |
| Makeup gain | Automatic (usually) | Manual |
| Best for | Reshaping | Controlling peaks / smoothing |
| Use together | Shape first, then compress | Compress after shaping |

### Recommended chain:
```
Transient Shaper → Compressor → Limiter
        ↓               ↓           ↓
   Shape punch     Smooth out    Catch peaks
```

## Transient Shaping by Genre

| Genre | Character | Transient Approach |
|-------|-----------|--------------------|
| Rock | Aggressive, punchy | Boost attack on drums (+4–8 dB), cut sustain (−2–4 dB) |
| Pop | Clean, controlled | Moderate attack (+2–4 dB), slight sustain cut (−1–2 dB) |
| EDM | Hyper-defined | Heavy attack boost on kicks (+6–10 dB), heavy sustain cut (−6 dB) |
| Hip-Hop | Hard-hitting 808 | Boost attack on kick (+6 dB), preserve 808 sustain (0 dB) |
| Metal | Blast beat clarity | Boost attack on kick/snare (+6 dB), cut sustain (−3 dB) |
| Jazz | Natural, dynamic | Minimal shaping (±1–2 dB max) |
| Classical | Preserve natural dynamics | No shaping, or very gentle (+1 dB) |
| Lo-Fi | Soft, rounded | Reduce attack on everything (−2–4 dB) |

## Multiband Transient Shaping

### Why multiband?
- Kick has transient energy in both low (punch) and high (click) ranges
- Bass might need attack in low-mid (finger) but not in sub
- Vocals need de-essing (high frequencies only)

### Typical bands
| Band | Range | Application |
|------|-------|-------------|
| Low | 20–200 Hz | Kick punch, bass attack, sub 808 control |
| Low-mid | 200–800 Hz | Snare body, guitar attack, mud control |
| Mid | 800–3 kHz | Vocal presence, snare crack |
| High | 3–20 kHz | Cymbal attack, sibilance, air |

## Transient Shaping + Compression Strategy

### Option A: Transient shaper first
```
TS → Compressor → Output
```
- TS shapes the attack/sustain balance
- Compressor catches leftover peaks
- Best for: drums, percussion

### Option B: Compressor first
```
Compressor → TS → Output
```
- Compressor evens out dynamics
- TS restores/reshapes attack afterward
- Best for: vocals, bass (after compression has already smoothed)

## Common Problems & Solutions

| Problem | Cause | Fix |
|---------|-------|-----|
| Artifacts/"zipper" noise | Too much attack boost | Reduce attack amount, increase speed setting |
| Pumps unnaturally | Attack too high on sustain | Reduce sustain, or use faster speed |
| No audible effect | Sensitivity too low | Lower threshold/sensitivity until it activates |
| Too much high-end click | Attack boost affecting sibilance | Reduce attack amount or use multiband |
| Drums sound disconnected | Attack too boosted, sustain too cut | Find balance: attack +3 dB, sustain −2 dB |
| Voice sounds unnatural | Too much de-essing via transient shaper | Use dedicated de-esser instead |

## Popular Transient Shapers

| Name | Controls | Best For |
|------|----------|----------|
| SPL Transient Designer | Attack, Sustain (2 knobs) | Drums — classic, simple |
| wavesfactory Transient | Attack, Sustain, Sensitivity | All-purpose, affordable |
| Soundtoys Little Plate | N/A (reverb) | Not a TS — use Radiator + transient shaper |
| Native Instruments Transient Master | Attack, Sustain, Sensitivity | Clean, transparent |
| Sonnox TransMod | Attack, Sustain | Subtle, high-quality |
| iZotope Neutron (Transient/Sustain) | Multiband transient shaping | Mix bus, mastering |

## Transient Shaper Workflow

1. **Solo the track**
2. **Set both knobs to 0** (no effect)
3. **Boost Attack 100%** → hear the transient
4. **Cut Sustain 100%** → hear only attack
5. **Find the balance**: gradually reduce attack, add sustain back
6. **A/B**: does the track have more or less punch/body than before?
7. **Check in context**: does it sit better in the mix?
8. **Fine-tune**: adjust by 1–2 dB increments

## Advanced Techniques

### Parallel transient shaping
- Send track to FX channel
- Apply heavy transient shaping on FX (attack +10 dB, sustain −10 dB)
- Blend with dry: 10–30% wet
- Effect: extreme punch without losing body

### Sidechain via transient shaper
- Use transient shaper's detection to trigger sidechain
- Kick triggers sidechain on bass via shaper detection
- More musical than traditional sidechain compression

### Transient shaper on reverb
- Insert transient shaper on reverb return
- Boost attack → reverb transients pop out
- Cut sustain → reverb tail is shorter and tighter
- Effect: clearer reverb with defined attack

## Transient Shaper on Master Bus

| Use | Setting | Caution |
|-----|---------|---------|
| More punch overall | Attack +1–2 dB | Can cause master to distort |
| Tighter mix | Sustain −1–2 dB | Can suck life out of mix |
| Transient detail | Attack +0.5–1 dB | Very subtle, or not at all |
| Smoother mix | Attack −1 dB, Sustain +1 dB | For ballads, classical |

**Rule:** On the master bus, transient shaping should be barely perceptible. If you hear it working, dial it back.