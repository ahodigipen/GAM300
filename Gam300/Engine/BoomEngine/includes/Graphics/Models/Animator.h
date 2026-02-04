#pragma once
#include "Animation.h"
#include "AnimationIO.h"
#include "Audio/Audio.hpp"  // For audio event playback
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <set>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <iostream>

namespace Boom
{
    struct Animator
    {
        // === STATE MACHINE STRUCTURES ===
        struct Transition
        {
            size_t targetStateIndex = 0;

            // Condition system
            enum ConditionType { NONE, FLOAT_GREATER, FLOAT_LESS, BOOL_EQUALS, TRIGGER };
            ConditionType conditionType = NONE;
            std::string parameterName;
            float floatValue = 0.0f;
            bool boolValue = false;

            // Transition settings
            float transitionDuration = 0.25f;
            bool hasExitTime = false;
            float exitTime = 0.9f;  // Normalized [0-1]

            bool EvaluateCondition(const Animator& animator) const;
        };

        // === BLEND TREE STRUCTURES ===
        struct BlendTreeMotion
        {
            size_t clipIndex = 0;
            float threshold = 0.0f;  // Parameter value for this motion
        };

        struct BlendTree1D
        {
            std::string parameterName;
            std::vector<BlendTreeMotion> motions;

            // Sort motions by threshold for efficient blending
            void SortMotions() {
                std::sort(motions.begin(), motions.end(),
                    [](const BlendTreeMotion& a, const BlendTreeMotion& b) {
                        return a.threshold < b.threshold;
                    });
            }
        };

        struct State
        {
            std::string name = "New State";

            // Motion type
            enum MotionType { SINGLE_CLIP, BLEND_TREE_1D };
            MotionType motionType = SINGLE_CLIP;

            // Single clip mode
            size_t clipIndex = 0;

            // Blend tree mode
            BlendTree1D blendTree;

            float speed = 1.0f;
            bool loop = true;
            std::vector<Transition> transitions;
        };

        BOOM_INLINE auto& Animate(float deltaTime)
        {
            // Store time before update for audio event processing
            float timeBeforeUpdate = m_Time;

            // State machine mode
            if (m_EnableStateMachine && !m_States.empty() && m_CurrentStateIndex < m_States.size())
            {
                EvaluateTransitions(deltaTime);

                // Handle blending between states
                if (m_IsBlending)
                {
                    m_BlendProgress += deltaTime / m_BlendDuration;

                    if (m_BlendProgress >= 1.0f)
                    {
                        // Blend complete
                        m_IsBlending = false;
                        m_BlendProgress = 1.0f;
                        m_CurrentStateIndex = m_TargetStateIndex;
                        m_Time = m_TargetTime;  // Continue from where blend left off
                    }

                    // Blend between two states
                    BlendStates(deltaTime);
                }
                else
                {
                    // Normal single-state animation
                    const State& currentState = m_States[m_CurrentStateIndex];

                    if (currentState.motionType == State::BLEND_TREE_1D)
                    {
                        // Blend tree mode
                        EvaluateBlendTree1D(currentState, deltaTime);
                    }
                    else if (currentState.clipIndex < m_Clips.size())
                    {
                        // Single clip mode
                        auto& clip = m_Clips[currentState.clipIndex];
                        float timeAfterAdvance = m_Time + clip->ticksPerSecond * currentState.speed * deltaTime;
                        bool looped = false;

                        if (currentState.loop)
                        {
                            looped = (timeAfterAdvance >= clip->duration);
                            m_Time = fmod(timeAfterAdvance, clip->duration);
                        }
                        else
                        {
                            m_Time = std::min(timeAfterAdvance, clip->duration);
                        }

                        // Process audio events for this clip
                        ProcessAudioEvents(clip.get(), timeBeforeUpdate, m_Time, looped);

                        // Check for Root Motion override in parameters
                        auto it = m_BoolParams.find("ApplyRootMotion");
                        if (it != m_BoolParams.end())
                        {
                            m_ApplyRootMotion = it->second;
                        }

                        UpdateJoints(m_Root, glm::identity<glm::mat4>(), true);
                    }
                }
            }
            // Legacy clip-based mode (fallback)
            else if (m_CurrentClip < m_Clips.size())
            {
                auto& clip = m_Clips[m_CurrentClip];
                float timeAfterAdvance = m_Time + clip->ticksPerSecond * deltaTime;
                bool looped = (timeAfterAdvance >= clip->duration);

                m_Time = fmod(timeAfterAdvance, clip->duration);

                // Process audio events for this clip
                ProcessAudioEvents(clip.get(), timeBeforeUpdate, m_Time, looped);

                // Check for Root Motion override in parameters
                auto it = m_BoolParams.find("ApplyRootMotion");
                if (it != m_BoolParams.end())
                {
                    m_ApplyRootMotion = it->second;
                }

                UpdateJoints(m_Root, glm::identity<glm::mat4>(), true);
            }

            // Update last processed time
            m_LastProcessedTime = m_Time;

            // Clear triggers after each frame
            m_Triggers.clear();

            return m_Transforms;
        }

        BOOM_INLINE auto& GetJoints() {
            return m_Transforms;
        }

        // NEW: Switch animation at runtime
        BOOM_INLINE void PlayClip(size_t clipIndex)
        {
            if (clipIndex < m_Clips.size())
            {
                m_CurrentClip = clipIndex;
                m_Time = 0.0f; // Reset time when switching
            }
        }

        BOOM_INLINE void PlayClip(const std::string& clipName)
        {
            for (size_t i = 0; i < m_Clips.size(); ++i)
            {
                if (m_Clips[i]->name == clipName)
                {
                    PlayClip(i);
                    return;
                }
            }
        }

