# 1D Blend Tree Guide

## What is a Blend Tree?

A **blend tree** smoothly blends between multiple animation clips based on a parameter value. Instead of abruptly switching from "walk" to "run", you get smooth transitions at any speed.

### Example: Locomotion
```
Parameter "Speed" = 0.0  →  Idle (100%)
Parameter "Speed" = 1.0  →  Walk (100%)
Parameter "Speed" = 1.5  →  Walk (50%) + Run (50%)  ← Blended!
Parameter "Speed" = 2.0  →  Run (100%)
```

---

## Step-by-Step: Creating a 1D Blend Tree

### Prerequisites
You need at least **2 animation clips** loaded (e.g., Idle, Walk, Run)

---

### Step 1: Create a Blend Tree State

**Option A: From Graph Panel**
1. Open **Animator Graph** panel
2. **Right-click** empty space
3. Select **"Add Blend Tree 1D"**
4. A new node appears labeled "Blend Tree"

**Option B: From Inspector**
1. Open **Inspector** panel
2. Click **[+ Add State]**
3. Click **[Edit]** on the new state
4. Change **Motion Type** to **"Blend Tree 1D"**

---

### Step 2: Configure the Blend Tree

1. **Right-click** the blend tree node
2. Select **"Edit State"**
3. You'll see:

```
Edit State
──────────────────────

Name:         [Blend Tree       ]
Motion Type:  [Blend Tree 1D ▼  ]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Blend Tree 1D
Parameter:    [Speed            ]  ← Control parameter name
Speed:        [1.00             ]  ← Playback speed multiplier
Loop:         [✓]                  ← Should loop

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Motions:
  Clip: [0] Threshold: [0.00]  [X]
  Clip: [1] Threshold: [1.00]  [X]

[Add Motion]  [Sort Motions]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Transitions:
(empty)

[Close]
```

---

### Step 3: Set the Parameter Name

1. In **Parameter** field, enter: `Speed`
   - This is the float parameter that controls blending
   - You can name it anything (Speed, Velocity, BlendWeight, etc.)

---

### Step 4: Add Motions (Animation Clips)

**Initial Setup:**
- By default, you get 2 motions at thresholds 0.0 and 1.0

**To Add More:**
1. Click **[Add Motion]**
2. A new motion appears with auto-incremented threshold
3. Set the **Clip** index (which animation to use)
4. Set the **Threshold** (parameter value for this clip)

**Example: Idle → Walk → Run**

```
Motion 1:
  Clip:      [0] (Idle)
  Threshold: [0.0]

Motion 2:
  Clip:      [1] (Walk)
  Threshold: [1.0]

Motion 3:
  Clip:      [2] (Run)
  Threshold: [2.0]
```

---

### Step 5: Sort Motions

**Important:** Motions MUST be sorted by threshold for correct blending!

1. Click **[Sort Motions]** after adding/editing
2. Or they're auto-sorted when you add via `[Add Motion]`

---

### Step 6: Create the Parameter

The blend tree needs a float parameter to control it:

1. Go to **Inspector** → **Parameters** section
2. Select **"Float"** from dropdown
3. Enter parameter name: `Speed`
4. Click **[Add]**

You'll see:
```
Parameters
──────────
[F] Speed  [====|====]  [X]
            ↑ Drag to change value
```

---

### Step 7: Test the Blend Tree

1. **Set this state as default:**
   - Right-click blend tree node → **"Set as Default"**

2. **Adjust the parameter:**
   - In Inspector, drag the **Speed** slider
   - Watch the animation blend smoothly!

**What Happens:**
```
Speed = 0.0   → Plays Idle (100%)
Speed = 0.5   → Blends Idle (50%) + Walk (50%)
Speed = 1.0   → Plays Walk (100%)
Speed = 1.5   → Blends Walk (50%) + Run (50%)
Speed = 2.0   → Plays Run (100%)
Speed > 2.0   → Plays Run (100%) - clamped to highest threshold
```

---

## Complete Example: Locomotion System

### Setup:

**Clips:**
- Clip 0: `idle.fbx` (character standing still)
- Clip 1: `walk.fbx` (slow walk)
- Clip 2: `run.fbx` (fast run)
- Clip 3: `sprint.fbx` (full sprint)

### Blend Tree Configuration:

1. **Create blend tree state** named "Movement"
2. **Parameter name:** `Speed`
3. **Motions:**
   ```
   Idle:   Clip 0, Threshold 0.0
   Walk:   Clip 1, Threshold 1.0
   Run:    Clip 2, Threshold 3.0
   Sprint: Clip 3, Threshold 5.0
   ```

4. **Set as default state**

### Control in Code:

```cpp
// Get animator
auto& animComp = entity.Get<Boom::AnimatorComponent>();
auto& animator = animComp.animator;

// Control character speed
float characterSpeed = GetMovementSpeed(); // e.g., 0-5

// Update blend tree parameter
animator->SetFloat("Speed", characterSpeed);

// Animation automatically blends based on speed!
```

### Result:

- Standing still (speed = 0) → Idle
- Walking slowly (speed = 1-2) → Smooth walk-to-run blend
- Running fast (speed = 3-4) → Smooth run-to-sprint blend
- Sprinting (speed = 5+) → Full sprint

**No manual animation switching needed!** The blend tree handles it automatically.

---

## Advanced: Multiple Blend Trees

