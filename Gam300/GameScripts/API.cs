// Boom/API.cs - FIXED VERSION
using Boom;
using System;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Boom
{
    /// <summary>
    /// Mark a field as exposed to the editor inspector.
    /// Fields marked with this attribute will appear in the IMGUI inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field, AllowMultiple = false)]
    public class EditorExposedAttribute : Attribute
    {
        public string DisplayName { get; }
        public string Tooltip { get; }
        public float MinValue { get; }
        public float MaxValue { get; }
        public bool UseSlider { get; }
        /// <summary>If non-null, the inspector renders a dropdown with these choices (string fields only).</summary>
        public string[] Options { get; }

        public EditorExposedAttribute(
            string displayName = null,
            string tooltip = null,
            float min = float.MinValue,
            float max = float.MaxValue,
            bool useSlider = false,
            string[] options = null)
        {
            DisplayName = displayName;
            Tooltip = tooltip;
            MinValue = min;
            MaxValue = max;
            UseSlider = useSlider;
            Options = options;
        }
    }

    /// <summary>
    /// Represents field information for editor exposure
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ScriptFieldInfo
    {
        public string FieldName;
        public string DisplayName;
        public string TypeName;
        public string Tooltip;
        public float MinValue;
        public float MaxValue;
        public bool UseSlider;
    }

    // Internal calls implemented in C++ and registered with Mono
    internal static class Native
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_Log(string msg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_Set30FPSLimit(bool enabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_Get30FPSLimit();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong Boom_API_FindEntity(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetPosition(ulong handle, out Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetPosition(ulong handle, ref Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsKeyDown(int key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsMouseDown(int button);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsGamepadButtonDown(int button);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetGamepadAxis(int axis);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsGamepadConnected();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong[] Boom_API_GetChildren(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasTransform(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasScript(ulong handle);

        // ========= PHYSICS / RIGIDBODY INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetRotation(ulong handle, out Vec3 outRot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetRotation(ulong handle, ref Vec3 rot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_DrawDebugVisionCone(ulong entityHandle,
            float range, float angle, float r, float g, float b, float a);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetAIFacingYaw(ulong entityHandle, float yawDegrees);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetAIVisionParams(ulong entityHandle, float range, float halfAngleDeg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetVisualConeParams(ulong entityHandle, float range, float halfAngleDeg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetVisualConeFacing(ulong entityHandle, float yawDegrees);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetVisualConeAlert(ulong entityHandle, bool isAlert);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetThirdPersonCameraYaw();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_TriggerCameraShake(float intensity, float duration);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetCameraFOV(float fov);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetProximityRedTint(float amount);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_HasCollider(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsTrigger(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetTrigger(ulong handle, bool isTrigger);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int Boom_API_GetSurfaceType(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_RegisterTriggerEnterCallback(ulong triggerHandle, object delegateObj);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_RegisterTriggerExitCallback(ulong triggerHandle, object delegateObj);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnregisterTriggerCallbacks(ulong triggerHandle);

        // ========= NEW PHYSICS / RIGIDBODY INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetLinearVelocity(ulong handle, out Vec3 vel);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetLinearVelocity(ulong handle, ref Vec3 vel);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsColliding(ulong handle);

        // ========= ANIMATOR INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetFloat(ulong h, string name, float v);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetFloat(ulong h, string name, double v);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetBool(ulong h, string name, bool v);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetTrigger(ulong h, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorPlay(ulong h, string state);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetStateMachineEnabled(ulong h, bool enabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int Boom_API_AnimatorGetClipCount(ulong h);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string Boom_API_AnimatorGetClipName(ulong h, int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasAnimator(ulong handle);

        // ========= TRANSFORM STRUCT INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetTransform(ulong handle, out TransformData transform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetTransform(ulong handle, ref TransformData transform);

        // ========= SCENE MANAGEMENT =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_LoadScene(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string Boom_API_GetCurrentSceneName();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_QuitGame();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_ShutdownApplication(); // CORRECT QUIT

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_LoadSceneAdditive(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_EnableFileWatcher(bool enable);
        
        // Gamma Adjust Menu
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_ShowGammaAdjustMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnloadGammaAdjustMenu();

        // Pause Menu
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnloadPauseMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_ShowPauseMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsPauseMenuLoaded();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetGameLogicPaused(bool paused);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetGroupVolume(string name, float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float Boom_API_GetGroupVolume(string name);

        // Gamma / Tone Mapping
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetGamma(float gamma);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float Boom_API_GetGamma();

        // Death Menu
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnloadDeathMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_ShowDeathMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsDeathMenuLoaded();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetPlayerDead(bool isDead);

        // End Menu
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnloadEndMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_ShowEndMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsEndMenuLoaded();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetGameEnd(bool isEnd);

        // Freeze
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Boom_API_DestroyEntity(ulong entity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_TogglePause();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int Boom_API_GetApplicationState();

        //AI STUFF
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int Boom_API_AI_GetPatrolPointCount(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AI_GetPatrolPoint(ulong handle, int index, out Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int Boom_API_AI_GetMode(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetNavAgentActive(ulong handle, bool active);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetNavAgentPosition(ulong handle, ref Vec3 pos);

        //Animator Stuff


        // ========= SOUND / AUDIO INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PlaySound(string name, string filePath, bool loop);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PlaySoundAt(string name, string filePath, ref Vec3 position, bool loop);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_StopSound(string name);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundVolume(string name, float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_Set3DMinMaxDistance(string name, float minDist, float maxDist);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsSoundPlaying(string name);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PauseSound(string name, bool pause);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PreloadSound(string name, string filePath, bool loop);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundPosition(string name, ref Vec3 position);

        // ========= SOUND COMPONENT MANIPULATION =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_HasSoundComponent(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_AddSoundComponent(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int Boom_API_GetSoundEntryCount(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_AddSoundEntry(ulong handle, string name, string filePath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_RemoveSoundEntry(ulong handle, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntryVolume(ulong handle, string name, float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntryPitch(ulong handle, string name, float pitch);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntryLoop(ulong handle, string name, bool loop);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntryMute(ulong handle, string name, bool mute);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntryPan(ulong handle, string name, float pan);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntrySpatialBlend(ulong handle, string name, float blend);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntryPriority(ulong handle, string name, int priority);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntry3DDistance(ulong handle, string name, float minDist, float maxDist);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundEntryPlayOnStart(ulong handle, string name, bool playOnStart);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PlaySoundEntry(ulong handle, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_StopSoundEntry(ulong handle, string name);

        // Raycasting & Main Menu
        [MethodImpl(MethodImplOptions.InternalCall)] 
        internal static extern ulong Boom_API_PickGameEntity();
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_GetMousePosInViewport(out Vec2 outPos);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_ProjectWorldToViewport(ref Vec3 worldPos, out Vec2 outViewportPos);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_Check2DViewportClick(ulong handle, float mouseX, float mouseY);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsControllerGrounded(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_MoveController(ulong handle, ref Vec3 displacement, float minDist, float dt);


        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong Boom_API_Raycast(ref Vec3 from, ref Vec3 dir, float maxDist);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_Linecast(ref Vec3 from, ref Vec3 to, ulong ignoreEntity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetRotationY(ulong handle, float yawDegrees);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_LinecastIgnoreBoth(Vec3 from, Vec3 to, ulong ignoreEntity1, ulong ignoreEntity2);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_TeleportRigidBody(ulong handle, ref Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetScreenFadeAlpha(float alpha);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasSprite(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetSpriteColor(ulong handle, out Vec4 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetSpriteColor(ulong handle, ref Vec4 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetSpriteAlpha(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetSpriteAlpha(ulong handle, float alpha);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetSpriteTexture(ulong handle, string texturePath);

        // ========= MODEL COMPONENT INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasModelComponent(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetModelOpacity(ulong handle, float opacity);

        // ========= TEXT COMPONENT INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasText(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetText(ulong handle, out string text);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetText(ulong handle, string text);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetTextColor(ulong handle, out Vec4 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetTextColor(ulong handle, ref Vec4 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetTextScale(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetTextHeight(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetTextScale(ulong handle, float scale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetTextPosition(ulong handle, out Vec2 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetTextPosition(ulong handle, ref Vec2 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int Boom_API_GetTextAlignment(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetTextAlignment(ulong handle, int alignment);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_CreateController(ulong handle, float radius, float height);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_TeleportController(ulong handle, ref Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong[] Boom_API_GetControllerTriggerOverlaps(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong Boom_API_GetControllerStandingOn(ulong handle);

        // ========= SPOTLIGHT COMPONENT INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasSpotLight(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetSpotLightColor(ulong handle, out Vec3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetSpotLightColor(ulong handle, ref Vec3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetSpotLightIntensity(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetSpotLightIntensity(ulong handle, float intensity);

        // ========= POINTLIGHT COMPONENT INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasPointLight(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetPointLightColor(ulong handle, out Vec3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetPointLightColor(ulong handle, ref Vec3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetPointLightIntensity(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetPointLightIntensity(ulong handle, float intensity);

        // ========= DIRECTLIGHT COMPONENT INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasDirectLight(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetDirectLightColor(ulong handle, out Vec3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetDirectLightColor(ulong handle, ref Vec3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetDirectLightIntensity(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetDirectLightIntensity(ulong handle, float intensity);

        // ========= VIDEO COMPONENT INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasVideoComponent(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsVideoPlaying(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasVideoEnded(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_PlayVideo(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_StopVideo(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static double Boom_API_GetVideoDuration(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static double Boom_API_GetVideoCurrentTime(ulong handle);


        // Video
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PlayVideoComponent(ulong handle);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_StopVideoComponent(ulong handle);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_GetViewportSize(out float width, out float height);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetVideoRemoveBlack(ulong handle, bool enabled);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetVideoBrightness(ulong handle, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float Boom_API_GetVideoBrightness(ulong handle);

        // Cutscene
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetCutsceneMode(bool active);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_DrawDebugLine(Vec3 start, Vec3 end, Vec3 color);

        // ========= PARTICLE EMITTER INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasParticleEmitter(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_PlayParticleEmitter(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_StopParticleEmitter(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsParticleEmitterPlaying(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleEmissionRate(ulong handle, float rate);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetParticleEmissionRate(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleStartColor(ulong handle, float r, float g, float b, float a);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleEndColor(ulong handle, float r, float g, float b, float a);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleGravity(ulong handle, float gravity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleSpeed(ulong handle, float speedMin, float speedMax);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleSize(ulong handle, float startMin, float startMax, float endSize);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AddParticleEmitter(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleShapeType(ulong handle, int shapeType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleShapeAngle(ulong handle, float angle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleShapeRange(ulong handle, float range);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleShapeRadius(ulong handle, float radius);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleShapeSize(ulong handle, float x, float y, float z);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleLooping(ulong handle, bool looping);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleAdditiveBlend(ulong handle, bool additive);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleMaxParticles(ulong handle, int maxParticles);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleDirection(ulong handle, float x, float y, float z);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetParticleLifetime(ulong handle, float lifetimeMin, float lifetimeMax);

        // Inventory Menu
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_ShowInventoryMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnloadInventoryMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsInventoryMenuLoaded();
    }

    // ========= DELEGATES =========
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TriggerCallback(ulong triggerEntity, ulong otherEntity);

    // ========= DATA STRUCTURES =========

    [StructLayout(LayoutKind.Sequential)]
    public struct Vec2
    {
        public float X, Y;
        public Vec2(float x, float y) { X = x; Y = y; }
        public override string ToString() => $"({X:F2}, {Y:F2})";

    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vec3
    {
        public float X, Y, Z;

        public Vec3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }
        public override string ToString() => $"({X:F2}, {Y:F2}, {Z:F2})";

    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vec4
    {
        public float X, Y, Z, W;

        public Vec4(float x, float y, float z, float w)
        {
            X = x; Y = y; Z = z; W = w;
        }

        public static Vec4 operator +(Vec4 a, Vec4 b) => new Vec4(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W);
        public static Vec4 operator -(Vec4 a, Vec4 b) => new Vec4(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W);
        public static Vec4 operator *(Vec4 a, float s) => new Vec4(a.X * s, a.Y * s, a.Z * s, a.W * s);
        public static Vec4 operator /(Vec4 a, float s) => new Vec4(a.X / s, a.Y / s, a.Z / s, a.W / s);
        public override string ToString() => $"({X:F2}, {Y:F2}, {Z:F2}, {W:F2})";

    }

    // RENAMED to avoid conflict with the static class below
    [StructLayout(LayoutKind.Sequential)]
    public struct TransformData
    {
        public float PositionX, PositionY, PositionZ;
        public float RotationX, RotationY, RotationZ;
        public float ScaleX, ScaleY, ScaleZ;

        public Vec3 Position
        {
            get => new Vec3(PositionX, PositionY, PositionZ);
            set { PositionX = value.X; PositionY = value.Y; PositionZ = value.Z; }
        }

        public Vec3 Rotation
        {
            get => new Vec3(RotationX, RotationY, RotationZ);
            set { RotationX = value.X; RotationY = value.Y; RotationZ = value.Z; }
        }

        public Vec3 Scale
        {
            get => new Vec3(ScaleX, ScaleY, ScaleZ);
            set { ScaleX = value.X; ScaleY = value.Y; ScaleZ = value.Z; }
        }
    }

    // ========= PUBLIC API =========
    public static class API
    {
        // Cache trigger callbacks to prevent garbage collection
        private static System.Collections.Generic.Dictionary<ulong, TriggerCallback> s_TriggerEnterCallbacks = new System.Collections.Generic.Dictionary<ulong, TriggerCallback>();
        private static System.Collections.Generic.Dictionary<ulong, TriggerCallback> s_TriggerExitCallbacks = new System.Collections.Generic.Dictionary<ulong, TriggerCallback>();

        public static void Set30FPSLimit(bool enabled) => Native.Boom_API_Set30FPSLimit(enabled);
        public static bool Get30FPSLimit() => Native.Boom_API_Get30FPSLimit();

        // ===== Logging =====
        public static void Log(string s) => Native.Boom_API_Log(s);

        // ===== Entity queries =====
        public static ulong FindEntity(string name) => Native.Boom_API_FindEntity(name);

        /// <summary>
        /// Get all children of an entity.
        /// </summary>
        public static ulong[] GetChildren(ulong entity) => Native.Boom_API_GetChildren(entity) ?? new ulong[0];

        //AI Helpers
        public enum AIMode
        {
            Auto = 0,
            Idle = 1,
            Patrol = 2,
            Seek = 3
        }
        public static int GetAIPatrolPointCount(ulong h)
    => Native.Boom_API_AI_GetPatrolPointCount(h);

        public static Vec3 GetAIPatrolPoint(ulong h, int index)
        {
            Native.Boom_API_AI_GetPatrolPoint(h, index, out var p);
            return p;
        }

        public static AIMode GetAIMode(ulong h)
        {
            int mode = Native.Boom_API_AI_GetMode(h);
            return (AIMode)mode;
        }

        public static void SetNavAgentActive(ulong h, bool active)
        {
            Native.Boom_API_SetNavAgentActive(h, active);
        }

        public static void SetNavAgentPosition(ulong h, Vec3 pos)
        {
            Native.Boom_API_SetNavAgentPosition(h, ref pos);
        }

        // ===== Transform with validation =====
        public static Vec3 GetPosition(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new Vec3(0, 0, 0);
            }
            Native.Boom_API_GetPosition(h, out var p);
            return p;
        }

        public static void SetPosition(ulong h, Vec3 p)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set position.");
                return;
            }
            Native.Boom_API_SetPosition(h, ref p);
        }

        public static Vec3 GetRotation(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new Vec3(0, 0, 0);
            }
            Native.Boom_API_GetRotation(h, out var r);
            return r;
        }

        public static void SetRotation(ulong h, Vec3 r)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set rotation.");
                return;
            }
            Native.Boom_API_SetRotation(h, ref r);
        }

        public static Vec3 GetScale(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new Vec3(1, 1, 1);
            }
            // Get the full transform and return only the scale part
            Native.Boom_API_GetTransform(h, out var t);
            return t.Scale;
        }

        public static void SetScale(ulong h, Vec3 s)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set scale.");
                return;
            }

            Native.Boom_API_GetTransform(h, out var t);
            t.Scale = s;
            Native.Boom_API_SetTransform(h, ref t);
        }
        // ===== Rotation axis helpers =====
        public static float GetRotationX(ulong h)
        {
            var r = GetRotation(h);   // uses Native.Boom_API_GetRotation
            return r.X;
        }

        public static float GetRotationY(ulong h)
        {
            var r = GetRotation(h);
            return r.Y;
        }

        public static float GetRotationZ(ulong h)
        {
            var r = GetRotation(h);
            return r.Z;
        }

        public static void SetRotationX(ulong h, float pitchDegrees)
        {
            var r = GetRotation(h);
            r.X = pitchDegrees;
            SetRotation(h, r);        // uses Native.Boom_API_SetRotation
        }

        public static void SetRotationYDeg(ulong h, float yawDegrees)
        {
            SetRotationY(h, yawDegrees); // calls Native.Boom_API_SetRotationY
        }

        public static void SetRotationZ(ulong h, float rollDegrees)
        {
            var r = GetRotation(h);
            r.Z = rollDegrees;
            SetRotation(h, r);
        }

        public static TransformData GetTransform(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new TransformData();
            }
            Native.Boom_API_GetTransform(h, out var t);
            return t;
        }

        public static void SetTransform(ulong h, TransformData t)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set transform.");
                return;
            }
            Native.Boom_API_SetTransform(h, ref t);
        }

        // ===== Component checking =====
        public static bool HasTransform(ulong h) => Native.Boom_API_HasTransform(h);
        public static bool HasScript(ulong h) => Native.Boom_API_HasScript(h);
        public static bool HasAnimator(ulong h) => Native.Boom_API_HasAnimator(h);
        public static bool HasCollider(ulong entity) => Native.Boom_API_HasCollider(entity);

        // ===== Input =====
        public static bool IsKeyDown(int glfwKey) => Native.Boom_API_IsKeyDown(glfwKey);
        public static bool IsMouseDown(int button) => Native.Boom_API_IsMouseDown(button);
        public static bool IsGamepadButtonDown(int button) => Native.Boom_API_IsGamepadButtonDown(button);
        public static float GetGamepadAxis(int axis) => Native.Boom_API_GetGamepadAxis(axis);
        public static bool IsGamepadConnected() => Native.Boom_API_IsGamepadConnected();

        // ===== Physics / Rigidbody =====
        public static Vec3 GetLinearVelocity(ulong h)
        {
            Native.Boom_API_GetLinearVelocity(h, out var v);
            return v;
        }

        public static void SetLinearVelocity(ulong h, Vec3 v)
        {
            Native.Boom_API_SetLinearVelocity(h, ref v);
        }

        public static bool IsColliding(ulong h) => Native.Boom_API_IsColliding(h);

        // ===== Triggers =====
        public static bool IsTrigger(ulong entity) => Native.Boom_API_IsTrigger(entity);

        public static void SetTrigger(ulong entity, bool isTrigger)
        {
            Native.Boom_API_SetTrigger(entity, isTrigger);
        }

        // ===== Surface Types =====
        /// <summary>
        /// Surface type enum matching Collider3D::SurfaceType in C++
        /// </summary>
        public enum SurfaceType
        {
            DEFAULT = 0,
            WOOD,
            STONE,
            METAL,
            SAND,
            GRASS,
            WATER,
            CARPET,
            TILE
        }

        /// <summary>
        /// Get the surface type of an entity's collider (for footstep sounds, etc.)
        /// </summary>
        public static SurfaceType GetSurfaceType(ulong entity)
        {
            if (!HasCollider(entity)) return SurfaceType.DEFAULT;
            return (SurfaceType)Native.Boom_API_GetSurfaceType(entity);
        }

        public static void RegisterTriggerEnterCallback(ulong triggerEntity, TriggerCallback callback)
        {
            // Cache the delegate to prevent garbage collection
            s_TriggerEnterCallbacks[triggerEntity] = callback;
            Native.Boom_API_RegisterTriggerEnterCallback(triggerEntity, callback);
        }

        public static void RegisterTriggerExitCallback(ulong triggerEntity, TriggerCallback callback)
        {
            // Cache the delegate to prevent garbage collection
            s_TriggerExitCallbacks[triggerEntity] = callback;
            Native.Boom_API_RegisterTriggerExitCallback(triggerEntity, callback);
        }

        public static void UnregisterTriggerCallbacks(ulong triggerEntity)
        {
            // Remove from cache
            s_TriggerEnterCallbacks.Remove(triggerEntity);
            s_TriggerExitCallbacks.Remove(triggerEntity);
            Native.Boom_API_UnregisterTriggerCallbacks(triggerEntity);
        }

        // ===== Debug Visualization =====
        public static void DrawDebugVisionCone(ulong entityHandle, float range, float halfAngle, Vec4 color)
        {
            Native.Boom_API_DrawDebugVisionCone(entityHandle, range, halfAngle,
                color.X, color.Y, color.Z, color.W);
        }

        public static void DrawDebugVisionCone(ulong entityHandle, float range, float halfAngle)
        {
            DrawDebugVisionCone(entityHandle, range, halfAngle, new Vec4(0f, 1f, 0f, 0.3f));
        }

        public static bool IsGrounded(ulong entity, float probeDistance = 0.25f)
        {
            // cast a short ray straight down from the entity
            var p = GetPosition(entity);
            var from = new Vec3(p.X, p.Y + 0.05f, p.Z);
            var to = new Vec3(p.X, p.Y - probeDistance, p.Z);

            // true if we hit anything (ignore self)
            return Linecast(from, to, entity);
        }

        // ===== Camera =====
        public static float GetThirdPersonCameraYaw() => Native.Boom_API_GetThirdPersonCameraYaw();

        /// <summary>Trigger a camera shake. intensity = max offset (world units). Call each frame to sustain.</summary>
        public static void TriggerCameraShake(float intensity, float duration) => Native.Boom_API_TriggerCameraShake(intensity, duration);

        /// <summary>Set the main camera FOV in degrees. Pass 45 to restore default.</summary>
        public static void SetCameraFOV(float fov) => Native.Boom_API_SetCameraFOV(fov);

        /// <summary>Set a red danger tint overlay (0 = none, 1 = full red). Reset to 0 when done.</summary>
        public static void SetProximityRedTint(float amount) => Native.Boom_API_SetProximityRedTint(amount);

        // ===== Scene Management =====
        public static void LoadScene(string name) => Native.Boom_API_LoadScene(name);
        public static string GetCurrentSceneName() => Native.Boom_API_GetCurrentSceneName();
        public static void QuitGame() => Native.Boom_API_QuitGame();
        public static void ShutdownApplication() => Native.Boom_API_ShutdownApplication(); // CORRECT QUIT 
        public static void LoadSceneAdditive(string name) => Native.Boom_API_LoadSceneAdditive(name);
        public static void TogglePause() => Native.Boom_API_TogglePause();
        public static int GetApplicationState() => Native.Boom_API_GetApplicationState();

        // Gamma Adjust Menu
        public static void ShowGammaAdjustMenu()   => Native.Boom_API_ShowGammaAdjustMenu();
        public static void UnloadGammaAdjustMenu() => Native.Boom_API_UnloadGammaAdjustMenu();

        // Pause Menu
        public static void UnloadPauseMenu() => Native.Boom_API_UnloadPauseMenu();
        public static void ShowPauseMenu() => Native.Boom_API_ShowPauseMenu();
        public static bool IsPauseMenuLoaded() => Native.Boom_API_IsPauseMenuLoaded();
        public static void SetGameLogicPaused(bool paused) => Native.Boom_API_SetGameLogicPaused(paused);
        public static void SetGroupVolume(string groupName, float volume)
        {
            Native.Boom_API_SetGroupVolume(groupName, volume);
        }

        public static float GetGroupVolume(string groupName)
        {
            return Native.Boom_API_GetGroupVolume(groupName);
        }

        // Gamma / Tone Mapping
        public static void SetGamma(float gamma) => Native.Boom_API_SetGamma(gamma);
        public static float GetGamma() => Native.Boom_API_GetGamma();

        // Death Menu
        public static void UnloadDeathMenu() => Native.Boom_API_UnloadDeathMenu();
        public static void ShowDeathMenu() => Native.Boom_API_ShowDeathMenu();
        public static bool IsDeathMenuLoaded() => Native.Boom_API_IsDeathMenuLoaded();
        public static void SetPlayerDead(bool isDead) => Native.Boom_API_SetPlayerDead(isDead);

        // End Menu
        public static void UnloadEndMenu() => Native.Boom_API_UnloadEndMenu();
        public static void ShowEndMenu() => Native.Boom_API_ShowEndMenu();
        public static bool IsEndMenuLoaded() => Native.Boom_API_IsEndMenuLoaded();
        public static void SetGameEnd(bool isEnd) => Native.Boom_API_SetGameEnd(isEnd);

        // Inventory Menu
        public static void ShowInventoryMenu() => Native.Boom_API_ShowInventoryMenu();
        public static void UnloadInventoryMenu() => Native.Boom_API_UnloadInventoryMenu();
        public static bool IsInventoryMenuLoaded() => Native.Boom_API_IsInventoryMenuLoaded();

        // Freeze
        public static void DestroyEntity(ulong entity) => Native.Boom_API_DestroyEntity(entity);

        // ===== Animator =====
        public static void AnimatorSetFloat(ulong h, string n, float v) => Native.Boom_API_AnimatorSetFloat(h, n, v);
        public static void AnimatorSetFloat(ulong h, string n, double v) => Native.Boom_API_AnimatorSetFloat(h, n, v);
        public static void AnimatorSetBool(ulong h, string n, bool v) => Native.Boom_API_AnimatorSetBool(h, n, v);
        public static void AnimatorSetTrigger(ulong h, string n) => Native.Boom_API_AnimatorSetTrigger(h, n);
        public static void AnimatorPlay(ulong h, string state) => Native.Boom_API_AnimatorPlay(h, state);
        public static void AnimatorSetStateMachineEnabled(ulong h, bool enabled) => Native.Boom_API_AnimatorSetStateMachineEnabled(h, enabled);
        public static int  AnimatorGetClipCount(ulong h) => Native.Boom_API_AnimatorGetClipCount(h);
        public static string AnimatorGetClipName(ulong h, int index) => Native.Boom_API_AnimatorGetClipName(h, index) ?? "";

        // ===== Input Constants =====
        // ===== SOUND / AUDIO API =====

        /// <summary>
        /// Play a 2D sound effect
        /// </summary>
        public static void PlaySound(string name, string filePath, bool loop = false)
        {
            Native.Boom_API_PlaySound(name, filePath, loop);
        }
        
        /// <summary>
        /// Play a 3D positional sound at specific world coordinates
        /// </summary>
        public static void PlaySoundAt(string name, string filePath, Vec3 position, bool loop = false)
        {
            Native.Boom_API_PlaySoundAt(name, filePath, ref position, loop);
        }
        
        /// <summary>
        /// Stop a playing sound
        /// </summary>
        public static void StopSound(string name)
        {
            Native.Boom_API_StopSound(name);
        }
        
        /// <summary>
        /// Set the volume of a sound (0.0 - 1.0)
        /// </summary>
        public static void SetSoundVolume(string name, float volume)
        {
            Native.Boom_API_SetSoundVolume(name, volume);
        }

        /// <summary>
        /// Set 3D audio min/max distance for spatial audio attenuation.
        /// </summary>
        /// <param name="name">Sound instance name</param>
        /// <param name="minDist">Distance at which sound is at full volume (world units)</param>
        /// <param name="maxDist">Distance at which sound becomes silent (world units)</param>
        public static void Set3DMinMaxDistance(string name, float minDist, float maxDist)
        {
            Native.Boom_API_Set3DMinMaxDistance(name, minDist, maxDist);
        }
        
        /// <summary>
        /// Check if a sound is currently playing
        /// </summary>
        public static bool IsSoundPlaying(string name)
        {
            return Native.Boom_API_IsSoundPlaying(name);
        }
        
        /// <summary>
        /// Pause or unpause a sound
        /// </summary>
        public static void PauseSound(string name, bool pause)
        {
            Native.Boom_API_PauseSound(name, pause);
        }
        
        /// <summary>
        /// Preload a sound for faster playback later
        /// </summary>
        public static void PreloadSound(string name, string filePath, bool loop = false)
        {
            Native.Boom_API_PreloadSound(name, filePath, loop);
        }
        
        /// <summary>
        /// Update the position of a 3D sound that's already playing
        /// </summary>
        public static void SetSoundPosition(string name, Vec3 position)
        {
            Native.Boom_API_SetSoundPosition(name, ref position);
        }

        // ===== SOUND COMPONENT API =====
        // These methods modify the SoundComponent on an entity, which is visible in the Inspector
        // and processed by SoundSystem for playback

        /// <summary>
        /// Check if entity has a SoundComponent
        /// </summary>
        public static bool HasSoundComponent(ulong entityHandle)
        {
            return Native.Boom_API_HasSoundComponent(entityHandle);
        }

        /// <summary>
        /// Add a SoundComponent to an entity (if it doesn't have one)
        /// </summary>
        public static void AddSoundComponent(ulong entityHandle)
        {
            Native.Boom_API_AddSoundComponent(entityHandle);
        }

        /// <summary>
        /// Get the number of sound entries in an entity's SoundComponent
        /// </summary>
        public static int GetSoundEntryCount(ulong entityHandle)
        {
            return Native.Boom_API_GetSoundEntryCount(entityHandle);
        }

        /// <summary>
        /// Add a new sound entry to an entity's SoundComponent
        /// </summary>
        public static void AddSoundEntry(ulong entityHandle, string name, string filePath)
        {
            Native.Boom_API_AddSoundEntry(entityHandle, name, filePath);
        }

        /// <summary>
        /// Remove a sound entry from an entity's SoundComponent by name
        /// </summary>
        public static void RemoveSoundEntry(ulong entityHandle, string name)
        {
            Native.Boom_API_RemoveSoundEntry(entityHandle, name);
        }

        /// <summary>
        /// Set the volume of a sound entry (0.0 - 1.0)
        /// </summary>
        public static void SetSoundEntryVolume(ulong entityHandle, string name, float volume)
        {
            Native.Boom_API_SetSoundEntryVolume(entityHandle, name, volume);
        }

        /// <summary>
        /// Set the pitch of a sound entry (0.5 = half speed, 1.0 = normal, 2.0 = double)
        /// </summary>
        public static void SetSoundEntryPitch(ulong entityHandle, string name, float pitch)
        {
            Native.Boom_API_SetSoundEntryPitch(entityHandle, name, pitch);
        }

        /// <summary>
        /// Set whether a sound entry loops
        /// </summary>
        public static void SetSoundEntryLoop(ulong entityHandle, string name, bool loop)
        {
            Native.Boom_API_SetSoundEntryLoop(entityHandle, name, loop);
        }

        /// <summary>
        /// Mute/unmute a sound entry
        /// </summary>
        public static void SetSoundEntryMute(ulong entityHandle, string name, bool mute)
        {
            Native.Boom_API_SetSoundEntryMute(entityHandle, name, mute);
        }

        /// <summary>
        /// Set stereo pan of a sound entry (-1.0 = left, 0.0 = center, 1.0 = right)
        /// </summary>
        public static void SetSoundEntryPan(ulong entityHandle, string name, float pan)
        {
            Native.Boom_API_SetSoundEntryPan(entityHandle, name, pan);
        }

        /// <summary>
        /// Set spatial blend of a sound entry (0.0 = 2D, 1.0 = 3D)
        /// </summary>
        public static void SetSoundEntrySpatialBlend(ulong entityHandle, string name, float blend)
        {
            Native.Boom_API_SetSoundEntrySpatialBlend(entityHandle, name, blend);
        }

        /// <summary>
        /// Set priority of a sound entry (0-256, lower = higher priority)
        /// </summary>
        public static void SetSoundEntryPriority(ulong entityHandle, string name, int priority)
        {
            Native.Boom_API_SetSoundEntryPriority(entityHandle, name, priority);
        }

        /// <summary>
        /// Set 3D distance settings for a sound entry
        /// </summary>
        public static void SetSoundEntry3DDistance(ulong entityHandle, string name, float minDist, float maxDist)
        {
            Native.Boom_API_SetSoundEntry3DDistance(entityHandle, name, minDist, maxDist);
        }

        /// <summary>
        /// Set whether a sound entry plays on start
        /// </summary>
        public static void SetSoundEntryPlayOnStart(ulong entityHandle, string name, bool playOnStart)
        {
            Native.Boom_API_SetSoundEntryPlayOnStart(entityHandle, name, playOnStart);
        }

        /// <summary>
        /// Trigger playback of a sound entry (sets playOnStart = true)
        /// </summary>
        public static void PlaySoundEntry(ulong entityHandle, string name)
        {
            Native.Boom_API_PlaySoundEntry(entityHandle, name);
        }

        /// <summary>
        /// Stop a sound entry
        /// </summary>
        public static void StopSoundEntry(ulong entityHandle, string name)
        {
            Native.Boom_API_StopSoundEntry(entityHandle, name);
        }

        public static bool LinecastIgnoreBoth(Vec3 from, Vec3 to, ulong ignoreEntity1, ulong ignoreEntity2)
        {
            return Native.Boom_API_LinecastIgnoreBoth(from, to, ignoreEntity1, ignoreEntity2);
        }

        // Raycasting
        public static ulong PickGameEntity() => Native.Boom_API_PickGameEntity();
        public static void MoveController(ulong h, Vec3 displacement, float minDist, float dt)
        {
            Native.Boom_API_MoveController(h, ref displacement, minDist, dt);
        }

        public static bool IsControllerGrounded(ulong h)
        {
            return Native.Boom_API_IsControllerGrounded(h);
        }
        public static bool GetMousePosInViewport(out Vec2 outPos)
        {
            return Native.Boom_API_GetMousePosInViewport(out outPos);
        }
        public static bool ProjectWorldToViewport(Vec3 worldPos, out Vec2 outViewportPos)
        {
            return Native.Boom_API_ProjectWorldToViewport(ref worldPos, out outViewportPos);
        }
        public static bool Check2DViewportClick(ulong entityID, float mouseX, float mouseY)
        {
            return Native.Boom_API_Check2DViewportClick(entityID, mouseX, mouseY);
        }


        public static ulong Raycast(Vec3 from, Vec3 dir, float maxDist)
        {
            return Native.Boom_API_Raycast(ref from, ref dir, maxDist);
        }

        public static bool Linecast(Vec3 from, Vec3 to, ulong ignoreEntity = 0)
        {
            return Native.Boom_API_Linecast(ref from, ref to, ignoreEntity);
        }

        public static void SetRotationY(ulong h, float yawDegrees)
        {
            Native.Boom_API_SetRotationY(h, yawDegrees);
        }

        /// <summary>
        /// Push the script's authoritative yaw into the C++ AIComponent.facingDir so the
        /// vision cone renderer always matches visual rotation, bypassing Euler decomposition.
        /// Call this every frame alongside SetRotationY.
        /// </summary>
        public static void SetAIFacingYaw(ulong h, float yawDegrees)
        {
            Native.Boom_API_SetAIFacingYaw(h, yawDegrees);
        }

        /// <summary>
        /// Sync the visual vision cone to match VisionComponent settings.
        /// range = detection range in world units.
        /// halfAngleDeg = HALF the FOV angle (pass detectionAngle * 0.5f).
        /// </summary>
        public static void SetAIVisionParams(ulong h, float range, float halfAngleDeg)
        {
            Native.Boom_API_SetAIVisionParams(h, range, halfAngleDeg);
        }

        /// Create/update a VisualConeComponent on the entity (for script-driven enemies without AIComponent).
        public static void SetVisualConeParams(ulong h, float range, float halfAngleDeg)
        {
            Native.Boom_API_SetVisualConeParams(h, range, halfAngleDeg);
        }

        /// Update the facing direction of the entity's VisualConeComponent.
        public static void SetVisualConeFacing(ulong h, float yawDegrees)
        {
            Native.Boom_API_SetVisualConeFacing(h, yawDegrees);
        }

        /// Set alert state on the entity's VisualConeComponent (true = alert/orange, false = patrol/yellow).
        public static void SetVisualConeAlert(ulong h, bool isAlert)
        {
            Native.Boom_API_SetVisualConeAlert(h, isAlert);
        }

        public static void EnableFileWatcher(bool enable)
        {
            Native.Boom_API_EnableFileWatcher(enable);
        }

        public static void TeleportRigidBody(ulong h, Vec3 p)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot teleport.");
                return;
            }
            Native.Boom_API_TeleportRigidBody(h, ref p);
        }

        public static void SetScreenFadeAlpha(float alpha)
        {
            Native.Boom_API_SetScreenFadeAlpha(alpha);
        }

        // ========= MODEL COMPONENT METHODS =========
        public static bool HasModelComponent(ulong entity) => Native.Boom_API_HasModelComponent(entity);

        public static void SetModelOpacity(ulong entity, float opacity)
        {
            Native.Boom_API_SetModelOpacity(entity, opacity);
        }

        // ========= SPRITE COMPONENT METHODS =========
        public static bool HasSprite(ulong entity) => Native.Boom_API_HasSprite(entity);

        public static Vec4 GetSpriteColor(ulong entity)
        {
            Native.Boom_API_GetSpriteColor(entity, out Vec4 color);
            return color;
        }

        public static void SetSpriteColor(ulong entity, Vec4 color)
        {
            Native.Boom_API_SetSpriteColor(entity, ref color);
        }

        public static float GetSpriteAlpha(ulong entity) => Native.Boom_API_GetSpriteAlpha(entity);

        public static void SetSpriteAlpha(ulong entity, float alpha)
        {
            Native.Boom_API_SetSpriteAlpha(entity, alpha);
        }

        public static void SetSpriteTexture(ulong entity, string texturePath)
        {
            if (!HasSprite(entity))
            {
                Log($"[WARNING] Entity {entity} does not have SpriteComponent! Cannot set texture.");
                return;
            }
            Native.Boom_API_SetSpriteTexture(entity, texturePath);
        }

        public static void CreateController(ulong handle, float radius, float height)
        {
            Native.Boom_API_CreateController(handle, radius, height);
        }

        public static void TeleportController(ulong entity, Vec3 pos)
        {
            Native.Boom_API_TeleportController(entity, ref pos);
        }

        public static ulong[] GetControllerTriggerOverlaps(ulong entity)
        {
            return Native.Boom_API_GetControllerTriggerOverlaps(entity) ?? new ulong[0];
        }

        public static ulong GetStandingOnEntity(ulong entity)
        {
            return Native.Boom_API_GetControllerStandingOn(entity);
        }

        // ========= SPOTLIGHT COMPONENT METHODS =========
        public static bool HasSpotLight(ulong entity) => Native.Boom_API_HasSpotLight(entity);

        public static Vec3 GetSpotLightColor(ulong entity)
        {
            Native.Boom_API_GetSpotLightColor(entity, out Vec3 color);
            return color;
        }

        public static void SetSpotLightColor(ulong entity, Vec3 color)
        {
            Native.Boom_API_SetSpotLightColor(entity, ref color);
        }

        public static float GetSpotLightIntensity(ulong entity) => Native.Boom_API_GetSpotLightIntensity(entity);

        public static void SetSpotLightIntensity(ulong entity, float intensity)
        {
            Native.Boom_API_SetSpotLightIntensity(entity, intensity);
        }

        // ========= POINTLIGHT COMPONENT METHODS =========
        public static bool HasPointLight(ulong entity) => Native.Boom_API_HasPointLight(entity);

        public static Vec3 GetPointLightColor(ulong entity)
        {
            Native.Boom_API_GetPointLightColor(entity, out Vec3 color);
            return color;
        }

        public static void SetPointLightColor(ulong entity, Vec3 color)
        {
            Native.Boom_API_SetPointLightColor(entity, ref color);
        }

        public static float GetPointLightIntensity(ulong entity) => Native.Boom_API_GetPointLightIntensity(entity);

        public static void SetPointLightIntensity(ulong entity, float intensity)
        {
            Native.Boom_API_SetPointLightIntensity(entity, intensity);
        }

        // ========= DIRECTLIGHT COMPONENT METHODS =========
        public static bool HasDirectLight(ulong entity) => Native.Boom_API_HasDirectLight(entity);

        public static Vec3 GetDirectLightColor(ulong entity)
        {
            Native.Boom_API_GetDirectLightColor(entity, out Vec3 color);
            return color;
        }

        public static void SetDirectLightColor(ulong entity, Vec3 color)
        {
            Native.Boom_API_SetDirectLightColor(entity, ref color);
        }

        public static float GetDirectLightIntensity(ulong entity) => Native.Boom_API_GetDirectLightIntensity(entity);

        public static void SetDirectLightIntensity(ulong entity, float intensity)
        {
            Native.Boom_API_SetDirectLightIntensity(entity, intensity);
        }

        // ========== VIDEO COMPONENT API ==========

        /// <summary>
        /// Check if entity has a VideoComponent
        /// </summary>
        public static bool HasVideoComponent(ulong entity) => Native.Boom_API_HasVideoComponent(entity);

        /// <summary>
        /// Check if the video is currently playing
        /// </summary>
        public static bool IsVideoPlaying(ulong entity) => Native.Boom_API_IsVideoPlaying(entity);

        /// <summary>
        /// Check if the video has finished playing (reached the end)
        /// </summary>
        public static bool HasVideoEnded(ulong entity) => Native.Boom_API_HasVideoEnded(entity);

        /// <summary>
        /// Get the total duration of the video in seconds
        /// </summary>
        public static double GetVideoDuration(ulong entity) => Native.Boom_API_GetVideoDuration(entity);

        /// <summary>
        /// Get the current playback time in seconds
        /// </summary>
        public static double GetVideoCurrentTime(ulong entity) => Native.Boom_API_GetVideoCurrentTime(entity);

        // ========== TEXT COMPONENT API ==========

        /// <summary>
        /// Check if entity has a TextComponent
        /// </summary>
        public static bool HasText(ulong entity) => Native.Boom_API_HasText(entity);

        /// <summary>
        /// Get the text content from a TextComponent
        /// </summary>
        public static string GetText(ulong entity)
        {
            Native.Boom_API_GetText(entity, out string text);
            return text;
        }

        /// <summary>
        /// Set the text content of a TextComponent
        /// </summary>
        public static void SetText(ulong entity, string text)
        {
            if (!HasText(entity))
            {
                Console.WriteLine($"[API] Warning: Entity {entity} has no TextComponent");
                return;
            }
            Native.Boom_API_SetText(entity, text);
        }

        /// <summary>
        /// Get the color of a TextComponent (RGBA)
        /// </summary>
        public static Vec4 GetTextColor(ulong entity)
        {
            Native.Boom_API_GetTextColor(entity, out Vec4 color);
            return color;
        }

        /// <summary>
        /// Set the color of a TextComponent (RGBA)
        /// </summary>
        public static void SetTextColor(ulong entity, Vec4 color)
        {
            Native.Boom_API_SetTextColor(entity, ref color);
        }

        /// <summary>
        /// Get the scale/size multiplier of text
        /// </summary>
        public static float GetTextScale(ulong entity) => Native.Boom_API_GetTextScale(entity);

        /// <summary>
        /// Get the total height of a multi-line text block in screen pixels
        /// </summary>
        public static float GetTextHeight(ulong entity) => Native.Boom_API_GetTextHeight(entity);

        /// <summary>
        /// Set the scale/size multiplier of text
        /// </summary>
        public static void SetTextScale(ulong entity, float scale)
        {
            Native.Boom_API_SetTextScale(entity, scale);
        }

        /// <summary>
        /// Get the screen position of text (2D pixel coordinates)
        /// </summary>
        public static Vec2 GetTextPosition(ulong entity)
        {
            Native.Boom_API_GetTextPosition(entity, out Vec2 pos);
            return pos;
        }

        /// <summary>
        /// Set the screen position of text (2D pixel coordinates)
        /// </summary>
        public static void SetTextPosition(ulong entity, Vec2 pos)
        {
            Native.Boom_API_SetTextPosition(entity, ref pos);
        }

        public enum TextAlignment
        {
            Left = 0,
            Center = 1,
            Right = 2
        }

        /// <summary>
        /// Get the alignment of text
        /// </summary>
        public static TextAlignment GetTextAlignment(ulong entity) => (TextAlignment)Native.Boom_API_GetTextAlignment(entity);

        /// <summary>
        /// Set the alignment of text
        /// </summary>
        public static void SetTextAlignment(ulong entity, TextAlignment alignment)
        {
            Native.Boom_API_SetTextAlignment(entity, (int)alignment);
        }

        // Video
        public static void PlayVideo(ulong entity)
        {
            if (HasTransform(entity)) // Simple check to ensure entity is valid
                Native.Boom_API_PlayVideoComponent(entity);
        }

        public static void StopVideo(ulong entity)
        {
            if (HasTransform(entity)) // Simple check to ensure entity is valid
                Native.Boom_API_StopVideoComponent(entity);
        }

        public static void GetViewportSize(out float width, out float height)
        {
            Native.Boom_API_GetViewportSize(out width, out height);
        }

        public static void SetVideoRemoveBlack(ulong entity, bool enabled)
        {
            Native.Boom_API_SetVideoRemoveBlack(entity, enabled);
        }

        /// <summary>
        /// Set video brightness multiplier (0.0 = black, 1.0 = normal, >1.0 = overbright)
        /// </summary>
        public static void SetVideoBrightness(ulong entity, float brightness)
        {
            Native.Boom_API_SetVideoBrightness(entity, brightness);
        }

        /// <summary>
        /// Get the current video brightness multiplier
        /// </summary>
        public static float GetVideoBrightness(ulong entity)
        {
            return Native.Boom_API_GetVideoBrightness(entity);
        }

        // Cutscene
        public static void SetCutsceneMode(bool active) => Native.Boom_API_SetCutsceneMode(active);

        public static void DrawDebugLine(Vec3 start, Vec3 end, Vec3 color) => Native.Boom_API_DrawDebugLine(start, end, color);

        // ========= PARTICLE EMITTER =========

        /// <summary>Check if entity has a ParticleEmitterComponent</summary>
        public static bool HasParticleEmitter(ulong entity) => Native.Boom_API_HasParticleEmitter(entity);

        /// <summary>Start playing the particle emitter (resets timer)</summary>
        public static void PlayParticleEmitter(ulong entity) => Native.Boom_API_PlayParticleEmitter(entity);

        /// <summary>Stop the particle emitter</summary>
        public static void StopParticleEmitter(ulong entity) => Native.Boom_API_StopParticleEmitter(entity);

        /// <summary>Check if the particle emitter is currently playing</summary>
        public static bool IsParticleEmitterPlaying(ulong entity) => Native.Boom_API_IsParticleEmitterPlaying(entity);

        /// <summary>Set particles emitted per second</summary>
        public static void SetParticleEmissionRate(ulong entity, float rate) => Native.Boom_API_SetParticleEmissionRate(entity, rate);

        /// <summary>Get particles emitted per second</summary>
        public static float GetParticleEmissionRate(ulong entity) => Native.Boom_API_GetParticleEmissionRate(entity);

        /// <summary>Set the start color of particles (RGBA 0-1)</summary>
        public static void SetParticleStartColor(ulong entity, float r, float g, float b, float a) => Native.Boom_API_SetParticleStartColor(entity, r, g, b, a);

        /// <summary>Set the end color of particles (RGBA 0-1)</summary>
        public static void SetParticleEndColor(ulong entity, float r, float g, float b, float a) => Native.Boom_API_SetParticleEndColor(entity, r, g, b, a);

        /// <summary>Set gravity multiplier for particles (-9.81 = normal gravity)</summary>
        public static void SetParticleGravity(ulong entity, float gravity) => Native.Boom_API_SetParticleGravity(entity, gravity);

        /// <summary>Set min/max initial speed of particles</summary>
        public static void SetParticleSpeed(ulong entity, float speedMin, float speedMax) => Native.Boom_API_SetParticleSpeed(entity, speedMin, speedMax);

        /// <summary>Set particle size over lifetime (startMin, startMax, endSize)</summary>
        public static void SetParticleSize(ulong entity, float startMin, float startMax, float endSize) => Native.Boom_API_SetParticleSize(entity, startMin, startMax, endSize);

        /// <summary>Add a ParticleEmitterComponent to an entity at runtime (no-op if it already has one)</summary>
        public static void AddParticleEmitter(ulong entity) => Native.Boom_API_AddParticleEmitter(entity);

        /// <summary>Set spawn shape: 0=point, 1=sphere, 2=cone, 3=box, 4=spotlight volume</summary>
        public static void SetParticleShapeType(ulong entity, int shapeType) => Native.Boom_API_SetParticleShapeType(entity, shapeType);

        /// <summary>Set cone/spotlight half-angle in degrees</summary>
        public static void SetParticleShapeAngle(ulong entity, float angle) => Native.Boom_API_SetParticleShapeAngle(entity, angle);

        /// <summary>Set spotlight volume range (how far the cone extends)</summary>
        public static void SetParticleShapeRange(ulong entity, float range) => Native.Boom_API_SetParticleShapeRange(entity, range);

        /// <summary>Set sphere/cone radius for particle spawn shape</summary>
        public static void SetParticleShapeRadius(ulong entity, float radius) => Native.Boom_API_SetParticleShapeRadius(entity, radius);

        /// <summary>Set box half-extents for box spawn shape (shapeType=3)</summary>
        public static void SetParticleShapeSize(ulong entity, float x, float y, float z) => Native.Boom_API_SetParticleShapeSize(entity, x, y, z);

        /// <summary>Set whether the particle emitter loops</summary>
        public static void SetParticleLooping(ulong entity, bool looping) => Native.Boom_API_SetParticleLooping(entity, looping);

        /// <summary>Set additive blending (true for fire/sparks, false for smoke/dust)</summary>
        public static void SetParticleAdditiveBlend(ulong entity, bool additive) => Native.Boom_API_SetParticleAdditiveBlend(entity, additive);

        /// <summary>Set max particle pool size</summary>
        public static void SetParticleMaxParticles(ulong entity, int maxParticles) => Native.Boom_API_SetParticleMaxParticles(entity, maxParticles);

        /// <summary>Set particle emission direction (local space)</summary>
        public static void SetParticleDirection(ulong entity, float x, float y, float z) => Native.Boom_API_SetParticleDirection(entity, x, y, z);

        /// <summary>Set particle lifetime range in seconds</summary>
        public static void SetParticleLifetime(ulong entity, float lifetimeMin, float lifetimeMax) => Native.Boom_API_SetParticleLifetime(entity, lifetimeMin, lifetimeMax);

        // ===== GLFW key codes =====
        public const int KEY_A = 65;
        public const int KEY_S = 83;
        public const int KEY_D = 68;
        public const int KEY_W = 87;
        public const int KEY_E = 69;
        public const int KEY_F = 70;
        public const int KEY_G = 71;
        public const int KEY_H = 72;
        public const int KEY_I = 73;
        public const int KEY_K = 75;
        public const int KEY_L = 76;
        public const int KEY_M = 77;
        public const int KEY_P = 80;
        public const int KEY_Q = 81;
        public const int KEY_R = 82;
        public const int KEY_Y = 89;
        public const int KEY_SPACE = 32;
        public const int KEY_LEFT = 263;
        public const int KEY_RIGHT = 262;
        public const int KEY_UP = 265;
        public const int KEY_DOWN = 264;
        public const int KEY_LEFT_CONTROL = 341;
        public const int KEY_LEFT_SHIFT = 340;
        public const int KEY_ESCAPE = 256;
        public const int KEY_ENTER = 257;
        public const int KEY_F1 = 290;
        public const int KEY_F2 = 291;
        public const int KEY_F3 = 292;
        public const int KEY_F4 = 293;
        public const int KEY_F5 = 294;
        public const int KEY_F6 = 295;
        public const int KEY_F7 = 296;
        public const int KEY_F8 = 297;
        public const int KEY_LEFT_BRACKET = 91;
        public const int KEY_RIGHT_BRACKET = 93;

        public const int MOUSE_LEFT = 0;
        public const int MOUSE_RIGHT = 1;
        public const int MOUSE_MIDDLE = 2;

        // ===== Gamepad Button Constants =====
        public const int GAMEPAD_BUTTON_A = 0;
        public const int GAMEPAD_BUTTON_B = 1;
        public const int GAMEPAD_BUTTON_X = 2;
        public const int GAMEPAD_BUTTON_Y = 3;
        public const int GAMEPAD_BUTTON_LEFT_BUMPER = 4;
        public const int GAMEPAD_BUTTON_RIGHT_BUMPER = 5;
        public const int GAMEPAD_BUTTON_BACK = 6;
        public const int GAMEPAD_BUTTON_START = 7;
        public const int GAMEPAD_BUTTON_GUIDE = 8;
        public const int GAMEPAD_BUTTON_LEFT_THUMB = 9;
        public const int GAMEPAD_BUTTON_RIGHT_THUMB = 10;
        public const int GAMEPAD_BUTTON_DPAD_UP = 11;
        public const int GAMEPAD_BUTTON_DPAD_RIGHT = 12;
        public const int GAMEPAD_BUTTON_DPAD_DOWN = 13;
        public const int GAMEPAD_BUTTON_DPAD_LEFT = 14;

        // ===== Gamepad Axis Constants =====
        public const int GAMEPAD_AXIS_LEFT_X = 0;
        public const int GAMEPAD_AXIS_LEFT_Y = 1;
        public const int GAMEPAD_AXIS_RIGHT_X = 2;
        public const int GAMEPAD_AXIS_RIGHT_Y = 3;
        public const int GAMEPAD_AXIS_LEFT_TRIGGER = 4;
        public const int GAMEPAD_AXIS_RIGHT_TRIGGER = 5;

        // ===== Application State Constants =====
        public const int APP_STATE_RUNNING = 0;
        public const int APP_STATE_PAUSED = 1;
        public const int APP_STATE_STOPPED = 2;
    }

    // Helper static class for legacy code (optional - can be removed if not used elsewhere)
    public static class Transform
    {
        public static Vec3 GetPosition(ulong handle) => API.GetPosition(handle);
        public static void SetPosition(ulong handle, Vec3 p) => API.SetPosition(handle, p);
    }
}

namespace GameScripts
{
    public static class ScriptRegistry
    {
        public static string[] GetAvailableScriptTypes()
        {
            try
            {
                var scriptTypes = System.Reflection.Assembly.GetExecutingAssembly()
                    .GetTypes()
                    .Where(t => t.IsClass && !t.IsAbstract && IsScriptType(t))
                    .Select(t => t.FullName ?? t.Name)
                    .OrderBy(name => name)
                    .ToArray();

                return scriptTypes;
            }
            catch (Exception ex)
            {
                Boom.API.Log($"[C# ScriptRegistry] Error getting script types: {ex.Message}");
                return Array.Empty<string>();
            }
        }

        private static bool IsScriptType(Type type)
        {
            bool hasOnStart = type.GetMethod("OnStart",
                BindingFlags.Public | BindingFlags.Instance,
                null, new[] { typeof(string) }, null) != null;

            bool hasOnUpdate = type.GetMethod("OnUpdate",
                BindingFlags.Public | BindingFlags.Instance,
                null, new[] { typeof(float) }, null) != null;

            bool hasOnDestroy = type.GetMethod("OnDestroy",
                BindingFlags.Public | BindingFlags.Instance,
                null, Type.EmptyTypes, null) != null;

            return hasOnStart || hasOnUpdate || hasOnDestroy;
        }

        /// <summary>
        /// Get all exposed fields for a script type.
        /// Returns JSON array with field info.
        /// </summary>
        public static string GetExposedFieldsJson(string typeName)
        {
            try
            {
                Type type = FindType(typeName);
                if (type == null)
                {
                    Boom.API.Log($"[ScriptRegistry] Type not found: {typeName}");
                    return "[]";
                }

                var fields = type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)
                    .Where(f => f.GetCustomAttribute<Boom.EditorExposedAttribute>() != null)
                    .Select(f =>
                    {
                        var attr = f.GetCustomAttribute<Boom.EditorExposedAttribute>();
                        return new
                        {
                            fieldName = f.Name,
                            displayName = attr.DisplayName ?? FormatFieldName(f.Name),
                            typeName = GetSimpleTypeName(f.FieldType),
                            tooltip = attr.Tooltip ?? "",
                            minValue = attr.MinValue,
                            maxValue = attr.MaxValue,
                            useSlider = attr.UseSlider,
                            options = attr.Options  // may be null
                        };
                    })
                    .ToArray();

                // Simple JSON serialization
                var json = "[" + string.Join(",", fields.Select(f =>
                {
                    // Serialize options array (or empty array if null)
                    string optionsJson;
                    if (f.options != null && f.options.Length > 0)
                        optionsJson = "[" + string.Join(",", f.options.Select(o => $"\"{EscapeJson(o)}\"")) + "]";
                    else
                        optionsJson = "[]";

                    return $"{{\"fieldName\":\"{f.fieldName}\"," +
                           $"\"displayName\":\"{EscapeJson(f.displayName)}\"," +
                           $"\"typeName\":\"{f.typeName}\"," +
                           $"\"tooltip\":\"{EscapeJson(f.tooltip)}\"," +
                           $"\"minValue\":{f.minValue}," +
                           $"\"maxValue\":{f.maxValue}," +
                           $"\"useSlider\":{f.useSlider.ToString().ToLower()}," +
                           $"\"options\":{optionsJson}}}";
                })) + "]";

                return json;
            }
            catch (Exception ex)
            {
                Boom.API.Log($"[ScriptRegistry] Error getting exposed fields: {ex.Message}");
                return "[]";
            }
        }

        /// <summary>
        /// Get the value of a field from a script instance (by GC handle)
        /// </summary>
        public static string GetFieldValueJson(object instance, string fieldName)
        {
            try
            {
                if (instance == null) return "null";

                Type type = instance.GetType();
                FieldInfo field = type.GetField(fieldName,
                    BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);

                if (field == null) return "null";

                object value = field.GetValue(instance);
                return SerializeValue(value, field.FieldType);
            }
            catch (Exception ex)
            {
                Boom.API.Log($"[ScriptRegistry] Error getting field value: {ex.Message}");
                return "null";
            }
        }

        /// <summary>
        /// Set the value of a field on a script instance
        /// </summary>
        public static bool SetFieldValue(object instance, string fieldName, string valueJson)
        {
            try
            {
                if (instance == null) return false;

                Type type = instance.GetType();
                FieldInfo field = type.GetField(fieldName,
                    BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);

                if (field == null)
                {
                    Boom.API.Log($"[ScriptRegistry] Field not found: {fieldName}");
                    return false;
                }

                object value = DeserializeValue(valueJson, field.FieldType);
                field.SetValue(instance, value);
                return true;
            }
            catch (Exception ex)
            {
                Boom.API.Log($"[ScriptRegistry] Error setting field value: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Apply saved params from ScriptComponent.Params to exposed fields on an instance.
        /// Called after instance creation to restore serialized field values.
        /// </summary>
        public static int ApplyParamsToExposedFields(object instance, string paramsJson)
        {
            if (instance == null || string.IsNullOrEmpty(paramsJson))
                return 0;

            try
            {
                Type type = instance.GetType();
                int appliedCount = 0;

                // Get all exposed fields for this type
                var exposedFields = type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)
                    .Where(f => f.GetCustomAttribute<Boom.EditorExposedAttribute>() != null)
                    .ToList();

                if (exposedFields.Count == 0)
                    return 0;

                // Parse the params JSON
                // Expected format: {"fieldName": value, "fieldName2": value2, ...}
                var paramsDict = ParseParamsJson(paramsJson);

                foreach (var field in exposedFields)
                {
                    if (paramsDict.TryGetValue(field.Name, out string valueJson))
                    {
                        try
                        {
                            object value = DeserializeValue(valueJson, field.FieldType);
                            field.SetValue(instance, value);
                            appliedCount++;
                            Boom.API.Log($"[ScriptRegistry] Applied param {field.Name} = {valueJson}");
                        }
                        catch (Exception ex)
                        {
                            Boom.API.Log($"[ScriptRegistry] Error applying param {field.Name}: {ex.Message}");
                        }
                    }
                }

                return appliedCount;
            }
            catch (Exception ex)
            {
                Boom.API.Log($"[ScriptRegistry] Error applying params: {ex.Message}");
                return 0;
            }
        }

        /// <summary>
        /// Parse a JSON object into a dictionary of field name -> JSON value strings
        /// </summary>
        private static System.Collections.Generic.Dictionary<string, string> ParseParamsJson(string json)
        {
            var result = new System.Collections.Generic.Dictionary<string, string>();

            if (string.IsNullOrEmpty(json) || json == "{}" || json == "null")
                return result;

            json = json.Trim();
            if (!json.StartsWith("{") || !json.EndsWith("}"))
                return result;

            // Remove outer braces
            json = json.Substring(1, json.Length - 2).Trim();
            if (string.IsNullOrEmpty(json))
                return result;

            // Simple JSON parsing - handle nested objects and strings properly
            int i = 0;
            while (i < json.Length)
            {
                // Skip whitespace
                while (i < json.Length && char.IsWhiteSpace(json[i])) i++;
                if (i >= json.Length) break;

                // Expect a quoted key
                if (json[i] != '"') { i++; continue; }
                i++; // skip opening quote

                // Read key
                int keyStart = i;
                while (i < json.Length && json[i] != '"') i++;
                string key = json.Substring(keyStart, i - keyStart);
                i++; // skip closing quote

                // Skip to colon
                while (i < json.Length && json[i] != ':') i++;
                i++; // skip colon

                // Skip whitespace
                while (i < json.Length && char.IsWhiteSpace(json[i])) i++;

                // Read value (could be string, number, bool, object, or array)
                string value = ReadJsonValue(json, ref i);
                if (!string.IsNullOrEmpty(key))
                {
                    result[key] = value;
                }

                // Skip to comma or end
                while (i < json.Length && json[i] != ',' && json[i] != '}') i++;
                if (i < json.Length && json[i] == ',') i++;
            }

            return result;
        }

        /// <summary>
        /// Read a JSON value starting at position i, advance i past the value
        /// </summary>
        private static string ReadJsonValue(string json, ref int i)
        {
            if (i >= json.Length) return "";

            char c = json[i];

            // String
            if (c == '"')
            {
                int start = i;
                i++; // skip opening quote
                while (i < json.Length)
                {
                    if (json[i] == '"' && json[i - 1] != '\\')
                    {
                        i++; // skip closing quote
                        break;
                    }
                    i++;
                }
                return json.Substring(start, i - start);
            }

            // Object
            if (c == '{')
            {
                int start = i;
                int depth = 0;
                while (i < json.Length)
                {
                    if (json[i] == '{') depth++;
                    else if (json[i] == '}') { depth--; if (depth == 0) { i++; break; } }
                    i++;
                }
                return json.Substring(start, i - start);
            }

            // Array
            if (c == '[')
            {
                int start = i;
                int depth = 0;
                while (i < json.Length)
                {
                    if (json[i] == '[') depth++;
                    else if (json[i] == ']') { depth--; if (depth == 0) { i++; break; } }
                    i++;
                }
                return json.Substring(start, i - start);
            }

            // Number, bool, or null
            int valStart = i;
            while (i < json.Length && json[i] != ',' && json[i] != '}' && json[i] != ']' && !char.IsWhiteSpace(json[i]))
            {
                i++;
            }
            return json.Substring(valStart, i - valStart);
        }

        private static Type FindType(string typeName)
        {
            // Try direct lookup first
            Type type = Type.GetType(typeName);
            if (type != null) return type;

            // Search in all loaded assemblies
            foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                type = asm.GetType(typeName);
                if (type != null) return type;
            }

            return null;
        }

        private static string FormatFieldName(string name)
        {
            // Convert _camelCase or m_camelCase to "Camel Case"
            if (name.StartsWith("_")) name = name.Substring(1);
            if (name.StartsWith("m_")) name = name.Substring(2);

            var result = new System.Text.StringBuilder();
            for (int i = 0; i < name.Length; i++)
            {
                char c = name[i];
                if (i > 0 && char.IsUpper(c))
                    result.Append(' ');
                result.Append(i == 0 ? char.ToUpper(c) : c);
            }
            return result.ToString();
        }

        private static string GetSimpleTypeName(Type type)
        {
            if (type == typeof(int)) return "int";
            if (type == typeof(float)) return "float";
            if (type == typeof(double)) return "double";
            if (type == typeof(bool)) return "bool";
            if (type == typeof(string)) return "string";
            if (type == typeof(Boom.Vec2)) return "Vec2";
            if (type == typeof(Boom.Vec3)) return "Vec3";
            if (type == typeof(Boom.Vec4)) return "Vec4";
            if (type == typeof(ulong)) return "ulong";
            if (type == typeof(long)) return "long";
            return type.Name;
        }

        private static string EscapeJson(string s)
        {
            if (s == null) return "";
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "\\r");
        }

        private static string SerializeValue(object value, Type type)
        {
            if (value == null) return "null";

            if (type == typeof(int) || type == typeof(long) || type == typeof(ulong))
                return value.ToString();
            if (type == typeof(float))
                return ((float)value).ToString(System.Globalization.CultureInfo.InvariantCulture);
            if (type == typeof(double))
                return ((double)value).ToString(System.Globalization.CultureInfo.InvariantCulture);
            if (type == typeof(bool))
                return ((bool)value) ? "true" : "false";
            if (type == typeof(string))
                return $"\"{EscapeJson((string)value)}\"";
            if (type == typeof(Boom.Vec2))
            {
                var v = (Boom.Vec2)value;
                return $"{{\"X\":{v.X.ToString(System.Globalization.CultureInfo.InvariantCulture)},\"Y\":{v.Y.ToString(System.Globalization.CultureInfo.InvariantCulture)}}}";
            }
            if (type == typeof(Boom.Vec3))
            {
                var v = (Boom.Vec3)value;
                return $"{{\"X\":{v.X.ToString(System.Globalization.CultureInfo.InvariantCulture)},\"Y\":{v.Y.ToString(System.Globalization.CultureInfo.InvariantCulture)},\"Z\":{v.Z.ToString(System.Globalization.CultureInfo.InvariantCulture)}}}";
            }
            if (type == typeof(Boom.Vec4))
            {
                var v = (Boom.Vec4)value;
                return $"{{\"X\":{v.X.ToString(System.Globalization.CultureInfo.InvariantCulture)},\"Y\":{v.Y.ToString(System.Globalization.CultureInfo.InvariantCulture)},\"Z\":{v.Z.ToString(System.Globalization.CultureInfo.InvariantCulture)},\"W\":{v.W.ToString(System.Globalization.CultureInfo.InvariantCulture)}}}";
            }

            return "null";
        }

        private static object DeserializeValue(string json, Type type)
        {
            if (json == null || json == "null") return null;

            json = json.Trim();

            if (type == typeof(int))
                return int.Parse(json, System.Globalization.CultureInfo.InvariantCulture);
            if (type == typeof(long))
                return long.Parse(json, System.Globalization.CultureInfo.InvariantCulture);
            if (type == typeof(ulong))
                return ulong.Parse(json, System.Globalization.CultureInfo.InvariantCulture);
            if (type == typeof(float))
                return float.Parse(json, System.Globalization.CultureInfo.InvariantCulture);
            if (type == typeof(double))
                return double.Parse(json, System.Globalization.CultureInfo.InvariantCulture);
            if (type == typeof(bool))
                return json.ToLower() == "true";
            if (type == typeof(string))
            {
                // Remove quotes
                if (json.StartsWith("\"") && json.EndsWith("\""))
                    json = json.Substring(1, json.Length - 2);
                return json.Replace("\\\"", "\"").Replace("\\n", "\n").Replace("\\r", "\r").Replace("\\\\", "\\");
            }
            if (type == typeof(Boom.Vec2))
            {
                var parts = ParseJsonObject(json);
                return new Boom.Vec2(
                    float.Parse(parts["X"], System.Globalization.CultureInfo.InvariantCulture),
                    float.Parse(parts["Y"], System.Globalization.CultureInfo.InvariantCulture)
                );
            }
            if (type == typeof(Boom.Vec3))
            {
                var parts = ParseJsonObject(json);
                return new Boom.Vec3(
                    float.Parse(parts["X"], System.Globalization.CultureInfo.InvariantCulture),
                    float.Parse(parts["Y"], System.Globalization.CultureInfo.InvariantCulture),
                    float.Parse(parts["Z"], System.Globalization.CultureInfo.InvariantCulture)
                );
            }
            if (type == typeof(Boom.Vec4))
            {
                var parts = ParseJsonObject(json);
                return new Boom.Vec4(
                    float.Parse(parts["X"], System.Globalization.CultureInfo.InvariantCulture),
                    float.Parse(parts["Y"], System.Globalization.CultureInfo.InvariantCulture),
                    float.Parse(parts["Z"], System.Globalization.CultureInfo.InvariantCulture),
                    float.Parse(parts["W"], System.Globalization.CultureInfo.InvariantCulture)
                );
            }

            return null;
        }

        private static System.Collections.Generic.Dictionary<string, string> ParseJsonObject(string json)
        {
            var result = new System.Collections.Generic.Dictionary<string, string>();
            json = json.Trim();
            if (json.StartsWith("{")) json = json.Substring(1);
            if (json.EndsWith("}")) json = json.Substring(0, json.Length - 1);

            // Simple parsing for {"X":1.0,"Y":2.0} style
            var parts = json.Split(',');
            foreach (var part in parts)
            {
                var kv = part.Split(':');
                if (kv.Length == 2)
                {
                    string key = kv[0].Trim().Trim('"');
                    string value = kv[1].Trim();
                    result[key] = value;
                }
            }
            return result;
        }
    }
}