        // Getters
        BOOM_INLINE size_t GetCurrentClip() const { return m_CurrentClip; }
        BOOM_INLINE float GetTime() const { return m_Time; }
        BOOM_INLINE size_t GetClipCount() const { return m_Clips.size(); }
        BOOM_INLINE const AnimationClip* GetClip(size_t index) const
        {
            return (index < m_Clips.size()) ? m_Clips[index].get() : nullptr;
        }

        // Mutable version for editing (used by Animation Timeline panel)
        BOOM_INLINE AnimationClip* GetClipMutable(size_t index)
        {
            return (index < m_Clips.size()) ? m_Clips[index].get() : nullptr;
        }

        BOOM_INLINE size_t GetSequence() const { return GetCurrentClip(); }
        BOOM_INLINE void SetSequence(size_t index) { PlayClip(index); }
        


        // === STATE MACHINE API ===

        // State management
        BOOM_INLINE size_t AddState(const std::string& name, size_t clipIndex)
        {
            State state;
            state.name = name;
            state.clipIndex = clipIndex;
            m_States.push_back(state);
            return m_States.size() - 1;
        }

        BOOM_INLINE void RemoveState(size_t stateIndex)
        {
            if (stateIndex < m_States.size())
            {
                m_States.erase(m_States.begin() + stateIndex);
            }
        }

        BOOM_INLINE State* GetState(size_t index)
        {
            return (index < m_States.size()) ? &m_States[index] : nullptr;
        }

        BOOM_INLINE const State* GetState(size_t index) const
        {
            return (index < m_States.size()) ? &m_States[index] : nullptr;
        }

        BOOM_INLINE size_t GetStateCount() const { return m_States.size(); }

        BOOM_INLINE size_t GetCurrentStateIndex() const { return m_CurrentStateIndex; }

        BOOM_INLINE const State* GetCurrentState() const
        {
            return GetState(m_CurrentStateIndex);
        }

        BOOM_INLINE bool IsBlending() const { return m_IsBlending; }
        BOOM_INLINE float GetBlendProgress() const { return m_BlendProgress; }

        BOOM_INLINE void SetDefaultState(size_t stateIndex)
        {
            if (stateIndex < m_States.size())
            {
                m_CurrentStateIndex = stateIndex;
                m_Time = 0.0f;
            }
        }

        // Transition management
        BOOM_INLINE void AddTransition(size_t fromState, size_t toState,
            Transition::ConditionType condType = Transition::NONE,
            const std::string& paramName = "",
            float floatVal = 0.0f,
            bool boolVal = false)
        {
            if (fromState < m_States.size())
            {
                Transition trans;
                trans.targetStateIndex = toState;
                trans.conditionType = condType;
                trans.parameterName = paramName;
                trans.floatValue = floatVal;
                trans.boolValue = boolVal;
                m_States[fromState].transitions.push_back(trans);
            }
        }

        // Parameter API
        BOOM_INLINE void SetFloat(const std::string& name, float value)
        {
            m_FloatParams[name] = value;
        }

        BOOM_INLINE float GetFloat(const std::string& name) const
        {
            auto it = m_FloatParams.find(name);
            return (it != m_FloatParams.end()) ? it->second : 0.0f;
        }

        BOOM_INLINE void SetBool(const std::string& name, bool value)
        {
            m_BoolParams[name] = value;
        }

        BOOM_INLINE bool GetBool(const std::string& name) const
        {
            auto it = m_BoolParams.find(name);
            return (it != m_BoolParams.end()) ? it->second : false;
        }

        BOOM_INLINE void SetTrigger(const std::string& name)
        {
            m_Triggers.insert(name);
        }

        BOOM_INLINE bool GetTrigger(const std::string& name) const
        {
            return m_Triggers.count(name) > 0;
        }

        // Access to all parameters (for editor UI)
        // Floats
        BOOM_INLINE const auto& GetFloatParams() const { return m_FloatParams; }
        BOOM_INLINE auto& GetFloatParams() { return m_FloatParams; }

        // Bools
        BOOM_INLINE const auto& GetBoolParams() const { return m_BoolParams; }
        BOOM_INLINE auto& GetBoolParams() { return m_BoolParams; }

        // Triggers
        BOOM_INLINE const auto& GetTriggers() const { return m_Triggers; }
        BOOM_INLINE auto& GetTriggers() { return m_Triggers; }

        BOOM_INLINE std::vector<State>& GetStates() { return m_States; }
        BOOM_INLINE const std::vector<State>& GetStates() const { return m_States; }

        BOOM_INLINE void AddClip(std::shared_ptr<AnimationClip> clip) {
            m_Clips.push_back(clip);
        }
        BOOM_INLINE void SetStateMachineEnabled(bool enabled) { m_EnableStateMachine = enabled; }
        BOOM_INLINE bool IsStateMachineEnabled() const { return m_EnableStateMachine; }

        BOOM_INLINE void RemoveClip(size_t index) {
            if (index < m_Clips.size()) {
                m_Clips.erase(m_Clips.begin() + index);
            }
        }

        // Replace clip at index with a new clip (for syncing between timeline and entity)
        BOOM_INLINE void SetClip(size_t index, std::shared_ptr<AnimationClip> clip) {
            if (index < m_Clips.size()) {
                m_Clips[index] = clip;
            }
        }

        // Get shared_ptr to clip (for sharing between animators)
        BOOM_INLINE std::shared_ptr<AnimationClip> GetClipShared(size_t index) {
            return (index < m_Clips.size()) ? m_Clips[index] : nullptr;
        }

