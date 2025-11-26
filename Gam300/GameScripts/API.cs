// Boom/API.cs
// Single API file for all script-side engine calls. You do NOT need a separate Native.cs.
using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Boom
{
    // Internal calls implemented in C++ and registered with Mono
    internal static class Native
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_Log(string msg);

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
        internal extern static bool Boom_API_HasTransform(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasScript(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetRotation(ulong handle, out Vec3 outRot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetRotation(ulong handle, ref Vec3 rot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_DrawDebugVisionCone(ulong entityHandle,
            float range, float angle, float r, float g, float b, float a);

        //
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetThirdPersonCameraYaw();
        //

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_HasCollider(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsTrigger(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetTrigger(ulong handle, bool isTrigger);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_RegisterTriggerEnterCallback(ulong triggerHandle, object delegateObj);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_RegisterTriggerExitCallback(ulong triggerHandle, object delegateObj);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnregisterTriggerCallbacks(ulong triggerHandle);
        // ========= NEW PHYSICS / RIGIDBODY INTERNAL CALLS =========

        // Get the current linear velocity from the physics engine
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetLinearVelocity(ulong handle, out Vec3 vel);

        // Set the current linear velocity in the physics engine
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetLinearVelocity(ulong handle, ref Vec3 vel);

        // Query if the rigidbody is currently colliding / grounded
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsColliding(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_LoadScene(string name);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern string Boom_API_GetCurrentSceneName();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_QuitGame();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_LoadSceneAdditive(string name);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_UnloadPauseMenu();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void Boom_API_TogglePause();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern int Boom_API_GetApplicationState();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern bool Boom_API_IsPauseMenuLoaded();

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
        internal static extern bool Boom_API_IsSoundPlaying(string name);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PauseSound(string name, bool pause);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PreloadSound(string name, string filePath, bool loop);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundPosition(string name, ref Vec3 position);

        // Raycasting & Main Menu
        [MethodImpl(MethodImplOptions.InternalCall)] 
        internal static extern ulong Boom_API_PickGameEntity();
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TriggerCallback(ulong triggerEntity, ulong otherEntity);

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
    }

    public static class API
    {
        // Cache trigger callbacks to prevent garbage collection
        private static System.Collections.Generic.Dictionary<ulong, TriggerCallback> s_TriggerEnterCallbacks = new System.Collections.Generic.Dictionary<ulong, TriggerCallback>();
        private static System.Collections.Generic.Dictionary<ulong, TriggerCallback> s_TriggerExitCallbacks = new System.Collections.Generic.Dictionary<ulong, TriggerCallback>();

        // ===== Logging =====
        public static void Log(string s) => Native.Boom_API_Log(s);

        // ===== Entity queries =====
        public static ulong FindEntity(string name) => Native.Boom_API_FindEntity(name);

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

        // ===== Component checking =====
        public static bool HasTransform(ulong h) => Native.Boom_API_HasTransform(h);
        public static bool HasScript(ulong h) => Native.Boom_API_HasScript(h);

        // ===== Input =====
        public static bool IsKeyDown(int glfwKey) => Native.Boom_API_IsKeyDown(glfwKey);
        public static bool IsMouseDown(int button) => Native.Boom_API_IsMouseDown(button);

        // ===== PHYSICS / RIGIDBODY HELPERS =====

        /// <summary>
        /// Get the current linear velocity of the rigidbody attached to this entity.
        /// </summary>
        public static Vec3 GetLinearVelocity(ulong h)
        {
            Native.Boom_API_GetLinearVelocity(h, out var v);
            return v;
        }

        /// <summary>
        /// Set the current linear velocity of the rigidbody attached to this entity.
        /// </summary>
        public static void SetLinearVelocity(ulong h, Vec3 v)
        {
            Native.Boom_API_SetLinearVelocity(h, ref v);
        }

        // ===== Rotation methods =====
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

        public static void DrawDebugVisionCone(ulong entityHandle, float range, float halfAngle, Vec4 color)
        {
            Native.Boom_API_DrawDebugVisionCone(entityHandle, range, halfAngle,
                color.X, color.Y, color.Z, color.W);
        }

        public static void DrawDebugVisionCone(ulong entityHandle, float range, float halfAngle)
        {
            DrawDebugVisionCone(entityHandle, range, halfAngle, new Vec4(0f, 1f, 0f, 0.3f));
        }

        public static float GetThirdPersonCameraYaw() => Native.Boom_API_GetThirdPersonCameraYaw();


        [StructLayout(LayoutKind.Sequential)]
        public struct Vec4
        {
            public float X, Y, Z, W;

            public Vec4(float x, float y, float z, float w)
            {
                X = x; Y = y; Z = z; W = w;
            }
        }
        /// <summary>
        /// Returns true if the rigidbody is colliding / grounded according to the engine.
        /// </summary>
        /// 

        public static bool HasCollider(ulong entity)
        {
            return Native.Boom_API_HasCollider(entity);
        }

        public static bool IsTrigger(ulong entity)
        {
            return Native.Boom_API_IsTrigger(entity);
        }

        public static void SetTrigger(ulong entity, bool isTrigger)
        {
            Native.Boom_API_SetTrigger(entity, isTrigger);
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
        public static bool IsColliding(ulong h) => Native.Boom_API_IsColliding(h);

        public static void LoadScene(string name) => Native.Boom_API_LoadScene(name);
        public static string GetCurrentSceneName() => Native.Boom_API_GetCurrentSceneName();
        public static void QuitGame() => Native.Boom_API_QuitGame();
        public static void LoadSceneAdditive(string name) => Native.Boom_API_LoadSceneAdditive(name);
        public static void UnloadPauseMenu() => Native.Boom_API_UnloadPauseMenu();
        public static void TogglePause() => Native.Boom_API_TogglePause();
        public static int GetApplicationState() => Native.Boom_API_GetApplicationState();
        public static bool IsPauseMenuLoaded() => Native.Boom_API_IsPauseMenuLoaded();

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

        // Raycasting
        public static ulong PickGameEntity() => Native.Boom_API_PickGameEntity();

        // ===== GLFW key codes =====
        public const int KEY_LEFT = 263;
        public const int KEY_RIGHT = 262;
        public const int KEY_UP = 265;
        public const int KEY_DOWN = 264;
        public const int KEY_W = 87;
        public const int KEY_A = 65;
        public const int KEY_S = 83;
        public const int KEY_D = 68;
        public const int KEY_SPACE = 32;

        public const int KEY_H = 72; // For How to Play

        public const int KEY_P = 80; // For Pause
        public const int KEY_R = 82; // For Resume
        public const int KEY_Y = 89; // For Restart
        public const int KEY_M = 77; // For Main Menu

        public const int KEY_Q = 81; // For Quit
        public const int KEY_LEFT_CONTROL = 341;

        public const int APP_STATE_RUNNING = 0;
        public const int APP_STATE_PAUSED = 1;
        public const int APP_STATE_STOPPED = 2;

        // ===== Mouse buttons =====
        public const int MOUSE_LEFT = 0;
        public const int MOUSE_RIGHT = 1;
        public const int MOUSE_MIDDLE = 2;
    }

    public static class Transform
    {
        public static Vec3 GetPosition(ulong handle) => API.GetPosition(handle);
        public static void SetPosition(ulong handle, Vec3 p) => API.SetPosition(handle, p);
    }
}
