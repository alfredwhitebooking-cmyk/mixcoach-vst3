# P8: Sincronizar EndOfSession con estado real del motor
# Aplica cambios a EndOfSessionComponent.cpp

import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'r', encoding='utf-8') as f:
    data = f.read()

changes = 0

# ═══ CHANGE 1: Insert session metadata capture after masterCorrelation ═══
old1 = '        masterCorrelation_   = currentScore_.masterCorrelation;\n\n        // \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 5. Gather per-track diagnosis'
new1 = '''        masterCorrelation_   = currentScore_.masterCorrelation;\n
        // \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 4b. Gather session metadata from real engine state
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
        }

        // \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 5. Gather per-track diagnosis'''

if old1 in data:
    data = data.replace(old1, new1, 1)
    print('CHANGE 1: Session metadata capture INSERTED')
    changes += 1
else:
    print('CHANGE 1: FAILED - pattern not found')

# ═══ CHANGE 2: Replace "Total Corrections" with Categories breakdown ═══
old2 = '        drawMetric(col1, "Total Corrections", juce::String(totalCorrections_), MixCoachTheme::info());'
new2 = '''        // \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 Track category breakdown (from real engine state)
        {
            int nCats = 0;
            juce::String cats;
            if (drumTracks_ > 0)   { if (nCats++ > 0) cats += " | "; cats += juce::String(drumTracks_) + " drums"; }
            if (bassTracks_ > 0)   { if (nCats++ > 0) cats += " | "; cats += juce::String(bassTracks_) + " bass"; }
            if (guitarTracks_ > 0) { if (nCats++ > 0) cats += " | "; cats += juce::String(guitarTracks_) + " gtr"; }
            if (keysTracks_ > 0)   { if (nCats++ > 0) cats += " | "; cats += juce::String(keysTracks_) + " keys"; }
            if (vocalTracks_ > 0)  { if (nCats++ > 0) cats += " | "; cats += juce::String(vocalTracks_) + " vox"; }
            if (fxTracks_ > 0)     { if (nCats++ > 0) cats += " | "; cats += juce::String(fxTracks_) + " fx"; }
            if (melodyTracks_ > 0) { if (nCats++ > 0) cats += " | "; cats += juce::String(melodyTracks_) + " mel"; }
            if (unknownTracks_ > 0) { if (nCats++ > 0) cats += " | "; cats += juce::String(unknownTracks_) + " ?"; }
            if (nCats == 0) cats = "\xe2\x80\x94";
            drawMetric(col1, "Categories", cats, MixCoachTheme::accentCyan());
        }'''

if old2 in data:
    data = data.replace(old2, new2, 1)
    print('CHANGE 2: Categories breakdown INSERTED')
    changes += 1
else:
    print('CHANGE 2: FAILED - pattern not found')

# ═══ CHANGE 3: Insert True Peak after Master Peak ═══
old3 = '        drawMetric(col2, "Master Peak", juce::String(masterPeakDb_, 1) + " dBFS",\n                   masterPeakDb_ > -1.0f ? MixCoachTheme::error() : MixCoachTheme::textBright());\n        drawMetric(col2, "Integrated LUFS"'
new3 = '''        drawMetric(col2, "Master Peak", juce::String(masterPeakDb_, 1) + " dBFS",
                   masterPeakDb_ > -1.0f ? MixCoachTheme::error() : MixCoachTheme::textBright());
        drawMetric(col2, "True Peak", juce::String(masterTruePeakDb_, 1) + " dBTP",
                   masterTruePeakDb_ > -1.0f ? MixCoachTheme::error() : MixCoachTheme::textMuted());
        drawMetric(col2, "Integrated LUFS"'''

if old3 in data:
    data = data.replace(old3, new3, 1)
    print('CHANGE 3: True Peak INSERTED')
    changes += 1
else:
    print('CHANGE 3: FAILED - pattern not found')

# ═══ CHANGE 4: Insert session metadata row before Confidence indicator ═══
old5 = '        // \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 Confidence indicator (cualitativo, sin porcentaje) \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n        inner.removeFromTop(MixCoachTheme::spacingXS);\n        {\n            ConfidenceScore cs;\n            if (coachEngine_ != nullptr) {\n                auto& shared = coachEngine_->getSharedData();\n                cs = ConfidenceScore::compute(*coachEngine_, shared, 0);\n            }'

new5 = '''        // \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 Session metadata row: duration + achievements + corrections \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80
        inner.removeFromTop(MixCoachTheme::spacingXS);
        {
            auto metaRow = inner.removeFromTop(18);

            // Session duration
            auto durArea = metaRow.removeFromLeft(140);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Session", durArea.removeFromLeft(52), juce::Justification::centredLeft);
            {
                juce::String durStr;
                int64_t secs = sessionDurationUs_ / 1000000;
                if (secs >= 3600)
                    durStr = juce::String((int)(secs / 3600)) + "h " + juce::String((int)((secs % 3600) / 60)) + "m";
                else if (secs >= 60)
                    durStr = juce::String((int)(secs / 60)) + "m " + juce::String((int)(secs % 60)) + "s";
                else
                    durStr = juce::String((int)secs) + "s";
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(MixCoachTheme::accentCyan());
                g.drawText(durStr, durArea, juce::Justification::centredLeft);
            }

            // Achievements
            auto achArea = metaRow.removeFromLeft(110);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Logros", achArea.removeFromLeft(44), juce::Justification::centredLeft);
            {
                juce::String achStr = juce::String(achievementCount_) + " \xf0\x9f\x8f\x86";
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(achievementCount_ > 0 ? MixCoachTheme::success() : MixCoachTheme::textMuted());
                g.drawText(achStr, achArea, juce::Justification::centredLeft);
            }

            // Corrections
            auto corrArea = metaRow;
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Correcciones", corrArea.removeFromLeft(78), juce::Justification::centredLeft);
            {
                juce::String corrStr = juce::String(appliedCorrections_) + "/" + juce::String(totalCorrections_);
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(appliedCorrections_ > 0 ? MixCoachTheme::info() : MixCoachTheme::textMuted());
                g.drawText(corrStr, corrArea, juce::Justification::centredLeft);
            }
        }

        // \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 Confidence indicator (cualitativo, sin porcentaje) \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80
        inner.removeFromTop(MixCoachTheme::spacingXS);
        {
            ConfidenceScore cs;
            if (coachEngine_ != nullptr) {
                auto& shared = coachEngine_->getSharedData();
                cs = ConfidenceScore::compute(*coachEngine_, shared, 0);
            }'''

if old5 in data:
    data = data.replace(old5, new5, 1)
    print('CHANGE 4: Session metadata row INSERTED before Confidence')
    changes += 1
else:
    print('CHANGE 4: FAILED - pattern not found')

# Write back
with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'w', encoding='utf-8') as f:
    f.write(data)

print(f'\nTotal changes applied: {changes}')
