#!/usr/bin/env python3
"""Update SceneManager.cpp with GroupFocus/TrackFocus scenes, transitions, and builders."""

import sys

def main():
    path = 'Source/MixCoach/engine/SceneManager.cpp'
    if len(sys.argv) > 1:
        path = sys.argv[1]

    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    changes = 0

    # === 1. Transition rules after ToolInFocus section ===
    old_rules = (
        '            { SceneId::ToolInFocus,  DirectorEvent::Type::UserInteracted,   SceneId::ToolInFocus },'
        '  // restart timer\n'
        '            { SceneId::ToolInFocus,  DirectorEvent::Type::TimerTick,         SceneId::Coaching },'
        '    // auto-return\n'
        '            { SceneId::ToolInFocus,  DirectorEvent::Type::CoachCommand,      SceneId::Coaching },'
        '    // return_to_coach'
    )

    new_rules = (
        '            { SceneId::ToolInFocus,  DirectorEvent::Type::UserInteracted,   SceneId::ToolInFocus },'
        '  // restart timer\n'
        '            { SceneId::ToolInFocus,  DirectorEvent::Type::TimerTick,         SceneId::Coaching },'
        '    // auto-return\n'
        '            { SceneId::ToolInFocus,  DirectorEvent::Type::CoachCommand,      SceneId::Coaching },'
        '    // return_to_coach\n'
        '\n'
        '            // --- GroupFocus / TrackFocus (SCENE 9) ---\n'
        '            { SceneId::Coaching,     DirectorEvent::Type::FocusGroupBus,    SceneId::GroupFocus },'
        '  // Focus DRUMS/Bass/Vocals\n'
        '            { SceneId::Coaching,     DirectorEvent::Type::FocusTrack,       SceneId::TrackFocus },'
        '  // Focus a specific track\n'
        '            { SceneId::Coaching,     DirectorEvent::Type::FocusDismissed,   SceneId::Coaching },'
        '    // dismiss while in coaching (safety)\n'
        '\n'
        '            { SceneId::GroupFocus,   DirectorEvent::Type::FocusTrack,       SceneId::TrackFocus },'
        '  // switch from group to track\n'
        '            { SceneId::GroupFocus,   DirectorEvent::Type::FocusGroupBus,    SceneId::GroupFocus },'
        '  // stay, new group\n'
        '            { SceneId::GroupFocus,   DirectorEvent::Type::FocusDismissed,   SceneId::Coaching },'
        '    // dismiss back to coaching\n'
        '            { SceneId::GroupFocus,   DirectorEvent::Type::UserInteracted,   SceneId::GroupFocus },'
        '  // restart auto-return\n'
        '            { SceneId::GroupFocus,   DirectorEvent::Type::TimerTick,         SceneId::Coaching },'
        '   // auto-return\n'
        '            { SceneId::GroupFocus,   DirectorEvent::Type::CoachCommand,      SceneId::Coaching },'
        '   // return_to_coach\n'
        '\n'
        '            { SceneId::TrackFocus,   DirectorEvent::Type::FocusGroupBus,    SceneId::GroupFocus },'
        '  // switch from track to group\n'
        '            { SceneId::TrackFocus,   DirectorEvent::Type::FocusTrack,       SceneId::TrackFocus },'
        '  // stay, new track\n'
        '            { SceneId::TrackFocus,   DirectorEvent::Type::FocusDismissed,   SceneId::Coaching },'
        '    // dismiss back to coaching\n'
        '            { SceneId::TrackFocus,   DirectorEvent::Type::UserInteracted,   SceneId::TrackFocus },'
        '  // restart auto-return\n'
        '            { SceneId::TrackFocus,   DirectorEvent::Type::TimerTick,         SceneId::Coaching },'
        '   // auto-return\n'
        '            { SceneId::TrackFocus,   DirectorEvent::Type::CoachCommand,      SceneId::Coaching }'
        '   // return_to_coach'
    )

    if old_rules in content:
        content = content.replace(old_rules, new_rules, 1)
        print('SUCCESS: Transition rules updated')
        changes += 1
    else:
        print('FAIL: Transition rules pattern not found')
        idx = content.find('TimerTick,         SceneId::Coaching },    // auto-return')
        if idx >= 0:
            print('  Found TimerTick auto-return at idx:', idx)

    # === 2. Add scene builders before buildScene_RefinementTool() ===
    old_build = '    SceneDef SceneManager::buildScene_RefinementTool()'

    new_builders = """
    SceneDef SceneManager::buildScene_GroupFocus()
    {
        SceneDef def;
        def.id = SceneId::GroupFocus;
        def.name = "Enfoque: Grupo";
        def.coachMessage = "";
        def.sidebarEnabled = true;
        def.autoReturnTimeout = 8.0f;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Visible,   0.0f, "Chat - enfoque activo" },
            { PanelId::Reference, PanelVisibility::Hidden,    0.3f },
            { PanelId::Messengers,PanelVisibility::Visible,   0.2f, "Tracks" },
            { PanelId::MixMap,    PanelVisibility::Focused,   0.5f, "GRUPO ENFOQUE" },
            { PanelId::Tools,     PanelVisibility::Hidden,    0.3f },
            { PanelId::Session,   PanelVisibility::Hidden,    0.2f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_TrackFocus()
    {
        SceneDef def;
        def.id = SceneId::TrackFocus;
        def.name = "Enfoque: Pista";
        def.coachMessage = "";
        def.sidebarEnabled = true;
        def.autoReturnTimeout = 8.0f;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Visible,   0.0f, "Chat - enfoque activo" },
            { PanelId::Reference, PanelVisibility::Hidden,    0.3f },
            { PanelId::Messengers,PanelVisibility::Visible,   0.2f, "Tracks" },
            { PanelId::MixMap,    PanelVisibility::Focused,   0.5f, "PISTA ENFOQUE" },
            { PanelId::Tools,     PanelVisibility::Hidden,    0.3f },
            { PanelId::Session,   PanelVisibility::Hidden,    0.2f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

""" + old_build  # prepend new builders before RefinementTool

    if old_build in content:
        content = content.replace(old_build, new_builders, 1)
        print('SUCCESS: GroupFocus/TrackFocus builders added')
        changes += 1
    else:
        print('FAIL: buildScene_RefinementTool() pattern not found')

    # === 3. Update buildScene() dispatch ===
    old_dispatch = (
        '            case SceneId::ToolInFocus:       return buildScene_ToolInFocus();\n'
        '            case SceneId::Refinement:        return buildScene_Refinement();'
    )
    new_dispatch = (
        '            case SceneId::ToolInFocus:       return buildScene_ToolInFocus();\n'
        '            case SceneId::GroupFocus:        return buildScene_GroupFocus();\n'
        '            case SceneId::TrackFocus:        return buildScene_TrackFocus();\n'
        '            case SceneId::Refinement:        return buildScene_Refinement();'
    )
    if old_dispatch in content:
        content = content.replace(old_dispatch, new_dispatch, 1)
        print('SUCCESS: Dispatch updated')
        changes += 1
    else:
        print('FAIL: Dispatch pattern not found')

    # === 4. Update isCoachingScene() ===
    old_coaching = (
        '    bool SceneManager::isCoachingScene() const noexcept\n'
        '    {\n'
        '        return currentScene_ == SceneId::Coaching\n'
        '            || currentScene_ == SceneId::ToolInFocus;\n'
        '    }'
    )
    new_coaching = (
        '    bool SceneManager::isCoachingScene() const noexcept\n'
        '    {\n'
        '        return currentScene_ == SceneId::Coaching\n'
        '            || currentScene_ == SceneId::ToolInFocus\n'
        '            || currentScene_ == SceneId::GroupFocus\n'
        '            || currentScene_ == SceneId::TrackFocus;\n'
        '    }'
    )
    if old_coaching in content:
        content = content.replace(old_coaching, new_coaching, 1)
        print('SUCCESS: isCoachingScene() updated')
        changes += 1
    else:
        print('FAIL: isCoachingScene() pattern not found')

    # === Write back ===
    if changes > 0:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f'\nDONE: {changes} edits applied, file written')
    else:
        print('\nABORTED: No changes made')

if __name__ == '__main__':
    main()
