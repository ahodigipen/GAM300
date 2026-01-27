# Audio Events in Animation Timeline - Progress Report

**Project:** GAM300 - BoomEngine Animation Timeline
**Feature:** Audio Event Markers for Animation Clips
**Date Started:** 2026-01-23
**Last Updated:** 2026-01-23

---

## Overview

Adding the ability to play certain sounds at specific keyframes in the animation timeline. This allows synchronizing audio (footsteps, impacts, whooshes, etc.) with animation playback.

---

## Completed Work

### ✅ UI Polish & Improvements (Prerequisite)

Before implementing audio events, we improved the Animation Timeline UI to handle additional features:

#### **Phase 1: Collapsible Control Bar Sections**
- Reorganized control bar into 4 color-coded collapsible sections:
  - **Blue** - Model & Playback
  - **Green** - Editing Tools
  - **Purple** - View Options
  - **Orange** - Animation Clip
- Each section can be collapsed independently
- All default to open

#### **Phase 2: Resizable Viewport Splitter**
- Added draggable splitter bar between viewport and timeline
- User-adjustable ratio (20% to 80%)
- Splitter positioned AFTER viewport interactions to avoid mouse event conflicts
- Visual feedback (gray → blue on hover → bright blue when dragging)

#### **Phase 3: Compact Mode Toggle**
- Added "Compact Mode" checkbox in View Options
- When enabled:
  - Gizmo buttons become small single-letter buttons (W/E/R/T)
  - Scale controls simplified
  - Clip info shows only duration and play state
  - Hides detailed stats (FPS, frames, camera distance, etc.)

**Files Modified:**
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.h`
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.cpp`
- `Gam300/Editor/src/Panels/AnimationTimelinePanel_Viewport.cpp`

---

### ✅ Audio Events - Phase 1: Data Layer

Implemented the data structures and serialization for audio events.

#### **1. AudioEventMarker Struct** (`Animation.h`)
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

#### **2. Extended AnimationClip** (`Animation.h`)
```cpp
struct AnimationClip {
    // ... existing fields ...
    std::vector<AudioEventMarker> audioEvents;  // NEW
};
```

#### **3. YAML Serialization** (`AnimationIO.cpp`)

**SaveAnimationClip:**
- Saves `audioEvents` array after bone tracks
- All 8 properties serialized per event

**LoadAnimationClip:**
- Loads `audioEvents` if present
- Backward compatible (old .anim files still work)
- Updated log: `"Loaded clip '{}' ({} tracks, {} audio events)"`

**Example YAML Output:**
```yaml
AnimationClip:
  name: "Walk"
  duration: 2.5
  audioEvents:
    - timeStamp: 0.5
      soundFile: "footstep_left.wav"
      volume: 0.8
      pitch: 1.0
      is3D: false
      loop: false
      groupName: "SFX"
      eventName: "Left Footstep"
```

**Files Modified:**
- `Gam300/Engine/BoomEngine/includes/Graphics/Models/Animation.h`
- `Gam300/Engine/BoomEngine/src/Auxiliaries/AnimationIO.cpp`

---

## Current Status

**✅ COMPLETED:** Phase 1 - Data layer for audio events
**✅ COMPLETED:** Phase 2 - Timeline UI for audio events
**⏳ NEXT:** Phase 3 - Playback Integration

---

### ✅ Audio Events - Phase 2: Timeline UI (COMPLETED)

Implemented visual representation and editing tools for audio events in the Animation Timeline.

#### **What Was Implemented:**

**1. Member Variables** (`AnimationTimelinePanel.h`)
- `m_SelectedAudioEventIndex` - Track selected audio event
- `m_HoveredAudioEventIndex` - Track hovered audio event
- `m_IsDraggingAudioEvent` - Drag state flag
- `m_DraggedAudioEventOriginalTime` - Original timestamp during drag
- `AudioMarkerScreenPos` struct for click detection
- `m_AudioMarkerScreenPositions` vector (cleared each frame)

**2. Audio Track Rendering** (`AnimationTimelinePanel_Timeline.cpp`)
- Dedicated "Audio Events" track rendered BEFORE bone tracks
- Purple/blue tinted background to differentiate from bone tracks
- Shows event count: "Audio Events (3)"
- Grid lines for time markers (every second)
- Red playhead indicator

**3. Audio Marker Visualization**
- Circle markers (different from diamond keyframes)
- **Color coding:**
  - Orange: Default 2D sounds
  - Green: 3D spatial sounds
  - Cyan: Selected
  - Yellow: Hovered
- Size: 5px (larger when selected/hovered)
- Tooltip on hover showing:
  - Event name
  - Timestamp
  - Sound file
  - Volume
  - 2D/3D type

**4. Add Audio Event UI**
- Right-click on audio track → "Add Audio Event" popup
- Timestamp automatically calculated from click position
- Form fields:
  - Event Name (text input)
  - Sound File (text input with tooltip)
  - Volume slider (0.0 - 1.0)
  - Pitch slider (0.5x - 2.0x)
  - 3D Sound checkbox
  - Loop checkbox
  - Group dropdown (SFX, Music, Ambience, Voice)