### Common Setup: Combat + Movement

**State 1: "Locomotion" (Blend Tree 1D)**
- Parameter: `Speed`
- Motions: Idle, Walk, Run

**State 2: "Combat" (Single Clip)**
- Clip: Attack animation
- Non-looping

**Transitions:**
```
Locomotion → Combat
  Condition: Trigger "Attack"
  Duration: 0.2s

Combat → Locomotion
  Condition: None (auto-return)
  Has Exit Time: ✓
  Exit Time: 0.9 (near end of attack)
```

### Usage:
```cpp
// Normal movement - blend tree handles it
animator->SetFloat("Speed", playerSpeed);

// Attack pressed - switch to combat
if (Input::IsKeyPressed(Key::Space)) {
    animator->SetTrigger("Attack");
}
```

---

## Blend Tree Best Practices

### 1. Threshold Spacing

**Good:**
```
Idle:  0.0  (stopped)
Walk:  1.0  (slow)
Jog:   2.5  (medium)
Run:   5.0  (fast)
Sprint: 10.0 (max)
```

**Bad:**
```
Idle:  0.0
Walk:  0.1  ← Too close!
Run:   0.2  ← Hard to control
Sprint: 0.3
```

**Rule:** Space thresholds based on your input range. If your speed goes 0-10, use that full range.

---

### 2. Clip Compatibility

**✓ Good - Similar animations:**
- Idle → Walk → Run (same skeleton, same style)
- All clips have matching bone structure

**✗ Bad - Incompatible animations:**
- Walk → Backflip (too different, will look broken)
- Different skeletons (crashes!)

**Rule:** Only blend similar motions (same skeleton, compatible poses).

---

### 3. Parameter Ranges

**Define clear ranges:**
```cpp
// Movement speed: 0 = stopped, 10 = max
constexpr float MIN_SPEED = 0.0f;
constexpr float MAX_SPEED = 10.0f;

float speed = std::clamp(rawSpeed, MIN_SPEED, MAX_SPEED);
animator->SetFloat("Speed", speed);
```

**Rule:** Clamp parameters to expected range for predictable blending.

---

### 4. Speed Multiplier

The **Speed** setting in blend tree state controls playback speed:

```
Speed = 1.0  → Normal playback
Speed = 0.5  → Slow motion (all clips play at half speed)
Speed = 2.0  → Fast forward (all clips play at double speed)
```

**Use case:** Sync animation to character movement:
```cpp
// Character moving at 5 m/s, walk animation is for 3 m/s
float speedMultiplier = characterSpeed / walkAnimationSpeed;
state->speed = speedMultiplier; // Animation matches ground speed
```

---

## Common Use Cases

### 1. Locomotion (Idle-Walk-Run)
- **Parameter:** `Speed` (0-5)
- **Motions:** Idle (0), Walk (1.5), Run (3), Sprint (5)

### 2. Strafing (Forward/Backward)
- **Parameter:** `Direction` (-1 to 1)
- **Motions:** BackwardWalk (-1), Idle (0), ForwardWalk (1)

### 3. Aim Offset (Look Up/Down)
- **Parameter:** `AimPitch` (-90 to 90)
- **Motions:** LookDown (-90), LookForward (0), LookUp (90)

### 4. Weapon Stance
- **Parameter:** `Weapon` (0-2)
- **Motions:** Unarmed (0), Pistol (1), Rifle (2)

### 5. Emotion Intensity
- **Parameter:** `Happiness` (0-1)
- **Motions:** Neutral (0), Smile (0.5), Laugh (1)

---

## Troubleshooting

### Blend tree not blending?

**Check:**
1. ✓ Parameter exists and has correct name (case-sensitive!)
2. ✓ Parameter value is changing (check slider in Inspector)
3. ✓ Motions are sorted by threshold
4. ✓ All clip indices are valid (< clip count)
5. ✓ State is actually playing (check if it's the current state)

### Animations look broken?

**Possible causes:**
1. Clips have different skeletons (incompatible)
2. Thresholds too close together (rapid switching)
3. Clips at very different speeds (need speed multiplier)
4. Missing clips (invalid clip index)

### Parameter doesn't update?

**Check:**
```cpp
// Make sure you're calling this every frame with current value
animator->SetFloat("Speed", currentSpeed);

// NOT just once at startup!
```

---

## Quick Reference

### Editor Actions:
| Action | Steps |
|--------|-------|
| Create blend tree | Right-click empty space → Add Blend Tree 1D |
| Edit blend tree | Right-click node → Edit State |
| Add motion | In edit dialog → [Add Motion] |
| Remove motion | Click [X] next to motion |
| Sort motions | Click [Sort Motions] |
| Add parameter | Inspector → Parameters → Select Float → Add |

### Code Actions:
| Action | Code |
|--------|------|
| Set parameter | `animator->SetFloat("Speed", value);` |
| Get parameter | `float val = animator->GetFloat("Speed");` |
| Check state | `size_t idx = animator->GetCurrentStateIndex();` |

---

## What's Next?

Once you master 1D blend trees, you can:
- ✅ Create smooth locomotion systems
- ✅ Blend weapon states
- ✅ Implement directional movement
- ✅ Control animation intensity

**Coming Soon:** 2D Blend Spaces (blend on TWO parameters for full directional movement!)

---

Enjoy your smooth animations! 🎮
