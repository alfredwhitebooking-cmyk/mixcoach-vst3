# P8 v3: Apply remaining changes using exact byte-level matching
import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'r', encoding='utf-8') as f:
    data = f.read()

changes = 0

# ═══ CHANGE 1: Insert session metadata capture after masterCorrelation ═══
# Find the exact text
idx = data.find('masterCorrelation_   = currentScore_.masterCorrelation;')
if idx >= 0:
    # Find the next occurrence of "// \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 5. Gather per-track diagnosis"
    # After masterCorrelation line, there's a blank line, then the comment
    after_master = data[idx:]
    diag_idx = after_master.find('\n\n        // \u2500\u2500\u2500 5. Gather per-track diagnosis')
    
    if diag_idx >= 0:
        end_pos = idx + diag_idx + 2  # +2 for the two newlines
        before = data[:end_pos]
        after = data[end_pos:]
        
        new_block = '''
        // \u2500\u2500\u2500 4b. Gather session metadata from real engine state
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

        // \u2500\u2500\u2500 5. Gather per-track diagnosis'''
        
        data = before + new_block + after
        print('CHANGE 1: Session metadata capture INSERTED')
        changes += 1
    else:
        print('CHANGE 1: FAILED - could not find diagnosis comment after masterCorrelation')
        print(repr(after_master[:200]))
else:
    print('CHANGE 1: FAILED - masterCorrelation not found')

# ═══ CHANGE 2: Remove Estado block (redundant, shown in Overall Score) ═══
# The Estado block is the one with "Score Status: cualitativo"
estado_start = data.find('            // Score Status: cualitativo (sin n')
if estado_start >= 0:
    # Find the end: the drawMetric call and then the closing of that block
    # It should end with the blank line before Confidence indicator
    end_marker = '\n        // \u2500\u2500\u2500 Confidence indicator'
    end_idx = data.find(end_marker, estado_start)
    if end_idx >= 0:
        # Remove everything from estado_start to end_marker (inclusive of the blank line before it)
        # Go back to find the start of the line
        line_start = data.rfind('\n', 0, estado_start)
        if line_start >= 0:
            before = data[:line_start]
            after = data[end_idx:]  # Keep the confidence marker
            data = before + after
            print('CHANGE 2: Estado block REMOVED')
            changes += 1
        else:
            print('CHANGE 2: FAILED - could not find line start before Estado')
    else:
        print('CHANGE 2: FAILED - could not find Confidence marker after Estado')
else:
    print('CHANGE 2: FAILED - Estado pattern not found')

# ═══ CHANGE 3: Insert session metadata row before Confidence indicator ═══
# After removing Estado, there should be a blank line then Confidence indicator
# Find the Confidence marker
conf_marker = '\n        // \u2500\u2500\u2500 Confidence indicator (cualitativo, sin porcentaje) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500'
conf_idx = data.find(conf_marker)
if conf_idx >= 0:
    # Insert session metadata row before this marker
    # Go back to find the blank line just before the marker
    line_start = data.rfind('\n', 0, conf_idx)
    
    # What's between line_start and conf_marker?
    between = data[line_start:conf_idx]
    
    session_row_block = '''
        // \u2500\u2500\u2500 Session metadata row: duration + achievements + corrections \u2500\u2500\u2500
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

    '''
    
    before = data[:line_start]
    after = data[line_start:]  # Keep the blank line + confidence marker
    
    data = before + session_row_block + after
    print('CHANGE 3: Session metadata row INSERTED before Confidence')
    changes += 1
else:
    print('CHANGE 3: FAILED - Confidence marker not found')

# ═══ CHANGE 4: Add session duration to drawOverallScore subtitle ═══
# Find "Reference match:" section
ref_match = data.find('subtitle += \"  |  Reference match: \" + refQual;')
if ref_match >= 0:
    # Find the end of this block - the next line with just g.drawText
    # We'll insert session duration after this line but before g.drawText
    rest = data[ref_match:]
    line_end = rest.find('\n')
    after_ref = data[ref_match + line_end:]
    
    session_dur_block = '''
        // Session duration (from real engine state)
        if (sessionDurationUs_ > 0) {
            int64_t secs = sessionDurationUs_ / 1000000;
            juce::String durStr;
            if (secs >= 3600)
                durStr = juce::String((int)(secs / 3600)) + "h " + juce::String((int)((secs % 3600) / 60)) + "m";
            else if (secs >= 60)
                durStr = juce::String((int)(secs / 60)) + "m " + juce::String((int)(secs % 60)) + "s";
            else
                durStr = juce::String((int)secs) + "s";
            subtitle += \"  |  Session: \" + durStr;
        }'''
    
    data = data[:ref_match + line_end] + session_dur_block + after_ref
    print('CHANGE 4: Session duration added to overall score subtitle')
    changes += 1
else:
    print('CHANGE 4: FAILED - Reference match subtitle not found')

# Write back
with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'w', encoding='utf-8') as f:
    f.write(data)

print(f'\nTotal changes applied: {changes}')
