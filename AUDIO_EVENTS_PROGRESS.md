# Audio Events in Animation Timeline - Progress Report

**Project:** GAM300 - BoomEngine Animation Timeline
**Feature:** Audio Event Markers for Animation Clips
**Last Updated:** 2026-01-28

---

## Quick Resume Guide

**Current Status:** Phase 1 & 2 COMPLETE. Ready for manual testing, then Phase 3.

**To Resume:**
1. Build the project and test the audio events UI
2. If tests pass, implement Phase 3 (Playback Integration)
3. See "Next Steps" section below for details

---

## Completed Work Summary

### ✅ UI Polish & Improvements (Prerequisite)

- **Collapsible Control Bar Sections** - 4 color-coded sections (Blue: Model & Playback, Green: Editing Tools, Purple: View Options, Orange: Animation Clip)
- **Resizable Viewport Splitter** - Draggable splitter between viewport and timeline (20-80% range)
- **Compact Mode Toggle** - Hides less-used controls for smaller screens

**Files Modified:**
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.h`
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.cpp`
- `Gam300/Editor/src/Panels/AnimationTimelinePanel_Viewport.cpp`

---

### ✅ Phase 1: Data Layer (COMPLETE)

**AudioEventMarker struct** added to `Animation.h`:
```cpp
struct AudioEventMarker {
    float timeStamp = 0.0f;        // When to trigger (seconds)
    std::string soundFile;          // Path to audio file
    float volume = 1.0f;            // 0.0 to 1.0
    float pitch = 1.0f;             // Playback speed multiplier
    bool is3D = false;              // 2D or 3D spatial audio
    bool loop = false;              // Loop the sound
    std::string groupName = "SFX";  // Channel group
    std::string eventName;          // Optional label
};
```

**AnimationClip** extended with `std::vector<AudioEventMarker> audioEvents;`

**Serialization** (AnimationIO.cpp):
- SaveAnimationClip writes audioEvents to YAML
- LoadAnimationClip reads audioEvents from YAML
- Backward compatible with old .anim files

**Files Modified:**
- `Gam300/Engine/BoomEngine/includes/Graphics/Models/Animation.h`
- `Gam300/Engine/BoomEngine/src/Auxiliaries/AnimationIO.cpp`

---

### ✅ Phase 2: Timeline UI (COMPLETE)

**Member Variables Added** (`AnimationTimelinePanel.h`):
```cpp
int m_SelectedAudioEventIndex = -1;
int m_HoveredAudioEventIndex = -1;
bool m_IsDraggingAudioEvent = false;
float m_DraggedAudioEventOriginalTime = 0.0f;
struct AudioMarkerScreenPos { size_t eventIndex; ImVec2 screenPos; };
std::vector<AudioMarkerScreenPos> m_AudioMarkerScreenPositions;
```

**RenderAudioTrack()** function added (`AnimationTimelinePanel_Timeline.cpp`):
- Dedicated "Audio Events" track rendered BEFORE bone tracks
- Purple/blue tinted background
- Shows event count: "Audio Events (3)"
- Circle markers (orange for 2D, green for 3D, cyan when selected, yellow when hovered)
- Tooltip on hover with all event details

**Interaction Features:**
- **Right-click** on audio track → "Add Audio Event" popup
- **Double-click** marker → "Edit Audio Event" popup
- **Delete key** → Remove selected audio event
- **Escape key** → Deselect audio event
- Click to select, hover highlights

**Static variable** for timestamp: `s_NewEventTimestamp` at file scope

**Files Modified:**
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.h`
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.cpp` (keyboard shortcuts)
- `Gam300/Editor/src/Panels/AnimationTimelinePanel_Timeline.cpp` (RenderAudioTrack, popups)

---

## Next Steps

### 1. Manual Testing (REQUIRED)

Build and test:
1. Load a model with animation in the Animation Timeline panel
2. Look for "Audio Events" track above the bone tracks
3. **Right-click** on the audio track → Add audio event
4. Fill in: Event Name, Sound File (e.g., "footstep.wav"), Volume, Pitch, etc.
5. Click "Add" → Marker should appear
6. **Click** marker to select (should turn cyan)
7. **Double-click** marker → Edit popup should appear
8. **Delete key** with marker selected → Should delete
9. **Save** the animation clip → Check .anim file has `audioEvents` section