        // === KEYFRAME EDITING API (for Animation Timeline) ===

        // Get mutable track for editing
        BOOM_INLINE std::vector<KeyFrame>* GetTrackMutable(size_t clipIndex, const std::string& jointName)
        {
            if (clipIndex >= m_Clips.size()) return nullptr;
            auto& clip = m_Clips[clipIndex];
            auto it = clip->tracks.find(jointName);
            return (it != clip->tracks.end()) ? &it->second : nullptr;
        }

        // Add keyframe to track (maintains sorted order by timestamp)
        BOOM_INLINE bool AddKeyframe(size_t clipIndex, const std::string& jointName, const KeyFrame& keyframe)
        {
            if (clipIndex >= m_Clips.size()) return false;
            auto& clip = m_Clips[clipIndex];

            // Create track if it doesn't exist
            auto& track = clip->tracks[jointName];

            // Find insertion point to maintain sorted order
            auto insertPos = std::lower_bound(track.begin(), track.end(), keyframe,
                [](const KeyFrame& a, const KeyFrame& b) { return a.timeStamp < b.timeStamp; });

            track.insert(insertPos, keyframe);
            return true;
        }

        // Remove keyframe at specific index from track
        BOOM_INLINE bool RemoveKeyframe(size_t clipIndex, const std::string& jointName, size_t keyframeIndex)
        {
            auto* track = GetTrackMutable(clipIndex, jointName);
            if (!track || keyframeIndex >= track->size()) return false;

            track->erase(track->begin() + keyframeIndex);
            return true;
        }

        // Update keyframe timestamp (re-sorts track)
        BOOM_INLINE bool UpdateKeyframeTime(size_t clipIndex, const std::string& jointName, size_t keyframeIndex, float newTime)
        {
            auto* track = GetTrackMutable(clipIndex, jointName);
            if (!track || keyframeIndex >= track->size()) return false;

            // Store the keyframe data
            KeyFrame kf = (*track)[keyframeIndex];
            kf.timeStamp = newTime;

            // Remove old keyframe
            track->erase(track->begin() + keyframeIndex);

            // Re-insert at correct position
            auto insertPos = std::lower_bound(track->begin(), track->end(), kf,
                [](const KeyFrame& a, const KeyFrame& b) { return a.timeStamp < b.timeStamp; });
            track->insert(insertPos, kf);

            return true;
        }

        // === AUDIO EVENT API ===

        // Enable/disable audio event triggering (useful when scrubbing timeline)
        BOOM_INLINE void SetAudioEventsEnabled(bool enabled)
        {
            m_AudioEventsEnabled = enabled;
            if (!enabled)
            {
                m_TriggeredAudioEvents.clear();  // Reset when disabling
            }
        }

        BOOM_INLINE bool GetAudioEventsEnabled() const { return m_AudioEventsEnabled; }

        // Set position for 3D audio events (call from entity's transform update)
        BOOM_INLINE void SetAudioSourcePosition(const glm::vec3& position)
        {
            m_AudioSourcePosition = position;
        }

        BOOM_INLINE const glm::vec3& GetAudioSourcePosition() const { return m_AudioSourcePosition; }

        // Process audio events for a time range (for editor use when not calling Animate())
        // Call this when manually advancing time (e.g., in editor timeline playback)
        BOOM_INLINE void ProcessAudioEventsForClip(size_t clipIndex, float lastTime, float currentTime, bool looped)
        {
            if (clipIndex < m_Clips.size())
            {
                ProcessAudioEvents(m_Clips[clipIndex].get(), lastTime, currentTime, looped);
            }
        }

        // Reset triggered events (call when restarting animation or seeking)
        BOOM_INLINE void ResetAudioEventTriggers()
        {
            m_TriggeredAudioEvents.clear();
            m_LastProcessedTime = m_Time;
        }

        // === ROOT MOTION API ===
        BOOM_INLINE void SetApplyRootMotion(bool apply) { m_ApplyRootMotion = apply; }
        BOOM_INLINE bool GetApplyRootMotion() const { return m_ApplyRootMotion; }

    private:
        // === AUDIO EVENT PROCESSING ===

        BOOM_INLINE void ProcessAudioEvents(const AnimationClip* clip, float lastTime, float currentTime, bool looped)
        {
            if (!m_AudioEventsEnabled || !clip || clip->audioEvents.empty())
                return;

            // Generate a unique prefix for this animator's sounds to avoid conflicts
            // Use pointer address as unique ID
            std::string soundPrefix = "anim_" + std::to_string(reinterpret_cast<uintptr_t>(this)) + "_";

            for (size_t i = 0; i < clip->audioEvents.size(); ++i)
            {
                const AudioEventMarker& event = clip->audioEvents[i];

                // Check if this event should trigger
                bool shouldTrigger = false;

                if (looped)
                {
                    // Animation looped - reset triggers and check if we crossed this event
                    // Event triggers if: (lastTime <= duration AND event.timeStamp > lastTime) OR event.timeStamp <= currentTime
                    if (event.timeStamp > lastTime || event.timeStamp <= currentTime)
                    {
                        shouldTrigger = true;
                    }
                }
                else
                {
                    // Normal playback - event triggers if we crossed its timestamp
                    // Forward: lastTime < event.timeStamp <= currentTime
                    if (lastTime < event.timeStamp && event.timeStamp <= currentTime)
                    {
                        shouldTrigger = true;
                    }
                }

                // Skip if already triggered this cycle (prevents double-triggering)
                if (shouldTrigger && m_TriggeredAudioEvents.find(i) == m_TriggeredAudioEvents.end())
                {
                    // Mark as triggered
                    m_TriggeredAudioEvents.insert(i);

                    // Generate unique sound name for this event
                    std::string soundName = soundPrefix + (event.eventName.empty() ? std::to_string(i) : event.eventName);

                    // Play the sound
                    if (event.is3D)
                    {
                        // PlaySoundAt signature: (name, filePath, position, loop)
                        SoundEngine::Instance().PlaySoundAt(soundName, event.soundFile, m_AudioSourcePosition, event.loop);
                    }
                    else
                    {
                        SoundEngine::Instance().PlaySound(soundName, event.soundFile, event.loop, event.groupName);
                    }

                    // Apply volume and pitch
                    SoundEngine::Instance().SetVolume(soundName, event.volume);
                    SoundEngine::Instance().SetPitch(soundName, event.pitch);

                    BOOM_INFO("[AudioEvent] Triggered '{}' at {:.2f}s (sound: {})",
                              event.eventName.empty() ? "Unnamed" : event.eventName,
                              event.timeStamp,
                              event.soundFile);
                }
            }

            // If looped, clear triggered events for next cycle
            if (looped)
            {
                m_TriggeredAudioEvents.clear();
            }
        }

