# Enhance the onApplyAll callback in NavigationShell.cpp
# Add: robot celebration (Happy + wave), combined deltas summary, celebration message

with open('Source/MixCoach/UI/NavigationShell.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Find the onApplyAll lambda and the summary block after it
# Pattern 1: Add celebration inside onApplyAll after postUIEvent
old_celebrate = '''                postUIEvent("\\xF0\\x9F\\x93\\x8A", "Aplicadas " + juce::String(applied) + " sugerencias de gain");
                setAvatarNod(600);
            };'''

new_celebrate = '''                postUIEvent("\\xF0\\x9F\\x93\\x8A", "Aplicadas " + juce::String(applied) + " sugerencias de gain");
                // Gap #3: Robot celebra al aplicar todas
                setAvatarExpression(AvatarExpression::Happy);
                setAvatarWave(true);
                setAvatarNod(600);
                if (auto* eMgr = &experienceManager_)
                    eMgr->celebrate("Gain Staging completado");
            };'''

if old_celebrate in content:
    content = content.replace(old_celebrate, new_celebrate, 1)
    print('Fix 1 applied: celebration added')
else:
    print('Fix 1 NOT FOUND - trying alternative pattern')
    # Try without the closing ;
    old2 = '''                postUIEvent("\\xF0\\x9F\\x93\\x8A", "Aplicadas " + juce::String(applied) + " sugerencias de gain");
                setAvatarNod(600);'''
    if old2 in content:
        content = content.replace(old2, new2, 1)
        print('Fix 1 applied (alt pattern)')

# Pattern 2: Enhance the summary block AFTER onApplyAll to include combined deltas
# Find the summary header and add combined delta calculation
old_summary = '''            if (actCount > 0) {
                juce::String summary;
                summary << "\\xF0\\x9F\\x93\\x8A **An\\xC3\\xA1lisis de Gain Staging**\\n"'''

new_summary = '''            if (actCount > 0) {
                juce::String summary;
                // Calcular delta combinado total
                float totalDeltaDb = 0.0f;
                for (auto& adv : advicesG) {
                    if (adv.isActionable())
                        totalDeltaDb += adv.suggestedDeltaDb;
                }
                summary << "\\xF0\\x9F\\x93\\x8A **An\\xC3\\xA1lisis de Gain Staging**\\n"'''

if old_summary in content:
    content = content.replace(old_summary, new_summary, 1)
    print('Fix 2 applied: combined delta calculation added')
else:
    print('Fix 2 NOT FOUND')

# Pattern 3: Add combined delta and celebration to the end of the summary message
# Find the end of the summary block where it adds instructions
old_end = '''                summary << "\\n\\nUsa los botones **Aplicar** debajo o **Aplicar todas** para ajustar.";
                coachPanel_->addSystemMessage(summary);'''

new_end = '''                // Gap #3: Incluir delta combinado y celebraci\\xF3\\x9En
                float absTotal = std::abs(totalDeltaDb);
                summary << "\\n\\n\\xF0\\x9F\\x93\\x8A **Resumen:** " << applied << " track(s) ajustados, delta combinado de ";
                if (totalDeltaDb > 0) summary << "+";
                summary << juce::String(totalDeltaDb, 1) << " dB.";
                summary << "\\n\\nUsa los botones **Aplicar** debajo o **Aplicar todas** para ajustar.";
                coachPanel_->addSystemMessage(summary);
                // Mensaje de celebraci\\xF3\\x9En del robot
                if (applied > 0) {
                    coachPanel_->addSystemMessage(
                        "\\xF0\\x9F\\x91\\x8F \\xC2\\xA1Excelente! Los niveles de gain est\\xC3\\xA1n mucho mejor. "
                        "La mezcla ya tiene un balance s\\xC3\\xB3lido. \\xF0\\x9F\\x8E\\xB5\\n\\n"
                        "**Siguiente paso:** Vamos a revisar el balance espectral y las frecuencias.");
                }'''

if old_end in content:
    content = content.replace(old_end, new_end, 1)
    print('Fix 3 applied: combined delta display and celebration message added')
else:
    print('Fix 3 NOT FOUND')

with open('Source/MixCoach/UI/NavigationShell.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print('Done')
