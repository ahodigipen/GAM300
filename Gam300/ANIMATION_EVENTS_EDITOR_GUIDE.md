# Animation Events - Simple Editor Guide

## NO CODE REQUIRED! 🎉

You can now add, edit, and test animation events **entirely in the editor** without writing any code.

---

## Unity vs Your Engine

### Unity Workflow:
1. Open Animation window
2. Add event marker at specific time
3. Set function name
4. Write C# method in MonoBehaviour script
5. Unity calls method via reflection

### Your Engine Workflow:
1. Select entity with Animator
2. Open Inspector panel
3. Click **[+ Add Event]** button
4. Fill in function name and time
5. Click **[Test]** button to verify

**Same concept, simpler execution!**

---

## Step-by-Step: Adding Your First Event

### 1. Load an Animation Clip

1. Select entity with AnimatorComponent
2. In Inspector, find "Animation Clips" section
3. Drag an animation file (.fbx, .gltf) into the drop zone
4. Your clip appears in the list:
   ```
   • Walk (2.50s)
   ```

### 2. Add an Event

1. Click **[+ Add Event]** button under the clip
2. A dialog opens:
   ```
   Add Animation Event
   ─────────────────────
   Function Name:  [OnFootstep      ]
   Time (seconds): [0.50            ] ← Drag to change

   Optional Parameters
   ─────────────────────
   String: [                        ]
   Float:  [0.00                    ]
   Int:    [0                       ]

   [Save]  [Cancel]
   ```
3. Enter:
   - **Function Name**: `OnFootstep` (or any name you want)
   - **Time**: `0.5` (when event should fire, in seconds)
4. Click **[Save]**

### 3. Test the Event

The event now appears under your clip:
```
• Walk (2.50s) [1 events]
  0.50s: OnFootstep [Test] [Edit] [Delete]
```

Click **[Test]** button - you'll see in console:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔔 Animation Event Fired!
   Function: 'OnFootstep'
   Time: 0.50s
   ⚠️  No handler registered for 'OnFootstep'
   ℹ️  Register handlers in code or use built-in logging
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

**Success!** The event system is working! The warning about "no handler" is expected - you haven't written code yet.

---

## Adding Events with Parameters

### Example: Spawn VFX Event

1. Click **[+ Add Event]**
2. Fill in:
   ```
   Function Name:  SpawnDust
   Time (seconds): 0.30

   Optional Parameters:
   String: dust_particle_large    ← VFX name
   Float:  0.75                   ← Intensity
   Int:    5                      ← Particle count
   ```
3. Click **[Save]**

Now when you click **[Test]**:
```
🔔 Animation Event Fired!
   Function: 'SpawnDust'
   Time: 0.30s
   String Param: "dust_particle_large"
   Float Param: 0.750
   Int Param: 5
   ⚠️  No handler registered for 'SpawnDust'
```

Perfect! The parameters are stored and ready to use.

---

## Editing and Deleting Events

### Edit Event
1. Click **[Edit]** button next to event
2. Modify values in dialog
3. Click **[Save]**

### Delete Event
1. Click **[Delete]** button next to event
2. Event is removed instantly

---

## Events are Saved Automatically! 💾

When you save your scene, all events are saved with it:
- Event names
- Times
- Parameters

**No code changes = events persist!**

---

## Testing During Playback

Events automatically fire when animations play:

1. Set up an animation state with your clip
2. Make it the default state
3. Play your game/scene
4. When animation reaches event time → event fires!

You'll see in console:
```
🔔 Animation Event Fired!
   Function: 'OnFootstep'
   Time: 0.50s
   ⚠️  No handler registered
```

The event is firing! You just need to add code to DO something with it.

---

## When Do I Need Code?

### You DON'T need code to:
- ✅ Add events in editor
- ✅ Edit event times and parameters
- ✅ Test that events fire
- ✅ Save events with your scene