         // === STATE MACHINE HELPERS ===

         BOOM_INLINE void EvaluateTransitions([[maybe_unused]] float deltaTime)
         {
             if (m_IsBlending) return;

             const State& currentState = m_States[m_CurrentStateIndex];

             for (const Transition& trans : currentState.transitions)
             {
                 bool shouldTransition = false;


                 // Check exit time
                 if (trans.hasExitTime && currentState.clipIndex < m_Clips.size())
                 {
                     const auto& clip = m_Clips[currentState.clipIndex];
                     float normalizedTime = m_Time / clip->duration;
                     if (normalizedTime < trans.exitTime) continue;
                 }

                 // Check condition
                 if (trans.EvaluateCondition(*this))
                 {
                     shouldTransition = true;
                 }

                 if (shouldTransition && trans.targetStateIndex < m_States.size())
                 {
                     m_TargetStateIndex = trans.targetStateIndex;
                     m_BlendDuration = trans.transitionDuration;
                     m_BlendProgress = 0.0f;
                     m_IsBlending = true;

                     // Sync time using normalized position
                     const State& targetState = m_States[trans.targetStateIndex];
                     if (targetState.clipIndex < m_Clips.size() && currentState.clipIndex < m_Clips.size())
                     {
                         auto& currentClip = m_Clips[currentState.clipIndex];
                         auto& targetClip = m_Clips[targetState.clipIndex];

                         // Match phase: if current is 70% through, target starts at 70%
                         float normalizedTime = std::fmod(m_Time / currentClip->duration, 1.0f);
                         m_TargetTime = normalizedTime * targetClip->duration;
                     }
                     else
                     {
                         m_TargetTime = 0.0f;
                     }

                     break;
                 }
             }
         }

         BOOM_INLINE void EvaluateBlendTree1D(const State& state, float deltaTime)
         {
             const BlendTree1D& blendTree = state.blendTree;

             if (blendTree.motions.empty()) return;
             if (blendTree.parameterName.empty()) return;

             // Get parameter value
             float paramValue = GetFloat(blendTree.parameterName);

             // Find two closest motions to blend
             size_t lowerIndex = 0;
             size_t upperIndex = 0;
             float blendWeight = 0.0f;

             if (blendTree.motions.size() == 1)
             {
                 // Only one motion, no blending needed
                 lowerIndex = upperIndex = 0;
                 blendWeight = 0.0f;
             }
             else
             {
                 // Find the two motions to blend between
                 bool found = false;
                 for (size_t i = 0; i < blendTree.motions.size() - 1; ++i)
                 {
                     if (paramValue >= blendTree.motions[i].threshold &&
                         paramValue <= blendTree.motions[i + 1].threshold)
                     {
                         lowerIndex = i;
                         upperIndex = i + 1;

                         float range = blendTree.motions[upperIndex].threshold - blendTree.motions[lowerIndex].threshold;
                         if (range > 0.001f)
                         {
                             blendWeight = (paramValue - blendTree.motions[lowerIndex].threshold) / range;
                         }
                         found = true;
                         break;
                     }
                 }

                 if (!found)
                 {
                     // Clamp to edges
                     if (paramValue < blendTree.motions[0].threshold)
                     {
                         lowerIndex = upperIndex = 0;
                         blendWeight = 0.0f;
                     }
                     else
                     {
                         lowerIndex = upperIndex = blendTree.motions.size() - 1;
                         blendWeight = 0.0f;
                     }
                 }
             }

             // Get the two clips
             size_t lowerClipIdx = blendTree.motions[lowerIndex].clipIndex;
             size_t upperClipIdx = blendTree.motions[upperIndex].clipIndex;

             if (lowerClipIdx >= m_Clips.size() || upperClipIdx >= m_Clips.size())
                 return;

             auto& lowerClip = m_Clips[lowerClipIdx];
             auto& upperClip = m_Clips[upperClipIdx];

             // Store time before advance for audio events
             float timeBeforeAdvance = m_Time;

             // Advance time
             float timeAfterAdvance = m_Time + lowerClip->ticksPerSecond * state.speed * deltaTime;
             bool looped = false;

             if (state.loop)
             {
                 looped = (timeAfterAdvance >= lowerClip->duration);
                 m_Time = fmod(timeAfterAdvance, lowerClip->duration);
             }
             else
             {
                 m_Time = std::min(timeAfterAdvance, lowerClip->duration);
             }

             // Process audio events from the primary (lower) clip
             ProcessAudioEvents(lowerClip.get(), timeBeforeAdvance, m_Time, looped);

             // Blend the two animations
             if (blendWeight < 0.001f)
             {
                 // Just use lower clip
                 UpdateJoints(m_Root, glm::identity<glm::mat4>(), true);
             }
             else
             {
                 // Blend between clips
                 BlendJointsFromClips(m_Root, glm::identity<glm::mat4>(), lowerClip.get(), upperClip.get(), blendWeight, true);
             }
         }

