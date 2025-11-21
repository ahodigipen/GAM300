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
    }

    public static class API
    {
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

        /// <summary>
        /// Returns true if the rigidbody is colliding / grounded according to the engine.
        /// </summary>
        public static bool IsColliding(ulong h) => Native.Boom_API_IsColliding(h);

        public static void LoadScene(string name) => Native.Boom_API_LoadScene(name);
        public static string GetCurrentSceneName() => Native.Boom_API_GetCurrentSceneName();
        public static void QuitGame() => Native.Boom_API_QuitGame();
        public static void LoadSceneAdditive(string name) => Native.Boom_API_LoadSceneAdditive(name);
        public static void UnloadPauseMenu() => Native.Boom_API_UnloadPauseMenu();
        public static void TogglePause() => Native.Boom_API_TogglePause();
        public static int GetApplicationState() => Native.Boom_API_GetApplicationState();
        public static bool IsPauseMenuLoaded() => Native.Boom_API_IsPauseMenuLoaded();

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