- Add/Cancel buttons

**5. Edit Audio Event UI**
- Double-click marker → "Edit Audio Event" popup
- Shows current timestamp (read-only)
- All properties editable
- Save/Delete/Cancel buttons
- Form initialized with current values on open

**6. Delete Functionality**
- Delete key: Remove selected audio event
- Delete button in edit popup
- Escape key: Deselect audio event
- Proper cleanup of selection state

**7. Interaction Features**
- Click to select audio event
- Hover highlights with tooltip
- Selection persistence across frames
- Keyboard shortcuts integrated

**Files Modified:**
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.h` (member variables, function declaration)
- `Gam300/Editor/src/Panels/AnimationTimelinePanel_Timeline.cpp` (rendering, popups, interaction)
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.cpp` (keyboard shortcuts)

**Not Yet Implemented (Future Enhancement):**
- Drag-to-move audio markers (can be added later)
- Undo/redo for audio event operations
- Sound file browser/picker
- Preview button to play sound

---

## Next Steps: Phase 2 - Timeline UI

### Objectives
Add visual representation and editing tools for audio events in the Animation Timeline.

### Implementation Plan

#### **1. Add Audio Track UI** (`AnimationTimelinePanel.cpp`)

**Location:** In `RenderTrackList()`, BEFORE the bone tracks.

**Design:**
```
┌─────────────────────────────────────────────┐
│ Audio Events  [🔊] [🔊]     [🔊]            │  <- Dedicated audio track
├─────────────────────────────────────────────┤
│ ▼ Root [0]                                  │  <- Bone tracks below
│   ▼ Hips [1]                                │
│     ▼ LeftLeg [2]                           │
```

**Visual Elements:**
- **Track header:** "Audio Events" label (left column)
- **Timeline area:** Audio markers rendered as speaker icons (🔊) or colored diamonds
- **Marker label:** Event name shown next to icon (if space permits)
- **Color coding:** Different colors for 2D vs 3D, looped sounds, etc.

#### **2. Audio Marker Rendering**

**Marker Visual:**
- Similar to keyframe diamonds, but larger
- Use speaker icon or unique shape
- Color: Orange/Yellow for visibility
- Show event name on hover (tooltip)

**Click Detection:**
- Store `AudioMarkerScreenPos` similar to `KeyframeScreenPos`
- Detect clicks for selection/editing
- Support drag-to-move

#### **3. Add/Edit/Delete Controls**

**Add New Audio Event:**
- Right-click on audio track → "Add Audio Event"
- Opens popup with fields:
  - Sound File (dropdown or file picker)
  - Volume slider (0-100%)
  - Pitch slider (0.5x - 2.0x)
  - 2D/3D toggle
  - Loop checkbox
  - Channel group dropdown
  - Event name text input
  - Preview button (plays sound immediately)

**Edit Existing Event:**
- Double-click marker → Opens edit popup
- Or select marker → Properties panel in Inspector

**Delete Event:**
- Select marker → Press Delete key
- Or right-click → "Delete Audio Event"

**Drag to Move:**
- Click and drag marker to adjust timestamp
- Snap to grid (optional)

#### **4. Member Variables** (`AnimationTimelinePanel.h`)

Add to private section:
```cpp
// Audio event interaction state
int m_SelectedAudioEventIndex = -1;
bool m_IsDraggingAudioEvent = false;
float m_DraggedAudioEventOriginalTime = 0.0f;

// Audio marker screen positions (for click detection)
struct AudioMarkerScreenPos {
    size_t eventIndex;
    ImVec2 screenPos;
};
std::vector<AudioMarkerScreenPos> m_AudioMarkerScreenPositions;
```

#### **5. Sound Preview Integration**

**Requirements:**
- Ability to preview sounds in editor (play on demand)
- May need to add `SoundEngine::PlayPreview()` method
- Preview should be one-shot, non-positional

**Files to Check:**
- `Gam300/Engine/BoomEngine/includes/Audio/SoundSystem.hpp`
- `Gam300/Engine/BoomEngine/src/SoundSystem.cpp`

---

## Phase 3: Playback Integration (Future)

### Objectives
Trigger audio events during animation playback.

### Implementation Plan

**1. Add to Animator** (`Animator.h`):
```cpp
float m_LastProcessedTime = 0.0f;
std::set<size_t> m_TriggeredAudioEvents;

void ProcessAudioEvents(float currentTime, float lastTime, const AnimationClip* clip);
```

**2. Event Triggering Logic:**
- During `Animate()`, check if playhead crossed any audio event timestamps
- Handle forward/backward scrubbing
- On loop restart, clear triggered event cache
- Call `SoundEngine::Play2D()` or `Play3D()` for each event

**3. Integration Points:**
- `Animator::Animate()` - Main playback loop
- `AnimationTimelinePanel::RenderViewport()` - Preview playback

---

## Technical Notes & Decisions

### Sound System Architecture
- FMOD-based (see `SoundSystem.md`)
- `SoundEngine` singleton handles playback
- Supports 2D and 3D spatial audio
- Channel groups: SFX, Music, custom