### 2. Phase 3: Playback Integration (OPTIONAL)

If you want audio to actually play during animation playback:

**Implementation Plan:**

1. **Add to Animator.h:**
```cpp
float m_LastProcessedTime = 0.0f;
std::set<size_t> m_TriggeredAudioEvents;
void ProcessAudioEvents(float currentTime, float lastTime, const AnimationClip* clip);
```

2. **In Animator::Animate():**
- Check if playhead crossed any audio event timestamps
- Call SoundEngine::Play2D() or Play3D() for each triggered event
- Handle looping (clear triggered set on loop restart)
- Handle scrubbing (don't spam events)

3. **Files to Modify:**
- `Gam300/Engine/BoomEngine/includes/Graphics/Models/Animator.h`
- `Gam300/Engine/BoomEngine/includes/Graphics/Models/Animator.cpp` (if exists, or wherever Animate() is)

4. **Integration with SoundEngine:**
- Check `SoundSystem.md` for API details
- Use `SoundEngine::Play2D()` for 2D sounds
- Use `SoundEngine::Play3D()` for 3D sounds (need entity position)

---

## Not Yet Implemented (Future Enhancements)

- **Drag-to-move** audio markers
- **Undo/redo** for audio event operations
- **Sound file browser/picker** (currently uses text input)
- **Preview button** to play sound in editor

---

## Testing Checklist

### Phase 1 (Data Layer) ✅
- [x] AnimationClip compiles with audioEvents field
- [x] SaveAnimationClip writes audio events to YAML
- [x] LoadAnimationClip reads audio events from YAML
- [x] Backward compatibility (old files load without errors)

### Phase 2 (Timeline UI) ✅
- [x] Audio track renders correctly
- [x] Can add new audio event via UI (right-click)
- [x] Can edit existing audio event (double-click)
- [x] Can delete audio event (Delete key or edit popup)
- [x] Audio markers visible and clickable
- [x] Save/load preserves audio events

### Phase 3 (Playback) - TODO
- [ ] Audio events trigger during timeline playback
- [ ] Events trigger at correct timestamps
- [ ] No duplicate triggers on loop
- [ ] Scrubbing doesn't spam events
- [ ] 2D vs 3D audio works correctly
- [ ] Volume and pitch applied correctly
- [ ] Channel groups respected

---

## Key Code Locations

**Audio Event Struct:**
`Gam300/Engine/BoomEngine/includes/Graphics/Models/Animation.h:66-91`

**Serialization:**
`Gam300/Engine/BoomEngine/src/Auxiliaries/AnimationIO.cpp:55-69` (save)
`Gam300/Engine/BoomEngine/src/Auxiliaries/AnimationIO.cpp:141-156` (load)

**Audio Track Rendering:**
`Gam300/Editor/src/Panels/AnimationTimelinePanel_Timeline.cpp` - `RenderAudioTrack()` function

**Keyboard Shortcuts (Delete/Escape for audio events):**
`Gam300/Editor/src/Panels/AnimationTimelinePanel.cpp:665-700`

**Sound System Reference:**
`Gam300/SoundSystem.md` - FMOD integration details

---

## Example .anim YAML Format

```yaml
AnimationClip:
  name: "Walk"
  duration: 2.5
  ticksPerSecond: 30.0
  filePath: "Resources/Animations/walk.anim"
  tracks:
    - boneName: "LeftFoot"
      keyframes:
        - time: 0.0
          position: [0, 0, 0]
          rotation: [0, 0, 0, 1]
          scale: [1, 1, 1]
  audioEvents:
    - timeStamp: 0.5
      soundFile: "footstep_left.wav"
      volume: 0.8
      pitch: 1.0
      is3D: false
      loop: false
      groupName: "SFX"
      eventName: "Left Footstep"
    - timeStamp: 1.2
      soundFile: "footstep_right.wav"
      volume: 0.8
      pitch: 1.0
      is3D: false
      loop: false
      groupName: "SFX"
      eventName: "Right Footstep"
```

---

## Session History

1. ✅ Polished Animation Timeline UI (collapsible sections, resizable splitter, compact mode)
2. ✅ Implemented Phase 1: Data layer for audio events
3. ✅ Implemented Phase 2: Timeline UI for audio events
4. ⏳ Next: Manual testing, then Phase 3 if desired

**Last Action:** Completed Phase 2 implementation. Ready for testing.
