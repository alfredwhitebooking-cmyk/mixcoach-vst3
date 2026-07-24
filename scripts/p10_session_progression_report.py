# P10: Show SessionProgression phase in EndOfSessionComponent report

import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'r', encoding='utf-8') as f:
    data = f.read()

total = 0

# ═══ CHANGE 1: Capture phase + progress in refresh() section 4b ═══
old1 = '''        // \u2500\u2500\u2500 4b. Gather session metadata from real engine state
        {
            auto ctx = coachEngine_->buildSessionContext();
            sessionDurationUs_ = ctx.sessionDurationUs;
            achievementCount_  = ctx.achievementCount;
            masterTruePeakDb_  = ctx.masterTruePeak;
            drumTracks_       = ctx.drumTracks;
            bassTracks_       = ctx.bassTracks;
            guitarTracks_     = ctx.guitarTracks;
            keysTracks_       = ctx.keysTracks;
            vocalTracks_      = ctx.vocalTracks;
            fxTracks_         = ctx.fxTracks;
            melodyTracks_     = ctx.melodyTracks;
            unknownTracks_    = ctx.unknownTracks;
        }'''

new1 = '''        // \u2500\u2500\u2500 4b. Gather session metadata from real engine state
        {
            auto ctx = coachEngine_->buildSessionContext();
            sessionDurationUs_ = ctx.sessionDurationUs;
            achievementCount_  = ctx.achievementCount;
            masterTruePeakDb_  = ctx.masterTruePeak;
            drumTracks_       = ctx.drumTracks;
            bassTracks_       = ctx.bassTracks;
            guitarTracks_     = ctx.guitarTracks;
            keysTracks_       = ctx.keysTracks;
            vocalTracks_      = ctx.vocalTracks;
            fxTracks_         = ctx.fxTracks;
            melodyTracks_     = ctx.melodyTracks;
            unknownTracks_    = ctx.unknownTracks;

            // Capture SessionProgression phase + progress
            auto& prog = coachEngine_->getSessionProgression();
            sessionProgressionPhase_    = prog.currentPhase;
            sessionProgressionProgress_ = prog.getOverallProgress();
        }'''

if old1 in data:
    data = data.replace(old1, new1, 1)
    print('CHANGE 1: SessionProgression capture added in refresh()')
    total += 1
else:
    print('CHANGE 1: FAILED')
    idx = data.find('4b. Gather session metadata')
    if idx >= 0:
        print(repr(data[idx:idx+400]))

# ═══ CHANGE 2: Add phase info to session metadata row in drawTrackSummary() ═══
# Find the metadata row that starts with "Session metadata row: duration + achievements + corrections"
# We'll replace the achievement display to also show phase progress

old2 = '''            // Achievements
            auto achArea = metaRow.removeFromLeft(110);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Logros", achArea.removeFromLeft(44), juce::Justification::centredLeft);
            {
                juce::String achStr = juce::String(achievementCount_) + " \\xf0\\x9f\\x8f\\x86";
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(achievementCount_ > 0 ? MixCoachTheme::success() : MixCoachTheme::textMuted());
                g.drawText(achStr, achArea, juce::Justification::centredLeft);
            }'''

new2 = '''            // Achievements + Phase progression
            auto achArea = metaRow.removeFromLeft(110);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Logros", achArea.removeFromLeft(44), juce::Justification::centredLeft);
            {
                juce::String achStr = juce::String(achievementCount_) + " \\xf0\\x9f\\x8f\\x86";
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(achievementCount_ > 0 ? MixCoachTheme::success() : MixCoachTheme::textMuted());
                g.drawText(achStr, achArea, juce::Justification::centredLeft);
            }

            // Phase progression
            auto phaseArea = metaRow.removeFromLeft(120);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Fase", phaseArea.removeFromLeft(32), juce::Justification::centredLeft);
            {
                juce::String phaseStr = juce::String(SessionProgression::phaseEmoji(sessionProgressionPhase_))
                                        + " " + juce::String(SessionProgression::phaseShortName(sessionProgressionPhase_));
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(MixCoachTheme::accentCyan());
                g.drawText(phaseStr, phaseArea, juce::Justification::centredLeft);
            }'''

if old2 in data:
    data = data.replace(old2, new2, 1)
    print('CHANGE 2: Phase progression added to metadata row')
    total += 1
else:
    print('CHANGE 2: FAILED')
    idx = data.find('Achievements')
    if idx >= 0:
        print(repr(data[idx:idx+350]))

# ═══ CHANGE 3: Also add a progress bar in the session metadata row for phase overall progress ═══
# Find the corrections display and replace it to also show phase progress bar

old3 = '''            // Corrections
            auto corrArea = metaRow;
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Correcciones", corrArea.removeFromLeft(78), juce::Justification::centredLeft);
            {
                juce::String corrStr = juce::String(appliedCorrections_) + "/" + juce::String(totalCorrections_);
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(appliedCorrections_ > 0 ? MixCoachTheme::info() : MixCoachTheme::textMuted());
                g.drawText(corrStr, corrArea, juce::Justification::centredLeft);
            }'''

new3 = '''            // Corrections + phase progress
            auto corrArea = metaRow;
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Progreso", corrArea.removeFromLeft(56), juce::Justification::centredLeft);
            {
                // Mini progress bar
                auto barArea = corrArea.removeFromLeft(60).reduced(0, 5);
                g.setColour(MixCoachTheme::bgDarker());
                g.fillRoundedRectangle(barArea.toFloat(), 3.0f);
                if (sessionProgressionProgress_ > 0.0f) {
                    int fillW = (int)(barArea.getWidth() * juce::jlimit(0.0f, 1.0f, sessionProgressionProgress_));
                    if (fillW > 2) {
                        g.setColour(MixCoachTheme::accent());
                        g.fillRoundedRectangle(barArea.withWidth(fillW).toFloat(), 3.0f);
                    }
                }
                // Percentage text
                int pct = (int)(sessionProgressionProgress_ * 100.0f);
                juce::String pctStr = juce::String(pct) + "%";
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(pctStr, corrArea, juce::Justification::centredLeft);
            }'''

if old3 in data:
    data = data.replace(old3, new3, 1)
    print('CHANGE 3: Phase progress bar added to metadata row')
    total += 1
else:
    print('CHANGE 3: FAILED')
    idx = data.find('Correcciones')
    if idx >= 0:
        print(repr(data[idx:idx+300]))

with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'w', encoding='utf-8') as f:
    f.write(data)

print(f'\nTotal changes applied: {total}')