         BOOM_INLINE void BlendStates(float deltaTime)
         {
             if (m_CurrentStateIndex >= m_States.size() || m_TargetStateIndex >= m_States.size())
                 return;

             const State& fromState = m_States[m_CurrentStateIndex];
             const State& toState = m_States[m_TargetStateIndex];

             if (fromState.clipIndex >= m_Clips.size() || toState.clipIndex >= m_Clips.size())
                 return;

             auto& fromClip = m_Clips[fromState.clipIndex];
             auto& toClip = m_Clips[toState.clipIndex];

             // Freeze "from" animation at blend start pose
             // (Don't advance m_Time)

             // Advance "to" animation
             m_TargetTime += toClip->ticksPerSecond * toState.speed * deltaTime;
             m_TargetTime = fmod(m_TargetTime, toClip->duration);

             BlendJoints(m_Root, glm::identity<glm::mat4>(), fromClip.get(), toClip.get(), m_BlendProgress, true);
         }

         BOOM_INLINE void BlendJoints(Joint& joint, const glm::mat4& parentTransform,
             const AnimationClip* fromClip, const AnimationClip* toClip, float weight, bool isRoot = false)
         {
             glm::mat4 fromTransform = glm::mat4(1.0f);
             glm::mat4 toTransform = glm::mat4(1.0f);

             // Get "from" clip transform
             const auto* fromKeys = fromClip->GetTrack(joint.name);
             if (fromKeys && !fromKeys->empty())
             {
                 if (fromKeys->size() >= 2)
                 {
                     KeyFrame prev, next;
                     GetPreviousAndNextFrames(*fromKeys, prev, next);
                     float prog = 0.0f;
                     float dt = next.timeStamp - prev.timeStamp;
                     if (dt > 0.0f) prog = (m_Time - prev.timeStamp) / dt;
                     fromTransform = Interpolate(prev, next, prog);
                 }
                 else
                 {
                     const KeyFrame& key = (*fromKeys)[0];
                     fromTransform = glm::translate(glm::mat4(1.0f), key.position) *
                         glm::toMat4(key.rotation) *
                         glm::scale(glm::mat4(1.0f), key.scale);
                 }
             }

             // Get "to" clip transform
             const auto* toKeys = toClip->GetTrack(joint.name);
             if (toKeys && !toKeys->empty())
             {
                 if (toKeys->size() >= 2)
                 {
                     KeyFrame prev, next;
                     float savedTime = m_Time;
                     m_Time = m_TargetTime;
                     GetPreviousAndNextFrames(*toKeys, prev, next);
                     m_Time = savedTime;

                     float prog = 0.0f;
                     float dt = next.timeStamp - prev.timeStamp;
                     if (dt > 0.0f) prog = (m_TargetTime - prev.timeStamp) / dt;
                     toTransform = Interpolate(prev, next, prog);
                 }
                 else
                 {
                     const KeyFrame& key = (*toKeys)[0];
                     toTransform = glm::translate(glm::mat4(1.0f), key.position) *
                         glm::toMat4(key.rotation) *
                         glm::scale(glm::mat4(1.0f), key.scale);
                 }
             }

             // Decompose and blend
             glm::vec3 fromPos, toPos, fromScale, toScale;
             glm::quat fromRot, toRot;

             DecomposeMatrix(fromTransform, fromPos, fromRot, fromScale);
             DecomposeMatrix(toTransform, toPos, toRot, toScale);

             glm::vec3 blendedPos = glm::mix(fromPos, toPos, weight);
             
             // ROOT MOTION STRIPPING
             if (isRoot && !m_ApplyRootMotion)
             {
                 blendedPos = glm::vec3(0.0f); // Zero out translation for root
             }

             glm::quat blendedRot = glm::slerp(fromRot, toRot, weight);
             glm::vec3 blendedScale = glm::mix(fromScale, toScale, weight);

             glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), blendedPos) *
                 glm::toMat4(blendedRot) *
                 glm::scale(glm::mat4(1.0f), blendedScale);

             glm::mat4 worldTransform = parentTransform * localTransform;
             m_Transforms[joint.index] = worldTransform * m_GlobalTransform * joint.offset;

