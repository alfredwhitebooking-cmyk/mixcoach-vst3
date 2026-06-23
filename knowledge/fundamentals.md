# MIXING FUNDAMENTALS — Core Knowledge for MixCoach

## GAIN STAGING
The foundation of every great mix. Proper levels prevent distortion and give headroom for processing.

### Targets by genre:
| Genre | Master Peak Target | Track Peak Target | Headroom |
|-------|-------------------|-------------------|----------|
| Pop | -6 dBFS to -3 dBFS | -12 dBFS to -6 dBFS | 6-10 dB |
| Rock | -3 dBFS to -1 dBFS | -10 dBFS to -6 dBFS | 3-6 dB |
| EDM | -0.5 dBFS (limiter) | -8 dBFS to -4 dBFS | 0.5-3 dB |
| Hip-Hop | -0.5 dBFS (limiter) | -8 dBFS to -4 dBFS | 0.5-3 dB |
| Jazz | -6 dBFS to -3 dBFS | -18 dBFS to -10 dBFS | 6-10 dB |
| Classical | -3 dBFS to -1 dBFS | -20 dBFS to -12 dBFS | 6-12 dB |
| Reggaeton | -0.5 dBFS (limiter) | -6 dBFS to -3 dBFS | 0.5-3 dB |
| Metal | -1 dBFS to -0.5 dBFS | -8 dBFS to -4 dBFS | 1-3 dB |

### Key rules:
1. **Never clip in the analog domain.** Digital clipping is harsh; analog clipping can be musical.
2. **Use the -18 dBFS = 0 dBu standard** for analog-emulated plugins (SSL, Neve, API).
3. **Start every track fader at -inf** and bring up one by one: drums first, then bass, then harmonic instruments, then vocals.
4. **The master bus should hit -6 dBFS to -3 dBFS** peak before mastering. This gives the mastering engineer (or limiter) 3-6 dB of headroom.
5. **Check gain reduction on every compressor**: 2-6 dB is typical. > 10 dB means you're squashing the life out of it.

---

## EQ (EQUALIZATION)
The art of carving space for every instrument in the frequency spectrum.

### Frequency Ranges:
| Range | Hz | Character | Problems |
|-------|-----|-----------|----------|
| Sub | 20-60 Hz | Felt more than heard. Power, rumble | Buildup below 30Hz = mud |
| Bass | 60-250 Hz | Body, fullness, warmth | 200-250 Hz boxiness |
| Low Mids | 250-500 Hz | Lower harmonics, warmth | 300-500 Hz muddiness, "honk" |
| Mids | 500 Hz-2 kHz | Presence, body of instruments | 800 Hz-1 kHz "nasal", 1-2 kHz "harsh" |
| Upper Mids | 2-6 kHz | Attack, definition, clarity | 3-5 kHz ear fatigue, sibilance |
| Presence | 6-12 kHz | Air, openness, detail | 8-10 kHz harshness on vocals |
| Air | 12-20 kHz | Brilliance, sparkle | >16 kHz can cause listening fatigue |

### Per-instrument EQ guide:
| Instrument | Low-cut (HPF) | Key frequencies (boost) | Problem frequencies (cut) |
|------------|---------------|------------------------|-------------------------|
| Kick Drum | 30-60 Hz | 60-100 Hz (thump), 2-5 kHz (attack) | 200-400 Hz (boxy), 300 Hz (mud) |
| Snare | 100-200 Hz | 150-250 Hz (body), 3-5 kHz (crack) | 400-600 Hz (boxy) |
| Hi-Hat | 300-500 Hz | 7-10 kHz (sparkle) | 200-400 Hz (clutter) |
| Toms | 60-100 Hz | 200-400 Hz (body), 3-5 kHz (attack) | 300-500 Hz (muddy) |
| Bass Guitar | 40-80 Hz | 60-120 Hz (fundamental), 700-1 kHz (growl) | 200-400 Hz (mud), 1-2 kHz (harsh) |
| 808 | 20-40 Hz | 40-80 Hz (sub), 1.5-3 kHz (click) | 200-400 Hz (mud) |
| Electric Guitar | 80-120 Hz | 2-4 kHz (presence), 120-200 Hz (body) | 200-400 Hz (mud), 5 kHz (harsh) |
| Acoustic Guitar | 200-400 Hz | 1-4 kHz (sparkle), 80-200 Hz (body) | 300-500 Hz (boxy) |
| Piano | 80-200 Hz | 2-4 kHz (presence), 80-120 Hz (body) | 300-500 Hz (mud) |
| Strings | 200-400 Hz | 3-6 kHz (presence), 400-600 Hz (warmth) | 2-3 kHz (harsh) |
| Lead Vocal | 120-200 Hz | 2-4 kHz (presence), 8-12 kHz (air) | 200-400 Hz (mud), 3-5 kHz (sibilance) |
| Backup Vocals | 200-400 Hz | 2-4 kHz (clarity) | Same as lead, less aggressive |
| Synth Pad | 200-400 Hz | 1-3 kHz (presence) | Depends on role in mix |
| Horns | 200-400 Hz | 2-5 kHz (bite), 500-800 Hz (body) | 800 Hz-1 kHz (honk) |

