import sys

with open('Source/MixCoach/engine/CoachEngine.h', 'r', encoding='utf-8') as f:
    content = f.read()

# Struct names to guard
structs_to_guard = [
    ('ReferenceFingerprint', 'struct ReferenceFingerprint'),
    ('ReferenceMatchData', 'struct ReferenceMatchData'),
    ('ReferenceMetadata', 'struct ReferenceMetadata'),
    ('BusGroupSummary', 'struct BusGroupSummary'),
]

guarded_count = 0
for tag, struct_name in structs_to_guard:
    guard_name = f'MIXCOACH_COACHENGINE_{tag.upper()}_DEFINED'
    
    # Find each occurrence
    idx = 0
    while True:
        idx = content.find(struct_name, idx)
        if idx < 0:
            break
        
        # Make sure this is a standalone struct definition (not inside a class)
        # Look backward to see if we're inside another struct or class
        # Simple check: find the last { or } before this point
        # If we're at namespace level, this is a standalone struct
        
        # Find the matching brace to locate the end
        brace_start = content.find('{', idx)
        if brace_start < 0:
            idx += 1
            continue
            
        # Add #ifndef guard BEFORE the struct
        guard_if = f'\n#ifndef {guard_name}\n#define {guard_name}\n'
        content = content[:idx] + guard_if + content[idx:]
        idx += len(guard_if) + len(struct_name)
        
        # Find the closing brace + semicolon for #endif
        # Look for '};' after the struct body
        end_marker = '};'
        end_pos = content.find(end_marker, idx)
        if end_pos < 0:
            print(f'  {tag}: could not find closing }};')
            continue
            
        # Add #endif after the };
        guard_endif = f'\n#endif // {guard_name}\n'
        content = content[:end_pos + 2] + guard_endif + content[end_pos + 2:]
        
        guarded_count += 1
        print(f'  Guarded {tag} at position {idx}')
        idx = end_pos + 2 + len(guard_endif)

with open('Source/MixCoach/engine/CoachEngine.h', 'w', encoding='utf-8') as f:
    f.write(content)

print(f'\nDone - guarded {guarded_count} struct(s)')
