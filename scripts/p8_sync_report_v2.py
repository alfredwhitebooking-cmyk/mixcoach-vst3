# P8 v2: Apply remaining changes to EndOfSessionComponent.cpp
# Uses exact repr-matched strings

import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'r', encoding='utf-8') as f:
    data = f.read()

changes = 0

# ═══ CHANGE 1: Insert session metadata capture after masterCorrelation line ═══
# Exact match: line with masterCorrelation + blank line + header comment
old1 = ('        masterCorrelation_   = currentScore_.masterCorrelation;\n'
        '\n'
        '        // \u2500\u2500\u2500 5. Gather per-track diagnosis \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n')

new1 = ('        masterCorrelation_   = currentScore_.masterCorrelation;\n'
        '\n'
        '        // \u2500\u2500\u2500 4b. Gather session metadata from real engine state\n'
        '        {\n'
        '            auto ctx = coachEngine_->buildSessionContext();\n'
        '            sessionDurationUs_ = ctx.sessionDurationUs;\n'
        '            achievementCount_  = ctx.achievementCount;\n'
        '            masterTruePeakDb_  = ctx.masterTruePeak;\n'
        '            drumTracks_       = ctx.drumTracks;\n'
        '            bassTracks_       = ctx.bassTracks;\n'
        '            guitarTracks_     = ctx.guitarTracks;\n'
        '            keysTracks_       = ctx.keysTracks;\n'
        '            vocalTracks_      = ctx.vocalTracks;\n'
        '            fxTracks_         = ctx.fxTracks;\n'
        '            melodyTracks_     = ctx.melodyTracks;\n'
        '            unknownTracks_    = ctx.unknownTracks;\n'
        '        }\n'
        '\n'
        '        // \u2500\u2500\u2500 5. Gather per-track diagnosis \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n')

if old1 in data:
    data = data.replace(old1, new1, 1)
    print('CHANGE 1: Session metadata capture INSERTED')
    changes += 1
else:
    print('CHANGE 1: FAILED - pattern not found')
    # Debug: show what's actually there
    idx = data.find('masterCorrelation_')
    if idx >= 0:
        print(repr(data[idx:idx+160]))

# ═══ CHANGE 4: Insert session metadata row before Confidence indicator ═══
# The Estado block was replaced with just a blank line, so find Confidence directly
old4 = ('\n        // \u2500\u2500\u2500 Confidence indicator (cualitativo, sin porcentaje) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n'
        '        inner.removeFromTop(MixCoachTheme::spacingXS);\n'
        '        {\n'
        '            ConfidenceScore cs;\n'
        '            if (coachEngine_ != nullptr) {\n'
        '                auto& shared = coachEngine_->getSharedData();\n'
        '                cs = ConfidenceScore::compute(*coachEngine_, shared, 0);\n'
        '            }')

new4 = ('\n'
        '        // \u2500\u2500\u2500 Session metadata row: duration + achievements + corrections \u2500\u2500\u2500\n'
        '        inner.removeFromTop(MixCoachTheme::spacingXS);\n'
        '        {\n'
        '            auto metaRow = inner.removeFromTop(18);\n'
        '\n'
        '            // Session duration\n'
        '            auto durArea = metaRow.removeFromLeft(140);\n'
        '            g.setFont(juce::Font(juce::FontOptions(10.0f)));\n'
        '            g.setColour(MixCoachTheme::textDim());\n'
        '            g.drawText(\"Session\", durArea.removeFromLeft(52), juce::Justification::centredLeft);\n'
        '            {\n'
        '                juce::String durStr;\n'
        '                int64_t secs = sessionDurationUs_ / 1000000;\n'
        '                if (secs >= 3600)\n'
        '                    durStr = juce::String((int)(secs / 3600)) + \"h \" + juce::String((int)((secs % 3600) / 60)) + \"m\";\n'
        '                else if (secs >= 60)\n'
        '                    durStr = juce::String((int)(secs / 60)) + \"m \" + juce::String((int)(secs % 60)) + \"s\";\n'
        '                else\n'
        '                    durStr = juce::String((int)secs) + \"s\";\n'
        '                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());\n'
        '                g.setColour(MixCoachTheme::accentCyan());\n'
        '                g.drawText(durStr, durArea, juce::Justification::centredLeft);\n'
        '            }\n'
        '\n'
        '            // Achievements\n'
        '            auto achArea = metaRow.removeFromLeft(110);\n'
        '            g.setFont(juce::Font(juce::FontOptions(10.0f)));\n'
        '            g.setColour(MixCoachTheme::textDim());\n'
        '            g.drawText(\"Logros\", achArea.removeFromLeft(44), juce::Justification::centredLeft);\n'
        '            {\n'
        '                juce::String achStr = juce::String(achievementCount_) + \" \";\n'
        '                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());\n'
        '                g.setColour(achievementCount_ > 0 ? MixCoachTheme::success() : MixCoachTheme::textMuted());\n'
        '                g.drawText(achStr, achArea, juce::Justification::centredLeft);\n'
        '            }\n'
        '\n'
        '            // Corrections\n'
        '            auto corrArea = metaRow;\n'
        '            g.setFont(juce::Font(juce::FontOptions(10.0f)));\n'
        '            g.setColour(MixCoachTheme::textDim());\n'
        '            g.drawText(\"Correcciones\", corrArea.removeFromLeft(78), juce::Justification::centredLeft);\n'
        '            {\n'
        '                juce::String corrStr = juce::String(appliedCorrections_) + \"/\" + juce::String(totalCorrections_);\n'
        '                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());\n'
        '                g.setColour(appliedCorrections_ > 0 ? MixCoachTheme::info() : MixCoachTheme::textMuted());\n'
        '                g.drawText(corrStr, corrArea, juce::Justification::centredLeft);\n'
        '            }\n'
        '        }\n'
        '\n'
        '        // \u2500\u2500\u2500 Confidence indicator (cualitativo, sin porcentaje) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n'
        '        inner.removeFromTop(MixCoachTheme::spacingXS);\n'
        '        {\n'
        '            ConfidenceScore cs;\n'
        '            if (coachEngine_ != nullptr) {\n'
        '                auto& shared = coachEngine_->getSharedData();\n'
        '                cs = ConfidenceScore::compute(*coachEngine_, shared, 0);\n'
        '            }')

if old4 in data:
    data = data.replace(old4, new4, 1)
    print('CHANGE 4: Session metadata row INSERTED before Confidence')
    changes += 1
else:
    print('CHANGE 4: FAILED - pattern not found')
    # Debug
    idx = data.find('Confidence indicator')
    if idx >= 0:
        print(repr(data[idx-30:idx+300]))

# Write back
with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'w', encoding='utf-8') as f:
    f.write(data)

print(f'\nTotal changes applied: {changes}')