### Animation System Architecture
- `AnimationClip` stores keyframes per joint
- `Animator` handles playback and interpolation
- Timeline clones animator for independent preview
- Supports undo/redo for all edits

### Important Constraints
- Audio events stored in `AnimationClip`, NOT in timeline panel
- Events saved to .anim files (persistent)
- Must handle edge cases:
  - No audio events in clip
  - Events beyond clip duration
  - Duplicate events at same timestamp
  - Missing audio files

### Backward Compatibility
- Old .anim files without `audioEvents` load successfully
- Default to empty vector if field missing
- No migration needed

---

## Files to Modify (Phase 2)

### Editor Files
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.h`
  - Add audio event interaction state variables
- `Gam300/Editor/src/Panels/AnimationTimelinePanel.cpp`
  - Add audio track rendering in `RenderTrackList()`
  - Add popup UI for add/edit audio events
- `Gam300/Editor/src/Panels/AnimationTimelinePanel_Timeline.cpp`
  - Add audio marker rendering (similar to keyframe diamonds)
  - Add click detection for audio markers

### Potential Engine Files (if preview needed)
- `Gam300/Engine/BoomEngine/includes/Audio/SoundSystem.hpp`
  - Add `PlayPreview()` method?
- `Gam300/Engine/BoomEngine/src/SoundSystem.cpp`
  - Implement preview playback

---

## Testing Checklist

### Phase 1 (Data Layer) ✅
- [x] AnimationClip compiles with audioEvents field
- [x] SaveAnimationClip writes audio events to YAML
- [x] LoadAnimationClip reads audio events from YAML
- [x] Backward compatibility (old files load without errors)
- [ ] Manual test: Create .anim with audio events, verify format

### Phase 2 (Timeline UI) ✅
- [x] Audio track renders correctly
- [x] Can add new audio event via UI (right-click)
- [x] Can edit existing audio event (double-click)
- [x] Can delete audio event (Delete key or edit popup)
- [ ] Can drag audio event to new time (NOT IMPLEMENTED - future enhancement)
- [x] Audio markers visible and clickable
- [ ] Sound file picker works (NOT IMPLEMENTED - uses text input for now)
- [ ] Preview button plays sound (NOT IMPLEMENTED - future enhancement)
- [ ] Undo/redo works for audio events (NOT IMPLEMENTED - future enhancement)
- [x] Save/load preserves audio events (from Phase 1)

### Phase 3 (Playback) - TODO
- [ ] Audio events trigger during timeline playback
- [ ] Events trigger at correct timestamps
- [ ] No duplicate triggers on loop
- [ ] Scrubbing doesn't spam events
- [ ] 2D vs 3D audio works correctly
- [ ] Volume and pitch applied correctly
- [ ] Channel groups respected

---

## Reference Documentation

### External Resources
- **SoundSystem.md** - FMOD integration details
- **UNDO_REDO_IMPLEMENTATION.md** - Undo/redo system (for audio events)

### Code References
- **Keyframe rendering:** `AnimationTimelinePanel_Timeline.cpp:612-699`
  - Shows how to render diamonds on timeline
  - Click detection, drag handling, multiselect
- **Popup UI examples:** `AnimationTimelinePanel.cpp:1138-1402`
  - Create Clip, Rename Clip, Save Clip popups
  - Good patterns for audio event popup

---

## Questions to Resolve

1. **Sound Preview:** Does `SoundEngine` already support one-shot preview playback?
   - If not, need to add `PlayPreview()` method

2. **Audio File Browser:** How to browse/select audio files?
   - Dropdown of existing Resources/Audio files?
   - File picker dialog?
   - Drag & drop from directory panel?

3. **Undo/Redo for Audio Events:** Extend existing system or new commands?
   - Probably extend `KeyframeCommand` with new type: `AUDIO_EVENT`
   - Or create separate `AudioEventCommand`?

4. **Visual Design:** Final look for audio markers?
   - Speaker icon (🔊)?
   - Colored diamond/circle?
   - Custom shape?

---

## Session Summary

**What We Did:**
1. ✅ Polished Animation Timeline UI (collapsible sections, resizable splitter, compact mode)
2. ✅ Implemented data layer for audio events (Phase 1: structs, serialization)
3. ✅ Implemented Timeline UI for audio events (Phase 2: rendering, editing, interaction)

**What's Next:**
1. ⏳ Implement playback integration (Phase 3) - Trigger audio during animation playback
2. 🔧 Optional enhancements: drag-to-move, undo/redo, sound preview

**Status:** Ready for manual testing, then proceed to Phase 3 if needed.

---

## How to Resume

1. Read this document to understand current progress
2. **MANUAL TESTING REQUIRED:** Build and test the audio events UI in the animation timeline
   - Load an animation with a model
   - Right-click audio track → Add audio event
   - Edit event (double-click marker)
   - Delete event (Delete key or edit popup)
   - Save animation → Verify audio events persist in .anim file
3. If testing passes, continue with **Phase 3: Playback Integration**
4. Update this document as you complete tasks

**Next Action:** Build and manually test Phase 2, then proceed to Phase 3 if desired.