### You DO need code to:
- ❌ Make events DO something (play sounds, spawn VFX, etc.)
- ❌ React to events in gameplay

---

## Adding Code Handlers (Optional)

If you want events to actually DO something, add handlers in your game code:

```cpp
// In your entity/component initialization
animator->RegisterEventHandler("OnFootstep",
    [](const Boom::AnimationEvent& e) {
        // Your code here
        PlaySound("footstep.wav");
    });
```

Now when you click **[Test]**:
```
🔔 Animation Event Fired!
   Function: 'OnFootstep'
   Time: 0.50s
   ✅ Handler found - executing...
   ✅ Handler completed!
```

---

## Common Event Names

Use these standard names for consistency:

### Locomotion:
- `OnFootstepLeft`
- `OnFootstepRight`
- `OnJumpStart`
- `OnJumpLand`

### Combat:
- `OnAttackStart`
- `OnAttackHit` (add damage as int parameter)
- `OnAttackEnd`

### VFX:
- `SpawnVFX` (add VFX name as string parameter)
- `SpawnDust`
- `SpawnBlood`

### Audio:
- `PlaySound` (add sound name as string parameter)
- `PlayFootstep`
- `PlayImpact`

---

## Troubleshooting

### "I clicked [Test] but nothing happens"
- Check the console window - output goes there
- Look for the box border with "🔔 Animation Event Fired!"

### "Event doesn't fire during playback"
- Verify animation is actually playing
- Check event time is within clip duration
- Make sure you're using the correct clip

### "Events disappeared after scene reload"
- This shouldn't happen - events ARE saved
- Check you saved the scene before reloading
- Verify the clip is loaded (file path is correct)

### "I want events to DO something"
- Events only log by default
- Add code handlers to make them functional
- See `AnimationEventRegistry.h` for examples

---

## Quick Reference

| Action | Button | Location |
|--------|--------|----------|
| Add event | `[+ Add Event]` | Under each clip |
| Test event | `[Test]` | Next to each event |
| Edit event | `[Edit]` | Next to each event |
| Delete event | `[Delete]` | Next to each event |
| Add clip | Drag & drop | Clip drop zone |

---

## Example: Complete Walk Animation Setup

1. **Load clip**: Drag `walk.fbx` to drop zone
2. **Add left footstep**:
   - Time: `0.3s`
   - Function: `OnFootstepLeft`
3. **Add right footstep**:
   - Time: `0.8s`
   - Function: `OnFootstepRight`
4. **Test both**: Click [Test] on each → see console logs
5. **Save scene**: Events are now permanent!

Result:
```
• walk (1.50s) [2 events]
  0.30s: OnFootstepLeft [Test] [Edit] [Delete]
  0.80s: OnFootstepRight [Test] [Edit] [Delete]
```

When animation plays, you'll see:
```
[0.30s] 🔔 OnFootstepLeft fired!
[0.80s] 🔔 OnFootstepRight fired!
```

Perfect! Your animation is now synced with footsteps! 🎉

---

## Comparison to Unity

| Feature | Unity | Your Engine |
|---------|-------|-------------|
| Add events in editor | ✅ Animation Window | ✅ Inspector Panel |
| Event timing | ✅ Timeline scrubber | ✅ Numeric time input |
| Parameters | ✅ Object/String/Float/Int | ✅ String/Float/Int |
| Testing | ❌ Must run game | ✅ **[Test] button!** |
| Code required | ✅ C# methods | ✅ C++ callbacks |
| Auto-discovery | ✅ Reflection | ❌ Manual registration |
| Serialization | ✅ Saved | ✅ Saved |

**Your engine is actually BETTER for testing** - you can test events without running the game!

---

## Next Steps

1. **Practice**: Add events to all your animation clips
2. **Test**: Use [Test] buttons to verify timing
3. **Refine**: Adjust event times to match visuals
4. **Code** (when ready): Add handlers for actual functionality

You now have a complete, editor-driven animation event system! 🎊