### HPF Guidelines (high-pass filter):
- **Kick**: 30-60 Hz (sub-kick), 20-30 Hz (no sub-kick)
- **Snare**: 100-200 Hz (depends on body desired)
- **Hi-Hat**: 300-500 Hz (remove all body)
- **Toms**: 60-100 Hz
- **Bass**: 40-80 Hz (keep the fundamental)
- **Electric Guitar**: 80-120 Hz
- **Acoustic Guitar**: 200-400 Hz
- **Piano**: 80-200 Hz (depends on arrangement)
- **Lead Vocal**: 120-200 Hz (remove rumble)
- **Backup Vocals**: 200-400 Hz
- **Strings**: 200-400 Hz
- **Synth**: 200-400 Hz (or lower if bass element)
- **Cymbals**: 300-500 Hz
- **FX/Ambience**: 200-400 Hz (or lower for special effects)

### EQ technique tips:
1. **Cut before you boost.** Removing problem frequencies is more natural than boosting what you like.
2. **Use narrow Q (high resonance) for cuts** (problem frequencies), **wide Q for boosts** (musical shaping).
3. **Surgical EQ** (narrow cuts) for resonances, **musical EQ** (wide curves) for tone shaping.
4. **The "sweep trick"**: boost a narrow Q and sweep through frequencies until you find the nastiest sound, then cut there.
5. **HPF everything except kick and bass.** Most instruments don't contribute useful energy below 100-200 Hz. Clearing subsonic rumble gives headroom.
6. **Complementary EQ**: carve space between competing instruments. If the bass is boosted at 100 Hz, cut the kick at 100 Hz, and vice versa.
7. **Mid/Side EQ**: process center (kick, bass, vocals) differently from sides (hi-hats, reverb, guitars).

---

## COMPRESSION
Dynamic control — shaping the envelope of sounds.

### Parameters quick reference:
| Parameter | Fast (1-5 ms) | Medium (10-30 ms) | Slow (50-100 ms+) |
|-----------|--------------|-------------------|-------------------|
| **Attack** | Preserves transients (drums, percussion) | Controls initial punch | Lets everything through, smooths only sustain |
| **Release** | Fast = pumping, aggressive | Natural feel | Slow = constant compression, "glue" |

### Ratio guide:
| Ratio | Use case |
|-------|----------|
| 1.5:1 - 2:1 | Gentle leveling, bus compression, "glue" |
| 3:1 - 4:1 | Standard for vocals, bass, individual tracks |
| 6:1 - 8:1 | Strong control, aggressive limiting |
| 10:1+ | Limiting mode, peak control |
| ∞:1 | Brickwall limiter (true peak limiting) |

### Threshold reference:
- **Subtle compression**: threshold at -10 dB below average RMS, gain reduction 1-3 dB
- **Moderate compression**: threshold at -15 dB below average RMS, gain reduction 3-6 dB
- **Heavy compression**: threshold at -20 dB below average RMS, gain reduction 6-10 dB+

### Per-instrument compression guide:
| Instrument | Attack | Release | Ratio | GR (gain reduction) | Notes |
|------------|--------|---------|-------|---------------------|-------|
| Kick | 1-5 ms | 50-150 ms | 3-5:1 | 3-6 dB | Fast attack = thump, slow = attack |
| Snare | 5-15 ms | 100-200 ms | 3-5:1 | 3-5 dB | Let the crack through |
| Hi-Hat | 5-15 ms | 50-100 ms | 2-4:1 | 1-3 dB | Light touch |
| Toms | 5-10 ms | 100-200 ms | 3-5:1 | 3-6 dB | Punchy |
| Bass | 20-50 ms | 50-100 ms | 3-4:1 | 2-5 dB | Let attack through |
| 808 | 30-60 ms | 200-500 ms | 2-3:1 | 2-4 dB | Slow to preserve sub |
| Electric Guitar | 10-30 ms | 100-300 ms | 3-4:1 | 3-6 dB | Smooth, even |
| Acoustic Guitar | 5-15 ms | 50-150 ms | 2-3:1 | 2-4 dB | Gentle |
| Piano | 20-40 ms | 200-400 ms | 2-3:1 | 2-4 dB | Natural sustain |
| Lead Vocal | 10-30 ms | 50-150 ms | 3-4:1 | 3-6 dB | Smooth, controlled |
| Backup Vocals | 10-20 ms | 100-200 ms | 3-4:1 | 3-5 dB | Blend together |
| Drums Bus | 10-30 ms | 50-100 ms | 2-3:1 | 2-4 dB | "Glue" the kit |
| Master Bus | 30-50 ms | 100-300 ms | 1.5-2:1 | 1-2 dB | Subtle "glue" only |