             for (auto& child : joint.children)
             {
                 BlendJoints(child, worldTransform, fromClip, toClip, weight, false);
             }
         }

         BOOM_INLINE void BlendJointsFromClips(Joint& joint, const glm::mat4& parentTransform,
             const AnimationClip* clip1, const AnimationClip* clip2, float weight, bool isRoot = false)
         {
             glm::mat4 transform1 = glm::mat4(1.0f);
             glm::mat4 transform2 = glm::mat4(1.0f);

             // Get transform from clip1 at current time
             const auto* keys1 = clip1->GetTrack(joint.name);
             if (keys1 && !keys1->empty())
             {
                 if (keys1->size() >= 2)
                 {
                     KeyFrame prev, next;
                     GetPreviousAndNextFrames(*keys1, prev, next);
                     float prog = 0.0f;
                     float dt = next.timeStamp - prev.timeStamp;
                     if (dt > 0.0f) prog = (m_Time - prev.timeStamp) / dt;
                     transform1 = Interpolate(prev, next, prog);
                 }
                 else
                 {
                     const KeyFrame& key = (*keys1)[0];
                     transform1 = glm::translate(glm::mat4(1.0f), key.position) *
                         glm::toMat4(key.rotation) *
                         glm::scale(glm::mat4(1.0f), key.scale);
                 }
             }

             // Get transform from clip2 at current time (synchronized)
             const auto* keys2 = clip2->GetTrack(joint.name);
             if (keys2 && !keys2->empty())
             {
                 if (keys2->size() >= 2)
                 {
                     KeyFrame prev, next;
                     GetPreviousAndNextFrames(*keys2, prev, next);
                     float prog = 0.0f;
                     float dt = next.timeStamp - prev.timeStamp;
                     if (dt > 0.0f) prog = (m_Time - prev.timeStamp) / dt;
                     transform2 = Interpolate(prev, next, prog);
                 }
                 else
                 {
                     const KeyFrame& key = (*keys2)[0];
                     transform2 = glm::translate(glm::mat4(1.0f), key.position) *
                         glm::toMat4(key.rotation) *
                         glm::scale(glm::mat4(1.0f), key.scale);
                 }
             }

             // Decompose and blend
             glm::vec3 pos1, pos2, scale1, scale2;
             glm::quat rot1, rot2;

             DecomposeMatrix(transform1, pos1, rot1, scale1);
             DecomposeMatrix(transform2, pos2, rot2, scale2);

             glm::vec3 blendedPos = glm::mix(pos1, pos2, weight);

             // ROOT MOTION STRIPPING
             if (isRoot && !m_ApplyRootMotion)
             {
                 blendedPos = glm::vec3(0.0f); // Zero out translation for root
             }

             glm::quat blendedRot = glm::slerp(rot1, rot2, weight);
             glm::vec3 blendedScale = glm::mix(scale1, scale2, weight);

             glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), blendedPos) *
                 glm::toMat4(blendedRot) *
                 glm::scale(glm::mat4(1.0f), blendedScale);

             glm::mat4 worldTransform = parentTransform * localTransform;
             m_Transforms[joint.index] = worldTransform * m_GlobalTransform * joint.offset;

             for (auto& child : joint.children)
             {
                 BlendJointsFromClips(child, worldTransform, clip1, clip2, weight, false);
             }
         }

     BOOM_INLINE void DecomposeMatrix(const glm::mat4& mat, glm::vec3& pos, glm::quat& rot, glm::vec3& scale)
     {
         pos = glm::vec3(mat[3]);

         glm::vec3 col0(mat[0]);
         glm::vec3 col1(mat[1]);
         glm::vec3 col2(mat[2]);

         scale.x = glm::length(col0);
         scale.y = glm::length(col1);
         scale.z = glm::length(col2);

         if (scale.x != 0) col0 /= scale.x;
         if (scale.y != 0) col1 /= scale.y;
         if (scale.z != 0) col2 /= scale.z;

         glm::mat3 rotMat(col0, col1, col2);
         rot = glm::quat_cast(rotMat);
     }

    private:
        BOOM_INLINE void GetPreviousAndNextFrames(
            const std::vector<KeyFrame>& keys,
            KeyFrame& prev,
            KeyFrame& next) const
        {
            if (keys.size() < 2)
            {
                if (!keys.empty()) prev = next = keys[0];
                return;
            }

            for (uint32_t i = 1; i < keys.size(); i++)
            {
                if (m_Time < keys[i].timeStamp)
                {
                    prev = keys[i - 1];
                    next = keys[i];
                    return;
                }
            }

            // If we're past the last keyframe (shouldn't happen with fmod)
            prev = next = keys.back();
        }

        BOOM_INLINE glm::mat4 Interpolate(const KeyFrame& prev, const KeyFrame& next, float progression) const
        {
            return glm::translate(glm::mat4(1.0f), glm::mix(prev.position, next.position, progression)) *
                glm::toMat4(glm::normalize(glm::slerp(prev.rotation, next.rotation, progression))) *
                glm::scale(glm::mat4(1.0f), glm::mix(prev.scale, next.scale, progression));
        }

        BOOM_INLINE void UpdateJoints(Joint& joint, const glm::mat4& parentTransform, bool isRoot = false)
        {
            glm::mat4 localTransform = glm::mat4(1.0f); // Default to identity

            // Determine which clip to use
            size_t clipIndex = m_CurrentClip;

            // If in state machine mode, use the current state's clip
            if (m_EnableStateMachine && !m_States.empty() && m_CurrentStateIndex < m_States.size())
            {
                clipIndex = m_States[m_CurrentStateIndex].clipIndex;
            }

            // Get the current animation clip
            if (clipIndex < m_Clips.size())
            {
                const AnimationClip* clip = m_Clips[clipIndex].get();

                // Look up this joint's track in the current animation
                const auto* keys = clip->GetTrack(joint.name);

                if (keys && keys->size() >= 2)
                {
                    KeyFrame prev, next;
                    GetPreviousAndNextFrames(*keys, prev, next);

                    float progression = 0.0f;
                    float dt = next.timeStamp - prev.timeStamp;
                    if (dt > 0.0f)
                    {
                        progression = (m_Time - prev.timeStamp) / dt;
                    }

                    localTransform = Interpolate(prev, next, progression);
                }
                else if (keys && keys->size() == 1)
                {
                    // Only one keyframe - use it as static pose
                    const KeyFrame& key = (*keys)[0];
                    localTransform = glm::translate(glm::mat4(1.0f), key.position) *
                        glm::toMat4(key.rotation) *
                        glm::scale(glm::mat4(1.0f), key.scale);
                }

                // ROOT MOTION STRIPPING
                if (isRoot && !m_ApplyRootMotion)
                {
                    // Decompose to strip position
                    glm::vec3 pos, scale;
                    glm::quat rot;
                    DecomposeMatrix(localTransform, pos, rot, scale);

                    // Rebuild without translation
                    localTransform = glm::toMat4(rot) * glm::scale(glm::mat4(1.0f), scale);
                }
            }

            // Combine with parent transform
            glm::mat4 worldTransform = parentTransform * localTransform;

            // Update joint transform
            m_Transforms[joint.index] = worldTransform * m_GlobalTransform * joint.offset;

            // Update children
            for (auto& child : joint.children)
            {
                UpdateJoints(child, worldTransform, false);
            }
        }

    public:
        // === SKELETON VISUALIZATION ===
        struct BoneLine {
            glm::vec3 start;
            glm::vec3 end;
            std::string boneName;
        };

    private:
        // Extract bone world positions recursively for visualization
        void ExtractBoneLines(const Joint& joint, const glm::mat4& parentWorldMat,
                             std::vector<BoneLine>& outLines, bool isRoot = false) const
        {
            // Calculate this joint's local transform (same logic as UpdateJoints but read-only)
            glm::mat4 localTransform = glm::mat4(1.0f);

            // Determine active clip
            size_t clipIndex = m_CurrentClip;
            if (!m_States.empty() && m_CurrentStateIndex < m_States.size())
            {
                clipIndex = m_States[m_CurrentStateIndex].clipIndex;
            }

            // Get animated transform if available
            if (clipIndex < m_Clips.size())
            {
                const AnimationClip* clip = m_Clips[clipIndex].get();
                const auto* keys = clip->GetTrack(joint.name);

                if (keys && keys->size() >= 2)
                {
                    KeyFrame prev, next;
                    GetPreviousAndNextFrames(*keys, prev, next);

                    float progression = 0.0f;
                    float dt = next.timeStamp - prev.timeStamp;
                    if (dt > 0.0f)
                    {
                        progression = (m_Time - prev.timeStamp) / dt;
                    }

                    localTransform = Interpolate(prev, next, progression);
                }
                else if (keys && keys->size() == 1)
                {
                    const KeyFrame& key = (*keys)[0];
                    localTransform = glm::translate(glm::mat4(1.0f), key.position) *
                        glm::toMat4(key.rotation) *
                        glm::scale(glm::mat4(1.0f), key.scale);
                }
            }

            // Calculate world space position
            glm::mat4 worldMat = parentWorldMat * localTransform;
            glm::vec3 jointPos = glm::vec3(worldMat[3]);

            // Draw line from parent to this joint (skip for root)
            if (!isRoot)
            {
                glm::vec3 parentPos = glm::vec3(parentWorldMat[3]);
                outLines.push_back({ parentPos, jointPos, joint.name });
            }

            // Recurse to children
            for (const auto& child : joint.children)
            {
                ExtractBoneLines(child, worldMat, outLines, false);
            }
        }

    public:
        // Access to skeleton root
        BOOM_INLINE const Joint& GetRoot() const { return m_Root; }

        // Extract skeleton as lines for debug visualization
        BOOM_INLINE std::vector<BoneLine> GetSkeletonLines() const
        {
            std::vector<BoneLine> lines;
            lines.reserve(100);
            ExtractBoneLines(m_Root, glm::mat4(1.0f), lines, true);
            return lines;
        }

        // Update skeleton from another animator (preserves states/clips/parameters)
        BOOM_INLINE void UpdateSkeletonFrom(const Animator& source)
        {
            m_Root = source.m_Root;
            m_GlobalTransform = source.m_GlobalTransform;
            m_Transforms = source.m_Transforms;  // Copy actual transform values
            // Keep existing m_States, m_Clips, parameters intact
        }

        // For cloning
        BOOM_INLINE std::shared_ptr<Animator> Clone() const
        {
            auto clone = std::make_shared<Animator>();

            // Skeleton data
            clone->m_GlobalTransform = m_GlobalTransform;
            clone->m_Root = m_Root;
            clone->m_Transforms = m_Transforms;  // Copy actual transform values

            // Animation data
            clone->m_Clips = m_Clips; // Shared ownership of clips
            clone->m_CurrentClip = m_CurrentClip;
            clone->m_Time = m_Time;

            // State machine data
            clone->m_States = m_States;
            clone->m_CurrentStateIndex = m_CurrentStateIndex;
            clone->m_IsBlending = m_IsBlending;
            clone->m_TargetStateIndex = m_TargetStateIndex;
            clone->m_BlendProgress = m_BlendProgress;
            clone->m_BlendDuration = m_BlendDuration;
            clone->m_TargetTime = m_TargetTime;

            // Parameters
            clone->m_FloatParams = m_FloatParams;
            clone->m_BoolParams = m_BoolParams;
            clone->m_Triggers = m_Triggers;

            // Audio event state
            clone->m_LastProcessedTime = m_LastProcessedTime;
            clone->m_TriggeredAudioEvents = m_TriggeredAudioEvents;
            clone->m_AudioEventsEnabled = m_AudioEventsEnabled;
            clone->m_AudioSourcePosition = m_AudioSourcePosition;

            clone->m_ApplyRootMotion = m_ApplyRootMotion;

            return clone;
        }

        BOOM_INLINE void SetTime(float time)
        {
            m_Time = time;
            if (m_CurrentClip < m_Clips.size())
            {
                // Clamp to clip duration
                m_Time = std::min(m_Time, m_Clips[m_CurrentClip]->duration);
            }
        }

        BOOM_INLINE void SetCurrentClip(size_t clipIndex)
        {
            // Set current clip WITHOUT resetting time (unlike PlayClip)
            if (clipIndex < m_Clips.size())
            {
                m_CurrentClip = clipIndex;
            }
        }

        BOOM_INLINE void UpdateJointsFromCurrentTime()
        {
            // Force joint transform update without advancing time
            // Used when seeking to a specific time position
            UpdateJoints(m_Root, glm::identity<glm::mat4>(), true);
        }

        BOOM_INLINE void LoadAnimationFromFile(const std::string& filepath, const std::string& clipName = "")
        {
            std::shared_ptr<AnimationClip> clip;

            // Check file extension
            if (filepath.ends_with(".anim")) {
                // Load from .anim file
                clip = LoadAnimationClip(filepath);
                if (clip && !clipName.empty()) {
                    clip->name = clipName;  // Override name if specified
                }
            }
            else if (filepath.ends_with(".fbx") || filepath.ends_with(".gltf")) {
                // Load from model file using Assimp
                Assimp::Importer importer;
                const aiScene* ai_scene = importer.ReadFile(
                    filepath.c_str(),
                    aiProcess_LimitBoneWeights
                );

                if (!ai_scene || !ai_scene->mAnimations || ai_scene->mNumAnimations == 0)
                {
                    BOOM_ERROR("Failed to load animation from: {}", filepath);
                    return;
                }

                // Load the first animation from the file
                auto ai_anim = ai_scene->mAnimations[0];

                clip = std::make_shared<AnimationClip>();
                clip->name = clipName.empty() ? ai_anim->mName.C_Str() : clipName;
                clip->duration = (float)ai_anim->mDuration;
                clip->ticksPerSecond = (float)ai_anim->mTicksPerSecond;
                clip->filePath = filepath; // Store source file path

                // Parse animation channels
                for (uint32_t j = 0; j < ai_anim->mNumChannels; j++)
                {
                    aiNodeAnim* ai_channel = ai_anim->mChannels[j];
                    std::string jointName(ai_channel->mNodeName.C_Str());

                    std::vector<KeyFrame>& track = clip->tracks[jointName];

                    uint32_t maxKeys = std::max({
                        ai_channel->mNumPositionKeys,
                        ai_channel->mNumRotationKeys,
                        ai_channel->mNumScalingKeys
                        });

                    track.reserve(maxKeys);

                    for (uint32_t k = 0; k < maxKeys; k++)
                    {
                        KeyFrame key;

                        if (k < ai_channel->mNumPositionKeys)
                        {
                            key.position = AssimpToVec3(ai_channel->mPositionKeys[k].mValue);
                            key.timeStamp = (float)ai_channel->mPositionKeys[k].mTime;
                        }
                        if (k < ai_channel->mNumRotationKeys)
                        {
                            key.rotation = AssimpToQuat(ai_channel->mRotationKeys[k].mValue);
                        }
                        if (k < ai_channel->mNumScalingKeys)
                        {
                            key.scale = AssimpToVec3(ai_channel->mScalingKeys[k].mValue);
                        }

                        track.push_back(key);
                    }
                }
            }
            else {
                BOOM_WARN("[Animator] Unsupported file format: {}", filepath);
                return;
            }

            if (clip) {
                m_Clips.push_back(clip);
                BOOM_INFO("Loaded animation '{}' from {} - Duration: {:.2f}s",
                    clip->name, filepath, clip->duration);
            }
        }

    private:
        // === STATE MACHINE DATA ===
        std::vector<State> m_States;
        size_t m_CurrentStateIndex = 0;

        // Blending state
        bool m_IsBlending = false;
        size_t m_TargetStateIndex = 0;
        float m_BlendProgress = 1.0f;
        float m_BlendDuration = 0.25f;
        float m_TargetTime = 0.0f;

        // Parameters
        std::unordered_map<std::string, float> m_FloatParams;
        std::unordered_map<std::string, bool> m_BoolParams;
        std::unordered_set<std::string> m_Triggers;

        std::vector<std::shared_ptr<AnimationClip>> m_Clips{}; // Now we store clips, not raw Animation structs
        std::vector<glm::mat4> m_Transforms{};
        glm::mat4 m_GlobalTransform{};
        Joint m_Root;
        size_t m_CurrentClip = 0;
        float m_Time = 0.0f;
        
        // Root Motion
        bool m_ApplyRootMotion = true;
        bool m_EnableStateMachine = true; // New Flag

        // === AUDIO EVENT TRACKING ===
        float m_LastProcessedTime = 0.0f;           // Previous frame's time (for detecting crossings)
        std::set<size_t> m_TriggeredAudioEvents;    // Events already triggered this loop cycle
        bool m_AudioEventsEnabled = true;           // Allow disabling audio during scrubbing
        glm::vec3 m_AudioSourcePosition = glm::vec3(0.0f);  // Position for 3D sounds

        friend struct SkeletalModel;
    };

    // Transition condition evaluation (outside struct)
    inline bool Animator::Transition::EvaluateCondition(const Animator& animator) const
    {
        switch (conditionType)
        {
        case NONE:
            return true;

        case FLOAT_GREATER:
            return animator.GetFloat(parameterName) > floatValue;

        case FLOAT_LESS:
            return animator.GetFloat(parameterName) < floatValue;

        case BOOL_EQUALS:
            return animator.GetBool(parameterName) == boolValue;

        case TRIGGER:
            return animator.GetTrigger(parameterName);

        default:
            return false;
        }
    }
}