### Advanced techniques:
1. **Parallel compression** (New York compression): mix a heavily compressed signal (8:1+, 10 dB+ GR) with the dry. Great for drums and vocals.
2. **Sidechain compression**: kick ducks bass. Very common in EDM and pop.
3. **Multiband compression**: compress specific frequency ranges independently. Useful for controlling harsh mids without squashing lows.
4. **Serial compression**: two compressors with low ratios (2:1 each) instead of one with high ratio (4:1). More transparent.
5. **De-essing**: compress only above 3-7 kHz (sidechain de-esser). Essential for vocals.

---

## REVERB & DELAY
Creating space and depth.

### Reverb types:
| Type | Character | Best for |
|------|-----------|----------|
| Plate | Smooth, dense, bright | Vocals, snares |
| Hall | Large, spacious, long decay | Orchestral, ambient, ballads |
| Room | Natural, short decay | Drums, acoustic instruments |
| Spring | Bouncy, bright, "surf" | Guitars, retro sounds |
| Chamber | Warm, medium decay | Vocals, horns |
| Shimmer | Ethereal, pitch-shifted | Pads, ambient |

### Reverb depths by genre:
| Genre | Kick | Snare | Vocals | Mix bus |
|-------|------|-------|--------|---------|
| Pop | None or gated | Medium (1.5-2s hall) | Medium (2s plate) | Subtle room |
| Rock | None | Medium (1.5s plate) | Short (1s chamber) | None |
| EDM | None | Long gated (2-3s) | Medium (2s plate) | Big hall on drops |
| Hip-Hop | None | Short (1s room) | Medium-Long (2-2.5s) | Subtle |
| Jazz | Very short | Short (1s room) | Medium (1.5s chamber) | Live room |
| Classical | Long hall | Long hall | Long (2.5s hall) | Concert hall |
| Reggaeton | None | Short gated | Medium (1.5s plate) | Subtle |
| Metal | None | Short room | Short (0.8s plate) | None |
| R&B | None | Medium (1.5s) | Medium (2s plate) | Subtle room |

### Delay guide:
- **Slap delay** (50-120ms): doubles vocals, adds width to guitars
- **Ping-pong delay** (1/8 note): rhythmic, creates movement
- **Dotted 1/8 note**: classic rockabilly, U2-style
- **1/4 note**: ambient, creates space between phrases
- **Filtered delay** (LPF at 5-8 kHz): prevents muddy buildup
- **Tape delay**: warm, saturated, self-oscillating

---

## STEREO IMAGING
Creating width, depth, and translation to mono.

### Panorama guide:
| Element | Position | Notes |
|---------|----------|-------|
| Kick | Center | Essential for mono compatibility |
| Snare | Slightly left or center (depends) | Room mics can be panned wide |
| Hi-Hat | Slightly right | Audience perspective or drummer perspective |
| Bass | Center | Keep it mono for translation |
| Lead Vocal | Center | The focus of the mix |
| Electric Guitar | Off-center | Often double-tracked L/R |
| Acoustic Guitar | Depends | Can be panned opposite to electric |
| Piano | Full stereo | Lows in center, highs wide |
| Strings | Full stereo | L/R for width |
| Backup Vocals | Wide L/R | Create width around lead |
| FX | Wide | Reverbs, delays, special effects |
| Synth Pads | Full stereo | Create atmospheric width |
| Horns | L/R depending on arrangement | Often panned like a stage |

### Mono compatibility rules:
1. **Always check in mono.** Anything panned hard L/R will collapse to center and lose width.
2. **Phasing from stereo wideners** can cause cancellation in mono.
3. **Keep low frequencies (below 200 Hz) mono** to prevent phase issues.
4. **Mid/Side processing**: boost the sides (above 200 Hz) for more perceived width.
5. **Haas effect**: delay one side by 10-30ms for perceived width. BUT check mono compatibility.

---

## LUFS & LOUDNESS
Mixing for modern streaming platforms.

### Platform targets (Integrated LUFS):
| Platform | Target | Max True Peak | Notes |
|----------|--------|---------------|-------|
| Spotify (normalized) | -14 LUFS | -1 dBTP | Tracks quieter than -14 are NOT boosted |
| Apple Music | -16 LUFS | -1 dBTP | Sound Check normalization |
| YouTube | -14 LUFS | -1 dBTP | Loudness normalization |
| Tidal | -14 LUFS | -1 dBTP | Loudness normalization |
| Amazon Music | -14 LUFS | -1 dBTP | Loudness normalization |
| CD | No limit | -0.1 dBTP (red book) | No loudness normalization |
| Club/Festival | -6 to -4 LUFS | -0.1 dBTP | Heavy limiting for impact |

### LUFS targets by genre (mastered):
| Genre | Integrated LUFS | Short-Term LUFS | True Peak |
|-------|----------------|-----------------|-----------|
| Pop | -9 to -7 | -12 to -8 | -0.5 to -1 dBTP |
| Rock | -10 to -8 | -13 to -9 | -0.5 to -1 dBTP |
| EDM | -7 to -4 | -10 to -6 | -0.3 to -0.5 dBTP |
| Hip-Hop | -8 to -6 | -11 to -7 | -0.3 to -0.5 dBTP |
| Jazz | -14 to -16 | -18 to -14 | -1 to -2 dBTP |
| Classical | -16 to -20 | -20 to -16 | -1 to -2 dBTP |
| Reggaeton | -7 to -5 | -10 to -6 | -0.3 to -0.5 dBTP |
| Metal | -8 to -5 | -11 to -7 | -0.3 to -1 dBTP |
| R&B | -9 to -7 | -12 to -8 | -0.5 to -1 dBTP |
| Latin | -8 to -6 | -11 to -7 | -0.5 to -1 dBTP |

### Crest factor ranges (Peak - RMS):
| Genre | Typical Crest | Character |
|-------|---------------|-----------|
| Pop | 5-8 dB | Balanced, polished |
| Rock | 8-12 dB | Dynamic, punchy |
| EDM | 4-7 dB | Highly compressed, loud |
| Hip-Hop | 5-8 dB | Bass-heavy, controlled |
| Jazz | 10-16 dB | Highly dynamic |
| Classical | 14-20 dB | Very wide dynamic range |
| Reggaeton | 5-7 dB | Sub-driven, compressed |
| Metal | 4-6 dB | Aggressively compressed |
| R&B | 6-9 dB | Smooth, controlled |
| Latin | 7-10 dB | Rhythmic, energetic |

---

## PHASE & CORRELATION
Ensuring mono compatibility and phase coherence.

### Correlation meter guide:
| Value | Meaning |
|-------|---------|
| +1.0 | Perfectly mono (identical L/R) |
| +0.5 to +0.9 | Healthy stereo field |
| 0.0 | No correlation (wide ambience, room mics) |
| -0.3 to 0.0 | Some phase cancellation possible |
| -1.0 | Complete phase cancellation in mono |

### Phase trouble spots:
1. **Multi-mic setups** (drums, guitar cabs): check polarity on each mic
2. **Stereo wideners**: often cause phase issues in mono
3. **M/S processing**: can cause correlation problems if done incorrectly
4. **Parallel compression**: check polarity, keep the dry signal in phase
5. **Reverb returns**: can blur the correlation — not always bad

### Fixes:
1. **Flip polarity** (180° phase reverse) on problematic tracks
2. **Align transients** visually — delay one track by a few ms
3. **Use correlation meter** as a guide, not a dictator. Some genres need low correlation for width
4. **Mono check**: if the mix collapses, you have phase issues

---

## MIXING WORKFLOW (Recommended order)

### Phase 1: PREPARATION
1. Import all tracks, name them
2. Assign colors by instrument family
3. Set up routing (buses, aux sends)
4. Set initial fader levels
5. HPF everything that doesn't need sub-bass

### Phase 2: BALANCE
1. Start with faders at -inf
2. Bring up kick first — it's the foundation
3. Add snare, then bass
4. Add harmonic instruments one by one
5. Vocals last — they sit on top
6. Check balance at low volume (85 dB SPL)

### Phase 3: CORRECTION
1. Resolve frequency conflicts with EQ
2. Control dynamics with compression
3. Fix any phase issues (flip polarity, align)
4. Pan for width and separation

### Phase 4: CREATIVE
1. Add reverb for depth (send to aux)
2. Add delay for movement
3. Automation (volume rides, filter sweeps)
4. Special effects (distortion, modulation)

### Phase 5: MASTERING PREP
1. Reference mix against commercial tracks
2. Check in mono
3. Check on multiple playback systems
4. Final level adjustments
5. Export at -6 dBFS peak for mastering
